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
 * File:        logger_stdout.h
 * Description: Interface for logging output
 *****************************************************************************/

#ifndef SRSRAN_LOGGER_STDOUT_H
#define SRSRAN_LOGGER_STDOUT_H

#include "srsran/common/logger.h"
#include <stdio.h>
#include <string>

namespace srsran {

class logger_stdout : public logger
{
public:
  void log(unique_log_str_t log_str) { fprintf(stdout, "%s", log_str->str()); }
};

} // namespace srsran

#endif // SRSRAN_LOGGER_STDOUT_H
