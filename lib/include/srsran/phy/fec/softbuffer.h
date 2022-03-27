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

/******************************************************************************
 *  File:         softbuffer.h
 *
 *  Description:  Buffer for RX and TX soft bits. This should be provided by MAC.
 *                Provided here basically for the examples.
 *
 *  Reference:
 *****************************************************************************/

#ifndef SRSRAN_SOFTBUFFER_H
#define SRSRAN_SOFTBUFFER_H

#include "srsran/config.h"
#include "srsran/phy/common/phy_common.h"

typedef struct SRSRAN_API {
  uint32_t  max_cb;
  int16_t** buffer_f;
  uint8_t** data;
  bool*     cb_crc;
  bool      tb_crc;
} srsran_softbuffer_rx_t;

typedef struct SRSRAN_API {
  uint32_t  max_cb;
  uint8_t** buffer_b;
} srsran_softbuffer_tx_t;

#define SOFTBUFFER_SIZE 18600

SRSRAN_API int srsran_softbuffer_rx_init(srsran_softbuffer_rx_t* q, uint32_t nof_prb);

SRSRAN_API void srsran_softbuffer_rx_reset(srsran_softbuffer_rx_t* p);

SRSRAN_API void srsran_softbuffer_rx_reset_tbs(srsran_softbuffer_rx_t* q, uint32_t tbs);

SRSRAN_API void srsran_softbuffer_rx_reset_cb(srsran_softbuffer_rx_t* q, uint32_t nof_cb);

SRSRAN_API void srsran_softbuffer_rx_free(srsran_softbuffer_rx_t* p);

SRSRAN_API int srsran_softbuffer_tx_init(srsran_softbuffer_tx_t* q, uint32_t nof_prb);

SRSRAN_API void srsran_softbuffer_tx_reset(srsran_softbuffer_tx_t* p);

SRSRAN_API void srsran_softbuffer_tx_reset_tbs(srsran_softbuffer_tx_t* q, uint32_t tbs);

SRSRAN_API void srsran_softbuffer_tx_reset_cb(srsran_softbuffer_tx_t* q, uint32_t nof_cb);

SRSRAN_API void srsran_softbuffer_tx_free(srsran_softbuffer_tx_t* p);

#endif // SRSRAN_SOFTBUFFER_H
