/*****************************************************************************
 * vlc_pgpkey.h: VideoLAN PGP Public Key used to sign releases
 *****************************************************************************
 * Copyright © 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* We trust this public key, and by extension, also keys signed by it. */

/*
 * VideoLAN Release Signing Key (2008)
 * expirates on 2009-01-01
 */

static uint8_t videolan_public_key_longid[8] = {
  0x8B, 0x08, 0x52, 0x31, 0xD0, 0x38, 0x35, 0x37
};

/* gpg --export --armor "VideoLAN Release"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static uint8_t videolan_public_key[] = {
"-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
"Version: GnuPG v2.0.4 (FreeBSD)\n"
"\n"
"mQGiBEd7jcYRBAD4NRNnzqPIq6QMI6M8nmI7G569zJjy8NQNhqtuTlpqRlNqhDdt\n"
"aYcYFSBKW7YXs03BCcDNFfUpB4wexsD9z+aOTzAFs+tVmB0XyKlPc2IaMuwV9tYS\n"
"6LG2TITzWgZ5kyEtyVdDr4xvdTD1S/E2sraW/i1CgJkA/5HtgC3LksvirwCg2yQn\n"
"d+sA8KQEC66+ELV4hNn4eAsD/0ObYdZEM0B6E0hVAyabKTVYGs7MT6UjbHTaxhzV\n"
"PN6Qss1Zmm/oKA5ClNIrvSO6dqzSC+OMQwwHYizOgfObO116LWzMo+YSDyWNonRT\n"
"Ex5BtJcvyA18qbNkka79I+VYCsoLlk7pRyEc14HhMCBpR0dVl53w102RmwkXigO3\n"
"FL5kBAC4Hvy3FsV7DmwM/QccrfTDzD7SFPXnn+w5HluhCXseoiYkCSjNa8iDpG/e\n"
"AKrlwnWwEH50Q/tsD+hysnLd7dk/tGP0a4VkqcZ69pyxAql8vClBpd76udrquMKq\n"
"IFN8m2MFzkYdYSezR4yro4NLmgyri4xomjxVjboR2eXnQPUnlrQjVmlkZW9MQU4g\n"
"UmVsZWFzZSBTaWduaW5nIEtleSAoMjAwOCmIZgQTEQIAJgUCR3uNxgIbAwUJAeEz\n"
"gAYLCQgHAwIEFQIIAwQWAgMBAh4BAheAAAoJEIsIUjHQODU3xtoAoLgtA2m+qmOD\n"
"0W07hdZkqtJPW8frAJwOr4Le14j1FB6jKs8FvDsW6EL1bIhGBBARAgAGBQJHe45m\n"
"AAoJEGFgnhjAr/EP4F8AoKa8Ip/bUqk/+yASpBuKNqLZgYduAKCKqJfK4Z8zN2We\n"
"8NvZLTT66/zGxIhGBBARAgAGBQJHe46fAAoJEJAoF+SqX03m1dEAoOWl4gQsSOQG\n"
"fHfke5hAy9O2FdFDAJwOynmqM7ZAlHmvlQsUHuP1gZXGBbkEDQRHe44rEBAAzygZ\n"
"HacW1jQCOt9pI1g3ilvQYEOAosXNUV9R7c+tUySFR+t8wIwkYnUZ9WMg94oBn618\n"
"7hQHFuRoKxlinH11Elv0PvkBQPbhLq2QFX7ItAkuoVMejoZ+vUHSuJt7UNJ1YOWg\n"
"cIxOkVDkgDLl5HVbXVFU/RzKfFDr45o02NnNi8wbyIU65QFnvPNz1lLjcqQ9nTCy\n"
"8ntdW1XozQap6IFE07ZmPhNfGeMx2JlauHnZvgxORTrDjDX9o5LjTt0ubmR7Nt0x\n"
"ShXcXU+HyIAn8ZD8GmvhiDDTYJjVUnrugzBFtpyGrT8J+x1GHKNNUXfXmzw9i5jK\n"
"WWa9XxDKoyi7ktr7ZrmJBHjYinLQs1KfAFHYWw9zdjtTnx3q5kPIPnE2PVR0zkbj\n"
"tD2dPrpdbcjZ/XgiJOUVx+wcGGaYSMlPor/Wii8fJLHbp6/ZV2NzXOm0v7+uIRR+\n"
"9SfG/Tx0B88ehw8pxmPXmsgawzz3XXz+indGv9SYm/0ZQLEQrIzpsyrQk3BlCnFg\n"
"AuyDHbKzsVg+bz8u3vJ3ELls9/A9g0Aka4RoHjstm/mcDsZ7gQ5+mO0kfVydg+Rt\n"
"V2Yct3dWwxAU8JxBlkE/iQ46dllrRXGlC+x3Sn8VUZn3WpoRQHwzt+ZNtirl5VOy\n"
"jilh44FqHqvAJj+nDRu3pDITDqkpuYO5Z2MqcNsAAwYP/3p4vW/UD4xC6zLwgznx\n"
"3wZLa1/ct9BA1OKThV3NE2QswajiIRWzEdk9ZbJwkSBx8TXFYXPcfvbxOvhmdlWY\n"
"o/0HuAkShymTcfroEAsznh1qpu3jEdVMMHNCbkPRtWdealXTGzH+MH4EmkoxDxZ4\n"
"qqQjMc1YjCEOFUiuzPiJryMepQhRlZ0Vgvvzw/1A6uEFXu28KV+xehgerALNDAWe\n"
"JHKSPBoJupykEM+c/Avg83NE5AayKXVPuWlehUfxAcKZwAHxQ+HwCmUoSJiyLYBF\n"
"CFfYGiwB7WrbD65AfBDU1sVD58H+MZhbj3lT5h8PPG57PelcVPXSbKD93qIW51TN\n"
"iSxGM77hFA0fnNj3FiMRnjM9wCE5FmmK/J0pP5aAekWE4IpaklzKSl7VlDqj097o\n"
"gA5nlfEIZjqtRhxtdYHSbXV/+Yy9PxoZAGImFSNf8ZlcMw9ioC8TpXkRcxQr2iBO\n"
"YmD3NRNGnSl7lG7fDdtAnZ9BbAYUtxFMaHNrwWHlqJn+X4rZsk5CZs2oF6obkQSI\n"
"FO27OgupwFOHIUcc38RTPTZN6wTLGY/j1twBmQdVpSHsRjjtdQ0qEOXe1rZK9Nh9\n"
"unX70TDBo1Ig0CGpKqk4I8hloyjrOk6szIfOpJFlT2LTrSWbDtPE0tMdwh9fnZUL\n"
"Rt021q8MvoRxyTbTWO7Nurw0iE8EGBECAA8FAkd7jisCGwwFCQHhM4AACgkQiwhS\n"
"MdA4NTeFXwCfc0eO+gbbE+aSCMoTTxZ8ivsjlR0An3WCvfP6aTEJnzJbmpqO4AMu\n"
"FltR\n"
"=Ic/K\n"
"-----END PGP PUBLIC KEY BLOCK-----\n"
};
