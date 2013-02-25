/*****************************************************************************
 * dvb-text.h:
 *****************************************************************************
 * Copyright (C) 2007-2011 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * Converts a DVB SI text item to UTF-8.
 * Refer to EN 800 486 annex A.
 * @return a heap-allocation nul-terminated UTF-8 string or NULL on error.
 */
static char *vlc_from_EIT (const void *buf, size_t length)
{
    if (unlikely(length == 0))
        return NULL;

    char encbuf[12];
    const char *encoding = encbuf;

    const char *in = buf;
    size_t offset = 1;
    unsigned char c = *in;

    if (c >= 0x20)
    {
        offset = 0;
        encoding = "ISO_6937";
    }
    else if ((1 << c) & 0x0EFE) /* 1-7, 9-11 -> ISO 8859-(c+4) */
    {
        snprintf (encbuf, sizeof (encbuf), "ISO_8859-%hhu", 4 + c);
    }
    else switch (c)
    {
        case 0x10: /* two more bytes */
            offset = 3;
            if (length < 3 || in[1] != 0x00)
                return NULL;

            c = in[2];
            if ((1 << c) & 0xEFFE) /* 1-11, 13-15 -> ISO 8859-(c) */
               snprintf (encbuf, sizeof (encbuf), "ISO_8859-%hhu", c);
           else
               return NULL;
           break;
        case 0x11: /* the BMP */
        case 0x14: /* Big5 subset of the BMP */
            encoding = "UCS-2BE";
            break;
        case 0x12:
            /* DVB has no clue about Korean. KS X 1001 (a.k.a. KS C 5601) is a
             * character set, not a character encoding... So we assume EUC-KR.
             * It is an encoding of KS X 1001. In practice, I guess nobody uses
             * this in any real DVB system. */
            encoding = "EUC-KR";
            break;
        case 0x13: /* GB-2312-1980 */
            encoding = "GB2312";
            break;
        case 0x15:
            encoding = "UTF-8";
            break;
#if 0
        case 0x1F: /* operator-specific(?) */
            offset = 2;
#endif
        default:
            return NULL;
    }

    in += offset;
    length -= offset;

    char *out = FromCharset (encoding, in, length);
    if (out == NULL)
    {   /* Fallback... */
        out = strndup (in, length);
        if (unlikely(out == NULL))
            return NULL;
        EnsureUTF8 (out);
    }

    /* Convert control codes */
    for (char *p = strchr (out, '\xC2'); p; p = strchr (p + 1, '\xC2'))
    {
        /* We have valid UTF-8, to 0xC2 is followed by a continuation byte. */
        /* 0x80-0x85,0x88-0x89 are reserved.
         * 0x86-0x87 are identical to Unicode and Latin-1.
         * 0x8A is CR/LF.
         * 0x8B-0x9F are unspecified. */
        if (p[1] == '\x8A')
            memcpy (p, "\r\n", 2);
    }

    /* Private use area */
    for (char *p = strchr (out, '\xEE'); p; p = strchr (p + 1, '\xEE'))
    {
        /* Within UTF-8, 0xEE is followed by a two continuation bytes. */
        if (p[1] != '\x82')
            continue;
        if (p[2] == '\x8A')
            memcpy (p, "\r\r\n", 3); /* we need three bytes, so to CRs ;) */
    }

    return out;
}
