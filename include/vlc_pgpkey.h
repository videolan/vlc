/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************/

/* We trust this public key, and by extension, also keys signed by it. */

/* NOTE:
 * We need a 1024 bits DSA key.
 * Don't forget to upload the key to http://download.videolan.org/pub/keys/
 */

/*
 * VideoLAN Release Signing Key (2013)
 * expires on 2014-02-03
 */

static const uint8_t videolan_public_key_longid[8] = {
  0x71, 0x80, 0x71, 0x3B, 0xE5, 0x8D, 0x1A, 0xDC
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static const uint8_t videolan_public_key[] = {
  "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
  "Version: GnuPG v2.0.19 (GNU/Linux)\n"
  "\n"
  "mQGiBFD9w2QRBACoEzH9KKirWE4wgiuPPynNnxks+p+t5i1z3CG+1XhagmTHoOf3\n"
  "v8i19kKHV6WnVMn2CKJFgwTTLYXOJTrBM/4ABVtu11cHeeueeo+pCSkdoLzYJ5QF\n"
  "HbByB6j33QUbwKF0frEs+ge4LxzvYyCDAmNAW560QtOAR9Lk1Fo5B1GXzwCg1kDk\n"
  "RkSe7EOZNm1U2rYAQ2VPrfsEAIHr4ooOyUByPR7XpoDOKoaXEG0hjpgh46lbgse+\n"
  "dQx8YrxS9vXQLwYokfWLrs55avx9Ys0iVv2TMv7X4Tn5sTVaK5K+NbKhxhLORxGI\n"
  "sgKqRn7W5SG5xoO0w/dmQj756ppjITGbxjFuhYE0X5S6NeMhUuFci7sJ42R7F1Ko\n"
  "6sYuA/wOMUxCk4XOXeQF16ApyyenjE/UWbBNEhBmjEsZkYAFNc89pAEnEFSnIxK8\n"
  "fcuCQioM6ojjaW+aEs/q3/klI0nat9LMLhNSCebjriMHwJDU70NeCn4nPWsfItT1\n"
  "eKvbHNcX+3bq3D/i2Wa3PZ5YFFF01C61dHmVC9YGh4sAOXO09LQjVmlkZW9MQU4g\n"
  "UmVsZWFzZSBTaWduaW5nIEtleSAoMjAxMymIaAQTEQIAKAUCUP3DZAIbAwUJAfJX\n"
  "AAYLCQgHAwIGFQgCCQoLBBYCAwECHgECF4AACgkQcYBxO+WNGtwQQwCg1JX6dDn5\n"
  "gMpV4oczkpwlj5noOQwAn0HdTOxfmefXNQF1x+Gt9BXBYXrNiEYEEBECAAYFAlD9\n"
  "xH4ACgkQp0FUn1ntKYdB+gCfS641cDBN2rOKf/+Fra/p3bXgAeAAn2sJtSdN07Dv\n"
  "rZeDWEbkhT620YOS\n"
  "=Npzf\n"
  "-----END PGP PUBLIC KEY BLOCK-----\n"
};
