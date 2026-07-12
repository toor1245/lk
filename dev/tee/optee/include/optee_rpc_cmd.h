/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2016-2021, Linaro Limited
 */

#ifndef __OPTEE_RPC_CMD_H
#define __OPTEE_RPC_CMD_H

/*
 * All RPC is done with a struct optee_msg_arg as bearer of information,
 * struct optee_msg_arg::arg holds values defined by OPTEE_RPC_CMD_* below.
 * Only the commands handled by the kernel driver are defined here.
 *
 * RPC communication with tee-supplicant is reversed compared to normal
 * client communication described above. The supplicant receives requests
 * and sends responses.
 */

/*
 * Suspend execution
 *
 * [in]    value[0].a	Number of milliseconds to suspend
 */
#define OPTEE_RPC_CMD_SUSPEND		5

/*
 * Allocate a piece of shared memory
 *
 * [in]    value[0].a	    Type of memory one of
 *			    OPTEE_RPC_SHM_TYPE_* below
 * [in]    value[0].b	    Requested size
 * [in]    value[0].c	    Required alignment
 * [out]   memref[0]	    Buffer
 */
#define OPTEE_RPC_CMD_SHM_ALLOC		6
/* Memory that can be shared with a non-secure user space application */
#define OPTEE_RPC_SHM_TYPE_APPL		0
/* Memory only shared with non-secure kernel */
#define OPTEE_RPC_SHM_TYPE_KERNEL	1

/*
 * Free shared memory previously allocated with OPTEE_RPC_CMD_SHM_ALLOC
 *
 * [in]     value[0].a	    Type of memory one of
 *			    OPTEE_RPC_SHM_TYPE_* above
 * [in]     value[0].b	    Value of shared memory reference or cookie
 */
#define OPTEE_RPC_CMD_SHM_FREE		7

/*
 * Reset RPMB probing
 *
 * Releases an eventually already used RPMB devices and starts over searching
 * for RPMB devices. Returns the kind of shared memory to use in subsequent
 * OPTEE_RPC_CMD_RPMB_PROBE_NEXT and OPTEE_RPC_CMD_RPMB calls.
 *
 * [out]    value[0].a	    OPTEE_RPC_SHM_TYPE_*, the parameter for
 *			    OPTEE_RPC_CMD_SHM_ALLOC
 */
#define OPTEE_RPC_CMD_RPMB_PROBE_RESET	22

/*
 * Probe next RPMB device
 *
 * [out]    value[0].a	    Type of RPMB device, OPTEE_RPC_RPMB_*
 * [out]    value[0].b	    EXT CSD-slice 168 "RPMB Size"
 * [out]    value[0].c	    EXT CSD-slice 222 "Reliable Write Sector Count"
 * [out]    memref[1]       Buffer with the raw CID
 */
#define OPTEE_RPC_CMD_RPMB_PROBE_NEXT	23

/* Type of RPMB device */
#define OPTEE_RPC_RPMB_EMMC		0
#define OPTEE_RPC_RPMB_UFS		1
#define OPTEE_RPC_RPMB_NVME		2

/*
 * Replay Protected Memory Block access
 *
 * [in]     memref[0]	    Frames to device
 * [out]    memref[1]	    Frames from device
 */
#define OPTEE_RPC_CMD_RPMB_FRAMES	24

#endif /*__OPTEE_RPC_CMD_H*/
