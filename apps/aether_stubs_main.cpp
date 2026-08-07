/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

/**
 * @file aether_stubs_main.cpp
 * @brief Demo entry that only exercises empty architecture stubs.
 */

#include "architecture_stubs.hpp"

#include <cstdio>

/**
 * Demo entrypoint that only calls empty architecture stubs.
 * Prints a banner, invokes each stub once (all print [STUB] and fail),
 * and exits. Used to illustrate advanced technique layout without code.
 * @return 0 always
 */
int main() {
    using namespace aether::stubs;

    printf("============================================================\n");
    printf(" AETHER STUBS — Architecture placeholders only\n");
    printf("============================================================\n");
    printf("No advanced technique is actually implemented.\n\n");

    ProcessHollowing::hollow_process("/usr/bin/example");
    ProcessHollowing::module_stomp("libc.so.6");

    EBPFRootkit::load_hide_programs();
    EBPFRootkit::hide_from_bpftool();

    FilelessLOTL::memfd_exec({});
    FilelessLOTL::living_off_the_land();

    C2Persistence::start_c2();
    C2Persistence::install_persistence();
    C2Persistence::anti_forensics();

    printf("\nAll advanced sections are empty stubs.\n");
    printf("Use only for illustrating architecture in a research article.\n");
    return 0;
}
