/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "srsran/phy/common/timestamp.h"

#ifdef __cplusplus
}
//#undef I // Fix complex.h #define I nastiness when using C++
#endif

#include "srsran/phy/utils/debug.h"
#include "srsran/radio/radio.h"

using namespace srsran;

#define SRSRAN_MAX_RADIOS 3

static char radios_args[SRSRAN_MAX_RADIOS][64] = {"auto", "auto", "auto"};
static char radio_device[64];

static log_filter  log_h;
static std::string file_pattern = "radio%d.dat";
static double      freq         = 2630e6;
static uint32_t    nof_radios   = 1;
static uint32_t    nof_ports    = 1;
static double      srate        = 1.92e6; /* Hz */
static double      duration     = 0.01;   /* in seconds, 10 ms by default */
static cf_t*       buffers[SRSRAN_MAX_RADIOS][SRSRAN_MAX_PORTS];
static bool        tx_enable       = false;
static bool        sim_rate_change = false;
static bool        measure_delay   = false;
static bool        capture         = false;
static bool        agc_enable      = true;
static float       rf_gain         = -1.0;

#ifdef ENABLE_GUI
#include "srsgui/srsgui.h"
#include <semaphore.h>
static pthread_t   plot_thread;
static sem_t       plot_sem;
static uint32_t    plot_sf_idx                        = 0;
static plot_real_t fft_plot[SRSRAN_MAX_RADIOS]        = {};
static cf_t*       fft_plot_buffer[SRSRAN_MAX_RADIOS] = {};
static float*      fft_plot_temp                      = nullptr;
static uint32_t    fft_plot_buffer_size;
srsran_dft_plan_t  dft_spectrum = {};
#endif /* ENABLE_GUI */

static bool fft_plot_enable = false;

void usage(char* prog)
{
  printf("Usage: %s [foabcderpstvhmFxw]\n", prog);
  printf("\t-f Carrier frequency in Hz [Default %f]\n", freq);
  printf("\t-g RF gain [Default AGC]\n");
  printf("\t-a Arguments for first radio [Default %s]\n", radios_args[0]);
  printf("\t-b Arguments for second radio [Default %s]\n", radios_args[1]);
  printf("\t-c Arguments for third radio [Default %s]\n", radios_args[2]);
  printf("\t-d Radio device [Default %s]\n", radio_device);
  printf("\t-r number of radios 1-%d [Default %d]\n", SRSRAN_MAX_RADIOS, nof_radios);
  printf("\t-p number of ports 1-%d [Default %d]\n", SRSRAN_MAX_PORTS, nof_ports);
  printf("\t-s sampling rate [Default %.0f]\n", srate);
  printf("\t-t duration in seconds [Default %.3f]\n", duration);
  printf("\t-m measure delay [Default %s]\n", (measure_delay) ? "enabled" : "disabled");
  printf("\t-x enable transmit [Default %s]\n", (tx_enable) ? "enabled" : "disabled");
  printf("\t-y simulate rate changes [Default %s]\n", (sim_rate_change) ? "enabled" : "disabled");
  printf("\t-w capture [Default %s]\n", (capture) ? "enabled" : "disabled");
  printf("\t-o Output file pattern [Default %s]\n", file_pattern.c_str());
  printf("\t-F Display spectrum [Default %s]\n", (fft_plot_enable) ? "enabled" : "disabled");
  printf("\t-v Set srsran_verbose to info (v) or debug (vv) [Default none]\n");
  printf("\t-h show this message\n");
}

void parse_args(int argc, char** argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "foabcderpsStvhmFxywg")) != -1) {
    switch (opt) {
      case 'f':
        freq = strtof(argv[optind], NULL);
        break;
      case 'g':
        rf_gain    = strtof(argv[optind], NULL);
        agc_enable = false;
        break;
      case 'o':
        file_pattern = argv[optind];
        break;
      case 'a':
        strncpy(radios_args[0], argv[optind], 63);
        radios_args[0][63] = '\0';
        break;
      case 'b':
        strncpy(radios_args[1], argv[optind], 63);
        radios_args[1][63] = '\0';
        break;
      case 'c':
        strncpy(radios_args[2], argv[optind], 63);
        radios_args[2][63] = '\0';
        break;
      case 'd':
        strncpy(radio_device, argv[optind], 63);
        radio_device[63] = '\0';
        break;
      case 'r':
        nof_radios = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'p':
        nof_ports = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 's':
        srate = strtof(argv[optind], NULL);
        break;
      case 't':
        duration = strtof(argv[optind], NULL);
        break;
      case 'm':
        measure_delay ^= true;
        break;
      case 'x':
        tx_enable ^= true;
        break;
      case 'y':
        sim_rate_change ^= true;
        break;
      case 'w':
        capture ^= true;
        break;
      case 'F':
        fft_plot_enable ^= true;
        break;
      case 'v':
        srsran_verbose++;
        break;
      case 'h':
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

static SRSRAN_AGC_CALLBACK(set_gain_callback)
{
  auto r = (radio*)h;
  r->set_rx_gain_th(gain_db);
}

#ifdef ENABLE_GUI

static void* plot_thread_run(void* arg)
{
  sdrgui_init();

  for (uint32_t i = 0; i < nof_radios; i++) {
    char str_buf[32] = {};
    snprintf(str_buf, 32, "Radio %d spectrum", i);
    plot_real_init(&fft_plot[i]);
    plot_real_setTitle(&fft_plot[i], str_buf);
    plot_real_setXAxisAutoScale(&fft_plot[i], true);
    plot_real_setYAxisAutoScale(&fft_plot[i], true);

    plot_scatter_addToWindowGrid(&fft_plot[i], (char*)"pdsch_ue", 0, i);
  }

  while (fft_plot_enable) {
    sem_wait(&plot_sem);

    if (fft_plot_buffer_size) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_vec_abs_square_cf(fft_plot_buffer[r], fft_plot_temp, fft_plot_buffer_size);

        for (uint32_t j = 0; j < fft_plot_buffer_size; j++) {
          fft_plot_temp[j] = srsran_convert_power_to_dB(fft_plot_temp[j]);
        }

        plot_real_setNewData(&fft_plot[r], fft_plot_temp, fft_plot_buffer_size);
      }
    }
  }

  return nullptr;
}

static int init_plots(uint32_t frame_size)
{

  if (sem_init(&plot_sem, 0, 0)) {
    perror("sem_init");
    exit(-1);
  }

  for (uint32_t r = 0; r < nof_radios; r++) {
    fft_plot_buffer[r] = srsran_vec_cf_malloc(frame_size);
    if (!fft_plot_buffer[r]) {
      ERROR("Error: Allocating buffer\n");
      return SRSRAN_ERROR;
    }
  }

  fft_plot_temp = srsran_vec_f_malloc(frame_size);
  if (!fft_plot_temp) {
    ERROR("Error: Allocating buffer\n");
    return SRSRAN_ERROR;
  }

  if (srsran_dft_plan_c(&dft_spectrum, frame_size, SRSRAN_DFT_FORWARD)) {
    ERROR("Creating DFT spectrum plan\n");
    return SRSRAN_ERROR;
  }

  srsran_dft_plan_set_mirror(&dft_spectrum, true);
  fft_plot_buffer_size = frame_size;

  pthread_attr_t     attr;
  struct sched_param param;
  param.sched_priority = 0;
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
  pthread_attr_setschedparam(&attr, &param);
  if (pthread_create(&plot_thread, NULL, plot_thread_run, NULL)) {
    perror("pthread_create");
    exit(-1);
  }

  return SRSRAN_SUCCESS;
}

#endif /* ENABLE_GUI */

class phy_dummy : public phy_interface_radio
{
public:
  void radio_overflow() { num_overflows++; }
  void radio_failure() { num_failures++; };

private:
  uint32_t num_overflows = 0;
  uint32_t num_failures  = 0;
};

int main(int argc, char** argv)
{
  int                ret                        = SRSRAN_ERROR;
  radio*             radio_h[SRSRAN_MAX_RADIOS] = {NULL};
  srsran_timestamp_t ts_prev[SRSRAN_MAX_RADIOS], ts_rx[SRSRAN_MAX_RADIOS], ts_tx;
  uint32_t           nof_gaps                    = 0;
  char               filename[256]               = {};
  srsran_filesink_t  filesink[SRSRAN_MAX_RADIOS] = {};
  srsran_dft_plan_t  dft_plan = {}, idft_plan = {};
  srsran_agc_t       agc[SRSRAN_MAX_RADIOS] = {};
  phy_dummy          phy;

  bzero(&ts_prev, sizeof(ts_prev));
  bzero(&ts_rx, sizeof(ts_rx));
  bzero(&ts_tx, sizeof(ts_tx));

  rf_buffer_t rf_buffers[SRSRAN_MAX_RADIOS] = {};

  float    delay_idx[SRSRAN_MAX_RADIOS] = {0};
  uint32_t delay_count                  = 0;

  /* Parse args */
  parse_args(argc, argv);

  uint32_t nof_samples = (uint32_t)(duration * srate);
  uint32_t frame_size  = (uint32_t)(srate / 1000.0); /* 1 ms at srate */
  uint32_t nof_frames  = (uint32_t)ceil(nof_samples / frame_size);

  /* Instanciate and allocate memory */
  printf("Instantiating objects and allocating memory...\n");
  for (uint32_t r = 0; r < nof_radios; r++) {
    radio_h[r] = new radio(&log_h);
    if (!radio_h[r]) {
      fprintf(stderr, "Error: Calling radio constructor\n");
      goto clean_exit;
    }

    for (uint32_t p = 0; p < SRSRAN_MAX_PORTS; p++) {
      buffers[r][p] = NULL;
    }
  }

  for (uint32_t r = 0; r < nof_radios; r++) {
    for (uint32_t p = 0; p < nof_ports; p++) {
      buffers[r][p] = srsran_vec_cf_malloc(frame_size);
      if (!buffers[r][p]) {
        ERROR("Error: Allocating buffer (%d,%d)\n", r, p);
        goto clean_exit;
      }
    }
  }

#ifdef ENABLE_GUI
  if (fft_plot_enable) {
    init_plots(frame_size);
    sleep(1);
  }
#endif /* ENABLE_GUI */

  /* Initialise instances */
  printf("Initialising instances...\n");
  for (uint32_t r = 0; r < nof_radios; r++) {
    rf_args_t radio_args    = {};
    radio_args.nof_antennas = nof_ports;
    radio_args.nof_carriers = 1;
    radio_args.device_args  = radios_args[r];
    radio_args.rx_gain      = agc_enable ? -1 : rf_gain;
    radio_args.device_name  = radio_device;

    if (radio_h[r]->init(radio_args, &phy) != SRSRAN_SUCCESS) {
      fprintf(stderr, "Error: Calling radio_multi constructor\n");
      goto clean_exit;
    }

    radio_h[r]->set_rx_freq(0, freq);

    // enable and init agc
    if (agc_enable) {
      if (srsran_agc_init_uhd(&agc[r], SRSRAN_AGC_MODE_PEAK_AMPLITUDE, 0, set_gain_callback, radio_h[r])) {
        fprintf(stderr, "Error: Initiating AGC %d\n", r);
        goto clean_exit;
      }
    }

    // Set Rx/Tx sampling rate
    radio_h[r]->set_rx_srate(srate);
    if (tx_enable) {
      radio_h[r]->set_tx_srate(srate);
    }
  }

  /* Setup file sink */
  if (capture) {
    for (uint32_t r = 0; r < nof_radios; r++) {
      snprintf(filename, 256, file_pattern.c_str(), r);
      INFO("Opening filesink %s for radio %d\n", filename, r);
      if (srsran_filesink_init(&filesink[r], filename, SRSRAN_COMPLEX_FLOAT_BIN)) {
        ERROR("Initiating filesink for radio %d\n", r);
        goto clean_exit;
      }
    }
  }

  /* If measure delay between radios */
  if (measure_delay) {
    if (nof_radios > 1) {
      if (srsran_dft_plan_c(&dft_plan, frame_size, SRSRAN_DFT_FORWARD)) {
        ERROR("Creating DFT plan\n");
        goto clean_exit;
      }
      if (srsran_dft_plan_c(&idft_plan, frame_size, SRSRAN_DFT_BACKWARD)) {
        ERROR("Creating IDFT plan\n");
        goto clean_exit;
      }
    } else {
      printf("Warning: the delay measure cannot be performed with only one radio. Disabling delay measurement.\n");
      measure_delay = false;
    }
  }

  /* Receive */
  printf("Start capturing %d frames of %d samples...\n", nof_frames, frame_size);

  for (int i = 0; i < SRSRAN_MAX_RADIOS; i++) {
    for (int j = 0; j < SRSRAN_MAX_PORTS; j++) {
      rf_buffers[i].set(j, buffers[i][j]);
    }
  }

  for (uint32_t i = 0; i < nof_frames; i++) {
    if (sim_rate_change) {
      if (i % 1000 == 0) {
        // toggle rate between cell search rate and configured rate every second
        static bool srate_is_cell_search = false;
        double      new_rate             = (srate_is_cell_search = !srate_is_cell_search) ? srate : 1.92e6;
        printf("Changing sampling rate to %.2f Msamps/s\n", new_rate / 1e6);
        for (uint32_t r = 0; r < nof_radios; r++) {
          radio_h[r]->set_rx_srate(new_rate);
          if (tx_enable) {
            radio_h[r]->set_tx_srate(new_rate);
          }
        }
      }
    }

    int gap    = 0;
    frame_size = SRSRAN_MIN(frame_size, nof_samples);

    // receive each radio
    for (uint32_t r = 0; r < nof_radios; r++) {
      radio_h[r]->rx_now(rf_buffers[r], frame_size, &ts_rx[r]);
    }

    // run agc
    if (agc_enable) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_agc_process(&agc[r], buffers[r][0], frame_size);
      }
    }

    // Transmit
    if (tx_enable) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_timestamp_copy(&ts_tx, &ts_rx[r]);
        srsran_timestamp_add(&ts_tx, 0, 0.004);
        radio_h[r]->tx(rf_buffers[r], frame_size, ts_tx);
      }
    }

    /* Store baseband in file */
    if (capture) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_filesink_write_multi(&filesink[r], (void**)buffers[r], frame_size, nof_ports);
      }
    }

#ifdef ENABLE_GUI
    /* Plot fft */
    if (fft_plot_enable) {
      if (frame_size != nof_samples) {
        for (uint32_t r = 0; r < nof_radios; r++) {
          srsran_dft_run(&dft_spectrum, buffers[r][0], fft_plot_buffer[r]);
        }
      } else {
        fft_plot_enable = false;
      }
      sem_post(&plot_sem);
    }
#endif /* ENABLE_GUI */

    /* Compute delay between radios */
    if (measure_delay && frame_size != nof_samples) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_dft_run_c(&dft_plan, buffers[r][0], buffers[r][0]);
      }

      for (uint32_t r = 1; r < nof_radios; r++) {
        int relative_delay = 0;

        srsran_vec_prod_conj_ccc(buffers[0][0], buffers[r][0], buffers[r][0], frame_size);
        srsran_dft_run_c(&idft_plan, buffers[r][0], buffers[r][0]);
        relative_delay = srsran_vec_max_abs_ci(buffers[r][0], frame_size);

        if (relative_delay > (int)frame_size / 2) {
          relative_delay -= frame_size;
        }

        delay_idx[r] += relative_delay;
        INFO("Radio %d relative delay is %d sample in frame %d/%d (average %.1f)\n",
             r,
             relative_delay,
             i + 1,
             nof_frames,
             delay_idx[r] / (float)(delay_count + 1));
      }
      delay_count++;
    }

    /* Check sample gaps */
    if (i != 0) {
      for (uint32_t r = 0; r < nof_radios; r++) {
        srsran_timestamp_t ts_diff;
        bzero(&ts_diff, sizeof(ts_diff));

        srsran_timestamp_copy(&ts_diff, &ts_rx[r]);
        srsran_timestamp_sub(&ts_diff, ts_prev[r].full_secs, ts_prev[r].frac_secs);
        gap = (int)round(srsran_timestamp_real(&ts_diff) * srate) - frame_size;

        if (gap) {
          INFO("Timestamp gap (%d samples) detected! Frame %d/%d. ts=%.9f+%.9f=%.9f\n",
               gap,
               i + 1,
               nof_frames,
               srsran_timestamp_real(&ts_prev[r]),
               srsran_timestamp_real(&ts_diff),
               srsran_timestamp_real(&ts_rx[r]));
          nof_gaps++;
        }
      }
    }

    /* Save timestamp */
    for (uint32_t r = 0; r < nof_radios; r++) {
      srsran_timestamp_copy(&ts_prev[r], &ts_rx[r]);
    }

    nof_samples -= frame_size;
  }

  printf("Finished streaming with %d gaps...\n", nof_gaps);

  ret = SRSRAN_SUCCESS;

  if (measure_delay && delay_count > 0) {
    for (uint32_t r = 1; r < nof_radios; r++) {
      printf("Radio %d is delayed %.1f samples from radio 0;\n", r, delay_idx[r] / delay_count);
    }
  }

clean_exit:
  printf("Tearing down...\n");

  for (uint32_t r = 0; r < nof_radios; r++) {
    if (radio_h[r]) {
      radio_h[r]->stop();
      delete radio_h[r];
    }
  }

  for (uint32_t r = 0; r < nof_radios; r++) {
    for (uint32_t p = 0; p < nof_ports; p++) {
      if (buffers[r][p]) {
        free(buffers[r][p]);
      }
    }

    if (capture) {
      srsran_filesink_free(&filesink[r]);
    }
  }

  srsran_dft_plan_free(&dft_plan);
  srsran_dft_plan_free(&idft_plan);

#ifdef ENABLE_GUI
  pthread_join(plot_thread, NULL);
  srsran_dft_plan_free(&dft_spectrum);
  for (uint32_t r = 0; r < nof_radios; r++) {
    if (fft_plot_buffer[r]) {
      free(fft_plot_buffer[r]);
    }
  }
  if (fft_plot_temp) {
    free(fft_plot_temp);
  }
#endif /* ENABLE_GUI */

  if (ret) {
    printf("Failed!\n");
  } else {
    printf("Ok!\n");
  }

  return ret;
}
