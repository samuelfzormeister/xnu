/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
#include <vm/vm_page.h>
#include <pexpert/pexpert.h>

#include <i386/cpu_threads.h>
#include <i386/cpuid.h>

int force_tecs_at_idle;
int tecs_mode_supported;

static  boolean_t       cpuid_dbg
#if DEBUG
        = TRUE;
#else
        = FALSE;
#endif
#define DBG(x...)                       \
	do {                            \
	        if (cpuid_dbg)          \
	                kprintf(x);     \
	} while (0)                     \

#define min(a, b) ((a) < (b) ? (a) : (b))
#define quad(hi, lo)     (((uint64_t)(hi)) << 32 | (lo))

/*
 * Leaf 2 cache descriptor encodings.
 */
typedef enum {
	_NULL_,         /* NULL (empty) descriptor */
	CACHE,          /* Cache */
	TLB,            /* TLB */
	STLB,           /* Shared second-level unified TLB */
	PREFETCH        /* Prefetch size */
} cpuid_leaf2_desc_type_t;

typedef enum {
	NA,             /* Not Applicable */
	FULLY,          /* Fully-associative */
	TRACE,          /* Trace Cache (P4 only) */
	INST,           /* Instruction TLB */
	DATA,           /* Data TLB */
	DATA0,          /* Data TLB, 1st level */
	DATA1,          /* Data TLB, 2nd level */
	L1,             /* L1 (unified) cache */
	L1_INST,        /* L1 Instruction cache */
	L1_DATA,        /* L1 Data cache */
	L2,             /* L2 (unified) cache */
	L3,             /* L3 (unified) cache */
	L2_2LINESECTOR, /* L2 (unified) cache with 2 lines per sector */
	L3_2LINESECTOR, /* L3(unified) cache with 2 lines per sector */
	SMALL,          /* Small page TLB */
	LARGE,          /* Large page TLB */
	BOTH            /* Small and Large page TLB */
} cpuid_leaf2_qualifier_t;

typedef struct cpuid_cache_descriptor {
	uint8_t         value;          /* descriptor code */
	uint8_t         type;           /* cpuid_leaf2_desc_type_t */
	uint8_t         level;          /* level of cache/TLB hierachy */
	uint8_t         ways;           /* wayness of cache */
	uint16_t        size;           /* cachesize or TLB pagesize */
	uint16_t        entries;        /* number of TLB entries or linesize */
} cpuid_cache_descriptor_t;

/*
 * These multipliers are used to encode 1*K .. 64*M in a 16 bit size field
 */
#define K       (1)
#define M       (1024)

/*
 * Intel cache descriptor table:
 */
static cpuid_cache_descriptor_t intel_cpuid_leaf2_descriptor_table[] = {
//	-------------------------------------------------------
//	value	type	level		ways	size	entries
//	-------------------------------------------------------
	{ 0x00, _NULL_, NA, NA, NA, NA  },
	{ 0x01, TLB, INST, 4, SMALL, 32  },
	{ 0x02, TLB, INST, FULLY, LARGE, 2   },
	{ 0x03, TLB, DATA, 4, SMALL, 64  },
	{ 0x04, TLB, DATA, 4, LARGE, 8   },
	{ 0x05, TLB, DATA1, 4, LARGE, 32  },
	{ 0x06, CACHE, L1_INST, 4, 8 * K, 32  },
	{ 0x08, CACHE, L1_INST, 4, 16 * K, 32  },
	{ 0x09, CACHE, L1_INST, 4, 32 * K, 64  },
	{ 0x0A, CACHE, L1_DATA, 2, 8 * K, 32  },
	{ 0x0B, TLB, INST, 4, LARGE, 4   },
	{ 0x0C, CACHE, L1_DATA, 4, 16 * K, 32  },
	{ 0x0D, CACHE, L1_DATA, 4, 16 * K, 64  },
	{ 0x0E, CACHE, L1_DATA, 6, 24 * K, 64  },
	{ 0x21, CACHE, L2, 8, 256 * K, 64  },
	{ 0x22, CACHE, L3_2LINESECTOR, 4, 512 * K, 64  },
	{ 0x23, CACHE, L3_2LINESECTOR, 8, 1 * M, 64  },
	{ 0x25, CACHE, L3_2LINESECTOR, 8, 2 * M, 64  },
	{ 0x29, CACHE, L3_2LINESECTOR, 8, 4 * M, 64  },
	{ 0x2C, CACHE, L1_DATA, 8, 32 * K, 64  },
	{ 0x30, CACHE, L1_INST, 8, 32 * K, 64  },
	{ 0x40, CACHE, L2, NA, 0, NA  },
	{ 0x41, CACHE, L2, 4, 128 * K, 32  },
	{ 0x42, CACHE, L2, 4, 256 * K, 32  },
	{ 0x43, CACHE, L2, 4, 512 * K, 32  },
	{ 0x44, CACHE, L2, 4, 1 * M, 32  },
	{ 0x45, CACHE, L2, 4, 2 * M, 32  },
	{ 0x46, CACHE, L3, 4, 4 * M, 64  },
	{ 0x47, CACHE, L3, 8, 8 * M, 64  },
	{ 0x48, CACHE, L2, 12, 3 * M, 64  },
	{ 0x49, CACHE, L2, 16, 4 * M, 64  },
	{ 0x4A, CACHE, L3, 12, 6 * M, 64  },
	{ 0x4B, CACHE, L3, 16, 8 * M, 64  },
	{ 0x4C, CACHE, L3, 12, 12 * M, 64  },
	{ 0x4D, CACHE, L3, 16, 16 * M, 64  },
	{ 0x4E, CACHE, L2, 24, 6 * M, 64  },
	{ 0x4F, TLB, INST, NA, SMALL, 32  },
	{ 0x50, TLB, INST, NA, BOTH, 64  },
	{ 0x51, TLB, INST, NA, BOTH, 128 },
	{ 0x52, TLB, INST, NA, BOTH, 256 },
	{ 0x55, TLB, INST, FULLY, BOTH, 7   },
	{ 0x56, TLB, DATA0, 4, LARGE, 16  },
	{ 0x57, TLB, DATA0, 4, SMALL, 16  },
	{ 0x59, TLB, DATA0, FULLY, SMALL, 16  },
	{ 0x5A, TLB, DATA0, 4, LARGE, 32  },
	{ 0x5B, TLB, DATA, NA, BOTH, 64  },
	{ 0x5C, TLB, DATA, NA, BOTH, 128 },
	{ 0x5D, TLB, DATA, NA, BOTH, 256 },
	{ 0x60, CACHE, L1, 16 * K, 8, 64  },
	{ 0x61, CACHE, L1, 4, 8 * K, 64  },
	{ 0x62, CACHE, L1, 4, 16 * K, 64  },
	{ 0x63, CACHE, L1, 4, 32 * K, 64  },
	{ 0x70, CACHE, TRACE, 8, 12 * K, NA  },
	{ 0x71, CACHE, TRACE, 8, 16 * K, NA  },
	{ 0x72, CACHE, TRACE, 8, 32 * K, NA  },
	{ 0x76, TLB, INST, NA, BOTH, 8   },
	{ 0x78, CACHE, L2, 4, 1 * M, 64  },
	{ 0x79, CACHE, L2_2LINESECTOR, 8, 128 * K, 64  },
	{ 0x7A, CACHE, L2_2LINESECTOR, 8, 256 * K, 64  },
	{ 0x7B, CACHE, L2_2LINESECTOR, 8, 512 * K, 64  },
	{ 0x7C, CACHE, L2_2LINESECTOR, 8, 1 * M, 64  },
	{ 0x7D, CACHE, L2, 8, 2 * M, 64  },
	{ 0x7F, CACHE, L2, 2, 512 * K, 64  },
	{ 0x80, CACHE, L2, 8, 512 * K, 64  },
	{ 0x82, CACHE, L2, 8, 256 * K, 32  },
	{ 0x83, CACHE, L2, 8, 512 * K, 32  },
	{ 0x84, CACHE, L2, 8, 1 * M, 32  },
	{ 0x85, CACHE, L2, 8, 2 * M, 32  },
	{ 0x86, CACHE, L2, 4, 512 * K, 64  },
	{ 0x87, CACHE, L2, 8, 1 * M, 64  },
	{ 0xB0, TLB, INST, 4, SMALL, 128 },
	{ 0xB1, TLB, INST, 4, LARGE, 8   },
	{ 0xB2, TLB, INST, 4, SMALL, 64  },
	{ 0xB3, TLB, DATA, 4, SMALL, 128 },
	{ 0xB4, TLB, DATA1, 4, SMALL, 256 },
	{ 0xB5, TLB, DATA1, 8, SMALL, 64  },
	{ 0xB6, TLB, DATA1, 8, SMALL, 128 },
	{ 0xBA, TLB, DATA1, 4, BOTH, 64  },
	{ 0xC1, STLB, DATA1, 8, SMALL, 1024},
	{ 0xCA, STLB, DATA1, 4, SMALL, 512 },
	{ 0xD0, CACHE, L3, 4, 512 * K, 64  },
	{ 0xD1, CACHE, L3, 4, 1 * M, 64  },
	{ 0xD2, CACHE, L3, 4, 2 * M, 64  },
	{ 0xD3, CACHE, L3, 4, 4 * M, 64  },
	{ 0xD4, CACHE, L3, 4, 8 * M, 64  },
	{ 0xD6, CACHE, L3, 8, 1 * M, 64  },
	{ 0xD7, CACHE, L3, 8, 2 * M, 64  },
	{ 0xD8, CACHE, L3, 8, 4 * M, 64  },
	{ 0xD9, CACHE, L3, 8, 8 * M, 64  },
	{ 0xDA, CACHE, L3, 8, 12 * M, 64  },
	{ 0xDC, CACHE, L3, 12, 1536 * K, 64  },
	{ 0xDD, CACHE, L3, 12, 3 * M, 64  },
	{ 0xDE, CACHE, L3, 12, 6 * M, 64  },
	{ 0xDF, CACHE, L3, 12, 12 * M, 64  },
	{ 0xE0, CACHE, L3, 12, 18 * M, 64  },
	{ 0xE2, CACHE, L3, 16, 2 * M, 64  },
	{ 0xE3, CACHE, L3, 16, 4 * M, 64  },
	{ 0xE4, CACHE, L3, 16, 8 * M, 64  },
	{ 0xE5, CACHE, L3, 16, 16 * M, 64  },
	{ 0xE6, CACHE, L3, 16, 24 * M, 64  },
	{ 0xF0, PREFETCH, NA, NA, 64, NA  },
	{ 0xF1, PREFETCH, NA, NA, 128, NA  },
	{ 0xFF, CACHE, NA, NA, 0, NA  }
};
#define INTEL_LEAF2_DESC_NUM (sizeof(intel_cpuid_leaf2_descriptor_table) / \
	                        sizeof(cpuid_cache_descriptor_t))


static void do_cwas(i386_cpu_info_t *cpuinfo, boolean_t on_slave);
static void cpuid_do_precpuid_was(void);

static inline cpuid_cache_descriptor_t *
cpuid_leaf2_find(uint8_t value)
{
	unsigned int    i;

	for (i = 0; i < INTEL_LEAF2_DESC_NUM; i++) {
		if (intel_cpuid_leaf2_descriptor_table[i].value == value) {
			return &intel_cpuid_leaf2_descriptor_table[i];
		}
	}
	return NULL;
}

typedef struct cpuid_amd_cache_associativity {
    uint32_t value;
    uint32_t actual;
} cpuid_amd_cache_associativity_t;

static cpuid_amd_cache_associativity_t amd_associativity_table[] = {
    {0x00, 0x00},
    {0x01, 0x01},
    {0x02, 0x02},
    {0x04, 0x04},
    {0x06, 0x08},
    {0x08, 0x10},
    {0x0A, 0x20},
    {0x0B, 0x30},
    {0x0C, 0x40},
    {0x0D, 0x60},
    {0x0E, 0x80},
    {0x0F, 0xFFFF},
};

#define AMD_ASSOCIATIVITY_NUM sizeof(amd_associativity_table) / \
                                sizeof(cpuid_amd_cache_associativity_t)

static inline cpuid_amd_cache_associativity_t *
cpuid_amd_find_associativity(uint32_t raw)
{
    unsigned int i;

    for (i = 0; i < AMD_ASSOCIATIVITY_NUM; i++) {
        if (amd_associativity_table[i].value == raw) {
            return &amd_associativity_table[i];
        }
    }

    return NULL;
}

/*
 * CPU identification routines.
 */

static i386_cpu_info_t  cpuid_cpu_info;
static i386_cpu_info_t  *cpuid_cpu_infop = NULL;

static void
cpuid_fn(uint32_t selector, uint32_t *result)
{
	do_cpuid(selector, result);
	DBG("cpuid_fn(0x%08x) eax:0x%08x ebx:0x%08x ecx:0x%08x edx:0x%08x\n",
	    selector, result[0], result[1], result[2], result[3]);
}

static const char *cache_type_str[LCACHE_MAX] = {
	"Lnone", "L1I", "L1D", "L2U", "L3U"
};

/*
 * This will eventually become a large section by force of nature.
 *
 * Should this be migrated to somewhere else?
 */
static cwa_classifier_e dummy_enabled(i386_cpu_info_t *)
{
	return CWA_OFF;
}

static void dummy_apply(i386_cpu_info_t *, boolean_t)
{
}

static cwa_classifier_e intel_segchk_enabled(i386_cpu_info_t *cpuinfo)
{
	/* First, check to see if this CPU requires the workaround */
	if ((cpuinfo->cpuid_leaf7_extfeatures & CPUID_LEAF7_EXTFEATURE_ACAPMSR) != 0) {
		/* We have ARCHCAP, so check it for either RDCL_NO or MDS_NO */
		uint64_t archcap_msr = rdmsr64(MSR_IA32_ARCH_CAPABILITIES);
		if ((archcap_msr & (MSR_IA32_ARCH_CAPABILITIES_RDCL_NO | MSR_IA32_ARCH_CAPABILITIES_MDS_NO)) != 0) {
			/* Workaround not needed */
			return CWA_OFF;
		}
	}

	if ((cpuinfo->cpuid_leaf7_extfeatures & CPUID_LEAF7_EXTFEATURE_MDCLEAR) != 0) {
		return CWA_ON;
	}

	/*
	 * If the CPU supports the ARCHCAP MSR and neither the RDCL_NO bit nor the MDS_NO
	 * bit are set, OR the CPU does not support the ARCHCAP MSR and the CPU does
	 * not enumerate the presence of the enhanced VERW instruction, report
	 * that the workaround should not be enabled.
	 */

	return CWA_OFF;
}

static void intel_segchk_apply(i386_cpu_info_t *cpuinfo, boolean_t on_slave)
{
	extern int force_thread_policy_tecs;

	if (on_slave) {
		return;
	}

	switch (cpuid_wa_required(CPU_INTEL_SEGCHK)) {
	case CWA_FORCE_ON:
		force_thread_policy_tecs = 1;

		/* If hyperthreaded, enable idle workaround */
		if (cpuinfo->thread_count > cpuinfo->core_count) {
			force_tecs_at_idle = 1;
		}

	/*FALLTHROUGH*/
	case CWA_ON:
		tecs_mode_supported = 1;
		break;

	case CWA_FORCE_OFF:
	case CWA_OFF:
		tecs_mode_supported = 0;
		force_tecs_at_idle = 0;
		force_thread_policy_tecs = 0;
		break;

	default:
		break;
	}
}

static cwa_classifier_e intel_tsxfa_enabled(i386_cpu_info_t *cpuinfo)
{
	/*
	 * Otherwise, if the CPU supports both TSX(HLE) and FORCE_ABORT, return that
	 * the workaround should be enabled.
	 */
	if ((cpuinfo->cpuid_leaf7_extfeatures & CPUID_LEAF7_EXTFEATURE_TSXFA) != 0 &&
	    (cpuinfo->cpuid_leaf7_features & CPUID_LEAF7_FEATURE_RTM) != 0) {
		return CWA_ON;
	}

	return CWA_OFF;
}

/*
 * Workaround for reclaiming perf counter 3 due to TSX memory ordering erratum.
 * This workaround does not support being forcibly set (since an MSR must be
 * enumerated, lest we #GP when forced to access it.)
 */
static void intel_tsxfa_apply(i386_cpu_info_t *cpuinfo, boolean_t)
{
	/* This must be executed on all logical processors */
	wrmsr64(MSR_IA32_TSX_FORCE_ABORT,
		 rdmsr64(MSR_IA32_TSX_FORCE_ABORT) | MSR_IA32_TSXFA_RTM_FORCE_ABORT);
}

/*
 * Jaguar and Bulldozer can and WILL have a broken RDRAND once the system wakes from
 * suspend, so as Linux does, we disable it when CPU workarounds are applied.
 */
static cwa_classifier_e amd_rdrand_suspend_enabled(i386_cpu_info_t *cpuinfo)
{
	if (cpuinfo->cpuid_vendor_id == CPUID_VENDOR_ID_AMD) {
		if (cpuinfo->cpuid_family == 0x15 || cpuinfo->cpuid_family == 0x16) {
			return CWA_ON;
		}
	}

	return CWA_OFF;
}

struct {
	cwa_classifier_e (*enabled)(i386_cpu_info_t *cpuinfo);
	void (*do_cwa)(i386_cpu_info_t *cpuinfo, boolean_t on_slave);
} cpuid_wa_list[CPU_WA_MAX] = {
	{&dummy_enabled, &dummy_apply},					/* !!! LEAVE EMPTY !!! */
	{&intel_segchk_enabled, &intel_segchk_apply}, 	/* CPU_INTEL_SEGCHK */
	{&intel_tsxfa_enabled, &intel_tsxfa_apply},		/* CPU_INTEL_TSXFA */
	{&dummy_enabled, &dummy_apply},					/* CPU_AMD_RDRAND_SUSPEND */
	{&dummy_enabled, &dummy_apply},					/* CPU_AMD_WAY_ACCESS_FILT */
	{&dummy_enabled, &dummy_apply},					/* CPU_AMD_ZEN_ERRATUM_1076 */
	{&dummy_enabled, &dummy_apply},					/* CPU_AMD_ZEN_ERRATUM_1054 */
	{NULL, NULL},
};

static void
do_cwas(i386_cpu_info_t *cpuinfo, boolean_t on_slave)
{
	for (int i = 0; i < CPU_WA_MAX; i++) {
		cwa_classifier_e en = cpuid_wa_required(i);
		if (en == CWA_ON || en == CWA_FORCE_ON) {
			cpuid_wa_list[i].do_cwa(cpuinfo, on_slave);
		}
	}
}

void
cpuid_do_was(void)
{
	do_cwas(cpuid_info(), TRUE);
}

static void
cpuid_set_keylocker_info( i386_cpu_info_t * info_p )
{
	uint32_t reg[4];

	cpuid_fn(0x19, reg);

	info_p->cpuid_keylocker_leaf.kle_cpl0_only = 0 != (reg[eax] & 0x1);
	info_p->cpuid_keylocker_leaf.kle_no_encrypt = 0 != (reg[eax] & 0x2);
	info_p->cpuid_keylocker_leaf.kle_no_decrypt = 0 != (reg[eax] & 0x4);

	info_p->cpuid_keylocker_leaf.kle_enabled = 0 != (reg[ebx] & 0x1);
	info_p->cpuid_keylocker_leaf.kle_supported = 0 != (reg[ebx] & 0x4);
	info_p->cpuid_keylocker_leaf.kle_iwkey_msrs = 0 != (reg[ebx] & 0x10);

	info_p->cpuid_keylocker_leaf.kle_no_backup_supported = 0 != (reg[ecx] & 0x1);
	info_p->cpuid_keylocker_leaf.kle_random_iwkey_supported = 0 != (reg[ecx] & 0x2);
}

/*
 * Tech doc refs:
 * - BIOS and Kernel Developer’s Guide for AMD NPT Family 0Fh Processors
 * - AMD Family 10h Processor BKDG
 * - AMD Family 11h Processor BKDG
 * - AMD Family 12h Processor BKDG
 * - BKDG for AMD Family 14h Models 00h-0Fh Processors
 */
static void
cpuid_set_cache_info_amd_legacy( i386_cpu_info_t * info_p )
{
    uint32_t reg[4];
    uint32_t assoc[LCACHE_MAX];
    uint32_t linesizes[LCACHE_MAX];
    uint32_t sets;
    uint32_t colors;
    int      i;

    DBG("cpuid_set_cache_info_amd_legacy(%p)\n", info_p);

    /* CPUID Fn8000_0005 TLB and L1 Cache Identifiers */
    reg[eax] = 0x80000005;
    cpuid(reg);
    DBG("cpuid(0x80000005)\n");

    /* L1 Data cache */
    info_p->cache_sharing[L1D] = 1;
    info_p->cache_size[L1D] = bitfield32(reg[ecx], 31, 24) * KB;
    info_p->cache_partitions[L1D] = bitfield32(reg[ecx], 15, 8);
    linesizes[L1D] = bitfield32(reg[ecx], 7, 0);
    assoc[L1D] = bitfield32(reg[ecx], 23, 16);

    DBG(" cache_size[L1D]      : %d\n",
		info_p->cache_size[L1D]);
	DBG(" cache_sharing[L1D]   : %d\n",
		info_p->cache_sharing[L1D]);
	DBG(" cache_partitions[L1D]: %d\n",
		info_p->cache_partitions[L1D]);

    /* L1 Instruction */
    info_p->cache_sharing[L1I] = 1;
    info_p->cache_size[L1I] = bitfield32(reg[edx], 31, 24);
    info_p->cache_partitions[L1I] = bitfield32(reg[edx], 15, 8) * KB;
    linesizes[L1I] = bitfield32(reg[edx], 7, 0);
    assoc[L1I] = bitfield32(reg[ecx], 23, 16);

    DBG(" cache_size[L1I]      : %d\n",
		info_p->cache_size[L1I]);
	DBG(" cache_sharing[L1I]   : %d\n",
		info_p->cache_sharing[L1I]);
	DBG(" cache_partitions[L1I]: %d\n",
		info_p->cache_partitions[L1I]);

	reg[eax] = 0x80000006;
    cpuid(reg);
    DBG("cpuid(0x80000006)\n");

    /* L2 cache */
    if (info_p->cpuid_cpufamily == CPUFAMILY_AMD_K8) {
        info_p->cache_sharing[L2U] = 1;
        info_p->cache_size[L2U] = bitfield32(reg[ecx], 31, 24);
        info_p->cache_partitions[L2U] = bitfield32(reg[ecx], 15, 8) * KB;
        linesizes[L2U] = bitfield32(reg[ecx], 7, 0);
        assoc[L2U] = cpuid_amd_find_associativity(bitfield32(reg[ecx], 23, 16))->actual;
    } else {
        /*
         * This is the case for:
         * - K10
         * - Bobcat
         *
         * I haven't validated beyond Bulldozer, as by that point 0x8000001d
         * was available.
         */
        info_p->cache_sharing[L2U] = 1;
        info_p->cache_size[L2U] = bitfield32(reg[ecx], 31, 16) * KB;
        info_p->cache_partitions[L2U] = bitfield32(reg[ecx], 11, 8);
        linesizes[L2U] = bitfield32(reg[ecx], 7, 0);
        assoc[L2U] = cpuid_amd_find_associativity(bitfield32(reg[ecx], 15, 12))->actual;
    }

    DBG(" cache_size[L2U]      : %d\n",
		info_p->cache_size[L2U]);
	DBG(" cache_sharing[L2U]   : %d\n",
		info_p->cache_sharing[L2U]);
	DBG(" cache_partitions[L2U]: %d\n",
		info_p->cache_partitions[L2U]);

    /* EDX is non-zero if there is L3 cache present. */
    if (reg[edx]) {
        /* According to the BKDGs, only true K10 (Family ID 0x0f) has L3 cache? */
        info_p->cache_sharing[L3U] = info_p->thread_count;
        info_p->cache_size[L3U] = bitfield32(reg[edx], 31, 18) * (512 * KB);
        info_p->cache_partitions[L3U] = bitfield32(reg[edx], 11, 8);
        linesizes[L3U] = bitfield32(reg[edx], 7, 0);
        assoc[L3U] = cpuid_amd_find_associativity(bitfield32(reg[ecx], 15, 12))->actual;

        DBG(" cache_size[L3U]      : %d\n",
            info_p->cache_size[L3U]);
        DBG(" cache_sharing[L3U]   : %d\n",
            info_p->cache_sharing[L3U]);
        DBG(" cache_partitions[L3U]: %d\n",
            info_p->cache_partitions[L3U]);
    }

    for (i = 0; i < LCACHE_MAX; i++) {
        sets = info_p->cache_size[i] / (info_p->cache_partitions[i] * linesizes[i] * assoc[i]);

        colors = (sets * linesizes[i]) >> 12;
        if (colors > vm_cache_geometry_colors) {
            vm_cache_geometry_colors = colors;
        }
    }

    /* Update other fields here */
    info_p->cpuid_cache_L2_associativity = assoc[L2U];
    info_p->cpuid_cache_linesize = linesizes[L2U];
}

/* this function is Intel-specific */
static void
cpuid_set_cache_info( i386_cpu_info_t * info_p )
{
	uint32_t        cpuid_result[4];
	uint32_t        reg[4];
	uint32_t        index;
	uint32_t        linesizes[LCACHE_MAX];
	unsigned int    i;
	unsigned int    j;
	boolean_t       cpuid_deterministic_supported = FALSE;
	uint32_t        cpuid_deterministic_leaf;

	DBG("cpuid_set_cache_info(%p)\n", info_p);

	bzero( linesizes, sizeof(linesizes));

	/* Get processor cache descriptor info using leaf 2.  We don't use
	 * this internally, but must publish it for KEXTs.
	 */
	cpuid_fn(2, cpuid_result);
	for (j = 0; j < 4; j++) {
		if ((cpuid_result[j] >> 31) == 1) {     /* bit31 is validity */
			continue;
		}
		((uint32_t *) info_p->cache_info)[j] = cpuid_result[j];
	}
	/* first byte gives number of cpuid calls to get all descriptors */
	for (i = 1; i < info_p->cache_info[0]; i++) {
		if (i * 16 > sizeof(info_p->cache_info)) {
			break;
		}
		cpuid_fn(2, cpuid_result);
		for (j = 0; j < 4; j++) {
			if ((cpuid_result[j] >> 31) == 1) {
				continue;
			}
			((uint32_t *) info_p->cache_info)[4 * i + j] =
			    cpuid_result[j];
		}
	}

	/*
	 * Get cache info using leaf 4, the "deterministic cache parameters."
	 * Most processors Mac OS X supports implement this flavor of CPUID.
	 * Loop over each cache on the processor.
	 */
	cpuid_fn(0, cpuid_result);
	if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_INTEL &&
	    cpuid_result[eax] >= 4) {
		cpuid_deterministic_supported = TRUE;
		cpuid_deterministic_leaf = 0x4;
	}

	cpuid_fn(0x80000000, cpuid_result);
	if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_AMD &&
	    cpuid_result[eax] >= 0x8000001d) {
		cpuid_deterministic_supported = TRUE;
		cpuid_deterministic_leaf = 0x8000001d;
	}

	for (index = 0; cpuid_deterministic_supported; index++) {
		cache_type_t    type = Lnone;
		uint32_t        cache_type;
		uint32_t        cache_level;
		uint32_t        cache_sharing;
		uint32_t        cache_linesize;
		uint32_t        cache_sets;
		uint32_t        cache_associativity;
		uint32_t        cache_size;
		uint32_t        cache_partitions;
		uint32_t        colors;

		reg[eax] = cpuid_deterministic_leaf;                      /* cpuid cache leaf request */
		reg[ecx] = index;       	                              /* index starting at 0 */
		cpuid(reg);
		DBG("cpuid(4) index=%d eax=0x%x\n", index, reg[eax]);
		cache_type = bitfield32(reg[eax], 4, 0);
		if (cache_type == 0) {
			break;          /* no more caches */
		}
		cache_level             = bitfield32(reg[eax], 7, 5);
		cache_sharing           = bitfield32(reg[eax], 25, 14) + 1;
		info_p->cpuid_cores_per_package
		        = bitfield32(reg[eax], 31, 26) + 1;
		cache_linesize          = bitfield32(reg[ebx], 11, 0) + 1;
		cache_partitions        = bitfield32(reg[ebx], 21, 12) + 1;
		cache_associativity     = bitfield32(reg[ebx], 31, 22) + 1;
		cache_sets              = bitfield32(reg[ecx], 31, 0) + 1;

		/* Map type/levels returned by CPUID into cache_type_t */
		switch (cache_level) {
		case 1:
			type = cache_type == 1 ? L1D :
			    cache_type == 2 ? L1I :
			    Lnone;
			break;
		case 2:
			type = cache_type == 3 ? L2U :
			    Lnone;
			break;
		case 3:
			type = cache_type == 3 ? L3U :
			    Lnone;
			break;
		default:
			type = Lnone;
		}

		/* The total size of a cache is:
		 *	( linesize * sets * associativity * partitions )
		 */
		if (type != Lnone) {
			cache_size = cache_linesize * cache_sets *
			    cache_associativity * cache_partitions;
			info_p->cache_size[type] = cache_size;
			info_p->cache_sharing[type] = cache_sharing;
			info_p->cache_partitions[type] = cache_partitions;
			linesizes[type] = cache_linesize;

			DBG(" cache_size[%s]      : %d\n",
			    cache_type_str[type], cache_size);
			DBG(" cache_sharing[%s]   : %d\n",
			    cache_type_str[type], cache_sharing);
			DBG(" cache_partitions[%s]: %d\n",
			    cache_type_str[type], cache_partitions);

			/*
			 * Overwrite associativity determined via
			 * CPUID.0x80000006 -- this leaf is more
			 * accurate
			 */
			if (type == L2U) {
				info_p->cpuid_cache_L2_associativity = cache_associativity;
			}
			/*
			 * Adjust #sets to account for the N CBos
			 * This is because addresses are hashed across CBos
			 */
			if (type == L3U && info_p->core_count) {
				cache_sets = cache_sets / info_p->core_count;
			}

			/* Compute the number of page colors for this cache,
			 * which is:
			 *	( linesize * sets ) / page_size
			 *
			 * To help visualize this, consider two views of a
			 * physical address.  To the cache, it is composed
			 * of a line offset, a set selector, and a tag.
			 * To VM, it is composed of a page offset, a page
			 * color, and other bits in the pageframe number:
			 *
			 *           +-----------------+---------+--------+
			 *  cache:   |       tag       |   set   | offset |
			 *           +-----------------+---------+--------+
			 *
			 *           +-----------------+-------+----------+
			 *  VM:      |    don't care   | color | pg offset|
			 *           +-----------------+-------+----------+
			 *
			 * The color is those bits in (set+offset) not covered
			 * by the page offset.
			 */
			colors = (cache_linesize * cache_sets) >> 12;

			if (colors > vm_cache_geometry_colors) {
				vm_cache_geometry_colors = colors;
			}
		}
	}

	if (cpuid_deterministic_supported == FALSE &&
	    info_p->cpuid_vendor_id == CPUID_VENDOR_ID_AMD) {
		cpuid_set_cache_info_amd_legacy(info_p);
		linesizes[L2U] = info_p->cpuid_cache_linesize;
	}

	DBG(" vm_cache_geometry_colors: %d\n", vm_cache_geometry_colors);

	/*
	 * If deterministic cache parameters are not available, use
	 * something else
	 */
	if (info_p->cpuid_cores_per_package == 0) {
		info_p->cpuid_cores_per_package = 1;

		/* cpuid define in 1024 quantities */
		info_p->cache_size[L2U] = info_p->cpuid_cache_size * 1024;
		info_p->cache_sharing[L2U] = 1;
		info_p->cache_partitions[L2U] = 1;

		linesizes[L2U] = info_p->cpuid_cache_linesize;

		DBG(" cache_size[L2U]      : %d\n",
		    info_p->cache_size[L2U]);
		DBG(" cache_sharing[L2U]   : 1\n");
		DBG(" cache_partitions[L2U]: 1\n");
		DBG(" linesizes[L2U]       : %d\n",
		    info_p->cpuid_cache_linesize);
	}

	/*
	 * What linesize to publish?  We use the L2 linesize if any,
	 * else the L1D.
	 */
	if (linesizes[L2U]) {
		info_p->cache_linesize = linesizes[L2U];
	} else if (linesizes[L1D]) {
		info_p->cache_linesize = linesizes[L1D];
	} else {
		panic("no linesize");
	}
	DBG(" cache_linesize    : %d\n", info_p->cache_linesize);

	/*
	 * Extract and publish TLB information from Leaf 2 descriptors.
	 */
	DBG(" %ld leaf2 descriptors:\n", sizeof(info_p->cache_info));
	for (i = 1; i < sizeof(info_p->cache_info); i++) {
		cpuid_cache_descriptor_t        *descp;
		int                             id;
		int                             level;
		int                             page;

		DBG(" 0x%02x", info_p->cache_info[i]);
		descp = cpuid_leaf2_find(info_p->cache_info[i]);
		if (descp == NULL) {
			continue;
		}

		switch (descp->type) {
		case TLB:
			page = (descp->size == SMALL) ? TLB_SMALL : TLB_LARGE;
			/* determine I or D: */
			switch (descp->level) {
			case INST:
				id = TLB_INST;
				break;
			case DATA:
			case DATA0:
			case DATA1:
				id = TLB_DATA;
				break;
			default:
				continue;
			}
			/* determine level: */
			switch (descp->level) {
			case DATA1:
				level = 1;
				break;
			default:
				level = 0;
			}
			info_p->cpuid_tlb[id][page][level] = descp->entries;
			break;
		case STLB:
			info_p->cpuid_stlb = descp->entries;
		}
	}
	DBG("\n");
}

static void
cpuid_set_generic_info(i386_cpu_info_t *info_p)
{
	uint32_t        reg[4];
	char            str[128], *p;

	DBG("cpuid_set_generic_info(%p)\n", info_p);

	/* do cpuid 0 to get vendor */
	cpuid_fn(0, reg);
	info_p->cpuid_max_basic = reg[eax];
	bcopy((char *)&reg[ebx], &info_p->cpuid_vendor[0], 4); /* ug */
	bcopy((char *)&reg[ecx], &info_p->cpuid_vendor[8], 4);
	bcopy((char *)&reg[edx], &info_p->cpuid_vendor[4], 4);
	info_p->cpuid_vendor[12] = 0;

	if ((strncmp(CPUID_VID_INTEL, info_p->cpuid_vendor,
		min(strlen(CPUID_STRING_UNKNOWN) + 1, sizeof(info_p->cpuid_vendor)))) == 0) {
		info_p->cpuid_vendor_id = CPUID_VENDOR_ID_INTEL;
	} else if ((strncmp(CPUID_VID_AMD, info_p->cpuid_vendor,
		min(strlen(CPUID_STRING_UNKNOWN) + 1, sizeof(info_p->cpuid_vendor)))) == 0) {
		info_p->cpuid_vendor_id = CPUID_VENDOR_ID_AMD;
	}

	/* get extended cpuid results */
	cpuid_fn(0x80000000, reg);
	info_p->cpuid_max_ext = reg[eax];

	/* check to see if we can get brand string */
	if (info_p->cpuid_max_ext >= 0x80000004) {
		/*
		 * The brand string 48 bytes (max), guaranteed to
		 * be NUL terminated.
		 */
		cpuid_fn(0x80000002, reg);
		bcopy((char *)reg, &str[0], 16);
		cpuid_fn(0x80000003, reg);
		bcopy((char *)reg, &str[16], 16);
		cpuid_fn(0x80000004, reg);
		bcopy((char *)reg, &str[32], 16);
		for (p = str; *p != '\0'; p++) {
			if (*p != ' ') {
				break;
			}
		}
		strlcpy(info_p->cpuid_brand_string,
		    p, sizeof(info_p->cpuid_brand_string));

		if (!strncmp(info_p->cpuid_brand_string, CPUID_STRING_UNKNOWN,
		    min(sizeof(info_p->cpuid_brand_string),
		    strlen(CPUID_STRING_UNKNOWN) + 1))) {
			/*
			 * This string means we have a firmware-programmable brand string,
			 * and the firmware couldn't figure out what sort of CPU we have.
			 */
			info_p->cpuid_brand_string[0] = '\0';
		}
	}

	/* Get cache and addressing info. */
	if (info_p->cpuid_max_ext >= 0x80000006) {
		uint32_t assoc;
		cpuid_fn(0x80000006, reg);
		info_p->cpuid_cache_linesize   = bitfield32(reg[ecx], 7, 0);
		assoc = bitfield32(reg[ecx], 15, 12);
		/*
		 * L2 associativity is encoded, though in an insufficiently
		 * descriptive fashion, e.g. 24-way is mapped to 16-way.
		 * Represent a fully associative cache as 0xFFFF.
		 * Overwritten by associativity as determined via CPUID.4
		 * if available.
		 */
		if (assoc == 6) {
			assoc = 8;
		} else if (assoc == 8) {
			assoc = 16;
		} else if (assoc == 0xF) {
			assoc = 0xFFFF;
		}
		info_p->cpuid_cache_L2_associativity = assoc;
		info_p->cpuid_cache_size       = bitfield32(reg[ecx], 31, 16);
		cpuid_fn(0x80000008, reg);
		info_p->cpuid_address_bits_physical =
		    bitfield32(reg[eax], 7, 0);
		info_p->cpuid_address_bits_virtual =
		    bitfield32(reg[eax], 15, 8);
	}

	/*
	 * Get processor signature and decode
	 * and bracket this with the approved procedure for reading the
	 * the microcode version number a.k.a. signature a.k.a. BIOS ID
	 */
	if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_AMD) {
		/*
		 * AMD stores the ucode rev in bits 31:0
		 */
		info_p->cpuid_microcode_version =
	    	(uint32_t) (rdmsr64(MSR_IA32_BIOS_SIGN_ID));
	} else {
		wrmsr64(MSR_IA32_BIOS_SIGN_ID, 0);
		cpuid_fn(1, reg);
		/*
		 * Intel stores the ucode rev in bits 63:31
		 */
		info_p->cpuid_microcode_version =
	    	(uint32_t) (rdmsr64(MSR_IA32_BIOS_SIGN_ID) >> 32);
	}

	info_p->cpuid_signature = reg[eax];
	info_p->cpuid_stepping  = bitfield32(reg[eax], 3, 0);
	info_p->cpuid_model     = bitfield32(reg[eax], 7, 4);
	info_p->cpuid_family    = bitfield32(reg[eax], 11, 8);
	info_p->cpuid_type      = bitfield32(reg[eax], 13, 12);
	info_p->cpuid_extmodel  = bitfield32(reg[eax], 19, 16);
	info_p->cpuid_extfamily = bitfield32(reg[eax], 27, 20);
	info_p->cpuid_brand     = bitfield32(reg[ebx], 7, 0);
	info_p->cpuid_features  = quad(reg[ecx], reg[edx]);

	/* Get "processor flag"; necessary for microcode update matching */
	if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_INTEL) {
		info_p->cpuid_processor_flag = (rdmsr64(MSR_IA32_PLATFORM_ID) >> 50) & 0x7;
	} else {
		/* TODO: Is there even an equivalent on AMD? */
		info_p->cpuid_processor_flag = 1;
	}

	/*
	 * Does the ucode subsystem actually get used because I can't find references to it.
	 * Anywhere.
	 */

	/* Fold extensions into family/model */
	if (info_p->cpuid_family == 0x0f) {
		info_p->cpuid_family += info_p->cpuid_extfamily;
	}
	if (info_p->cpuid_family >= 0x0f || info_p->cpuid_family == 0x06) {
		info_p->cpuid_model += (info_p->cpuid_extmodel << 4);
	}

	if (info_p->cpuid_features & CPUID_FEATURE_HTT) {
		info_p->cpuid_logical_per_package =
		    bitfield32(reg[ebx], 23, 16);
	} else {
		info_p->cpuid_logical_per_package = 1;
	}

	if (info_p->cpuid_max_ext >= 0x80000001) {
		cpuid_fn(0x80000001, reg);
		info_p->cpuid_extfeatures =
		    quad(reg[ecx], reg[edx]);
	}

	DBG(" max_basic           : %d\n", info_p->cpuid_max_basic);
	DBG(" max_ext             : 0x%08x\n", info_p->cpuid_max_ext);
	DBG(" vendor              : %s\n", info_p->cpuid_vendor);
	DBG(" brand_string        : %s\n", info_p->cpuid_brand_string);
	DBG(" signature           : 0x%08x\n", info_p->cpuid_signature);
	DBG(" stepping            : %d\n", info_p->cpuid_stepping);
	DBG(" model               : %d\n", info_p->cpuid_model);
	DBG(" family              : %d\n", info_p->cpuid_family);
	DBG(" type                : %d\n", info_p->cpuid_type);
	DBG(" extmodel            : %d\n", info_p->cpuid_extmodel);
	DBG(" extfamily           : %d\n", info_p->cpuid_extfamily);
	DBG(" brand               : %d\n", info_p->cpuid_brand);
	DBG(" features            : 0x%016llx\n", info_p->cpuid_features);
	DBG(" extfeatures         : 0x%016llx\n", info_p->cpuid_extfeatures);
	DBG(" logical_per_package : %d\n", info_p->cpuid_logical_per_package);
	DBG(" microcode_version   : 0x%08x\n", info_p->cpuid_microcode_version);
	DBG(" vendor_id           : %d\n", info_p->cpuid_vendor_id);

	/* Fold in the Invariant TSC feature bit, if present */
	if (info_p->cpuid_max_ext >= 0x80000007) {
		cpuid_fn(0x80000007, reg);
		info_p->cpuid_extfeatures |=
		    reg[edx] & (uint32_t)CPUID_EXTFEATURE_TSCI;
		DBG(" extfeatures         : 0x%016llx\n",
		    info_p->cpuid_extfeatures);
	}

	if (info_p->cpuid_max_basic >= 0x5) {
		cpuid_mwait_leaf_t      *cmp = &info_p->cpuid_mwait_leaf;

		/*
		 * Extract the Monitor/Mwait Leaf info:
		 */
		cpuid_fn(5, reg);
		cmp->linesize_min = reg[eax];
		cmp->linesize_max = reg[ebx];
		cmp->extensions   = reg[ecx];
		cmp->sub_Cstates  = reg[edx];
		info_p->cpuid_mwait_leafp = cmp;

		DBG(" Monitor/Mwait Leaf:\n");
		DBG("  linesize_min : %d\n", cmp->linesize_min);
		DBG("  linesize_max : %d\n", cmp->linesize_max);
		DBG("  extensions   : %d\n", cmp->extensions);
		DBG("  sub_Cstates  : 0x%08x\n", cmp->sub_Cstates);
	}

	if (info_p->cpuid_max_basic >= 0x6) {
		cpuid_thermal_leaf_t    *ctp = &info_p->cpuid_thermal_leaf;

		/*
		 * The thermal and Power Leaf:
		 */
		cpuid_fn(6, reg);
		ctp->sensor               = bitfield32(reg[eax], 0, 0);
		ctp->dynamic_acceleration = bitfield32(reg[eax], 1, 1);
		ctp->invariant_APIC_timer = bitfield32(reg[eax], 2, 2);
		ctp->core_power_limits    = bitfield32(reg[eax], 4, 4);
		ctp->fine_grain_clock_mod = bitfield32(reg[eax], 5, 5);
		ctp->package_thermal_intr = bitfield32(reg[eax], 6, 6);
		ctp->thresholds           = bitfield32(reg[ebx], 3, 0);
		ctp->ACNT_MCNT            = bitfield32(reg[ecx], 0, 0);
		ctp->hardware_feedback    = bitfield32(reg[ecx], 1, 1);
		ctp->energy_policy        = bitfield32(reg[ecx], 3, 3);
		ctp->hardware_feedback_ix = bitfield32(reg[ecx], 19, 19);
		ctp->itd                  = bitfield32(reg[ecx], 23, 23);
		info_p->cpuid_thermal_leafp = ctp;

		DBG(" Thermal/Power Leaf:\n");
		DBG("  sensor               : %d\n", ctp->sensor);
		DBG("  dynamic_acceleration : %d\n", ctp->dynamic_acceleration);
		DBG("  invariant_APIC_timer : %d\n", ctp->invariant_APIC_timer);
		DBG("  core_power_limits    : %d\n", ctp->core_power_limits);
		DBG("  fine_grain_clock_mod : %d\n", ctp->fine_grain_clock_mod);
		DBG("  package_thermal_intr : %d\n", ctp->package_thermal_intr);
		DBG("  thresholds           : %d\n", ctp->thresholds);
		DBG("  ACNT_MCNT            : %d\n", ctp->ACNT_MCNT);
		DBG("  ACNT2                : %d\n", ctp->hardware_feedback);
		DBG("  energy_policy        : %d\n", ctp->energy_policy);
		DBG("  hardware_feedback_ix : %d\n", ctp->hardware_feedback_ix);
		DBG("  itd					: %d\n", ctp->itd);
	}

	if (info_p->cpuid_max_basic >= 0xa) {
		cpuid_arch_perf_leaf_t  *capp = &info_p->cpuid_arch_perf_leaf;

		/*
		 * Architectural Performance Monitoring Leaf:
		 */
		cpuid_fn(0xa, reg);
		capp->version       = bitfield32(reg[eax], 7, 0);
		capp->number        = bitfield32(reg[eax], 15, 8);
		capp->width         = bitfield32(reg[eax], 23, 16);
		capp->events_number = bitfield32(reg[eax], 31, 24);
		capp->events        = reg[ebx];
		capp->fixed_number  = bitfield32(reg[edx], 4, 0);
		capp->fixed_width   = bitfield32(reg[edx], 12, 5);
		info_p->cpuid_arch_perf_leafp = capp;

		DBG(" Architectural Performance Monitoring Leaf:\n");
		DBG("  version       : %d\n", capp->version);
		DBG("  number        : %d\n", capp->number);
		DBG("  width         : %d\n", capp->width);
		DBG("  events_number : %d\n", capp->events_number);
		DBG("  events        : %d\n", capp->events);
		DBG("  fixed_number  : %d\n", capp->fixed_number);
		DBG("  fixed_width   : %d\n", capp->fixed_width);
	}

	if (info_p->cpuid_max_basic >= 0xd) {
		cpuid_xsave_leaf_t      *xsp;
		/*
		 * XSAVE Features:
		 */
		xsp = &info_p->cpuid_xsave_leaf[0];
		info_p->cpuid_xsave_leafp = xsp;
		xsp->extended_state[eax] = 0xd;
		xsp->extended_state[ecx] = 0;
		cpuid(xsp->extended_state);
		DBG(" XSAVE Main leaf:\n");
		DBG("  EAX           : 0x%x\n", xsp->extended_state[eax]);
		DBG("  EBX           : 0x%x\n", xsp->extended_state[ebx]);
		DBG("  ECX           : 0x%x\n", xsp->extended_state[ecx]);
		DBG("  EDX           : 0x%x\n", xsp->extended_state[edx]);

		xsp = &info_p->cpuid_xsave_leaf[1];
		xsp->extended_state[eax] = 0xd;
		xsp->extended_state[ecx] = 1;
		cpuid(xsp->extended_state);
		DBG(" XSAVE Sub-leaf1:\n");
		DBG("  EAX           : 0x%x\n", xsp->extended_state[eax]);
		DBG("  EBX           : 0x%x\n", xsp->extended_state[ebx]);
		DBG("  ECX           : 0x%x\n", xsp->extended_state[ecx]);
		DBG("  EDX           : 0x%x\n", xsp->extended_state[edx]);
	}

	if (info_p->cpuid_max_basic >= 0x7) {
		/*
		 * Leaf7 Features:
		 */
		cpuid_fn(0x7, reg);
		info_p->cpuid_leaf7_features = quad(reg[ecx], reg[ebx]);
		info_p->cpuid_leaf7_extfeatures = reg[edx];

		DBG(" Feature Leaf7:\n");
		DBG("  EBX           : 0x%x\n", reg[ebx]);
		DBG("  ECX           : 0x%x\n", reg[ecx]);
		DBG("  EDX           : 0x%x\n", reg[edx]);

		if (reg[eax] >= 1) {
			reg[eax] = 0x7;
			reg[ecx] = 1;
			cpuid(reg);
			info_p->cpuid_leaf7_sl1_features = quad(reg[eax], reg[ebx]);
			info_p->cpuid_leaf7_sl1_extfeatures = quad(reg[ecx], reg[edx]);
		}

		/* sub-leaf 2 exists. god dang it. */
	}

	if (info_p->cpuid_max_basic >= 0x15) {
		/*
		 * TCS/CCC frequency leaf:
		 */
		cpuid_fn(0x15, reg);
		info_p->cpuid_tsc_leaf.denominator = reg[eax];
		info_p->cpuid_tsc_leaf.numerator   = reg[ebx];

		DBG(" TSC/CCC Information Leaf:\n");
		DBG("  numerator     : 0x%x\n", reg[ebx]);
		DBG("  denominator   : 0x%x\n", reg[eax]);
	}

	if ((info_p->cpuid_max_basic >= 0x19) && 
		 info_p->cpuid_leaf7_features & CPUID_LEAF7_FEATURE_KEYLOCKER) {
		cpuid_set_keylocker_info(info_p);
	}

	/*
	 * Extract from the Intel SDM:
	 * If sub-leaf index “N” returns an invalid domain type in ECX[15:08] (00H), then all sub-leaves with an
	 * index greater than “N” shall also return an invalid domain type. A sub-leaf returning an invalid domain
	 * always returns 0 in EAX and EBX.
	 */
	if (info_p->cpuid_max_basic >= 0x1f) {
		uint32_t ext_topo[4] = {0,0,0,0};
		uint32_t domain;

		for (int i = 0; true; i++) { /* please enumerate nicely and give me the E-core count */
			ext_topo[eax] = 0x1f;
			ext_topo[ecx] = i;
			cpuid(ext_topo);
			if (bitfield32(ext_topo[ecx], 15, 8) > 0) {
				domain = bitfield32(ext_topo[ecx], 15, 8);
				info_p->cpuid_ext_topo_leaf.domains[domain].enabled = true;
				info_p->cpuid_ext_topo_leaf.domains[domain].apicid_shift = bitfield32(ext_topo[eax], 4, 0);
				info_p->cpuid_ext_topo_leaf.domains[domain].logical_in_domain = bitfield32(ext_topo[ebx], 15, 0);
			} else {
				break;
			}
		}
		DBG(" Extended Topology Leaf:\n");
		for (int i = 0; i < EXT_TOPO_DOMAIN_MAX; i++) {
			DBG("  Domain %d:            \n", i);
			DBG("    enabled            : %d\n", info_p->cpuid_ext_topo_leaf.domains[i].enabled);
			DBG("    apicid_shift       : %d\n", info_p->cpuid_ext_topo_leaf.domains[i].apicid_shift);
			DBG("    logical_in_domain  : %d\n", info_p->cpuid_ext_topo_leaf.domains[i].logical_in_domain);
		}
	}

	return;
}

static uint32_t
cpuid_set_cpufamily_intel(i386_cpu_info_t *info_p)
{
	uint32_t cpufamily = CPUFAMILY_UNKNOWN;

	switch (info_p->cpuid_family) {
	case 6:
		switch (info_p->cpuid_model) {
		case 23:
			cpufamily = CPUFAMILY_INTEL_PENRYN;
			break;
		case CPUID_MODEL_DIAMONDVILLE:
		case CPUID_MODEL_SILVERTHORNE:
			cpufamily = CPUFAMILY_INTEL_BONNELL;
			break;
		case CPUID_MODEL_NEHALEM:
		case CPUID_MODEL_FIELDS:
		case CPUID_MODEL_DALES:
		case CPUID_MODEL_NEHALEM_EX:
			cpufamily = CPUFAMILY_INTEL_NEHALEM;
			break;
		case CPUID_MODEL_DALES_32NM:
		case CPUID_MODEL_WESTMERE:
		case CPUID_MODEL_WESTMERE_EX:
			cpufamily = CPUFAMILY_INTEL_WESTMERE;
			break;
		case CPUID_MODEL_PENWELL:
		case CPUID_MODEL_CLOVERVIEW:
		case CPUID_MODEL_CEDARVIEW:
			cpufamily = CPUFAMILY_INTEL_SALTWELL;
			break;
		case CPUID_MODEL_SANDYBRIDGE:
		case CPUID_MODEL_JAKETOWN:
			cpufamily = CPUFAMILY_INTEL_SANDYBRIDGE;
			break;
		case CPUID_MODEL_IVYBRIDGE:
		case CPUID_MODEL_IVYBRIDGE_EP:
			cpufamily = CPUFAMILY_INTEL_IVYBRIDGE;
			break;
		case CPUID_MODEL_BAYTRAIL:
		case CPUID_MODEL_TANGIER:
		case CPUID_MODEL_AVOTON:
		case CPUID_MODEL_ANNIEDALE:
		case CPUID_MODEL_SOFIA:
			cpufamily = CPUFAMILY_INTEL_SILVERMONT;
			break;
		case CPUID_MODEL_HASWELL:
		case CPUID_MODEL_HASWELL_EP:
		case CPUID_MODEL_HASWELL_ULT:
		case CPUID_MODEL_CRYSTALWELL:
			cpufamily = CPUFAMILY_INTEL_HASWELL;
			break;
		case CPUID_MODEL_BROADWELL:
		case CPUID_MODEL_BRYSTALWELL:
			cpufamily = CPUFAMILY_INTEL_BROADWELL;
			break;
		case CPUID_MODEL_BRASWELL:
			cpufamily = CPUFAMILY_INTEL_AIRMONT;
			break;
		case CPUID_MODEL_SKYLAKE:
		case CPUID_MODEL_SKYLAKE_DT:
		case CPUID_MODEL_SKYLAKE_W:
			cpufamily = CPUFAMILY_INTEL_SKYLAKE;
			break;
		case CPUID_MODEL_APOLLOLAKE:
		case CPUID_MODEL_DENVERTON:
			cpufamily = CPUFAMILY_INTEL_GOLDMONT;
			break;
		case CPUID_MODEL_KABYLAKE:
		case CPUID_MODEL_KABYLAKE_DT:
			cpufamily = CPUFAMILY_INTEL_KABYLAKE;
			break;
		case CPUID_MODEL_GEMINILAKE:
			cpufamily = CPUFAMILY_INTEL_GOLDMONTPLUS;
			break;
		case CPUID_MODEL_ICELAKE_SP:
		case CPUID_MODEL_ICELAKE_DE:
		case CPUID_MODEL_ICELAKE:
		case CPUID_MODEL_ICELAKE_DT:
		case CPUID_MODEL_ICELAKE_H:
			cpufamily = CPUFAMILY_INTEL_ICELAKE;
			break;
		case CPUID_MODEL_COMETLAKE_DT:
			cpufamily = CPUFAMILY_INTEL_COMETLAKE;
			break;
		case CPUID_MODEL_JACOBSVILLE:
		case CPUID_MODEL_LAKEFIELD:
		case CPUID_MODEL_ELKHARTLAKE:
		case CPUID_MODEL_JASPERLAKE:
			cpufamily = CPUFAMILY_INTEL_TREMONT;
			break;
		case CPUID_MODEL_TIGERLAKE_U:
		case CPUID_MODEL_TIGERLAKE_H:
			cpufamily = CPUFAMILY_INTEL_TIGERLAKE;
			break;
		case CPUID_MODEL_ROCKETLAKE:
			cpufamily = CPUFAMILY_INTEL_ROCKETLAKE;
			break;
		case CPUID_MODEL_ALDERLAKE:
		case CPUID_MODEL_ALDERLAKE_P:
			cpufamily = CPUFAMILY_INTEL_ALDERLAKE;
			break;
		case CPUID_MODEL_RAPTORLAKE:
		case CPUID_MODEL_RAPTORLAKE_P:
			cpufamily = CPUFAMILY_INTEL_RAPTORLAKE;
			break;
		case CPUID_MODEL_SAPPHIRERAPIDS:
			cpufamily = CPUFAMILY_INTEL_SAPPHIRERAPIDS;
			break;
		case CPUID_MODEL_EMERALDRAPIDS:
			cpufamily = CPUFAMILY_INTEL_EMERALDRAPIDS;
			break;
		case CPUID_MODEL_METEORLAKE_S:
		case CPUID_MODEL_METEORLAKE_U:
		    cpufamily = CPUFAMILY_INTEL_METEORLAKE;
			break;
		case CPUID_MODEL_ARROWLAKE_H:
		case CPUID_MODEL_ARROWLAKE_S:
		case CPUID_MODEL_ARROWLAKE_U:
		    cpufamily = CPUFAMILY_INTEL_ARROWLAKE;
			break;
		}
		break;
	case 19:
		switch (info_p->cpuid_model) {
		case CPUID_MODEL_DIAMONDRAPIDS:
			cpufamily = CPUFAMILY_INTEL_DIAMONDRAPIDS;
			break;
		}
	}

	return cpufamily;
}

static uint32_t
cpuid_set_cpufamily_amd(i386_cpu_info_t *info_p)
{
	uint32_t cpufamily = CPUFAMILY_UNKNOWN;

	switch (info_p->cpuid_family) {
	case 0x0f:
	    cpufamily = CPUFAMILY_AMD_K8;
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	    cpufamily = CPUFAMILY_AMD_K10;
		break;
	case 0x14:
	    cpufamily = CPUFAMILY_AMD_BOBCAT;
		break;
	case 0x15:
        switch (info_p->cpuid_model) {
        case CPUID_MODEL_ZAMBEZI:
            cpufamily = CPUFAMILY_AMD_BULLDOZER;
            break;
        case CPUID_MODEL_VISHERA:
        case CPUID_MODEL_TRINITY:
        case CPUID_MODEL_RICHLAND:
            cpufamily = CPUFAMILY_AMD_PILEDRIVER;
            break;
        case CPUID_MODEL_KAVERI:
        case CPUID_MODEL_GODAVARI:
            cpufamily = CPUFAMILY_AMD_STEAMROLLER;
            break;
        case CPUID_MODEL_CARRIZO:
        case CPUID_MODEL_BRISTOLRIDGE:
        case CPUID_MODEL_STONEYRIDGE:
            cpufamily = CPUFAMILY_AMD_EXCAVATOR;
            break;
        }
        break;
	case 0x16:
		switch (info_p->cpuid_model) {
		case CPUID_MODEL_KABINI:
		    cpufamily = CPUFAMILY_AMD_JAGUAR;
			break;
		case CPUID_MODEL_MULLINS:
			cpufamily = CPUFAMILY_AMD_PUMA;
			break;
		}
		break;
	case 0x17:
		switch (info_p->cpuid_model) {
		case CPUID_MODEL_SUMMITRIDGE:
		case CPUID_MODEL_RAVENRIDGE:
		case CPUID_MODEL_DALI:
			cpufamily = CPUFAMILY_AMD_ZEN;
			break;
		case CPUID_MODEL_PINNACLERIDGE:
		case CPUID_MODEL_PICASSO:
			cpufamily = CPUFAMILY_AMD_ZENX;
			break;
		case CPUID_MODEL_ROME:
		case CPUID_MODEL_RENOIR:
		case CPUID_MODEL_LUCIENNE:
		case CPUID_MODEL_MATISSE:
		case CPUID_MODEL_VANGOGH:
		case CPUID_MODEL_MENDOCINO:
			cpufamily = CPUFAMILY_AMD_ZEN2;
			break;
		}
		break;
	case 0x19:
		switch (info_p->cpuid_model) {
		case CPUID_MODEL_MILAN:
		case CPUID_MODEL_CHAGALL:
		case CPUID_MODEL_VERMEER:
		case CPUID_MODEL_BADAMI:
		case CPUID_MODEL_CEZANNE:
			cpufamily = CPUFAMILY_AMD_ZEN3;
			break;
		}
		break;
	}

	return cpufamily;
}

static uint32_t
cpuid_set_cpufamily(i386_cpu_info_t *info_p)
{
	uint32_t cpufamily = CPUFAMILY_UNKNOWN;

	switch (info_p->cpuid_vendor_id) {
	case CPUID_VENDOR_ID_INTEL:
		cpufamily = cpuid_set_cpufamily_intel(info_p);
		break;
	case CPUID_VENDOR_ID_AMD:
		cpufamily = cpuid_set_cpufamily_amd(info_p);
		break;
	default:
		break;
	}

	info_p->cpuid_cpufamily = cpufamily;
	DBG("cpuid_set_cpufamily(%p) returning 0x%x\n", info_p, cpufamily);
	return cpufamily;
}

static boolean_t
cpuid_is_unsupported_cpu(i386_cpu_info_t *info_p)
{
	return (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_UNKNOWN)
			|| (cpuid_set_cpufamily(info_p) == CPUFAMILY_UNKNOWN);
}

/*
 * Must be invoked either when executing single threaded, or with
 * independent synchronization.
 */
void
cpuid_set_info(void)
{
	i386_cpu_info_t         *info_p = &cpuid_cpu_info;
	boolean_t               enable_x86_64h = TRUE;

	/* Perform pre-cpuid workarounds (since their effects impact values returned via cpuid) */
	cpuid_do_precpuid_was();

	cpuid_set_generic_info(info_p);

	/* verify we are running on a supported CPU */
	if (cpuid_is_unsupported_cpu(info_p)) {
		panic("Unsupported CPU");
	}

	info_p->cpuid_cpu_type = CPU_TYPE_X86;

	if (!PE_parse_boot_argn("-enable_x86_64h", &enable_x86_64h, sizeof(enable_x86_64h))) {
		boolean_t               disable_x86_64h = FALSE;

		if (PE_parse_boot_argn("-disable_x86_64h", &disable_x86_64h, sizeof(disable_x86_64h))) {
			enable_x86_64h = FALSE;
		}
	}

	if (enable_x86_64h &&
	    ((info_p->cpuid_features & CPUID_X86_64_H_FEATURE_SUBSET) == CPUID_X86_64_H_FEATURE_SUBSET) &&
	    ((info_p->cpuid_extfeatures & CPUID_X86_64_H_EXTFEATURE_SUBSET) == CPUID_X86_64_H_EXTFEATURE_SUBSET) &&
	    ((info_p->cpuid_leaf7_features & CPUID_X86_64_H_LEAF7_FEATURE_SUBSET) == CPUID_X86_64_H_LEAF7_FEATURE_SUBSET)) {
		info_p->cpuid_cpu_subtype = CPU_SUBTYPE_X86_64_H;
	} else {
		info_p->cpuid_cpu_subtype = CPU_SUBTYPE_X86_ARCH1;
	}
	/* cpuid_set_cache_info must be invoked after set_generic_info */

	/*
	 * Find the number of enabled cores and threads
	 * (which determines whether SMT/Hyperthreading is active).
	 */

	if (0 != (info_p->cpuid_features & CPUID_FEATURE_VMM) &&
	    PE_parse_boot_argn("-nomsr35h", NULL, 0)) {
		info_p->core_count = 1;
		info_p->thread_count = 1;
		cpuid_set_cache_info(info_p);
	} else {
		switch (info_p->cpuid_cpufamily) {
		case CPUFAMILY_INTEL_PENRYN:
			cpuid_set_cache_info(info_p);
			info_p->core_count   = info_p->cpuid_cores_per_package;
			info_p->thread_count = info_p->cpuid_logical_per_package;
			break;
		case CPUFAMILY_INTEL_WESTMERE: {
			uint64_t msr = rdmsr64(MSR_CORE_THREAD_COUNT);
			if (0 == msr) {
				/* Provide a non-zero default for some VMMs */
				msr = (1 << 16) | 1;
			}
			info_p->core_count   = bitfield32((uint32_t)msr, 19, 16);
			info_p->thread_count = bitfield32((uint32_t)msr, 15, 0);
			cpuid_set_cache_info(info_p);
			break;
		}
		case CPUFAMILY_AMD_K8:
		case CPUFAMILY_AMD_K10:
		case CPUFAMILY_AMD_BOBCAT: {
		    uint32_t reg[4];

			cpuid_fn(0x80000008, reg);
			info_p->core_count = bitfield32(reg[ecx], 7, 0) + 1;
			info_p->thread_count = info_p->core_count;
			info_p->cpuid_cores_per_package = info_p->core_count;
			info_p->cpuid_logical_per_package = info_p->thread_count;

			cpuid_set_cache_info(info_p);
		    break;
		}
		case CPUFAMILY_AMD_PILEDRIVER: {
			uint32_t cpuid[4];

			/*
			 * 15h & 16h w/ CMT ERRATA:
			 * Treat each 'compute unit' as a single core, and treat the cores as logical processors.
			 * Hacky solution, I know.
			 */

			cpuid_fn(0x80000008, cpuid);
			info_p->cpuid_logical_per_package = bitfield32(cpuid[ecx], 7, 0) + 1;

			/*
			 * PILEDRIVER ERRATA:
			 * The CoresPerComputeUnit field is bits 9:8 on Piledriver, for some reason.
			 * They reverted this in Steamroller.
			 */

			cpuid_fn(0x8000001e, cpuid);
			info_p->cpuid_cores_per_package = info_p->cpuid_logical_per_package / (bitfield32(cpuid[ebx], 9, 8) + 1);

			cpuid_set_cache_info(info_p);

			info_p->core_count = info_p->cpuid_cores_per_package;
			info_p->thread_count = info_p->cpuid_logical_per_package;
			break;
		}
		case CPUFAMILY_AMD_BULLDOZER:
		case CPUFAMILY_AMD_STEAMROLLER:
		case CPUFAMILY_AMD_EXCAVATOR:
		case CPUFAMILY_AMD_JAGUAR:
		case CPUFAMILY_AMD_PUMA: {
			uint32_t cpuid[4];

			/*
			 * 15h & 16h w/ CMT ERRATA:
			 * Treat each 'compute unit' as a single core, and treat the cores as logical processors.
			 * Hacky solution, I know.
			 */

			cpuid_fn(0x80000008, cpuid);
			info_p->cpuid_logical_per_package = bitfield32(cpuid[ecx], 7, 0) + 1;

			cpuid_fn(0x8000001e, cpuid);
			info_p->cpuid_cores_per_package = info_p->cpuid_logical_per_package / (bitfield32(cpuid[ebx], 15, 8) + 1);

			cpuid_set_cache_info(info_p);

			info_p->core_count = info_p->cpuid_cores_per_package;
			info_p->thread_count = info_p->cpuid_logical_per_package;
			break;
		}
		case CPUFAMILY_AMD_ZEN:
		case CPUFAMILY_AMD_ZENX:
		case CPUFAMILY_AMD_ZEN2:
		case CPUFAMILY_AMD_ZEN3: {
			uint32_t cpuid[4];

			cpuid_fn(0x80000008, cpuid);
			info_p->cpuid_logical_per_package = bitfield32(cpuid[ecx], 7, 0) + 1;
			info_p->thread_count = info_p->cpuid_logical_per_package;

			cpuid_fn(0x8000001e, cpuid);
			info_p->cpuid_cores_per_package = info_p->cpuid_logical_per_package / (bitfield32(cpuid[ebx], 15, 8) + 1);
			info_p->core_count = info_p->cpuid_cores_per_package;

			cpuid_set_cache_info(info_p);
			break;
		}
		default: {
			uint64_t msr = rdmsr64(MSR_CORE_THREAD_COUNT);
			if (0 == msr) {
				/* Provide a non-zero default for some VMMs */
				msr = (1 << 16) | 1;
			}
			info_p->core_count   = bitfield32((uint32_t)msr, 31, 16);
			info_p->thread_count = bitfield32((uint32_t)msr, 15, 0);
			cpuid_set_cache_info(info_p);
			break;
		}
		}
	}

	DBG("cpuid_set_info():\n");
	DBG("  core_count   : %d\n", info_p->core_count);
	DBG("  thread_count : %d\n", info_p->thread_count);
	DBG("       cpu_type: 0x%08x\n", info_p->cpuid_cpu_type);
	DBG("    cpu_subtype: 0x%08x\n", info_p->cpuid_cpu_subtype);

	info_p->cpuid_model_string = ""; /* deprecated */

	do_cwas(info_p, FALSE);
}

static struct table {
	uint64_t        mask;
	const char      *name;
} feature_map[] = {
	{CPUID_FEATURE_FPU, "FPU"},
	{CPUID_FEATURE_VME, "VME"},
	{CPUID_FEATURE_DE, "DE"},
	{CPUID_FEATURE_PSE, "PSE"},
	{CPUID_FEATURE_TSC, "TSC"},
	{CPUID_FEATURE_MSR, "MSR"},
	{CPUID_FEATURE_PAE, "PAE"},
	{CPUID_FEATURE_MCE, "MCE"},
	{CPUID_FEATURE_CX8, "CX8"},
	{CPUID_FEATURE_APIC, "APIC"},
	{CPUID_FEATURE_SEP, "SEP"},
	{CPUID_FEATURE_MTRR, "MTRR"},
	{CPUID_FEATURE_PGE, "PGE"},
	{CPUID_FEATURE_MCA, "MCA"},
	{CPUID_FEATURE_CMOV, "CMOV"},
	{CPUID_FEATURE_PAT, "PAT"},
	{CPUID_FEATURE_PSE36, "PSE36"},
	{CPUID_FEATURE_PSN, "PSN"},
	{CPUID_FEATURE_CLFSH, "CLFSH"},
	{CPUID_FEATURE_DS, "DS"},
	{CPUID_FEATURE_ACPI, "ACPI"},
	{CPUID_FEATURE_MMX, "MMX"},
	{CPUID_FEATURE_FXSR, "FXSR"},
	{CPUID_FEATURE_SSE, "SSE"},
	{CPUID_FEATURE_SSE2, "SSE2"},
	{CPUID_FEATURE_SS, "SS"},
	{CPUID_FEATURE_HTT, "HTT"},
	{CPUID_FEATURE_TM, "TM"},
	{CPUID_FEATURE_PBE, "PBE"},
	{CPUID_FEATURE_SSE3, "SSE3"},
	{CPUID_FEATURE_PCLMULQDQ, "PCLMULQDQ"},
	{CPUID_FEATURE_DTES64, "DTES64"},
	{CPUID_FEATURE_MONITOR, "MON"},
	{CPUID_FEATURE_DSCPL, "DSCPL"},
	{CPUID_FEATURE_VMX, "VMX"},
	{CPUID_FEATURE_SMX, "SMX"},
	{CPUID_FEATURE_EST, "EST"},
	{CPUID_FEATURE_TM2, "TM2"},
	{CPUID_FEATURE_SSSE3, "SSSE3"},
	{CPUID_FEATURE_CID, "CID"},
	{CPUID_FEATURE_FMA, "FMA"},
	{CPUID_FEATURE_CX16, "CX16"},
	{CPUID_FEATURE_xTPR, "TPR"},
	{CPUID_FEATURE_PDCM, "PDCM"},
	{CPUID_FEATURE_SSE4_1, "SSE4.1"},
	{CPUID_FEATURE_SSE4_2, "SSE4.2"},
	{CPUID_FEATURE_x2APIC, "x2APIC"},
	{CPUID_FEATURE_MOVBE, "MOVBE"},
	{CPUID_FEATURE_POPCNT, "POPCNT"},
	{CPUID_FEATURE_AES, "AES"},
	{CPUID_FEATURE_VMM, "VMM"},
	{CPUID_FEATURE_PCID, "PCID"},
	{CPUID_FEATURE_XSAVE, "XSAVE"},
	{CPUID_FEATURE_OSXSAVE, "OSXSAVE"},
	{CPUID_FEATURE_SEGLIM64, "SEGLIM64"},
	{CPUID_FEATURE_TSCTMR, "TSCTMR"},
	{CPUID_FEATURE_AVX1_0, "AVX1.0"},
	{CPUID_FEATURE_RDRAND, "RDRAND"},
	{CPUID_FEATURE_F16C, "F16C"},
	{0, 0}
},
    extfeature_map[] = {
    {CPUID_EXTFEATURE_SVM, "SVM"},
	{CPUID_EXTFEATURE_SYSCALL, "SYSCALL"},
	{CPUID_EXTFEATURE_XD, "XD"},
	{CPUID_EXTFEATURE_1GBPAGE, "1GBPAGE"},
	{CPUID_EXTFEATURE_EM64T, "EM64T"},
	{CPUID_EXTFEATURE_LAHF, "LAHF"},
	{CPUID_EXTFEATURE_LZCNT, "LZCNT"},
	{CPUID_EXTFEATURE_PREFETCHW, "PREFETCHW"},
	{CPUID_EXTFEATURE_RDTSCP, "RDTSCP"},
	{CPUID_EXTFEATURE_TSCI, "TSCI"},
	{0, 0}
},
    leaf7_feature_map[] = {
	{CPUID_LEAF7_FEATURE_RDWRFSGS, "RDWRFSGS"},
	{CPUID_LEAF7_FEATURE_TSCOFF, "TSC_THREAD_OFFSET"},
	{CPUID_LEAF7_FEATURE_SGX, "SGX"},
	{CPUID_LEAF7_FEATURE_BMI1, "BMI1"},
	{CPUID_LEAF7_FEATURE_HLE, "HLE"},
	{CPUID_LEAF7_FEATURE_AVX2, "AVX2"},
	{CPUID_LEAF7_FEATURE_FDPEO, "FDPEO"},
	{CPUID_LEAF7_FEATURE_SMEP, "SMEP"},
	{CPUID_LEAF7_FEATURE_BMI2, "BMI2"},
	{CPUID_LEAF7_FEATURE_ERMS, "ERMS"},
	{CPUID_LEAF7_FEATURE_INVPCID, "INVPCID"},
	{CPUID_LEAF7_FEATURE_RTM, "RTM"},
	{CPUID_LEAF7_FEATURE_PQM, "PQM"},
	{CPUID_LEAF7_FEATURE_FPU_CSDS, "FPU_CSDS"},
	{CPUID_LEAF7_FEATURE_MPX, "MPX"},
	{CPUID_LEAF7_FEATURE_PQE, "PQE"},
	{CPUID_LEAF7_FEATURE_AVX512F, "AVX512F"},
	{CPUID_LEAF7_FEATURE_AVX512DQ, "AVX512DQ"},
	{CPUID_LEAF7_FEATURE_RDSEED, "RDSEED"},
	{CPUID_LEAF7_FEATURE_ADX, "ADX"},
	{CPUID_LEAF7_FEATURE_SMAP, "SMAP"},
	{CPUID_LEAF7_FEATURE_AVX512IFMA, "AVX512IFMA"},
	{CPUID_LEAF7_FEATURE_CLFSOPT, "CLFSOPT"},
	{CPUID_LEAF7_FEATURE_CLWB, "CLWB"},
	{CPUID_LEAF7_FEATURE_IPT, "IPT"},
	{CPUID_LEAF7_FEATURE_AVX512CD, "AVX512CD"},
	{CPUID_LEAF7_FEATURE_SHA, "SHA"},
	{CPUID_LEAF7_FEATURE_AVX512BW, "AVX512BW"},
	{CPUID_LEAF7_FEATURE_AVX512VL, "AVX512VL"},
	{CPUID_LEAF7_FEATURE_PREFETCHWT1, "PREFETCHWT1"},
	{CPUID_LEAF7_FEATURE_AVX512VBMI, "AVX512VBMI"},
	{CPUID_LEAF7_FEATURE_UMIP, "UMIP"},
	{CPUID_LEAF7_FEATURE_PKU, "PKU"},
	{CPUID_LEAF7_FEATURE_OSPKE, "OSPKE"},
	{CPUID_LEAF7_FEATURE_WAITPKG, "WAITPKG"},
	{CPUID_LEAF7_FEATURE_GFNI, "GFNI"},
	{CPUID_LEAF7_FEATURE_VAES, "VAES"},
	{CPUID_LEAF7_FEATURE_VPCLMULQDQ, "VPCLMULQDQ"},
	{CPUID_LEAF7_FEATURE_AVX512VNNI, "AVX512VNNI"},
	{CPUID_LEAF7_FEATURE_AVX512BITALG, "AVX512BITALG"},
	{CPUID_LEAF7_FEATURE_TME, "TME"},
	{CPUID_LEAF7_FEATURE_AVX512VPCDQ, "AVX512VPOPCNTDQ"},
	{CPUID_LEAF7_FEATURE_LA57, "LA57"},
	{CPUID_LEAF7_FEATURE_RDPID, "RDPID"},
	{CPUID_LEAF7_FEATURE_KEYLOCKER, "KEYLOCKER"},
	{CPUID_LEAF7_FEATURE_BUSLOCKDETECT, "BUSLOCKDETECT"},
	{CPUID_LEAF7_FEATURE_CLDEMOTE, "CLDEMOTE"},
	{CPUID_LEAF7_FEATURE_MOVDIRI, "MOVDIRI"},
	{CPUID_LEAF7_FEATURE_MOVDIRI64B, "MOVDIRI64B"},
	{CPUID_LEAF7_FEATURE_ENQCMD, "ENQCMD"},
	{CPUID_LEAF7_FEATURE_SGXLC, "SGXLC"},
	{CPUID_LEAF7_FEATURE_PKS, "PKS"},
	{0, 0}
},
    leaf7_extfeature_map[] = {
	{ CPUID_LEAF7_EXTFEATURE_AVX5124VNNIW, "AVX5124VNNIW" },
	{ CPUID_LEAF7_EXTFEATURE_AVX5124FMAPS, "AVX5124FMAPS" },
	{ CPUID_LEAF7_EXTFEATURE_FSREPMOV, "FSREPMOV" },
	{ CPUID_LEAF7_EXTFEATURE_MDCLEAR, "MDCLEAR" },
	{ CPUID_LEAF7_EXTFEATURE_TSXFA, "TSXFA" },
	{ CPUID_LEAF7_EXTFEATURE_HYBRID, "HYBRID" },
	{ CPUID_LEAF7_EXTFEATURE_IBRS, "IBRS" },
	{ CPUID_LEAF7_EXTFEATURE_STIBP, "STIBP" },
	{ CPUID_LEAF7_EXTFEATURE_L1DF, "L1DF" },
	{ CPUID_LEAF7_EXTFEATURE_ACAPMSR, "ACAPMSR" },
	{ CPUID_LEAF7_EXTFEATURE_CCAPMSR, "CCAPMSR" },
	{ CPUID_LEAF7_EXTFEATURE_SSBD, "SSBD" },
	{0, 0}
}, 
	leaf7_sl1_feature_map[] = {
	{CPUID_LEAF7_SL1_FEATURE_SHA512, "SHA512"},
	{CPUID_LEAF7_SL1_FEATURE_SM3, "SM3"},
	{CPUID_LEAF7_SL1_FEATURE_SM4, "SM4"},
	{CPUID_LEAF7_SL1_FEATURE_RAOINT, "RAOINT"},
	{CPUID_LEAF7_SL1_FEATURE_AVXVNNI, "AVXVNNI"},
	{CPUID_LEAF7_SL1_FEATURE_AVX512BF16, "AVX512BF16"},
	{CPUID_LEAF7_SL1_FEATURE_LASS, "LASS"},
	{CPUID_LEAF7_SL1_FEATURE_CMPCCXADD, "CMPCCXADD"},
	{CPUID_LEAF7_SL1_FEATURE_PERFMONEXT, "PERFMONEXT"},
	{CPUID_LEAF7_SL1_FEATURE_ZLMOVSB, "ZLMOVSB"},
	{CPUID_LEAF7_SL1_FEATURE_FSSTOSB, "FSSTOSB"},
	{CPUID_LEAF7_SL1_FEATURE_FSCMPSB, "FSCMPSB"},
	{CPUID_LEAF7_SL1_FEATURE_FRED, "FRED"},
	{CPUID_LEAF7_SL1_FEATURE_LKGS, "LKGS"},
	{CPUID_LEAF7_SL1_FEATURE_WRMSRNS, "WRMSRNS"},
	{CPUID_LEAF7_SL1_FEATURE_NMISRC, "NMISRC"},
	{CPUID_LEAF7_SL1_FEATURE_AMXFP16, "AMXFP16"},
	{CPUID_LEAF7_SL1_FEATURE_HRESET, "HRESET"},
	{CPUID_LEAF7_SL1_FEATURE_AVXIFMA, "AVXIFMA"},
	{CPUID_LEAF7_SL1_FEATURE_LAM, "LAM"},
	{CPUID_LEAF7_SL1_FEATURE_MSRLIST, "MSRLIST"},
	{CPUID_LEAF7_SL1_FEATURE_NOINVDPOSTBIOS, "NOINVDPOSTBIOS"},
	{CPUID_LEAF7_SL1_FEATURE_MOVRS, "MOVRS"},
	{CPUID_LEAF7_SL1_FEATURE_PPIN, "PPIN"},
	{CPUID_LEAF7_SL1_FEATURE_PBNDKB, "PBNDKB"},
	{CPUID_LEAF7_SL1_FEATURE_NOCPUIDLIMIT, "NOCPUIDLIMIT"},
	{0, 0}
}, leaf7_sl1_extfeature_map[] = {
	{CPUID_LEAF7_SL1_EXTFEATURE_RDTM, "RDTM"},
	{CPUID_LEAF7_SL1_EXTFEATURE_RDTA, "RDTA"},
	{CPUID_LEAF7_SL1_EXTFEATURE_MSRIMM, "MSRIMM"},
	{CPUID_LEAF7_SL1_EXTFEATURE_AVXVNNIINT8, "AVXVNNIINT8"},
	{CPUID_LEAF7_SL1_EXTFEATURE_AVXNECONVERT, "AVXNECONVERT"},
	{CPUID_LEAF7_SL1_EXTFEATURE_AMXCOMPLEX, "AMXCOMPLEX"},
	{CPUID_LEAF7_SL1_EXTFEATURE_AVXVNNIINT16, "AVXVNNIINT16"},
	{CPUID_LEAF7_SL1_EXTFEATURE_UTMR, "UTMR"},
	{CPUID_LEAF7_SL1_EXTFEATURE_PREFTECHI, "PREFETCHI"},
	{CPUID_LEAF7_SL1_EXTFEATURE_USERMSR, "USERMSR"},
	{CPUID_LEAF7_SL1_EXTFEATURE_UIRETUIF, "UIRETUIF"},
	{CPUID_LEAF7_SL1_EXTFEATURE_CETSSS, "CETSSS"},
	{CPUID_LEAF7_SL1_EXTFEATURE_AVX10, "AVX10"},
	{CPUID_LEAF7_SL1_EXTFEATURE_APXF, "APXF"},
	{CPUID_LEAF7_SL1_EXTFEATURE_MWAIT, "MWAIT"},
	{CPUID_LEAF7_SL1_EXTFEATURE_SLSM, "SLSM"},
	{0, 0}
};

static char *
cpuid_get_names(struct table *map, uint64_t bits, char *buf, unsigned buf_len)
{
	size_t  len = 0;
	char    *p = buf;
	int     i;

	for (i = 0; map[i].mask != 0; i++) {
		if ((bits & map[i].mask) == 0) {
			continue;
		}
		if (len && ((size_t) (p - buf) < (buf_len - 1))) {
			*p++ = ' ';
		}
		len = min(strlen(map[i].name), (size_t)((buf_len - 1) - (p - buf)));
		if (len == 0) {
			break;
		}
		bcopy(map[i].name, p, len);
		p += len;
	}
	*p = '\0';
	return buf;
}

i386_cpu_info_t *
cpuid_info(void)
{
	/* Set-up the cpuid_info stucture lazily */
	if (cpuid_cpu_infop == NULL) {
		PE_parse_boot_argn("-cpuid", &cpuid_dbg, sizeof(cpuid_dbg));
		cpuid_set_info();
		cpuid_cpu_infop = &cpuid_cpu_info;
	}
	return cpuid_cpu_infop;
}

char *
cpuid_get_feature_names(uint64_t features, char *buf, unsigned buf_len)
{
	return cpuid_get_names(feature_map, features, buf, buf_len);
}

char *
cpuid_get_extfeature_names(uint64_t extfeatures, char *buf, unsigned buf_len)
{
	return cpuid_get_names(extfeature_map, extfeatures, buf, buf_len);
}

char *
cpuid_get_leaf7_feature_names(uint64_t features, char *buf, unsigned buf_len)
{
	return cpuid_get_names(leaf7_feature_map, features, buf, buf_len);
}

char *
cpuid_get_leaf7_extfeature_names(uint64_t features, char *buf, unsigned buf_len)
{
	return cpuid_get_names(leaf7_extfeature_map, features, buf, buf_len);
}

char *
cpuid_get_leaf7_sl1_feature_names(uint64_t features, char *buf, unsigned buf_len)
{
	return cpuid_get_names(leaf7_sl1_feature_map, features, buf, buf_len);
}

char *
cpuid_get_leaf7_sl1_extfeature_names(uint64_t features, char *buf, unsigned buf_len)
{
	return cpuid_get_names(leaf7_sl1_extfeature_map, features, buf, buf_len);
}

void
cpuid_feature_display(
	const char      *header)
{
	/*
	 * FIXME: Update the size of this buffer?
	 */
	char    buf[320];

	kprintf("%s: %s", header,
	    cpuid_get_feature_names(cpuid_features(), buf, sizeof(buf)));
	if (cpuid_leaf7_features()) {
		kprintf(" %s", cpuid_get_leaf7_feature_names(
			    cpuid_leaf7_features(), buf, sizeof(buf)));
	}
	if (cpuid_leaf7_extfeatures()) {
		kprintf(" %s", cpuid_get_leaf7_extfeature_names(
			    cpuid_leaf7_extfeatures(), buf, sizeof(buf)));
	}
	if (cpuid_leaf7_sl1_features()) {
		kprintf(" %s", cpuid_get_leaf7_sl1_feature_names(
			    cpuid_leaf7_sl1_features(), buf, sizeof(buf)));
	}
	if (cpuid_leaf7_sl1_extfeatures()) {
		kprintf(" %s", cpuid_get_leaf7_sl1_extfeature_names(
			    cpuid_leaf7_sl1_extfeatures(), buf, sizeof(buf)));
	}
	kprintf("\n");
	if (cpuid_features() & CPUID_FEATURE_HTT) {
#define s_if_plural(n)  ((n > 1) ? "s" : "")
		kprintf("  HTT: %d core%s per package;"
		    " %d logical cpu%s per package\n",
		    cpuid_cpu_infop->cpuid_cores_per_package,
		    s_if_plural(cpuid_cpu_infop->cpuid_cores_per_package),
		    cpuid_cpu_infop->cpuid_logical_per_package,
		    s_if_plural(cpuid_cpu_infop->cpuid_logical_per_package));
	}
}

void
cpuid_extfeature_display(
	const char      *header)
{
	char    buf[256];

	kprintf("%s: %s\n", header,
	    cpuid_get_extfeature_names(cpuid_extfeatures(),
	    buf, sizeof(buf)));
}

void
cpuid_cpu_display(
	const char      *header)
{
	if (cpuid_cpu_infop->cpuid_brand_string[0] != '\0') {
		kprintf("%s: %s\n", header, cpuid_cpu_infop->cpuid_brand_string);
	}
}

unsigned int
cpuid_family(void)
{
	return cpuid_info()->cpuid_family;
}

uint32_t
cpuid_cpufamily(void)
{
	return cpuid_info()->cpuid_cpufamily;
}

cpu_type_t
cpuid_cputype(void)
{
	return cpuid_info()->cpuid_cpu_type;
}

cpu_subtype_t
cpuid_cpusubtype(void)
{
	return cpuid_info()->cpuid_cpu_subtype;
}

uint64_t
cpuid_features(void)
{
	static int checked = 0;
	char    fpu_arg[20] = { 0 };

	(void) cpuid_info();
	if (!checked) {
		/* check for boot-time fpu limitations */
		if (PE_parse_boot_argn("_fpu", &fpu_arg[0], sizeof(fpu_arg))) {
			printf("limiting fpu features to: %s\n", fpu_arg);
			if (!strncmp("387", fpu_arg, sizeof("387")) || !strncmp("mmx", fpu_arg, sizeof("mmx"))) {
				printf("no sse or sse2\n");
				cpuid_cpu_infop->cpuid_features &= ~(CPUID_FEATURE_SSE | CPUID_FEATURE_SSE2 | CPUID_FEATURE_FXSR);
			} else if (!strncmp("sse", fpu_arg, sizeof("sse"))) {
				printf("no sse2\n");
				cpuid_cpu_infop->cpuid_features &= ~(CPUID_FEATURE_SSE2);
			}
		}
		checked = 1;
	}
	return cpuid_cpu_infop->cpuid_features;
}

uint64_t
cpuid_extfeatures(void)
{
	return cpuid_info()->cpuid_extfeatures;
}

uint64_t
cpuid_leaf7_features(void)
{
	return cpuid_info()->cpuid_leaf7_features;
}

uint64_t
cpuid_leaf7_extfeatures(void)
{
	return cpuid_info()->cpuid_leaf7_extfeatures;
}

uint64_t
cpuid_leaf7_sl1_features(void)
{
	return cpuid_info()->cpuid_leaf7_sl1_features;
}

uint64_t
cpuid_leaf7_sl1_extfeatures(void)
{
	return cpuid_info()->cpuid_leaf7_sl1_extfeatures;
}

static i386_vmm_info_t  *_cpuid_vmm_infop = NULL;
static i386_vmm_info_t  _cpuid_vmm_info;

static void
cpuid_init_vmm_info(i386_vmm_info_t *info_p)
{
	uint32_t        reg[4];
	uint32_t        max_vmm_leaf;

	bzero(info_p, sizeof(*info_p));

	if (!cpuid_vmm_present()) {
		return;
	}

	DBG("cpuid_init_vmm_info(%p)\n", info_p);

	/* do cpuid 0x40000000 to get VMM vendor */
	cpuid_fn(0x40000000, reg);
	max_vmm_leaf = reg[eax];
	bcopy((char *)&reg[ebx], &info_p->cpuid_vmm_vendor[0], 4);
	bcopy((char *)&reg[ecx], &info_p->cpuid_vmm_vendor[4], 4);
	bcopy((char *)&reg[edx], &info_p->cpuid_vmm_vendor[8], 4);
	info_p->cpuid_vmm_vendor[12] = '\0';

	if (0 == strcmp(info_p->cpuid_vmm_vendor, CPUID_VMM_ID_VMWARE)) {
		/* VMware identification string: kb.vmware.com/kb/1009458 */
		info_p->cpuid_vmm_family = CPUID_VMM_FAMILY_VMWARE;
	} else if (0 == strcmp(info_p->cpuid_vmm_vendor, CPUID_VMM_ID_PARALLELS)) {
		/* Parallels identification string */
		info_p->cpuid_vmm_family = CPUID_VMM_FAMILY_PARALLELS;
	} else {
		info_p->cpuid_vmm_family = CPUID_VMM_FAMILY_UNKNOWN;
	}

	/* VMM generic leaves: https://lkml.org/lkml/2008/10/1/246 */
	if (max_vmm_leaf >= 0x40000010) {
		cpuid_fn(0x40000010, reg);

		info_p->cpuid_vmm_tsc_frequency = reg[eax];
		info_p->cpuid_vmm_bus_frequency = reg[ebx];
	}

	DBG(" vmm_vendor          : %s\n", info_p->cpuid_vmm_vendor);
	DBG(" vmm_family          : %u\n", info_p->cpuid_vmm_family);
	DBG(" vmm_bus_frequency   : %u\n", info_p->cpuid_vmm_bus_frequency);
	DBG(" vmm_tsc_frequency   : %u\n", info_p->cpuid_vmm_tsc_frequency);
}

boolean_t
cpuid_vmm_present(void)
{
	return (cpuid_features() & CPUID_FEATURE_VMM) ? TRUE : FALSE;
}

i386_vmm_info_t *
cpuid_vmm_info(void)
{
	if (_cpuid_vmm_infop == NULL) {
		cpuid_init_vmm_info(&_cpuid_vmm_info);
		_cpuid_vmm_infop = &_cpuid_vmm_info;
	}
	return _cpuid_vmm_infop;
}

uint32_t
cpuid_vmm_family(void)
{
	return cpuid_vmm_info()->cpuid_vmm_family;
}

cwa_classifier_e
cpuid_wa_required(cpu_wa_e wa)
{
	i386_cpu_info_t *info_p = &cpuid_cpu_info;
	static uint64_t bootarg_cpu_wa_enables = 0;
	static uint64_t bootarg_cpu_wa_disables = 0;
	static int bootargs_overrides_processed = 0;

	if (!bootargs_overrides_processed) {
		if (!PE_parse_boot_argn("cwae", &bootarg_cpu_wa_enables, sizeof(bootarg_cpu_wa_enables))) {
			bootarg_cpu_wa_enables = 0;
		}

		if (!PE_parse_boot_argn("cwad", &bootarg_cpu_wa_disables, sizeof(bootarg_cpu_wa_disables))) {
			bootarg_cpu_wa_disables = 0;
		}
		bootargs_overrides_processed = 1;
	}

	if (bootarg_cpu_wa_enables & (1 << wa)) {
		return CWA_FORCE_ON;
	}

	if (bootarg_cpu_wa_disables & (1 << wa)) {
		return CWA_FORCE_OFF;
	}

	cpuid_wa_list[wa].enabled(cpuid_info());

	return CWA_OFF;
}

static void
cpuid_do_precpuid_was(void)
{
	/*
	 * Note that care must be taken not to use any data from the cached cpuid data since it is
	 * likely uninitialized at this point.  That includes calling functions that make use of
	 * that data as well.
	 */

}

/*
 * This should be able to collect the data of the calling processor.
 */
x86_core_type_t
cpuid_get_current_core_type(void)
{
    i386_cpu_info_t *info_p = cpuid_info();
    x86_core_type_t type = X86_CORE_TYPE_SMP;
    uint32_t reg[4] = {0, 0, 0, 0};
    uint32_t raw_type;

    if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_INTEL &&
        info_p->cpuid_max_basic >= 0x1a) {
        /* Fetch the part value from the Native Model ID leaf */
        reg[eax] = 0x1a;
        cpuid(reg);
        raw_type = bitfield32(reg[eax], 31, 24);
        if (raw_type == INTEL_CORE_TYPE_ATOM) {
            /*
             * https://community.intel.com/t5/Mobile-and-Desktop-Processors/Detecting-LP-E-Cores-on-Meteor-Lake-in-software/m-p/1584555#M70732
             */
            reg[eax] = 0x4;
            reg[ecx] = 0x3;
            cpuid(reg);

			/*
			 * This is such a bold assumption.
			 *
			 * Arrow Lake:
			 * Desktop silicon has no LP E-cores in the SoC tile
			 * Mobile silicon, however, does (excluding HX silicon).
			 *
			 * Meteor Lake:
			 * All silicon has LP E-cores
			 *
			 * Lunar Lake:
			 * Zero E-cores. Lion Cove P-cores + Gracemont LP E-cores.
			 *
			 * Rolling with this for now since Intel didn't bother to give anyone
			 * a sane method of enumerating core hierarchy via the CPUID.
			 */
            if (reg[eax] == 0) {
                type = X86_CORE_TYPE_EFFICIENCY_LP;
            } else {
                type = X86_CORE_TYPE_EFFICIENCY;
            }
        } else if (!(raw_type == INTEL_CORE_TYPE_CORE)) {
            panic("cpuid_get_current_core_type: unexpected core type 0x%x", raw_type);
        }
    } else if (info_p->cpuid_vendor_id == CPUID_VENDOR_ID_AMD &&
               info_p->cpuid_max_ext >= 0x80000026) {
        cpuid_fn(0x80000026, reg);
        raw_type = bitfield32(reg[ebx], 31, 28);
        if (raw_type == 0x1) {
            type = X86_CORE_TYPE_EFFICIENCY;
        } else if (raw_type != 0) {
            panic("cpuid_get_current_core_type: unexpected core type 0x%x", raw_type);
        }
    }

    return type;
}
