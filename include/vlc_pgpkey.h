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
 * VideoLAN Release Signing Key (2009)
 * expirates on 2010-01-01
 */

static uint8_t videolan_public_key_longid[8] = {
  0xDA, 0x51, 0x31, 0x79, 0x57, 0x88, 0x08, 0x7D
};

/* gpg --export --armor "<id>"|sed -e s/^/\"/ -e s/\$/\\\\n\"/ */
static uint8_t videolan_public_key[] = {
"-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
"Version: GnuPG v2.0.11 (GNU/Linux)\n"
"\n"
"mQGiBElzTLkRBACkdfcMqJ5dJx+u1+dj7w4vjsJUNpJjdcw12wnixBGT+gfKC66p\n"
"Iqk9XCjTPbzi82hrP7/1fTgzdbj+i2iIlfPJgrkrw9whpV1UXWQB6pLWASy6bnpL\n"
"x+igFmHcmz7wTnbvLvONayHT9D2DjFmVlIy+j0A/oZIxo/v36FqI8qXaRwCg+gzp\n"
"5pbXrAnR0WIl57ChCvqt7G8D/AsVh754hiUfubLosOH54j4AZT3GE70/a58qvYv8\n"
"Lkkj7lfGH3fhDMFqLlG6DrQxvzn56UMFQ+BHu8OS1nVWs7cLsTx0r7mvURuiahJM\n"
"wFitZ+oZ+n5VIWGkoNfIXQuu6CMKm+6pvh5GyqLoZlfGt6wXvXRbvgv6Z07tKMjX\n"
"L0RbA/9GrS+9x7eZ+KfonqHYWmxCzjll8QQIF+uR+TffYxB6eifFEMKGlffjTxCj\n"
"1i1umGzWRCARUxrLniHhHfob+vXT/bCfzU+lZPl3quHfJTWbWkAB+eKl20vm3YJR\n"
"/e5R0ftZ6C2JjyHVPU6cZYvqqQupXo8R9xIeEzwls2FJ11dmfbQjVmlkZW9MQU4g\n"
"UmVsZWFzZSBTaWduaW5nIEtleSAoMjAwOSmIZgQTEQIAJgUCSXNMuQIbAwUJAdqc\n"
"AAYLCQgHAwIEFQIIAwQWAgMBAh4BAheAAAoJENpRMXlXiAh9LAEAoKAaav5HW26z\n"
"kuLe+cuvqCz8u/faAKCjf+gRr+F/JSTIekqAjBcZgpIsP4hGBBARAgAGBQJJc07j\n"
"AAoJEP0hvDusPgh53psAnRowIxGJYKoPyrYoWiUr0skcSVHNAJ9UVhMElR7bUTQH\n"
"m/oy0gfnfLFFD4hGBBARAgAGBQJJc1D0AAoJEIsIUjHQODU31BIAnjyJBqxAXVby\n"
"QR5yj5nm35A8k2aTAJ4ovCmGablUx3Z9vJYCh/+RhvRvtrkEDQRJc0y5EBAA6z0U\n"
"siB5abYyTJxZ4ERO+2POY9xnPZmBlb5zn5/9A12gOS3ayF6QpnS8tNgE4WzSXzD8\n"
"sA6/ZyX4O1W+NiL1Uu9BYjncS0+H4UWfYf0p/DpvAReMv3HD8y1SYkV7WducrohA\n"
"6fPehuwvnnl1vNlD47GQwmwIsUcBTsBlUX4K7VwkNx0pvdQt2uYXPj3nYyQjNVPL\n"
"Kqe1cHn+vULB6UebTbBsmkE8PfcJxFYTZyrsKWr8Kw5KI+XbagLxub0MxDN1cWYu\n"
"y4EmrhGI/mAh4oSCmSAv1qihb//lW3p0IvSJ3K7AW8wjTM21uQOFFwGirdZrtJTZ\n"
"0FBJl3aHoQOzhUHq0q/vOxsX/8JFcrVgonyiac+OMd4HufqjUbcqe1vxqvQOH+Dl\n"
"L4vyu5CCOn7F8hILvqF7JMq67t8lTyzx3my3L8WuJLynj7JAWGmLRzNn8CmR67Ma\n"
"JQO9TMLYEISKMkl9VAmbNRJsUCFYSVLeUQq0DmRHjl8Vj7ss13OdbaPyEEBAOQQZ\n"
"wIyKx5HXfH5xrQ4ADJQnfTJhSNfgkCOITzV7qizzYf3+lPJfk+pCw5bzCTcJXGwj\n"
"oncZIdYedOAFb0cwRT0RiQu9o2kpi180v0EUE5ZBnDN9JlRjNPjvGAWcPeZZXmJt\n"
"RHBZQkqXjobys24lekwIl9awR5CbREFfuAEU308AAwUQAMvuOASDKuW6frOH0gr8\n"
"aWtufqz5GWaiwvn7/0LXYQujN/wIno8QJT8c5Ym6DOLZ2YtYg2uS0RnA4y3JDYeR\n"
"VDjuX4MFFQGiq/2pF1SGLi5KqmnqJ/IgrvStxBTpR1dvdTOJCwQkUO4a5Xo0mjK/\n"
"N4Uxbxik/DFy5ajpzjDQbyTX/xsjO0przUO7u9GZpnYyB+ElMlrNAl+0+5Kc44p/\n"
"mf2oajuOmqLlaGt8yLOS/XKOzPUe2ZDWAj+Eru0lue5xTAlIbl6H343FUbh3PwCB\n"
"mUI8fEQQRS+0wwPLVU7f2xCXvg363XCxNsS52mez5wk0WRrx2qVMB1kLNM2yAjLP\n"
"Y/uzYB0JSOVSgU6mydp9MUgSLBdP+fRR7NHmb2K5ua0L58rc91iCdU/HcVGk27ia\n"
"4jnyGGWBtCxgSQy1fiJXszfU7W5C3hHq99Zd6jUktg3N/WaQTNX2U6P2bZmgunVf\n"
"s0YNNS/K8x6uDzte3WGFJ9as9ZFp/KN7oj4fG1hYPNPCMINZcUSZlKnWq2wjMpVV\n"
"AH8p0/QqXOZ2r5sCpsuUeQDfheXKsdSkakSivOiAfomGxJ8sioFIIcNjIQmxRedB\n"
"7vxW/ebRilQxrEtkxOJmNwcncfBUvWFwckZuRhSV+h1ztgnilfVxhrTqDFHAhJya\n"
"5PIvCH44uLnRlIuNtG2uWuANiE8EGBECAA8FAklzTLkCGwwFCQHanAAACgkQ2lEx\n"
"eVeICH0X8QCdHFCmxQRlJZf9cbUSSzu1wyDqAw4AoL726vv4WrxM6P+QkSgLbNNA\n"
"NUNV\n"
"=qT28\n"
"-----END PGP PUBLIC KEY BLOCK-----\n"
};
