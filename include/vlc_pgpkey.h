/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************/

/* We trust this public key, and by extension, also keys signed by it. */

/* NOTE:
 * We need a 1024 bits DSA key.
 * Don't forget to upload the key to http://download.videolan.org/pub/keys/
 */

/*
 * VideoLAN Release Signing Key (2012)
 * expirates on 2013-01-31
 */

static const uint8_t videolan_public_key_longid[8] = {
  0xA7, 0x41, 0x54, 0x9F, 0x59, 0xED, 0x29, 0x87
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static const uint8_t videolan_public_key[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "Version: GnuPG v1.4.11 (GNU/Linux)\n"
    "\n"
    "mQGiBE8KGesRBACEWZkBTfHcFtG1PdGxm0FJXnrCjZPesGZwc6lkx27r6rOKmwNi\n"
    "gQLUYwSli56ut667zVswI212djfY94hKu9ZbrzoWgr+HQ8lJz0cAJgAdPBM6Zr5E\n"
    "ChIEo4MQc6aRI6SNAId72+w9GqSku9KN8iScB/Tz9UE58sHKTFdnc7gBFwCgkTlP\n"
    "4biiX1rnDXUXt/4/edfbpi8D/3bk4QN6EIqoFXQcpryX3BdLsrXAH2azpwstRiOs\n"
    "bDtK42oEE628zivZO+clQdb81+PkL7MX0vgz9EslOrmPQYOP8snhF8UoSMIQnpqx\n"
    "GafvggPXGNb6g2eGkH5SwA7MFChKO+UcrOeTrdx5PPdzMm3keWWhhd6gTO0czTMQ\n"
    "QU0MA/42NDxQM0BbloBOL1/Wj34vjWFImrruMAJBJjOseoraLORAXt9gKJJdVnBu\n"
    "GpEr4zJ98I9IS52J5EpfD6eqdYQTz8hoOUl21EOueNmdItZpR2emry6flh/9EzAR\n"
    "xeNDPfEec82WlxyDPYjfwqyhdVKtrDGz19LvkA7EX5CnoxUro7QjVmlkZW9MQU4g\n"
    "UmVsZWFzZSBTaWduaW5nIEtleSAoMjAxMimIaAQTEQIAKAIbAwYLCQgHAwIGFQgC\n"
    "CQoLBBYCAwECHgECF4AFAk8KHHoFCQIA2ecACgkQp0FUn1ntKYdgxwCeJYguCuOn\n"
    "z1HXiaCDJZHpUEs+JzYAn1hKu2OLisAar7ys0Yw5mNq5e3ERiEYEEBECAAYFAk8L\n"
    "bwAACgkQYrl/VEXBHKcZ2ACeJ41Za2kfwiAwB4y3AmqNqWZpXhQAnjUyejZlVSbE\n"
    "GLQkOVG8RXz7GLY9\n"
    "=+5Kj\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};
