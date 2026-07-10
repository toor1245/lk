/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include "rpmb_hmac.h"

#include <string.h>

#include <lib/mincrypt/sha256.h>

#define SHA256_BLOCK_SIZE 64

const uint8_t rpmb_test_key[RPMB_KEY_MAC_SIZE] = {
	0xD3, 0xEB, 0x3E, 0xC3, 0x6E, 0x33, 0x4C, 0x9F,
	0x98, 0x8C, 0xE2, 0xC0, 0xB8, 0x59, 0x54, 0x61,
	0x0D, 0x2B, 0xCF, 0x86, 0x64, 0x84, 0x4D, 0xF2,
	0xAB, 0x56, 0xE6, 0xC6, 0x1B, 0xB7, 0x01, 0xE4
};

void rpmb_hmac(const uint8_t *key, const uint8_t *buff, size_t len, uint8_t *output) {
    SHA256_CTX ctx;
    uint8_t k_ipad[SHA256_BLOCK_SIZE];
    uint8_t k_opad[SHA256_BLOCK_SIZE];
    size_t i;

    /* According to RFC 4634, the HMAC transform looks like:
       SHA(K XOR opad, SHA(K XOR ipad, text))

       where K is an n byte key.
       ipad is the byte 0x36 repeated blocksize times
       opad is the byte 0x5c repeated blocksize times
       and text is the data being protected.
    */

    for (i = 0; i < RPMB_KEY_MAC_SIZE; i++) {
        k_ipad[i] = key[i] ^ 0x36;
        k_opad[i] = key[i] ^ 0x5c;
    }
    /* remaining pad bytes are '\0' XOR'd with ipad and opad values */
    for (; i < SHA256_BLOCK_SIZE; i++) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5c;
    }

    SHA256_init(&ctx);
    SHA256_update(&ctx, k_ipad, SHA256_BLOCK_SIZE);
    SHA256_update(&ctx, buff, len);
    memcpy(output, SHA256_final(&ctx), RPMB_KEY_MAC_SIZE);

    /* Init context for second pass */
    SHA256_init(&ctx);

    /* start with outer pad */
    SHA256_update(&ctx, k_opad, SHA256_BLOCK_SIZE);

    /* then results of 1st hash */
    SHA256_update(&ctx, output, RPMB_KEY_MAC_SIZE);

    /* finish up 2nd pass */
    memcpy(output, SHA256_final(&ctx), RPMB_KEY_MAC_SIZE);
}
