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
#include "srtp.c"

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
        "\xE1\xF9\x7A\x0D\x3E\x01\x8B\xE0\xD6\x4F\xA3\x2C\x06\xDE\x41\x39";
    static const uint8_t salt[14] =
        "\x0E\xC6\x75\xAD\x49\x8A\xFE\xEB\xB6\x96\x0B\x3A\xAB\xE6";

    static const uint8_t good_cipher[16] =
        "\xC6\x1E\x7A\x93\x74\x4F\x39\xEE\x10\x73\x4A\xFE\x3F\xF7\xA0\x87";
    static const uint8_t good_salt[14] =
        "\x30\xCB\xBC\x08\x86\x3D\x8C\x85\xD4\x9D\xB3\x4A\x9A\xE1";
    static const uint8_t good_auth[94] =
        "\xCE\xBE\x32\x1F\x6F\xF7\x71\x6B\x6F\xD4\xAB\x49\xAF\x25\x6A\x15"
        "\x6D\x38\xBA\xA4\x8F\x0A\x0A\xCF\x3C\x34\xE2\x35\x9E\x6C\xDB\xCE"
        "\xE0\x49\x64\x6C\x43\xD9\x32\x7A\xD1\x75\x57\x8E\xF7\x22\x70\x98"
        "\x63\x71\xC1\x0C\x9A\x36\x9A\xC2\xF9\x4A\x8C\x5F\xBC\xDD\xDC\x25"
        "\x6D\x6E\x91\x9A\x48\xB6\x10\xEF\x17\xC2\x04\x1E\x47\x40\x35\x76"
        "\x6B\x68\x64\x2C\x59\xBB\xFC\x2F\x34\xDB\x60\xDB\xDF\xB2";

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
        "\x2B\x7E\x15\x16\x28\xAE\xD2\xA6\xAB\xF7\x15\x88\x09\xCF\x4F\x3C";
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
        "\xE0\x3E\xAD\x09\x35\xC9\x5E\x80\xE1\x66\xB1\x6D\xD9\x2B\x4E\xB4"
        "\xD2\x35\x13\x16\x2B\x02\xD0\xF7\x2A\x43\xA2\xFE\x4A\x5F\x97\xAB"
        "\x41\xE9\x5B\x3B\xB0\xA2\xE8\xDD\x47\x79\x01\xE4\xFC\xA8\x94\xC0";
    static const uint8_t good_end[48] =
        "\xEC\x8C\xDF\x73\x98\x60\x7C\xB0\xF2\xD2\x16\x75\xEA\x9E\xA1\xE4"
        "\x36\x2B\x7C\x3C\x67\x73\x51\x63\x18\xA0\x77\xD7\xFC\x50\x73\xAE"
        "\x6A\x2C\xC3\x78\x78\x89\x37\x4F\xBE\xB4\xC8\x1B\x17\xBA\x6C\x44";

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
