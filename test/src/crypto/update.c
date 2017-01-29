/*****************************************************************************
 * update.c: test VLC updating engine
 *****************************************************************************
 * Copyright (C) 2014 Rafaël Carré
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <vlc_common.h>
#include "../src/misc/update_crypto.c"

static const uint8_t key_longid[] = {
    0x9F, 0x8A, 0xB1, 0x13, 0x9F, 0x39, 0x9B, 0xEE
};

/* RSA 4096 */
static const uint8_t public_key[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "Version: GnuPG v2\n"
    "\n"
    "mQINBFR5uMsBEADBIrQB0ga41UbPkeJzrnUKQiEcOJtdJ1MS4wqRW9Bc4S8N4995\n"
    "CDnHRjQ6VIVqvMuU6ceAKKjjFH911bbmpJiXagsifm+G/WUbYcmDhF8bXpC0u2Ky\n"
    "1A0N7FsQ4QK0U8qvqdYDf3Uu8d7ANjoe50CkaQP0KhWm1i6P4rzHaNYNhg6/J5s+\n"
    "wRwSVg9eej9O3jjK+enYC/2gqt0U43qYBhHegZkMEKtKfQd18l2+5vWghjlOlP3w\n"
    "VsWBcMl7MRujMEoCtyCUpnJFO9rnGdbQLPAUPnuG5LFapROUfcQFe8EzJmlDx+jR\n"
    "3gBoJ8hv3pRLo7iuxBtFnbZY/nZRwyQ6G7qxtGsBwJEjjMjXBG+bcjtLQVamwgbI\n"
    "ElRTB98BWTAKY82TH5+76q4UNMjZM6Zn2zp7zuHNatrl2CCYbP0uCtZgh8PFaA6E\n"
    "saN2L4A2A4ZS6jnIIRGZXHpqcLrZESuWiWGtu0TdkOJ5kG9L9LySCoK/RteAkyno\n"
    "g3aiMtF4t97zEiULU9ioTGA57fHPJM2qm+44//nxtrVZIV4eQ69dFzd7XZ+z8PPx\n"
    "AtnRbraKLwvg8uqIkWKSdFdt8GEtuw80J8uBu1D8Ea8YhdeBiCcWs9C6bGIFo7Uy\n"
    "NqbK6xyCfG7c8W5xdnMW6nShuI7w5c5kjrMr3o87MifvgoHPPrt5HeJ4EQARAQAB\n"
    "tCVWaWRlb0xBTiB0ZXN0IDx2aWRlb2xhbkB2aWRlb2xhbi5vcmc+iQI3BBMBCAAh\n"
    "BQJUebjLAhsDBQsJCAcCBhUICQoLAgQWAgMBAh4BAheAAAoJEJ+KsROfOZvuCVIQ\n"
    "AK4F+niVQ8LUzSV4Hj5Bc9o6uzC6OYLRH43PJNfeHv2jzc5lyPff906CTp+W/Evn\n"
    "+VLZUz7AqRIz/U59PQgTbSEUGpDSt09cjYESqglV8dQEi81y93EIPnQPF3+jv+H0\n"
    "1/r9bRUef9D8eGS5Vrnu+WDJAlQUMfLJgdxAL7XLPIXNlSAqBRUguRK8Y6kmlAwC\n"
    "apODSog+u7TH5845bjeuVedqRyGsl1SoKIChSMq+zCubWsrKoyaKExNm4bYs7pOi\n"
    "8y3VESbCb6t3rHRMzmuEjV3SyHtpZrGXPN0OROm8hSNGEPtcu08IO8VgXlp6D3Cc\n"
    "TX2tLZs+4VN+x4iakScO3vdlyqMzsVL5OrV6cijSPE0GBNALmwowkWeVwEcykUYx\n"
    "MOSLJtuHJL2fTWRZ0rfPSemxyCT+74YslkbLkl1/cg4YKkkREBHOhXjgFAxiGlJF\n"
    "E43DIR0kaeqXOFkBsk/T7SHqWnBq8aWa7BrQNlM/b3FVhE/JgYsFoTWS5Yh0aYJE\n"
    "nmc9U66Ym/BaOkuIIoqHq+9xH90ho0crjBDTBm+9XkY+BviwcFDYVS+dpWj5i3X+\n"
    "2UfwMqMRJRXjYUK50z9Ap4r0ekafGmm0+ox0xn0bxLZOh7zrAzE/+TSG/h8HIPKZ\n"
    "qzcDd/n/EiFF23kBktC3ljzJoBxxUEGEE8YwzRWraAdV\n"
    "=6eGQ\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};

static const uint8_t key_longid2[] = {
    0xD3, 0x78, 0x31, 0xFD, 0x2D, 0xCD, 0xEC, 0x0F
};

/* RSA 4096, signed by public_key */
static const uint8_t public_key2[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "Version: GnuPG v2\n"
    "\n"
    "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\n"
    "=oMbP\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};

/* data to verify */
static const char status[] = {
    "42.0.19.1\n"
    "http://get.videolan.org/vlc/foo/bar.xz\n"
    "Bla bla bla.\n"
    "Foo bar.\n"
};

/* signature of status[] verifiable using public_key2[] */
static const uint8_t status_sign2[] = {
    "-----BEGIN PGP SIGNATURE-----\n"
    "Version: GnuPG v2\n"
    "\n"
    "iQIcBAABCAAGBQJUedMOAAoJENN4Mf0tzewPeCEQAIpkDYdJ+fLT7fG6MOOTAx8v\n"
    "cMHU2avRax9ttQ+10ZJXFRIlP52mtRMKjZ1hG47f3F4leccaKfTBOXsO20TjrsDk\n"
    "BByy+8TYK5IFk4AFQVEB4XuFw6almuf2ham0W5UaK7qdmUXHp3gU37ZKHkrmfF9C\n"
    "CDUsiogWMyUI8tL0sCmx8HXEAXxT9mDszLR3DcNxgKbmxUrxYuY3pifsmyZ4dDYZ\n"
    "g60INP3pcGm781lsS+D7Mc7GxvpqbZpZqzDNU1JMrCosm4hK81N50MLZDLc6N28T\n"
    "jYAC3I2VqctNOm4mIDSrdCTYWVhQHNe1d3FXkalj/x+kWz59qYdkRR6JkMbIvL2U\n"
    "m7tHalN9URiIX6jXDg/0K9WjEfFfYxnbTzn2opMasgPLzqvsryFgzkGliRitnGGV\n"
    "0HAiRMOw80HiA5I0T5z2lUX68k1gR0+jgOemYXua3ZBusxPXxKYMWELL7QoHUTjH\n"
    "q3urZlp5eaVP17I0H0yS+H3bX9h285AolTENbe05fh/gYjEUzRodLnejbCmlFcj2\n"
    "IeeZpQPjU2GozPzw7zc3tTkAfPb3pMu5RsYZH81PeohXMjUoATQH4BCHrV7s6kmt\n"
    "BttU9zAm4SCZJyTHR2PPUeQw3hNNrellyK+J/GJdeC6IFssIMF9I9rHpKf5YA2gL\n"
    "WvTgfHRwPJQCso4eMOJV\n"
    "=LwcX\n"
    "-----END PGP SIGNATURE-----\n"
};

/* signature of status[] verifiable using public_key[] */
static const uint8_t status_sign[] = {
    "-----BEGIN PGP SIGNATURE-----\n"
    "Version: GnuPG v2\n"
    "\n"
    "iQIcBAABCAAGBQJUechbAAoJEJ+KsROfOZvuFdsP/0SZ2rmOTz/ILWJgZOfyNhZU\n"
    "p8zAfcqrQdpTnuyrP8M4oEm4Kv1l/C/yGvOadHjmnqoJVCtE5kHmUV3rnBbaHZom\n"
    "BKKlY9i4UY4mhb+jIYW+jlxMs8FdZwnKYxCdxhk1kfPXtrR1uQ/q3XRiumlR3BJJ\n"
    "xbh2rjFZikqRnB/zZJyss/Q1cIn+tosqynAwgM/0uA5seWaKGafXr38dnBYN0bgx\n"
    "khNy89SMxpgx2K/TOGPRimVP+u+RBTtm86jzQJGLF4MqWdxkWPUyfd58KMegdJ6N\n"
    "sTVlfoRAjS2+H+fYDycqjU0oKt8bSeuu6qi//nrwn8U1F4qLHxxig5t0uChSUod0\n"
    "MxknIHUh+lqO0zaLGU7/gFfprIWGqYOsbqNVJA78NkUIbiJT2SX5WpTjNP+Iqn2v\n"
    "/ig+sWDUQkmTbpRSN62rP9DvxXNoPhT9Oe8kQjUW+T2oLFHd2xwhAO3aKx/9N7p7\n"
    "0qnUX/5wo5i1hqTBDU6VFqYT5N2fE6KQYRIIgvlpxCA7IQFGSlwCSM4Kh6gn3CfZ\n"
    "D7G7sG2fLsN1qc7WuucgIWf1yAUYxLI4vpw0d048SOJ2SRxbEpy6M6uQRPnMgQIU\n"
    "fp1b1Mv2SMYfMx/bPDUYoApjef4hM3Tw8tPFLdfLXfNW4wCjH/BkF8LK2uMorDpi\n"
    "aOCVn5XsqzeePNV82Pm0\n"
    "=jMLi\n"
    "-----END PGP SIGNATURE-----\n"
};

/* */

static void parse_sig(signature_packet_t *s, const uint8_t *sig)
{
    size_t sig_size = strlen((char*)sig);
    uint8_t *unarmored_sig = malloc((sig_size * 3) / 4 + 1);
    assert(sig[0] < 0x80); // ASCII
    int bytes = pgp_unarmor((char*)sig, sig_size, unarmored_sig, sig_size);

    assert(packet_type(unarmored_sig[0]) == SIGNATURE_PACKET);

    int header_len = packet_header_len(unarmored_sig[0]);
    assert(header_len == 1 || header_len == 2 || header_len == 4);
    assert(header_len < bytes);

    int len = scalar_number(&unarmored_sig[1], header_len);
    assert(len + header_len + 1 == bytes);

    assert(!parse_signature_packet(s, &unarmored_sig[1+header_len], len));

    assert(s->type == BINARY_SIGNATURE || s->type == TEXT_SIGNATURE);
}

static void check(public_key_t *key, public_key_t *key2, const char *data,
    signature_packet_t *sig, const uint8_t *key_longid, const uint8_t *key_longid2)
{
    uint8_t *hash;
    const char *type;

    if (data) {
        hash = hash_from_text(data, sig);
        type = "text";
    } else {
        hash = hash_from_public_key(key);
        type = "public key";
    }

    assert(hash);

    // TODO : binary file
    assert(!memcmp(hash, sig->hash_verification, 2));

    if (memcmp(sig->issuer_longid, key_longid, 8)) {
        assert(key2 && key_longid2);
        check(key, NULL, NULL, &key->sig, key_longid, NULL);
        key = key2;
    }

    assert(!verify_signature(sig, &key->key, hash));

    fprintf(stderr, "Good %s %s signature from %s (%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X)\n",
            type, gcry_md_algo_name(sig->digest_algo), key->psz_username,
            key->longid[0], key->longid[1], key->longid[2], key->longid[3],
            key->longid[4], key->longid[5], key->longid[6], key->longid[7]
            );
}

static void print_key(public_key_t *key)
{
    uint8_t *mpi;
    switch (key->key.algo) {
        case GCRY_PK_DSA: mpi = key->key.sig.dsa.p; break;
        case GCRY_PK_RSA: mpi = key->key.sig.rsa.n; break;
        default: abort();
    }

    fprintf(stderr, "Key %.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X %s %d bits\n",
            key->longid[0], key->longid[1], key->longid[2], key->longid[3],
            key->longid[4], key->longid[5], key->longid[6], key->longid[7],
            gcry_pk_algo_name(key->key.algo), scalar_number(mpi, 2));
}

int main(void)
{
    signature_packet_t sig;

    public_key_t key;
    assert(!parse_public_key(public_key, sizeof(public_key),
            &key, key_longid));
    memcpy(key.longid, key_longid, 8);
    print_key(&key);

    parse_sig(&sig, status_sign);
    check(&key, NULL, status, &sig, key_longid, NULL);

    /* */

    public_key_t key2;
    assert(!parse_public_key(public_key2, sizeof(public_key2),
            &key2, key_longid2));
    memcpy(key2.longid, key_longid2, 8);
    print_key(&key2);

    parse_sig(&sig, status_sign2);
    check(&key, &key2, status, &sig, key_longid, key_longid2);

}
