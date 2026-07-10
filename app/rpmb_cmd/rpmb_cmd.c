/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/console_cmd.h>

#include "rpmb_cmd.h"

STATIC_COMMAND_START
STATIC_COMMAND("rpmb_program_key", "RPMB program key", &rpmb_cmd_program_key)
STATIC_COMMAND("rpmb_read_counter", "RPMB read write counter / check key programmed", &rpmb_cmd_read_counter)
STATIC_COMMAND("rpmb_read", "RPMB authenticated read", &rpmb_cmd_read)
STATIC_COMMAND("rpmb_write", "RPMB authenticated write", &rpmb_cmd_write)
STATIC_COMMAND_END(rpmb_cmd);
