/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _I386_UCODE_H_
#define _I386_UCODE_H_

/*
 *  ucode.h
 *
 *  Interface definitions for the microcode updater interface sysctl
 */

/* Intel defined microcode format */
struct intel_ucupdate {
	/* Header information */
	uint32_t header_version;
	uint32_t update_revision;
	uint32_t date;
	uint32_t processor_signature;
	uint32_t checksum;
	uint32_t loader_revision;
	uint32_t processor_flags;
	uint32_t data_size;
	uint32_t total_size;

	/* Reserved for future expansion */
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t reserved2;

	/* First word of the update data */
	uint32_t data;
};

struct amd_ucupdate {
	uint32_t data_code;
	uint32_t update_revision; /* patch_id */
	uint16_t mc_patch_data_id;
	uint8_t  mc_patch_data_len;
	uint8_t  init_flag;
	uint32_t mc_patch_data_checksum;
	uint32_t nb_device_id;
	uint32_t sb_device_id;
	uint16_t cpu_revision_id;
	uint8_t  nb_revision_id;
	uint8_t  sb_revision_id;
	uint8_t  bios_api_rev;

	uint8_t  reserved0;
	uint8_t  reserved1;
	uint8_t  reserved2;

	uint32_t match_reg0;
	uint32_t match_reg1;
	uint32_t match_reg2;
	uint32_t match_reg3;
	uint32_t match_reg4;
	uint32_t match_reg5;
	uint32_t match_reg6;
	uint32_t match_reg7;
};

extern int ucode_interface(uint64_t addr);
extern void ucode_update_wake_and_apply_cpu_was(void);

#endif
