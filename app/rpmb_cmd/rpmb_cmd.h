/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <lk/console_cmd.h>
#include <lk/err.h>

#include <dev/rpmb.h>

status_t rpmb_read_write_counter(struct rpmb_dev *rpmb_dev, struct rpmb_frame *resp);

/* RPMB commands */
int rpmb_cmd_program_key(int argc, const console_cmd_args *argv);
int rpmb_cmd_read_counter(int argc, const console_cmd_args *argv);
int rpmb_cmd_read(int argc, const console_cmd_args *argv);
int rpmb_cmd_write(int argc, const console_cmd_args *argv);
