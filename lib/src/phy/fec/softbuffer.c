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

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "srsran/phy/common/phy_common.h"
#include "srsran/phy/fec/rm_turbo.h"
#include "srsran/phy/fec/softbuffer.h"
#include "srsran/phy/fec/turbodecoder_gen.h"
#include "srsran/phy/phch/ra.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"

#define MAX_PDSCH_RE(cp) (2 * SRSRAN_CP_NSYMB(cp) * 12)

int srsran_softbuffer_rx_init(srsran_softbuffer_rx_t* q, uint32_t nof_prb)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    bzero(q, sizeof(srsran_softbuffer_rx_t));

    ret = srsran_ra_tbs_from_idx(SRSRAN_RA_NOF_TBS_IDX - 1, nof_prb);
    if (ret != SRSRAN_ERROR) {
      q->max_cb = (uint32_t)ret / (SRSRAN_TCOD_MAX_LEN_CB - 24) + 1;
      ret       = SRSRAN_ERROR;

      q->buffer_f = srsran_vec_malloc(sizeof(int16_t*) * q->max_cb);
      if (!q->buffer_f) {
        perror("malloc");
        goto clean_exit;
      }

      q->data = srsran_vec_malloc(sizeof(uint8_t*) * q->max_cb);
      if (!q->data) {
        perror("malloc");
        goto clean_exit;
      }

      q->cb_crc = srsran_vec_malloc(sizeof(bool) * q->max_cb);
      if (!q->cb_crc) {
        perror("malloc");
        goto clean_exit;
      }
      bzero(q->cb_crc, sizeof(bool) * q->max_cb);

      // TODO: Use HARQ buffer limitation based on UE category
      for (uint32_t i = 0; i < q->max_cb; i++) {
        q->buffer_f[i] = srsran_vec_i16_malloc(SOFTBUFFER_SIZE);
        if (!q->buffer_f[i]) {
          perror("malloc");
          goto clean_exit;
        }

        q->data[i] = srsran_vec_u8_malloc(6144 / 8);
        if (!q->data[i]) {
          perror("malloc");
          goto clean_exit;
        }
      }
      // srsran_softbuffer_rx_reset(q);
      ret = SRSRAN_SUCCESS;
    }
  }

clean_exit:
  if (ret != SRSRAN_SUCCESS) {
    srsran_softbuffer_rx_free(q);
  }

  return ret;
}

void srsran_softbuffer_rx_free(srsran_softbuffer_rx_t* q)
{
  if (q) {
    if (q->buffer_f) {
      for (uint32_t i = 0; i < q->max_cb; i++) {
        if (q->buffer_f[i]) {
          free(q->buffer_f[i]);
        }
      }
      free(q->buffer_f);
    }
    if (q->data) {
      for (uint32_t i = 0; i < q->max_cb; i++) {
        if (q->data[i]) {
          free(q->data[i]);
        }
      }
      free(q->data);
    }
    if (q->cb_crc) {
      free(q->cb_crc);
    }
    bzero(q, sizeof(srsran_softbuffer_rx_t));
  }
}

void srsran_softbuffer_rx_reset_tbs(srsran_softbuffer_rx_t* q, uint32_t tbs)
{
  uint32_t nof_cb = (tbs + 24) / (SRSRAN_TCOD_MAX_LEN_CB - 24) + 1;
  srsran_softbuffer_rx_reset_cb(q, nof_cb);
}

void srsran_softbuffer_rx_reset(srsran_softbuffer_rx_t* q)
{
  srsran_softbuffer_rx_reset_cb(q, q->max_cb);
}

void srsran_softbuffer_rx_reset_cb(srsran_softbuffer_rx_t* q, uint32_t nof_cb)
{
  if (q->buffer_f) {
    if (nof_cb > q->max_cb) {
      nof_cb = q->max_cb;
    }
    for (uint32_t i = 0; i < nof_cb; i++) {
      if (q->buffer_f[i]) {
        bzero(q->buffer_f[i], SOFTBUFFER_SIZE * sizeof(int16_t));
      }
      if (q->data[i]) {
        bzero(q->data[i], sizeof(uint8_t) * 6144 / 8);
      }
    }
  }
  if (q->cb_crc) {
    bzero(q->cb_crc, sizeof(bool) * q->max_cb);
  }
  q->tb_crc = false;
}

int srsran_softbuffer_tx_init(srsran_softbuffer_tx_t* q, uint32_t nof_prb)
{
  int ret = SRSRAN_ERROR_INVALID_INPUTS;

  if (q != NULL) {
    bzero(q, sizeof(srsran_softbuffer_tx_t));

    ret = srsran_ra_tbs_from_idx(SRSRAN_RA_NOF_TBS_IDX - 1, nof_prb);
    if (ret != SRSRAN_ERROR) {
      q->max_cb = (uint32_t)ret / (SRSRAN_TCOD_MAX_LEN_CB - 24) + 1;

      q->buffer_b = srsran_vec_malloc(sizeof(uint8_t*) * q->max_cb);
      if (!q->buffer_b) {
        perror("malloc");
        return SRSRAN_ERROR;
      }

      // TODO: Use HARQ buffer limitation based on UE category
      for (uint32_t i = 0; i < q->max_cb; i++) {
        q->buffer_b[i] = srsran_vec_u8_malloc(SOFTBUFFER_SIZE);
        if (!q->buffer_b[i]) {
          perror("malloc");
          return SRSRAN_ERROR;
        }
      }
      srsran_softbuffer_tx_reset(q);
      ret = SRSRAN_SUCCESS;
    }
  }
  return ret;
}

void srsran_softbuffer_tx_free(srsran_softbuffer_tx_t* q)
{
  if (q) {
    if (q->buffer_b) {
      for (uint32_t i = 0; i < q->max_cb; i++) {
        if (q->buffer_b[i]) {
          free(q->buffer_b[i]);
        }
      }
      free(q->buffer_b);
    }
    bzero(q, sizeof(srsran_softbuffer_tx_t));
  }
}

void srsran_softbuffer_tx_reset_tbs(srsran_softbuffer_tx_t* q, uint32_t tbs)
{
  uint32_t nof_cb = (tbs + 24) / (SRSRAN_TCOD_MAX_LEN_CB - 24) + 1;
  srsran_softbuffer_tx_reset_cb(q, nof_cb);
}

void srsran_softbuffer_tx_reset(srsran_softbuffer_tx_t* q)
{
  srsran_softbuffer_tx_reset_cb(q, q->max_cb);
}

void srsran_softbuffer_tx_reset_cb(srsran_softbuffer_tx_t* q, uint32_t nof_cb)
{
  int i;
  if (q->buffer_b) {
    if (nof_cb > q->max_cb) {
      nof_cb = q->max_cb;
    }
    for (i = 0; i < nof_cb; i++) {
      if (q->buffer_b[i]) {
        bzero(q->buffer_b[i], sizeof(uint8_t) * SOFTBUFFER_SIZE);
      }
    }
  }
}
