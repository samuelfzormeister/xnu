/*
 * Copyright (c) 2026, Samuel Zormeister
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <i386/cpuid.h>
#include <i386/hfi.h>

//************************************************************************************************
// Intel's Hardware Feedback Interface is a feature in Alder Lake and newer processors.
//
// AMD; However, ALSO has a Hardware Feedback Interface.
//
// TODO:
// Modularise the HFI subsystem.
//************************************************************************************************

#define DBG(...) kprintf("hfi: " __VA_ARGS__)

static hfi_caps_t hfi_caps;

void hfi_bootstrap(void)
{
    i386_cpu_info_t *cpu_infop = cpuid_info();
    uint32_t reg[4];

    DBG("begin bootstrap...\n");

    if (cpu_infop->cpuid_thermal_leaf.hardware_feedback_ix == 0) {
        DBG("Hardware Feedback Intreface is not supported.\n");
        return;
    }

    DBG("Hardware Feedback Interface is supported.\n");

    do_cpuid(6, reg);

    //
    // get capabilities
    //
    hfi_caps.hfi_perf_cap       = bitfield32(reg[edx], 0, 0);
    hfi_caps.hfi_efficiency_cap = bitfield32(reg[edx], 1, 1);
}
