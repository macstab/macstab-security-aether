/**
 * SPDX-License-Identifier: LicenseRef-Aether-Research
 * Copyright (c) 2024–2026 Macstab GmbH.
 *
 * Licensed under the Aether Research License in the repository root file
 * LICENSE. Authorized lab / lawful private use only as defined there.
 * By using this file you accept LICENSE. See docs/LICENSE_ACCEPTANCE.md.
 */

#include "aether/meta/crypto_cascade.hpp"

#include "aether/common/rng.hpp"

#include <cstring>

namespace aether {
namespace {

// Package: magic[4] "AeC1" | mode[1] | key[32] | nonce[12] | ct_len[4 LE] | ct[ct_len]
constexpr uint8_t kMagic[4] = {'A', 'e', 'C', '1'};

void write_u32_le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)v);
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 24));
}

uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/** Rotl32 */
uint32_t rotl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

/**
 * One block of keystream (64 bytes) from key/nonce/counter — ChaCha-inspired.
 * Not claiming wire-compatible ChaCha20; research-grade stream cipher structure.
 */
void keystream_block(const uint8_t key[32],
                     const uint8_t nonce[12],
                     uint32_t counter,
                     uint8_t out[64]) {
    uint32_t s[16];
    // constants "expa nd 3 2-byte k"
    s[0] = 0x61707865;
    s[1] = 0x3320646e;
    s[2] = 0x79622d32;
    s[3] = 0x6b206574;
    for (int i = 0; i < 8; i++) {
        s[4 + i] = (uint32_t)key[4 * i] | ((uint32_t)key[4 * i + 1] << 8) |
                   ((uint32_t)key[4 * i + 2] << 16) | ((uint32_t)key[4 * i + 3] << 24);
    }
    s[12] = counter;
    s[13] = (uint32_t)nonce[0] | ((uint32_t)nonce[1] << 8) | ((uint32_t)nonce[2] << 16) |
            ((uint32_t)nonce[3] << 24);
    s[14] = (uint32_t)nonce[4] | ((uint32_t)nonce[5] << 8) | ((uint32_t)nonce[6] << 16) |
            ((uint32_t)nonce[7] << 24);
    s[15] = (uint32_t)nonce[8] | ((uint32_t)nonce[9] << 8) | ((uint32_t)nonce[10] << 16) |
            ((uint32_t)nonce[11] << 24);

    uint32_t w[16];
    for (int i = 0; i < 16; i++)
        w[i] = s[i];

    auto QR = [&](int a, int b, int c, int d) {
        w[a] += w[b];
        w[d] ^= w[a];
        w[d] = rotl(w[d], 16);
        w[c] += w[d];
        w[b] ^= w[c];
        w[b] = rotl(w[b], 12);
        w[a] += w[b];
        w[d] ^= w[a];
        w[d] = rotl(w[d], 8);
        w[c] += w[d];
        w[b] ^= w[c];
        w[b] = rotl(w[b], 7);
    };

    for (int i = 0; i < 10; i++) {
        QR(0, 4, 8, 12);
        QR(1, 5, 9, 13);
        QR(2, 6, 10, 14);
        QR(3, 7, 11, 15);
        QR(0, 5, 10, 15);
        QR(1, 6, 11, 12);
        QR(2, 7, 12, 13);
        QR(3, 4, 13, 14);
    }
    for (int i = 0; i < 16; i++)
        w[i] += s[i];
    for (int i = 0; i < 16; i++) {
        out[4 * i] = (uint8_t)w[i];
        out[4 * i + 1] = (uint8_t)(w[i] >> 8);
        out[4 * i + 2] = (uint8_t)(w[i] >> 16);
        out[4 * i + 3] = (uint8_t)(w[i] >> 24);
    }
}

void secure_wipe(std::vector<uint8_t>& v) {
    if (!v.empty()) {
        volatile uint8_t* p = v.data();
        for (size_t i = 0; i < v.size(); i++)
            p[i] = 0;
    }
    v.clear();
    v.shrink_to_fit();
}

void random_bytes(uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++)
        p[i] = (uint8_t)rnd(0, 255);
}

} // namespace

void stream_crypt(std::vector<uint8_t>& buf, const uint8_t key[32], const uint8_t nonce[12]) {
    uint8_t block[64];
    uint32_t counter = 0;
    size_t off = 0;
    while (off < buf.size()) {
        keystream_block(key, nonce, counter++, block);
        size_t n = buf.size() - off;
        if (n > 64)
            n = 64;
        for (size_t i = 0; i < n; i++)
            buf[off + i] ^= block[i];
        off += n;
    }
    // wipe keystream block
    volatile uint8_t* p = block;
    for (int i = 0; i < 64; i++)
        p[i] = 0;
}

bool is_cascade_package(const std::vector<uint8_t>& buf) {
    if (buf.size() < 4 + 1 + 32 + 12 + 4)
        return false;
    return buf[0] == kMagic[0] && buf[1] == kMagic[1] && buf[2] == kMagic[2] && buf[3] == kMagic[3];
}

std::vector<uint8_t>
cascade_build(const std::vector<uint8_t>& leaf, int layers, std::vector<CascadeStep>* steps) {
    if (layers < 1)
        layers = 1;

    std::vector<uint8_t> inner = leaf;

    for (int i = 0; i < layers; i++) {
        uint8_t key[32];
        uint8_t nonce[12];
        random_bytes(key, 32);
        random_bytes(nonce, 12);

        LayerMode mode = random_layer_mode();
        if (rnd(0, 2) == 0)
            mode = LayerMode::PermuteHeavy; // bias structural diversity

        std::vector<uint8_t> ct = inner;
        stream_crypt(ct, key, nonce);

        std::vector<uint8_t> pack;
        pack.insert(pack.end(), kMagic, kMagic + 4);
        pack.push_back((uint8_t)mode);
        pack.insert(pack.end(), key, key + 32);
        pack.insert(pack.end(), nonce, nonce + 12);
        write_u32_le(pack, (uint32_t)ct.size());
        pack.insert(pack.end(), ct.begin(), ct.end());

        if (steps) {
            CascadeStep st;
            st.index = i;
            st.mode = layer_mode_name(mode);
            st.cipher_bytes = ct.size();
            st.plain_bytes = inner.size();
            st.is_leaf = false;
            steps->push_back(st);
        }

        // Wipe previous plaintext and key material from stack buffers.
        secure_wipe(inner);
        secure_wipe(ct);
        volatile uint8_t* kp = key;
        for (int k = 0; k < 32; k++)
            kp[k] = 0;
        volatile uint8_t* np = nonce;
        for (int k = 0; k < 12; k++)
            np[k] = 0;

        inner = std::move(pack);
    }

    if (steps && !steps->empty()) {
        // mark build order outer = last pushed
    }
    return inner;
}

bool cascade_peel(std::vector<uint8_t>& package, std::vector<CascadeStep>* steps) {
    int round = 0;
    while (is_cascade_package(package)) {
        if (package.size() < 4 + 1 + 32 + 12 + 4)
            return false;

        const uint8_t mode_b = package[4];
        uint8_t key[32];
        uint8_t nonce[12];
        std::memcpy(key, package.data() + 5, 32);
        std::memcpy(nonce, package.data() + 37, 12);
        uint32_t ct_len = read_u32_le(package.data() + 49);
        if (package.size() < 53 + ct_len)
            return false;

        std::vector<uint8_t> plain(package.begin() + 53, package.begin() + 53 + ct_len);
        stream_crypt(plain, key, nonce); // decrypt

        if (steps) {
            CascadeStep st;
            st.index = round++;
            st.mode = layer_mode_name(static_cast<LayerMode>(mode_b % (int)LayerMode::Count));
            st.cipher_bytes = ct_len;
            st.plain_bytes = plain.size();
            st.is_leaf = !is_cascade_package(plain);
            steps->push_back(st);
        }

        // WIPE this onion layer completely before continuing.
        secure_wipe(package);
        volatile uint8_t* kp = key;
        for (int k = 0; k < 32; k++)
            kp[k] = 0;
        volatile uint8_t* np = nonce;
        for (int k = 0; k < 12; k++)
            np[k] = 0;

        package = std::move(plain);
    }
    return true;
}

} // namespace aether
