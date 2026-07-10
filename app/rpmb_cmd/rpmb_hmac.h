/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include <dev/rpmb.h>

/* Shared test authentication key, also used to program the RPMB key. */
extern const uint8_t rpmb_test_key[RPMB_KEY_MAC_SIZE];

void rpmb_hmac(const uint8_t *key, const uint8_t *buff, size_t len, uint8_t *output);
