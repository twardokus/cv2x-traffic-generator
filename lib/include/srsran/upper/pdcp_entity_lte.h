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

#ifndef SRSRAN_PDCP_ENTITY_LTE_H
#define SRSRAN_PDCP_ENTITY_LTE_H

#include "srsran/common/buffer_pool.h"
#include "srsran/common/common.h"
#include "srsran/common/log.h"
#include "srsran/common/security.h"
#include "srsran/common/threads.h"
#include "srsran/interfaces/ue_interfaces.h"
#include "srsran/upper/pdcp_entity_base.h"

namespace srsran {

/****************************************************************************
 * Structs and Defines
 * Ref: 3GPP TS 36.323 v10.1.0
 ***************************************************************************/

#define PDCP_CONTROL_MAC_I 0x00000000

/****************************************************************************
 * LTE PDCP Entity
 * Class for LTE PDCP entities
 ***************************************************************************/
class pdcp_entity_lte final : public pdcp_entity_base
{
public:
  pdcp_entity_lte(srsue::rlc_interface_pdcp*      rlc_,
                  srsue::rrc_interface_pdcp*      rrc_,
                  srsue::gw_interface_pdcp*       gw_,
                  srsran::task_handler_interface* task_executor_,
                  srsran::log_ref                 log_);
  ~pdcp_entity_lte();
  void init(uint32_t lcid_, pdcp_config_t cfg_);
  void reset();
  void reestablish();

  // GW/RRC interface
  void write_sdu(unique_byte_buffer_t sdu, bool blocking);
  void get_bearer_status(uint16_t* dlsn, uint16_t* dlhfn, uint16_t* ulsn, uint16_t* ulhfn);

  // RLC interface
  void write_pdu(unique_byte_buffer_t pdu);

  // State variable setters (should be used only for testing)
  void set_tx_count(uint32_t tx_count_) { tx_count = tx_count_; }
  void set_rx_hfn(uint32_t rx_hfn_) { rx_hfn = rx_hfn_; }
  void set_next_pdcp_rx_sn(uint32_t next_pdcp_rx_sn_) { next_pdcp_rx_sn = next_pdcp_rx_sn_; }
  void set_last_submitted_pdcp_rx_sn(uint32_t last_submitted_pdcp_rx_sn_)
  {
    last_submitted_pdcp_rx_sn = last_submitted_pdcp_rx_sn_;
  }

  // Config helpers
  bool check_valid_config();

private:
  srsue::rlc_interface_pdcp* rlc = nullptr;
  srsue::rrc_interface_pdcp* rrc = nullptr;
  srsue::gw_interface_pdcp*  gw  = nullptr;

  uint32_t tx_count = 0;

  uint32_t rx_hfn                    = 0;
  uint32_t next_pdcp_rx_sn           = 0;
  uint32_t reordering_window         = 0;
  uint32_t last_submitted_pdcp_rx_sn = 0;
  uint32_t maximum_pdcp_sn           = 0;

  void handle_srb_pdu(srsran::unique_byte_buffer_t pdu);
  void handle_um_drb_pdu(srsran::unique_byte_buffer_t pdu);
  void handle_am_drb_pdu(srsran::unique_byte_buffer_t pdu);
};

} // namespace srsran
#endif // SRSRAN_PDCP_ENTITY_LTE_H
