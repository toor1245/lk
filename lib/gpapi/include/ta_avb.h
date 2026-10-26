/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2018, Linaro Limited */

/*
 * Client interface of the OP-TEE AVB trusted application (optee_os/ta/avb).
 */

#pragma once

/*
 * AVB TA UUID 023f8f1a-292a-432b-8fc4-de8471358067
 */
#define TA_AVB_UUID { 0x1a8f3f02, 0x2a29, 0x2b43, \
              { 0x8f, 0xc4, 0xde, 0x84, 0x71, 0x35, 0x80, 0x67 } }

#define TA_AVB_MAX_ROLLBACK_LOCATIONS   256

/*
 * Gets the rollback index corresponding to the given rollback index slot.
 *
 * in   params[0].value.a:  rollback index slot
 * out  params[1].value.a:  upper 32 bits of rollback index
 * out  params[1].value.b:  lower 32 bits of rollback index
 */
#define TA_AVB_CMD_READ_ROLLBACK_INDEX  0

/*
 * Updates the rollback index corresponding to the given rollback index slot.
 *
 * Will refuse to update a slot with a lower value.
 *
 * in   params[0].value.a:  rollback index slot
 * in   params[1].value.a:  upper 32 bits of rollback index
 * in   params[1].value.b:  lower 32 bits of rollback index
 */
#define TA_AVB_CMD_WRITE_ROLLBACK_INDEX 1

/*
 * Gets the lock state of the device.
 *
 * out  params[0].value.a:  lock state
 */
#define TA_AVB_CMD_READ_LOCK_STATE      2

/*
 * Sets the lock state of the device.
 *
 * If the lock state is changed all rollback slots will be reset to 0.
 *
 * in   params[0].value.a:  lock state
 */
#define TA_AVB_CMD_WRITE_LOCK_STATE     3

/*
 * Reads a persistent value corresponding to the given name.
 *
 * in   params[0].memref:   persistent value name
 * out  params[1].memref:   read persistent value buffer
 */
#define TA_AVB_CMD_READ_PERSIST_VALUE   4

/*
 * Writes a persistent value corresponding to the given name.
 *
 * in   params[0].memref:   persistent value name
 * in   params[1].memref:   persistent value buffer to write
 */
#define TA_AVB_CMD_WRITE_PERSIST_VALUE  5

/*
 * Refuses rollback index, lock state and persistent value writes until
 * the next reboot.
 */
#define TA_AVB_CMD_LOCK_BOOT_STATE      6

/*
 * Reads whether the owner allows unlocking ("OEM unlocking").
 *
 * out  params[0].value.a:  0 = not allowed, 1 = allowed
 */
#define TA_AVB_CMD_READ_OEM_UNLOCK_ALLOWED  7

/*
 * Sets whether the owner allows unlocking. Not affected by
 * TA_AVB_CMD_LOCK_BOOT_STATE.
 *
 * in   params[0].value.a:  0 = not allowed, 1 = allowed
 */
#define TA_AVB_CMD_WRITE_OEM_UNLOCK_ALLOWED 8

/* Lock state values stored by the TA. */
#define TA_AVB_LOCK_STATE_UNLOCKED      0
#define TA_AVB_LOCK_STATE_LOCKED        1
