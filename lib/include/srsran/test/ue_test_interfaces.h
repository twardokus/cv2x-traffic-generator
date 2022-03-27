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

#ifndef SRSUE_DUMMY_CLASSES_H
#define SRSUE_DUMMY_CLASSES_H

#include "srsran/interfaces/ue_interfaces.h"

namespace srsue {

class stack_test_dummy : public stack_interface_rrc
{
public:
  stack_test_dummy() { stack_queue_id = pending_tasks.add_queue(); }

  srsran::timer_handler::unique_timer get_unique_timer() override { return timers.get_unique_timer(); }
  void                                start_cell_search() override {}
  void                                start_cell_select(const phy_interface_rrc_lte::phy_cell_t* cell) override {}
  srsran::tti_point get_current_tti() override { return srsran::tti_point{timers.get_cur_time() % 10240}; }
  srsran::task_multiqueue::queue_handler make_task_queue() final { return pending_tasks.get_queue_handler(); }
  void                                   enqueue_background_task(std::function<void(uint32_t)> f) override { f(0); }
  void                                   notify_background_task_result(srsran::move_task_t task) override { task(); }
  void                                   defer_callback(uint32_t duration_ms, std::function<void()> func) final
  {
    timers.defer_callback(duration_ms, func);
  }
  void defer_task(srsran::move_task_t task) final { pending_tasks.push(stack_queue_id, std::move(task)); }

  // Testing utility functions
  void call_on_every_tti(srsran::move_task_t t) { tti_callbacks.push_back(std::move(t)); }
  void process_tasks()
  {
    // Make sure to process any stack pending tasks
    srsran::move_task_t task;
    while (pending_tasks.try_pop(&task) >= 0) {
      task();
    }
  }
  void run_tti()
  {
    process_tasks();
    for (auto& t : tti_callbacks) {
      t();
    }
    timers.step_all();
  }

  srsran::timer_handler            timers{100};
  srsran::task_multiqueue          pending_tasks;
  std::vector<srsran::move_task_t> tti_callbacks;
  int                              stack_queue_id = -1;
};

class rlc_dummy_interface : public rlc_interface_mac
{
public:
  bool     has_data(const uint32_t lcid) override { return false; }
  uint32_t get_buffer_state(const uint32_t lcid) override { return 0; }
  int      read_pdu(uint32_t lcid, uint8_t* payload, uint32_t nof_bytes) override { return 0; }
  void     write_pdu(uint32_t lcid, uint8_t* payload, uint32_t nof_bytes) override {}
  void     write_pdu_bcch_bch(srsran::unique_byte_buffer_t payload) override {}
  void     write_pdu_bcch_dlsch(uint8_t* payload, uint32_t nof_bytes) override {}
  void     write_pdu_pcch(srsran::unique_byte_buffer_t payload) override {}
  void     write_pdu_mch(uint32_t lcid, uint8_t* payload, uint32_t nof_bytes) override {}
};

} // namespace srsue

#endif // SRSUE_DUMMY_CLASSES_H
