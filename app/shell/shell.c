/*
 * Copyright (c) 2009 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <app.h>
#include <lk/debug.h>
#include <lib/console.h>
#include <kernel/thread.h>

static void shell_entry(const struct app_descriptor *app, void *args) {
    console_t *con = console_create(true);
    if (!con)
        return;

    console_start(con);

    // TODO: destroy console and free resources
}

APP_START(shell)
.entry = shell_entry,
.flags = APP_FLAG_CUSTOM_STACK_SIZE,
.stack_size = DEFAULT_STACK_SIZE * 2,
APP_END

