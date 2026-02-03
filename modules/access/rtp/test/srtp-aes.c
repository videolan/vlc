/*
 * Secure RTP with libgcrypt
 * Copyright (C) 2007  Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include "../srtp.c"

static void printhex (const void *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf ("%02X", ((uint8_t *)buf)[i]);
    fputc ('\n', stdout);
}

static void fatal (const char *msg)
{
    puts (msg);
    exit (1);
}

/** AES-CM key derivation test vectors */
static void test_derivation (void)
{
    static const uint8_t key[16] =
        {0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0, 0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39 };
    static const uint8_t salt[14] =
        {0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB, 0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6 };

    static const uint8_t good_cipher[16] =
        {0xC6, 0x1E, 0x7A, 0x93, 0x74, 0x4F, 0x39, 0xEE, 0x10, 0x73, 0x4A, 0xFE, 0x3F, 0xF7, 0xA0, 0x87 };
    static const uint8_t good_salt[14] =
        {0x30, 0xCB, 0xBC, 0x08, 0x86, 0x3D, 0x8C, 0x85, 0xD4, 0x9D, 0xB3, 0x4A, 0x9A, 0xE1 };
    static const uint8_t good_auth[94] =
        {0xCE, 0xBE, 0x32, 0x1F, 0x6F, 0xF7, 0x71, 0x6B, 0x6F, 0xD4, 0xAB, 0x49, 0xAF, 0x25, 0x6A, 0x15,
         0x6D, 0x38, 0xBA, 0xA4, 0x8F, 0x0A, 0x0A, 0xCF, 0x3C, 0x34, 0xE2, 0x35, 0x9E, 0x6C, 0xDB, 0xCE,
         0xE0, 0x49, 0x64, 0x6C, 0x43, 0xD9, 0x32, 0x7A, 0xD1, 0x75, 0x57, 0x8E, 0xF7, 0x22, 0x70, 0x98,
         0x63, 0x71, 0xC1, 0x0C, 0x9A, 0x36, 0x9A, 0xC2, 0xF9, 0x4A, 0x8C, 0x5F, 0xBC, 0xDD, 0xDC, 0x25,
         0x6D, 0x6E, 0x91, 0x9A, 0x48, 0xB6, 0x10, 0xEF, 0x17, 0xC2, 0x04, 0x1E, 0x47, 0x40, 0x35, 0x76,
         0x6B, 0x68, 0x64, 0x2C, 0x59, 0xBB, 0xFC, 0x2F, 0x34, 0xDB, 0x60, 0xDB, 0xDF, 0xB2 };

    static const uint8_t r[6] = { 0, 0, 0, 0, 0, 0 };
    gcry_cipher_hd_t prf;
    uint8_t out[94];

    puts ("AES-CM key derivation test...");
    printf (" master key:  ");
    printhex (key, sizeof (key));
    printf (" master salt: ");
    printhex (salt, sizeof (salt));

    if (gcry_cipher_open (&prf, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CTR, 0)
     || gcry_cipher_setkey (prf, key, sizeof (key)))
        fatal ("Internal PRF error");

    if (do_derive (prf, salt, r, sizeof (r), SRTP_CRYPT, out, 16))
        fatal ("Internal cipher derivation error");
    printf (" cipher key:  ");
    printhex (out, 16);
    if (memcmp (out, good_cipher, 16))
        fatal ("Test failed");

    if (do_derive (prf, salt, r, sizeof (r), SRTP_SALT, out, 14))
        fatal ("Internal salt derivation error");
    printf (" cipher salt: ");
    printhex (out, 14);
    if (memcmp (out, good_salt, 14))
        fatal ("Test failed");

    if (do_derive (prf, salt, r, sizeof (r), SRTP_AUTH, out, 94))
        fatal ("Internal auth key derivation error");
    printf (" auth key:    ");
    printhex (out, 94);
    if (memcmp (out, good_auth, 94))
        fatal ("Test failed");

    gcry_cipher_close (prf);
}

/** AES-CM key derivation test vectors */
static void test_keystream (void)
{
    static const uint8_t key[16] =
        {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
    const uint32_t salt[4]=
        { htonl (0xf0f1f2f3), htonl (0xf4f5f6f7),
          htonl (0xf8f9fafb), htonl (0xfcfd0000) };

    puts ("AES-CM key stream test...");
    uint8_t *buf = calloc (0xff02, 16);
    if (buf == NULL)
    {
        fputs ("Not enough memory for test\n", stderr);
        return;
    }

    printf (" session key: ");
    printhex (key, sizeof (key));

    gcry_cipher_hd_t hd;
    if (gcry_cipher_open (&hd, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_CTR, 0))
        fatal ("Cipher initialization error");
    if (gcry_cipher_setkey (hd, key, sizeof (key)))
        fatal ("Cipher key error");

    if (rtp_crypt (hd, 0, 0, 0, salt, buf, 0xff020))
        fatal ("Encryption failure");
    gcry_cipher_close (hd);

    static const uint8_t good_start[48] =
        {0xE0, 0x3E, 0xAD, 0x09, 0x35, 0xC9, 0x5E, 0x80, 0xE1, 0x66, 0xB1, 0x6D, 0xD9, 0x2B, 0x4E, 0xB4,
         0xD2, 0x35, 0x13, 0x16, 0x2B, 0x02, 0xD0, 0xF7, 0x2A, 0x43, 0xA2, 0xFE, 0x4A, 0x5F, 0x97, 0xAB,
         0x41, 0xE9, 0x5B, 0x3B, 0xB0, 0xA2, 0xE8, 0xDD, 0x47, 0x79, 0x01, 0xE4, 0xFC, 0xA8, 0x94, 0xC0 };
    static const uint8_t good_end[48] =
        {0xEC, 0x8C, 0xDF, 0x73, 0x98, 0x60, 0x7C, 0xB0, 0xF2, 0xD2, 0x16, 0x75, 0xEA, 0x9E, 0xA1, 0xE4,
         0x36, 0x2B, 0x7C, 0x3C, 0x67, 0x73, 0x51, 0x63, 0x18, 0xA0, 0x77, 0xD7, 0xFC, 0x50, 0x73, 0xAE,
         0x6A, 0x2C, 0xC3, 0x78, 0x78, 0x89, 0x37, 0x4F, 0xBE, 0xB4, 0xC8, 0x1B, 0x17, 0xBA, 0x6C, 0x44 };

    printf (" key stream:  ");
    printhex (buf, sizeof (good_start));
    printf (" ... cont'd : ");
    printhex (buf + 0xff020 - sizeof (good_end), sizeof (good_end));
    if (memcmp (buf, good_start, sizeof (good_start))
     || memcmp (buf + 0xff020 - sizeof (good_end), good_end,
                sizeof (good_end)))
        fatal ("Key stream test failed");
    free (buf);
}

static void srtp_test (void)
{
    test_derivation ();
    test_keystream ();
}

int main (void)
{
    gcry_control (GCRYCTL_DISABLE_SECMEM, NULL);
    srtp_test ();
    return 0;
}
