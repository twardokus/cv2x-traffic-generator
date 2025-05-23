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

#ifndef SRSRAN_RLC_TM_H
#define SRSRAN_RLC_TM_H

#include "srsran/common/buffer_pool.h"
#include "srsran/common/common.h"
#include "srsran/common/log.h"
#include "srsran/interfaces/ue_interfaces.h"
#include "srsran/upper/rlc_common.h"
#include "srsran/upper/rlc_tx_queue.h"

namespace srsran {

class rlc_tm final : public rlc_common
{
public:
  rlc_tm(srsran::log_ref            log_,
         uint32_t                   lcid_,
         srsue::pdcp_interface_rlc* pdcp_,
         srsue::rrc_interface_rlc*  rrc_,
         srsran::timer_handler*     timers_,
         uint32_t                   queue_len = 16);
  ~rlc_tm() override;
  bool configure(const rlc_config_t& cnfg) override;
  void stop() override;
  void reestablish() override;
  void empty_queue() override;

  rlc_mode_t get_mode() override;
  uint32_t   get_bearer() override;

  rlc_bearer_metrics_t get_metrics() override;
  void                 reset_metrics() override;

  // PDCP interface
  void write_sdu(unique_byte_buffer_t sdu, bool blocking) override;
  void discard_sdu(uint32_t discard_sn) override;

  // MAC interface
  bool     has_data() override;
  uint32_t get_buffer_state() override;
  int      read_pdu(uint8_t* payload, uint32_t nof_bytes) override;
  void     write_pdu(uint8_t* payload, uint32_t nof_bytes) override;

private:
  byte_buffer_pool*          pool = nullptr;
  srsran::log_ref            log;
  uint32_t                   lcid = 0;
  srsue::pdcp_interface_rlc* pdcp = nullptr;
  srsue::rrc_interface_rlc*  rrc  = nullptr;

  bool tx_enabled = true;

  rlc_bearer_metrics_t metrics = {};

  // Thread-safe queues for MAC messages
  rlc_tx_queue ul_queue;
};

} // namespace srsran

#endif // SRSRAN_RLC_TM_H
