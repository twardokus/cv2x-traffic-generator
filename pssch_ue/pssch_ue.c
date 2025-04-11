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

#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "srsran/common/pcap.h"
#include "srsran/phy/ch_estimation/chest_sl.h"
#include "srsran/phy/common/phy_common_sl.h"
#include "srsran/phy/dft/ofdm.h"
#include "srsran/phy/phch/pscch.h"
#include "srsran/phy/phch/pssch.h"
#include "srsran/phy/phch/ra_sl.h"
#include "srsran/phy/phch/sci.h"
#include "srsran/phy/rf/rf.h"
#include "srsran/phy/ue/ue_sync.h"
#include "srsran/phy/utils/bit.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"

#define PCAP_FILENAME "/tmp/pssch.pcap"

bool keep_running = true;

void pcap_pack_and_write(FILE*    pcap_file,
                         uint8_t* pdu,
                         uint32_t pdu_len_bytes,
                         uint8_t  reTX,
                         bool     crc_ok,
                         uint32_t tti,
                         uint16_t crnti,
                         uint8_t  direction,
                         uint8_t  rnti_type)
{
  MAC_Context_Info_t context = {.radioType      = FDD_RADIO,
                                .direction      = direction,
                                .rntiType       = rnti_type,
                                .rnti           = crnti,
                                .ueid           = 1,
                                .isRetx         = reTX,
                                .crcStatusOK    = crc_ok,
                                .sysFrameNumber = (uint16_t)(tti / SRSRAN_NOF_SF_X_FRAME),
                                .subFrameNumber = (uint16_t)(tti % SRSRAN_NOF_SF_X_FRAME),
                                .nbiotMode      = 0};
  if (pdu) {
    LTE_PCAP_MAC_WritePDU(pcap_file, &context, pdu, pdu_len_bytes);
  }
}

srsran_cell_sl_t cell_sl = {.nof_prb = 50, .tm = SRSRAN_SIDELINK_TM4, .cp = SRSRAN_CP_NORM, .N_sl_id = 0};

typedef struct {
  bool     use_standard_lte_rates;
  char*    log_file_name;
  uint32_t file_start_sf_idx;
  uint32_t nof_rx_antennas;
  char*    rf_dev;
  char*    rf_args;
  double   rf_freq;
  float    rf_gain;

  // Sidelink specific args
  uint32_t size_sub_channel;
  uint32_t num_sub_channel;
} prog_args_t;

void args_default(prog_args_t* args)
{
  args->use_standard_lte_rates = false;
  args->log_file_name          = NULL;
  args->file_start_sf_idx      = 0;
  args->nof_rx_antennas        = 1;
  args->rf_dev                 = "";
  args->rf_args                = "";
  args->rf_freq                = 5.92e9;
  args->rf_gain                = 50;
  args->size_sub_channel       = 10;
  args->num_sub_channel        = 5;
}

static srsran_rf_t radio;
static prog_args_t prog_args;

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    keep_running = false;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

void usage(prog_args_t* args, char* prog)
{
  printf("Usage: %s [aAcdgmnoprstv] -f rx_frequency_hz\n", prog);
  printf("\t-a RF args [Default %s]\n", args->rf_args);
  printf("\t-A nof_rx_antennas [Default %d]\n", args->nof_rx_antennas);
  printf("\t-c N_sl_id [Default %d]\n", cell_sl.N_sl_id);
  printf("\t-d RF devicename [Default %s]\n", args->rf_dev);
  printf("\t-g RF Gain [Default %.2f dB]\n", args->rf_gain);
  printf("\t-m Start subframe_idx [Default %d]\n", args->file_start_sf_idx);
  printf("\t-n num_sub_channel [Default for 50 prbs %d]\n", args->num_sub_channel);
  printf("\t-o log_file_name.\n");
  printf("\t-p nof_prb [Default %d]\n", cell_sl.nof_prb);
  printf("\t-r use_standard_lte_rates [Default %i]\n", args->use_standard_lte_rates);
  printf("\t-s size_sub_channel [Default for 50 prbs %d]\n", args->size_sub_channel);
  printf("\t-t Sidelink transmission mode {1,2,3,4} [Default %d]\n", (cell_sl.tm + 1));
  printf("\t-v srsran_verbose\n");
}

void parse_args(prog_args_t* args, int argc, char** argv)
{
  int opt;
  args_default(args);

  while ((opt = getopt(argc, argv, "aAcdfgmnoprsv")) != -1) {
    switch (opt) {
      case 'a':
        args->rf_args = argv[optind];
        break;
      case 'A':
        args->nof_rx_antennas = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'c':
        cell_sl.N_sl_id = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'd':
        args->rf_dev = argv[optind];
        break;
      case 'f':
        args->rf_freq = strtof(argv[optind], NULL);
        break;
      case 'g':
        args->rf_gain = strtof(argv[optind], NULL);
        break;
      case 'm':
        args->file_start_sf_idx = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'n':
        args->num_sub_channel = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'o':
        args->log_file_name = argv[optind];
        break;
      case 'p':
        cell_sl.nof_prb = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        args->use_standard_lte_rates = true;
        break;
      case 's':
        args->size_sub_channel = (int32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'v':
        srsran_verbose++;
        break;
      default:
        usage(args, argv[0]);
        exit(-1);
    }
  }
  if (args->rf_freq < 0) {
    usage(args, argv[0]);
    exit(-1);
  }
}

#ifndef DISABLE_RF
int srsran_rf_recv_wrapper(void* h, cf_t* data[SRSRAN_MAX_PORTS], uint32_t nsamples, srsran_timestamp_t* t)
{
  DEBUG(" ----  Receive %d samples  ---- \n", nsamples);
  void* ptr[SRSRAN_MAX_PORTS];
  for (int i = 0; i < SRSRAN_MAX_PORTS; i++) {
    ptr[i] = data[i];
  }
  return srsran_rf_recv_with_time_multi(h, ptr, nsamples, true, &t->full_secs, &t->frac_secs);
}
#endif // DISABLE_RF

int main(int argc, char** argv)
{
  signal(SIGINT, sig_int_handler);
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  uint32_t num_decoded_sci = 0;
  uint32_t num_decoded_tb  = 0;

  parse_args(&prog_args, argc, argv);

  FILE* pcap_file = LTE_PCAP_Open(MAC_LTE_DLT, PCAP_FILENAME);

  /***** logfile *******/
  struct tm* timeinfo;
  time_t     current_time = time(0); // Get the system time
  timeinfo                = localtime(&current_time);

  FILE* logfile;
  if (prog_args.log_file_name) {
    logfile = fopen(prog_args.log_file_name, "w");
  } else {

    const char* home    = getenv("HOME");
    const char* subPath = "/v2x_pssch_ue_logfiles";
    char*       path    = (char*)malloc(strlen(home) + strlen(subPath) + 1);
    strcpy(path, home);
    strcat(path, subPath);

    mkdir(path, 0700);
    char filename[100];

    sprintf(filename,
            "%s/v2x_pssch_ue_%d_%d_%d-%d_%d_%d.csv",
            path,
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday,
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec);

    logfile = fopen(filename, "w");

    free(path);
  }

  // write header
  fprintf(logfile, "rx_timestamp_us,prb_start_idx,nof_prb,N_x_id,mcs_idx,rv_idx,sf_idx\n");

  /***** Init *******/
  srsran_use_standard_symbol_size(prog_args.use_standard_lte_rates);

  srsran_sl_comm_resource_pool_t sl_comm_resource_pool;
  if (srsran_sl_comm_resource_pool_get_default_config(&sl_comm_resource_pool, cell_sl) != SRSRAN_SUCCESS) {
    ERROR("Error initializing sl_comm_resource_pool\n");
    return SRSRAN_ERROR;
  }
  sl_comm_resource_pool.num_sub_channel  = prog_args.num_sub_channel;
  sl_comm_resource_pool.size_sub_channel = prog_args.size_sub_channel;

  printf("Opening RF device...\n");

  if (srsran_rf_open_multi(&radio, prog_args.rf_args, prog_args.nof_rx_antennas)) {
    ERROR("Error opening rf\n");
    exit(-1);
  }

  printf("Set RX freq: %.6f MHz\n", srsran_rf_set_rx_freq(&radio, prog_args.nof_rx_antennas, prog_args.rf_freq) / 1e6);
  printf("Set RX gain: %.1f dB\n", srsran_rf_set_rx_gain(&radio, prog_args.rf_gain));
  int srate = srsran_sampling_freq_hz(cell_sl.nof_prb);

  if (srate != -1) {
    printf("Setting sampling rate %.2f MHz\n", (float)srate / 1000000);
    float srate_rf = srsran_rf_set_rx_srate(&radio, (double)srate);
    if (srate_rf != srate) {
      ERROR("Could not set sampling rate\n");
      exit(-1);
    }
  } else {
    ERROR("Invalid number of PRB %d\n", cell_sl.nof_prb);
    exit(-1);
  }

  // allocate Rx buffers for 1ms worth of samples
  uint32_t sf_len = SRSRAN_SF_LEN_PRB(cell_sl.nof_prb);
  printf("Using a SF len of %d samples\n", sf_len);

  cf_t* rx_buffer[SRSRAN_MAX_CHANNELS] = {};     //< For radio to receive samples
  cf_t* sf_buffer[SRSRAN_MAX_PORTS]    = {NULL}; ///< For OFDM object to store subframe after FFT

  for (int i = 0; i < prog_args.nof_rx_antennas; i++) {
    rx_buffer[i] = srsran_vec_cf_malloc(sf_len);
    if (!rx_buffer[i]) {
      perror("malloc");
      exit(-1);
    }
    sf_buffer[i] = srsran_vec_cf_malloc(sf_len);
    if (!sf_buffer[i]) {
      perror("malloc");
      exit(-1);
    }
  }

  uint32_t sf_n_re             = SRSRAN_CP_NSYMB(SRSRAN_CP_NORM) * SRSRAN_NRE * 2 * cell_sl.nof_prb;
  cf_t*    equalized_sf_buffer = srsran_vec_malloc(sizeof(cf_t) * sf_n_re);

  // RX
  srsran_ofdm_t     fft[SRSRAN_MAX_PORTS];
  srsran_ofdm_cfg_t ofdm_cfg = {};
  ofdm_cfg.nof_prb           = cell_sl.nof_prb;
  ofdm_cfg.cp                = SRSRAN_CP_NORM;
  ofdm_cfg.rx_window_offset  = 0.0f;
  ofdm_cfg.normalize         = true;
  ofdm_cfg.sf_type           = SRSRAN_SF_NORM;
  ofdm_cfg.freq_shift_f      = -0.5;
  for (int i = 0; i < prog_args.nof_rx_antennas; i++) {
    ofdm_cfg.in_buffer  = rx_buffer[0];
    ofdm_cfg.out_buffer = sf_buffer[0];

    if (srsran_ofdm_rx_init_cfg(&fft[i], &ofdm_cfg)) {
      ERROR("Error initiating FFT\n");
      exit(-1);
    }
  }

  // SCI
  srsran_sci_t sci;
  srsran_sci_init(&sci, cell_sl, sl_comm_resource_pool);
  uint8_t sci_rx[SRSRAN_SCI_MAX_LEN]      = {};
  char    sci_msg[SRSRAN_SCI_MSG_MAX_LEN] = {};

  // init PSCCH object
  static srsran_pscch_t pscch = {};
  if (srsran_pscch_init(&pscch, SRSRAN_MAX_PRB) != SRSRAN_SUCCESS) {
    ERROR("Error in PSCCH init\n");
    return SRSRAN_ERROR;
  }

  if (srsran_pscch_set_cell(&pscch, cell_sl) != SRSRAN_SUCCESS) {
    ERROR("Error in PSCCH set cell\n");
    return SRSRAN_ERROR;
  }

  // PSCCH Channel estimation
  srsran_chest_sl_cfg_t pscch_chest_sl_cfg = {};
  srsran_chest_sl_t     pscch_chest        = {};
  if (srsran_chest_sl_init(&pscch_chest, SRSRAN_SIDELINK_PSCCH, cell_sl, sl_comm_resource_pool) != SRSRAN_SUCCESS) {
    ERROR("Error in chest PSCCH init\n");
    return SRSRAN_ERROR;
  }

  // init PSSCH object
  static srsran_pssch_t pssch = {};
  if (srsran_pssch_init(&pssch, cell_sl, sl_comm_resource_pool) != SRSRAN_SUCCESS) {
    ERROR("Error initializing PSSCH\n");
    return SRSRAN_ERROR;
  }

  srsran_chest_sl_cfg_t pssch_chest_sl_cfg = {};
  srsran_chest_sl_t     pssch_chest        = {};
  if (srsran_chest_sl_init(&pssch_chest, SRSRAN_SIDELINK_PSSCH, cell_sl, sl_comm_resource_pool) != SRSRAN_SUCCESS) {
    ERROR("Error in chest PSSCH init\n");
    return SRSRAN_ERROR;
  }

  uint8_t          tb[SRSRAN_SL_SCH_MAX_TB_LEN]            = {};
  uint8_t          packed_tb[SRSRAN_SL_SCH_MAX_TB_LEN / 8] = {};
  srsran_ue_sync_t ue_sync                                 = {};
  srsran_cell_t    cell                                    = {};
  cell.nof_prb                                             = cell_sl.nof_prb;
  cell.cp                                                  = SRSRAN_CP_NORM;
  cell.nof_ports                                           = 1;

  if (srsran_ue_sync_init_multi_decim_mode(&ue_sync,
                                           cell.nof_prb,
                                           false,
                                           srsran_rf_recv_wrapper,
                                           prog_args.nof_rx_antennas,
                                           (void*)&radio,
                                           1,
                                           SYNC_MODE_GNSS)) {
    fprintf(stderr, "Error initiating sync_gnss\n");
    exit(-1);
  }

  if (srsran_ue_sync_set_cell(&ue_sync, cell)) {
    ERROR("Error initiating ue_sync\n");
    exit(-1);
  }

  srsran_rf_start_rx_stream(&radio, false);

  uint32_t subframe_count      = 0;
  uint32_t pscch_prb_start_idx = 0;

  uint32_t current_sf_idx = 0;

  while (keep_running) {

    // receive subframe from radio
    int ret = srsran_ue_sync_zerocopy(&ue_sync, rx_buffer, sf_len);
    if (ret < 0) {
      ERROR("Error calling srsran_ue_sync_work()\n");
    }

    // update SF index
    current_sf_idx = srsran_ue_sync_get_sfidx(&ue_sync);

    // do FFT (on first port)
    srsran_ofdm_rx_sf(&fft[0]);

    for (int sub_channel_idx = 0; sub_channel_idx < sl_comm_resource_pool.num_sub_channel; sub_channel_idx++) {
      pscch_prb_start_idx = sub_channel_idx * sl_comm_resource_pool.size_sub_channel;

      for (uint32_t cyclic_shift = 0; cyclic_shift <= 9; cyclic_shift += 3) {

        // PSCCH Channel estimation
        pscch_chest_sl_cfg.cyclic_shift  = cyclic_shift;
        pscch_chest_sl_cfg.prb_start_idx = pscch_prb_start_idx;
        srsran_chest_sl_set_cfg(&pscch_chest, pscch_chest_sl_cfg);
        srsran_chest_sl_ls_estimate_equalize(&pscch_chest, sf_buffer[0], equalized_sf_buffer);

        if (srsran_pscch_decode(&pscch, equalized_sf_buffer, sci_rx, pscch_prb_start_idx) == SRSRAN_SUCCESS) {
          if (srsran_sci_format1_unpack(&sci, sci_rx) == SRSRAN_SUCCESS) {
            srsran_sci_info(&sci, sci_msg, sizeof(sci_msg));
            fprintf(stdout, "%s", sci_msg);

            num_decoded_sci++;

            // Decode PSSCH
            uint32_t sub_channel_start_idx = 0;
            uint32_t L_subCH               = 0;
            srsran_ra_sl_type0_from_riv(
                sci.riv, sl_comm_resource_pool.num_sub_channel, &L_subCH, &sub_channel_start_idx);

            // 3GPP TS 36.213 Section 14.1.1.4C
            uint32_t pssch_prb_start_idx = (sub_channel_idx * sl_comm_resource_pool.size_sub_channel) +
                                           pscch.pscch_nof_prb + sl_comm_resource_pool.start_prb_sub_channel;
            uint32_t nof_prb_pssch = ((L_subCH + sub_channel_idx) * sl_comm_resource_pool.size_sub_channel) -
                                     pssch_prb_start_idx + sl_comm_resource_pool.start_prb_sub_channel;

            // make sure PRBs are valid for DFT precoding
            nof_prb_pssch = srsran_dft_precoding_get_valid_prb(nof_prb_pssch);

            uint32_t N_x_id = 0;
            for (int j = 0; j < SRSRAN_SCI_CRC_LEN; j++) {
              N_x_id += pscch.sci_crc[j] * exp2(SRSRAN_SCI_CRC_LEN - 1 - j);
            }

            uint32_t rv_idx = 0;
            if (sci.retransmission == true) {
              rv_idx = 1;
            }

            // PSSCH Channel estimation
            pssch_chest_sl_cfg.N_x_id        = N_x_id;
            pssch_chest_sl_cfg.sf_idx        = current_sf_idx;
            pssch_chest_sl_cfg.prb_start_idx = pssch_prb_start_idx;
            pssch_chest_sl_cfg.nof_prb       = nof_prb_pssch;
            srsran_chest_sl_set_cfg(&pssch_chest, pssch_chest_sl_cfg);
            srsran_chest_sl_ls_estimate_equalize(&pssch_chest, sf_buffer[0], equalized_sf_buffer);

            srsran_pssch_cfg_t pssch_cfg = {
                pssch_prb_start_idx, nof_prb_pssch, N_x_id, sci.mcs_idx, rv_idx, current_sf_idx};
            if (srsran_pssch_set_cfg(&pssch, pssch_cfg) == SRSRAN_SUCCESS) {
              if (srsran_pssch_decode(&pssch, equalized_sf_buffer, tb, SRSRAN_SL_SCH_MAX_TB_LEN) == SRSRAN_SUCCESS) {
                num_decoded_tb++;

                srsran_bit_pack_vector(tb, packed_tb, pssch.sl_sch_tb_len);
                pcap_pack_and_write(pcap_file,
                                    packed_tb,
                                    pssch.sl_sch_tb_len / 8,
                                    0,
                                    true,
                                    current_sf_idx,
                                    0x1001,
                                    DIRECTION_UPLINK,
                                    SL_RNTI);
                // write logfile
                fprintf(logfile,
                        "%lu,%d,%d,%d,%d,%d,%d\n",
                        (uint64_t)round(srsran_timestamp_real(&ue_sync.last_timestamp) * 1e6),
                        pssch_prb_start_idx,
                        nof_prb_pssch,
                        N_x_id,
                        sci.mcs_idx,
                        rv_idx,
                        current_sf_idx);
              }
              // } else {
              //
              //   fprintf(logfile,
              //           "%lu,%d,%d,%d,%d,%d,%d\n",
              //           (uint64_t)round(srsran_timestamp_real(&ue_sync.last_timestamp) * 1e6),
              //           pssch_prb_start_idx,
              //           nof_prb_pssch,
              //           N_x_id,
              //           sci.mcs_idx,
              //           rv_idx,
              //           current_sf_idx);
              // }
            }
          }
        }
        if (SRSRAN_VERBOSE_ISDEBUG()) {
          char filename[64];
          snprintf(filename,
                   64,
                   "pscch_rx_syms_sf%d_shift%d_prbidx%d.bin",
                   subframe_count,
                   cyclic_shift,
                   pscch_prb_start_idx);
          printf("Saving PSCCH symbols (%d) to %s\n", pscch.E / SRSRAN_PSCCH_QM, filename);
          srsran_vec_save_file(filename, pscch.mod_symbols, pscch.E / SRSRAN_PSCCH_QM * sizeof(cf_t));
        }
      }
    }

    current_sf_idx = (current_sf_idx + 1) % 10;
    subframe_count++;
  }

  fclose(logfile);
  printf("num_decoded_sci=%d num_decoded_tb=%d\n", num_decoded_sci, num_decoded_tb);

  srsran_rf_stop_rx_stream(&radio);
  srsran_rf_close(&radio);
  srsran_ue_sync_free(&ue_sync);
  srsran_sci_free(&sci);
  srsran_pscch_free(&pscch);
  srsran_chest_sl_free(&pscch_chest);
  srsran_chest_sl_free(&pssch_chest);

  for (int i = 0; i < prog_args.nof_rx_antennas; i++) {
    if (rx_buffer[i]) {
      free(rx_buffer[i]);
    }
    if (sf_buffer[i]) {
      free(sf_buffer[i]);
    }
    srsran_ofdm_rx_free(&fft[i]);
  }

  if (equalized_sf_buffer) {
    free(equalized_sf_buffer);
  }

  return SRSRAN_SUCCESS;
}
