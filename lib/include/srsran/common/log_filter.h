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
 * File:        log_filter.h
 * Description: Log filter for a specific layer or element.
 *              Performs filtering based on log level, generates
 *              timestamped log strings and passes them to the
 *              common logger object.
 *****************************************************************************/

#ifndef SRSRAN_LOG_FILTER_H
#define SRSRAN_LOG_FILTER_H

#include <stdarg.h>
#include <string>

#include "srsran/common/log.h"
#include "srsran/common/logger.h"
#include "srsran/common/logger_stdout.h"
#include "srsran/phy/common/timestamp.h"

namespace srsran {

typedef std::string* str_ptr;

class log_filter : public srsran::log
{
public:
  log_filter();
  log_filter(std::string layer);
  log_filter(std::string layer, logger* logger_, bool tti = false);

  void init(std::string layer, logger* logger_, bool tti = false);

  void console(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void error(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void warning(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void info(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void info_long(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void debug(const char* message, ...) __attribute__((format(printf, 2, 3)));
  void debug_long(const char* message, ...) __attribute__((format(printf, 2, 3)));

  void error_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void warning_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void info_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));
  void debug_hex(const uint8_t* hex, int size, const char* message, ...) __attribute__((format(printf, 4, 5)));

  srsran::LOG_LEVEL_ENUM get_level(std::string l);

  class time_itf
  {
  public:
    virtual srsran_timestamp_t get_time() = 0;
  };

  typedef enum { TIME, EPOCH } time_format_t;

  void set_time_src(time_itf* source, time_format_t format);

protected:
  logger* logger_h;
  bool    do_tti;

  static const int char_buff_size = logger::preallocated_log_str_size - 64 * 3;

  time_itf*     time_src;
  time_format_t time_format;

  logger_stdout def_logger_stdout;

  void        all_log(srsran::LOG_LEVEL_ENUM level,
                      uint32_t               tti,
                      const char*            msg,
                      const uint8_t*         hex      = nullptr,
                      int                    size     = 0,
                      bool                   long_msg = false);
  void        now_time(char* buffer, const uint32_t buffer_len);
  void        get_tti_str(const uint32_t tti_, char* buffer, const uint32_t buffer_len);
  std::string hex_string(const uint8_t* hex, int size);
};

} // namespace srsran

#endif // SRSRAN_LOG_FILTER_H
