/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************

/* We trust this public key, and by extension, also keys signed by it. */

/* NOTE:
 * We need a 1024 bits DSA key.
 * Don't forget to upload the key to http://download.videolan.org/pub/keys/
 */

/*
 * VideoLAN Release Signing Key (2010)
 * expirates on 2011-01-25
 */

static uint8_t videolan_public_key_longid[8] = {
  0x77, 0x72, 0xA5, 0x9D, 0x71, 0x44, 0xD4, 0x85
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static uint8_t videolan_public_key[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "Version: GnuPG v2.0.14 (GNU/Linux)\n"
    "\n"
    "mQGiBEtfSLMRBADp810mFfU9tOq5S8+HFAfCgOubfRBK36bFXb7cp1lYGjp8bEcS\n"
    "tP6Vki4vpFGpLyGO4b5SJbh8dld6lFQjhOosswrbq0xC5OkaVfehNDmzfFsnRJOm\n"
    "/fsC4G2N6yhg5gsgwd9BgEopSINr/yfm9uhb68T6KhWz/TFujBtZAEdibwCgx6qY\n"
    "65yRbP1A+XY+5PFC3CEnWoEEAN4AIYHYhg038lz95e08/VVR4UDCr6lWCL3Lvhot\n"
    "eQLCrwObxV+Qw5N4Llagc/CbSeP9UueSxJTvfSjZsU3X83lwOEd5psL7Ck5rmcfA\n"
    "Rnxvv6T/q2//H1D+OVgBGCqFZdWRos/wmnvmREVhW//jLyKHu8Hi19OO0OMt/d8E\n"
    "jWOqA/wLzLc7vE1Yh1xv4o4UhdwfrL6h336FUi6d8O3oEl6pZ/1qr29MFCOa3fw2\n"
    "nyuuKfO725CiWlkl4UXCkWpL+b9OiTwFUEL5+806DbkaB9heYMdfLM8TVhno5kAi\n"
    "K1wn9+Agm0S9lrtgO7a8qFcdWOiGdqlliTRDSWiIdxACBQ4pJbQjVmlkZW9MQU4g\n"
    "UmVsZWFzZSBTaWduaW5nIEtleSAoMjAxMCmIaAQTEQIAKAUCS19IswIbAwUJAeEz\n"
    "gAYLCQgHAwIGFQgCCQoLBBYCAwECHgECF4AACgkQd3KlnXFE1IVTcQCgpFNlNeQw\n"
    "6B0A4lN+7iW3yQfUSasAnj0yBAGaqUc7n5U4w4CDk4R8bwOAiEYEEBECAAYFAktf\n"
    "SNkACgkQ2lExeVeICH3CxgCeLxAZJxKiVK/JzjBOlZpTHwNsOgcAnjDePDBke5HR\n"
    "5ag3WmOScs6M76hhiEYEEBECAAYFAktfSQYACgkQ/SG8O6w+CHmfKQCaA7chfXSD\n"
    "AL7iPBe9mtMJnAB2QkYAn082cZyQTknI3V5ag/3+XjUcZaQV\n"
    "=9EFC\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};
