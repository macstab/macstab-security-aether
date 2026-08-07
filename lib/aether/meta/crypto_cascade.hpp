/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#pragma once
/**
 * @file crypto_cascade.hpp
 * @brief Real multi-layer crypto cascade (onion) for research init.
 *
 * Build (inside-out):
 *   leaf = research implementation bytes
 *   for each outer layer:
 *     key = random 32 B
 *     ct  = stream_crypt(inner_package, key)   // keystream XOR (ChaCha-like)
 *     pack = magic | key | ct | morph_tag
 *     inner_package = pack
 *
 * Peel (outside-in, on execution only):
 *   while package is onion:
 *     decrypt ct with key → next
 *     WIPE current package completely
 *     next becomes current
 *   final plaintext = impl leaf
 *
 * This is a true decrypt cascade, not “morph only”. Educational / research use.
 */

#include "aether/meta/transforms.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aether {

/** One recorded peel/build step for demos. */
struct CascadeStep {
    int index = 0;
    std::string mode; ///< morph design of this shell (e.g. permute-heavy)
    size_t cipher_bytes = 0;
    size_t plain_bytes = 0;
    bool is_leaf = false;
};

/**
 * Stream crypt: keystream XOR (encrypt == decrypt).
 * Keystream from expanded key + nonce counter (ChaCha-inspired quarter rounds lite).
 */
void stream_crypt(std::vector<uint8_t>& buf, const uint8_t key[32], const uint8_t nonce[12]);

/**
 * Build an onion of @p layers wraps around @p leaf.
 * Each wrap uses a random LayerMode for logging diversity (shell identity).
 * @return outermost package only (inners exist only encrypted inside)
 */
std::vector<uint8_t> cascade_build(const std::vector<uint8_t>& leaf,
                                   int layers,
                                   std::vector<CascadeStep>* steps = nullptr);

/**
 * Peel onion outside-in. Each layer is decrypted then the ciphertext package
 * is wiped from @p package (in/out: starts as outer, ends as leaf plaintext).
 * @return true if magic/structure OK through all layers
 */
bool cascade_peel(std::vector<uint8_t>& package, std::vector<CascadeStep>* steps = nullptr);

/** True if buffer looks like an Aether onion package. */
bool is_cascade_package(const std::vector<uint8_t>& buf);

} // namespace aether
