/****************************************************************************
 *
 *   Copyright (c) 2018, 2021 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file manifest.c
 *
 * This module supplies the interface to the manifest of hardware that is
 * optional and dependent on the simple Hardware versioning
 * BOARD_HAS_SIMPLE_HW_VERSIONING
 *
 * The manifest allows the system to know whether a hardware option
 * say for example the PX4IO is an no-pop option vs it is broken.
 *
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <board_config.h>

#include <inttypes.h>
#include <stdbool.h>
#include <syslog.h>

#include "systemlib/px4_macros.h"

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

typedef struct {
	uint32_t                hw_ver_rev; /* the version and revision */
	const px4_hw_mft_item_t *mft;       /* The first entry */
	uint32_t                entries;    /* the lenght of the list */
} px4_hw_mft_list_entry_t;

typedef px4_hw_mft_list_entry_t *px4_hw_mft_list_entry;
#define px4_hw_mft_list_uninitialized (px4_hw_mft_list_entry) -1

static const px4_hw_mft_item_t device_unsupported = {0, 0, 0};

static const px4_hw_mft_item_t hw_mft_list_FMUv2[] = {
	{
		.present     = 1,
		.mandatory   = 1,
		.connection  = px4_hw_con_onboard,
	},
};

static const px4_hw_mft_item_t hw_mft_list_FMUv2Mini[] = {
	{
		.present     = 1,
		.mandatory   = 1,
		.connection  = px4_hw_con_onboard,
	},
};
static const px4_hw_mft_item_t hw_mft_list_FMUv2X[] = {
	{
		.present     = 1,
		.mandatory   = 1,
		.connection  = px4_hw_con_onboard,
	},
};
static const px4_hw_mft_item_t hw_mft_list_FMUv3[] = {
	{
		.present     = 1,
		.mandatory   = 1,
		.connection  = px4_hw_con_onboard,
	},
};

static px4_hw_mft_list_entry_t mft_lists[] = {
	{HW_VER_FMUV2,        hw_mft_list_FMUv2,     arraySize(hw_mft_list_FMUv2)},
	{HW_VER_FMUV2MINI,    hw_mft_list_FMUv2Mini, arraySize(hw_mft_list_FMUv2Mini)},
	{HW_VER_FMUV2X,       hw_mft_list_FMUv2X,    arraySize(hw_mft_list_FMUv2X)},
	{HW_VER_FMUV3,        hw_mft_list_FMUv3,     arraySize(hw_mft_list_FMUv3)},
};

/************************************************************************************
 * Name: board_query_manifest
 *
 * Description:
 *   Optional returns manifest item.
 *
 * Input Parameters:
 *   manifest_id - the ID for the manifest item to retrieve
 *
 * Returned Value:
 *   0 - item is not in manifest => assume legacy operations
 *   pointer to a manifest item
 *
 ************************************************************************************/

__EXPORT px4_hw_mft_item board_query_manifest(px4_hw_mft_item_id_t id)
{
	static px4_hw_mft_list_entry boards_manifest = px4_hw_mft_list_uninitialized;

	if (boards_manifest == px4_hw_mft_list_uninitialized) {
		uint32_t ver_rev = board_get_hw_version();

		for (unsigned i = 0; i < arraySize(mft_lists); i++) {
			if (mft_lists[i].hw_ver_rev == ver_rev) {
				boards_manifest = &mft_lists[i];
				break;
			}
		}

		if (boards_manifest == px4_hw_mft_list_uninitialized) {
			syslog(LOG_ERR, "[boot] Board %4"  PRIx32 " is not supported!\n", ver_rev);
		}
	}

	px4_hw_mft_item rv = &device_unsupported;

	if (boards_manifest != px4_hw_mft_list_uninitialized &&
	    id < boards_manifest->entries) {
		rv = &boards_manifest->mft[id];
	}

	return rv;
}
