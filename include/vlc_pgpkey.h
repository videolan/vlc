/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************/

/* We trust this public key, and by extension, also keys signed by it. */

/* NOTE:
 * We need a 1024 bits DSA key.
 * Don't forget to upload the key to http://download.videolan.org/pub/keys/
 */

/*
 * VideoLAN Release Signing Key (2011)
 * expirates on 2012-01-30
 */

static uint8_t videolan_public_key_longid[8] = {
  0x62, 0xB9, 0x7F, 0x54, 0x45, 0xC1, 0x1C, 0xA7
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static uint8_t videolan_public_key[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "Version: GnuPG v1.4.10 (GNU/Linux)\n"
    "\n"
    "mQGiBE1FtrwRBADbKjn1vF9l6b+/mHVXMdly7Sk3jDh+lgs6nlfvrhTamhUrbCeV\n"
    "mYCfUziDppWJB4q9ohgwv3vGJeYvTklqw2+A2XNSm/XL2zWtL8enMUXnbCtTe2mD\n"
    "nA+XNOl7Ef+S5xG3MtLwCrSt4KBg4+BQs8PUIrmEUnQCGszB4zn6BMajNwCgkXbj\n"
    "lJ8ZPts8Ykuer2K3RHEe2/ED+wV+2cdfmlGphs6aIsUymd3AV2BTa3KylEQ8ccxl\n"
    "H4j6CrD2wc3ylSiRkXklh1zLalIy2KndxD8XfCrZrwbPc5GvpGcNoZsUwFunwHnW\n"
    "uApaI9LREGFNb9Oz++9sO8Qwgyw9DML0B1/paduOHdIlfVNEaRLt66RhohPm0VlA\n"
    "ReKDA/9x/TCn43IMghkij8kOUIFHTvnfONuWpRZ6DCbHWeSYg7OaRR+7grpry6b2\n"
    "cPwrhTyUXzvPs7qZnJmwBawqNisSwl5J4CcXIo068DkIn8NMRXOZP+HczY22NDkk\n"
    "02FKI+ZG5Iw7+n41LdoirGe0WvHa6W/tbsfZaT9uXj/QsRsCc7QjVmlkZW9MQU4g\n"
    "UmVsZWFzZSBTaWduaW5nIEtleSAoMjAxMSmIaAQTEQIAKAUCTUW2vAIbAwUJAeEz\n"
    "gAYLCQgHAwIGFQgCCQoLBBYCAwECHgECF4AACgkQYrl/VEXBHKcUoQCeJfYU1zm8\n"
    "z+6TkDjohUtTSVAJCgMAn0DYiSaJUUitIRj6xIlBCHTflRkM\n"
    "=T2wi\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};
