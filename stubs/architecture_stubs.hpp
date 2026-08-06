/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Aether Research Project contributors.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#pragma once
/**
 * @file architecture_stubs.hpp
 * @brief Empty placeholders for advanced techniques (article architecture only).
 *
 * NOTHING here performs the technique. Functions print [STUB] and return false.
 * Kept separate from the working metamorphic engine for clear research framing.
 */

#include <cstdint>
#include <cstdio>
#include <vector>

namespace aether::stubs {

// ---------------------------------------------------------------------------
// Process hollowing / module stomping
// ---------------------------------------------------------------------------
namespace ProcessHollowing {

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would suspend @p target_path, unmap its image,
 * map a payload, set the entry point, and resume the process.
 * @param target_path path that would be hollowed (unused)
 * @return always false (not implemented)
 */
inline bool hollow_process(const char* target_path) {
    printf("[STUB] ProcessHollowing::hollow_process(\"%s\") — empty\n", target_path);
    return false;
}

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would overwrite .text of the named loaded module
 * with attacker code (module stomping).
 * @param legitimate_module module name that would be stomped (unused)
 * @return always false (not implemented)
 */
inline bool module_stomp(const char* legitimate_module) {
    printf("[STUB] ProcessHollowing::module_stomp(\"%s\") — empty\n", legitimate_module);
    return false;
}

} // namespace ProcessHollowing

// ---------------------------------------------------------------------------
// eBPF rootkit (self-hiding)
// ---------------------------------------------------------------------------
namespace EBPFRootkit {

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would load BPF programs to hide pids/files/maps.
 * @return always false (not implemented)
 */
inline bool load_hide_programs() {
    printf("[STUB] EBPFRootkit::load_hide_programs() — empty\n");
    return false;
}

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would intercept sys_bpf so bpftool cannot list
 * the rootkit's own programs.
 * @return always false (not implemented)
 */
inline bool hide_from_bpftool() {
    printf("[STUB] EBPFRootkit::hide_from_bpftool() — empty\n");
    return false;
}

} // namespace EBPFRootkit

// ---------------------------------------------------------------------------
// Fileless + living-off-the-land
// ---------------------------------------------------------------------------
namespace FilelessLOTL {

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would create a memfd, write @p payload, and execveat.
 * @param payload bytes that would be executed from memory (unused)
 * @return always false (not implemented)
 */
inline bool memfd_exec(const std::vector<uint8_t>& /*payload*/) {
    printf("[STUB] FilelessLOTL::memfd_exec() — empty\n");
    return false;
}

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would chain only existing system binaries (LOTL).
 * @return always false (not implemented)
 */
inline bool living_off_the_land() {
    printf("[STUB] FilelessLOTL::living_off_the_land() — empty\n");
    return false;
}

} // namespace FilelessLOTL

// ---------------------------------------------------------------------------
// C2 + persistence + anti-forensics
// ---------------------------------------------------------------------------
namespace C2Persistence {

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would open a command-and-control channel/beacon.
 * @return always false (not implemented)
 */
inline bool start_c2() {
    printf("[STUB] C2Persistence::start_c2() — empty\n");
    return false;
}

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would install reboot survival (cron/systemd/etc.).
 * @return always false (not implemented)
 */
inline bool install_persistence() {
    printf("[STUB] C2Persistence::install_persistence() — empty\n");
    return false;
}

/**
 * Does nothing real. Prints a stub line and returns false.
 * A real implementation would wipe logs, timestomp files, or remove artifacts.
 * @return always false (not implemented)
 */
inline bool anti_forensics() {
    printf("[STUB] C2Persistence::anti_forensics() — empty\n");
    return false;
}

} // namespace C2Persistence

} // namespace aether::stubs
