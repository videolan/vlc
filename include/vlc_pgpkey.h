/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************/

/* We trust this public key, and by extension, also keys signed by it. */

/* NOTE:
 * The embedded key is an OpenPGP v4 RSA-4096 primary key.
 * Don't forget to upload the key to http://download.videolan.org/pub/keys/
 */

/*
 * VideoLAN Release Signing Key (2026)
 * created on 2026-09-14
 */

static const uint8_t videolan_public_key_longid[8] = {
    0x1D, 0x77, 0xC0, 0xAE, 0x83, 0x5B, 0x91, 0x1E
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static const uint8_t videolan_public_key[] = {
    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
    "\n"
    "mQINBGqngYUBEADETQiomfQGsYJXlYynUZ0hrzgH5YlYq1qhXAIRrhcupN2eNUPl\n"
    "+Yx0usbWhe4+a0jizrK1Gukc8rtVq7MpgapQSfj/39Q/rjzoR/UDcE//WNAs1DhO\n"
    "A7TNkWeDCwINjKOKhypHMuqMxAo6Ax35VDc2rHW3Q0XPJJF9Pxl+SyRz4gCR43a4\n"
    "ZZZHRveJElC8aq+hyu0rirdA5VpTc4Ha2avbgBKI3cu7xUvuRH4i4yF3dJH+C4Qn\n"
    "Gf/4XqsI26FfT9OgcEh4lfhUQLAucJwXwPtfTsv6+QmwU+JyzMhfy8mYc46r3OSX\n"
    "DHr+jU39cBWMChDgNSPiTUqFU+d7fUVtUgold6orV8iB9NhqpP4yQ+b0UszDqyak\n"
    "rZsOgxedWbL6B67D804FgUUWyHjG8oNYahgLhLGDymTtmw4J7J3GE5eQV1u0K3sL\n"
    "6NaXZAnT44Knhq/iaA2rmgGwH1a58p3v7Q33n8tCcu0DI//8ytiWmJdTPooXVOCD\n"
    "txJsYht4HARQR+GiKPBl25p4Nolh4xCX3PtVJKeHWYOglEs8BCCLNcrUfky/zExa\n"
    "rT6eE/oXQ++kUC30XiW8oRquCkCfVsRr+gWuRQLbXFhDGbLdeDWuL2Z0dam/L+lr\n"
    "kJbel8m4BwMLZhrKskLiyGmLfbX72LfvrM917OOd4SKIZP4whlxfb/KJTwARAQAB\n"
    "tDlWaWRlb0xBTiBSZWxlYXNlIFNpZ25pbmcgS2V5IDIwMjYgPHZpZGVvbGFuQHZp\n"
    "ZGVvbGFuLm9yZz6JAlcEEwEKAEECGwMFCRLMAwAFCwkIBwIGFQoJCAsCBBYCAwEC\n"
    "HgECF4AWIQSjQf12hNvB/9abwlodd8Cug1uRHgUCaqkwaQIZAQAKCRAdd8Cug1uR\n"
    "HvOKD/wIHt7eerEmEda5tevwgrVRc3SpfIcNQXPVw3F66sX0YseToFz/ov8FsrK2\n"
    "WKiM4EGKsvrzySspUyvkukCUJ+J6A5ZjaFPGdFu6DXkx7opxIaSSgMESkxUgT6/F\n"
    "c0cVprnpnXU0hugjVQFpkvFE2+wivAiyNliOOWaGiCAl0adrM7k0Q+BJZ38LIgYR\n"
    "ZYRD7g7zjWRHBvosboPGLsmjPvz3SmG/h+dgPgK1ABvHDRUgTfsoRCDfrmhCYpsv\n"
    "lxAo+070OIiI6yyi1+lVKdtdqidzsjsel+L9InvsEvmvPj7b0eeMwx4niXOWqLHM\n"
    "9KFG8NLIZ9kCr3XtQ8CA2WYqNzjlvJhetcOzOE+ZdNsbvWMKrRVhYCxWV9+7Ez8u\n"
    "IS35xcDhWyOdxmYLzfzxHDA0QOOQC+BJGVdUlnOyzA2XcZ0/0Ajqxl7ToPcvR3ga\n"
    "+tGqywHjetATbQBfZg8Sro2AxJtq/UKfjTAi9MeAER52Pz7SuTVNvMkjSjlTpdgd\n"
    "1pCFQh8afEJ0ZZAKo63GxE4PtmUCuGH04zr1xD91sQHTLgjJOqL+2gVdzs+Li2xZ\n"
    "JXim5XH2g3tcQb2bCdNSp78b216nqa9S9DfQsIHez2dp+cJ+vot6bKglD/sqsPNm\n"
    "gtlZ8hRR13f++XgRyJW8CWEqOuyU5thd1QKpaQkjhJQwogBMTw==\n"
    "=e7qD\n"
    "-----END PGP PUBLIC KEY BLOCK-----\n"
};
