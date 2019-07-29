/* Copyright (C)2012 Xiph.Org Foundation
   File: opus_header.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "opus_header.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_codec.h>
#include "../demux/xiph.h"

/* Header contents:
  - "OpusHead" (64 bits)
  - version number (8 bits)
  - Channels C (8 bits)
  - Pre-skip (16 bits)
  - Sampling rate (32 bits)
  - Gain in dB (16 bits, S7.8)
  - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
             2..254: reserved, 255: multistream with no mapping)

  - if (mapping != 0)
     - N = totel number of streams (8 bits)
     - M = number of paired streams (8 bits)
     - C times channel origin
          - if (C<2*M)
             - stream = byte/2
             - if (byte&0x1 == 0)
                 - left
               else
                 - right
          - else
             - stream = byte-M
*/

typedef struct {
    unsigned char *data;
    int maxlen;
    int pos;
} Packet;

typedef struct {
    const unsigned char *data;
    int maxlen;
    int pos;
} ROPacket;

static int write_uint32(Packet *p, uint32_t val)
{
    if (p->pos>p->maxlen-4)
        return 0;
    p->data[p->pos  ] = (val    ) & 0xFF;
    p->data[p->pos+1] = (val>> 8) & 0xFF;
    p->data[p->pos+2] = (val>>16) & 0xFF;
    p->data[p->pos+3] = (val>>24) & 0xFF;
    p->pos += 4;
    return 1;
}

static int write_uint16(Packet *p, uint16_t val)
{
    if (p->pos>p->maxlen-2)
        return 0;
    p->data[p->pos  ] = (val    ) & 0xFF;
    p->data[p->pos+1] = (val>> 8) & 0xFF;
    p->pos += 2;
    return 1;
}

static int write_chars(Packet *p, const unsigned char *str, int nb_chars)
{
    if (p->pos>p->maxlen-nb_chars)
        return 0;
    for (int i=0;i<nb_chars;i++)
        p->data[p->pos++] = str[i];
    return 1;
}

static int read_uint32(ROPacket *p, uint32_t *val)
{
    if (p->pos>p->maxlen-4)
        return 0;
    *val =  (uint32_t)p->data[p->pos  ];
    *val |= (uint32_t)p->data[p->pos+1]<< 8;
    *val |= (uint32_t)p->data[p->pos+2]<<16;
    *val |= (uint32_t)p->data[p->pos+3]<<24;
    p->pos += 4;
    return 1;
}

static int read_uint16(ROPacket *p, uint16_t *val)
{
    if (p->pos>p->maxlen-2)
        return 0;
    *val =  (uint16_t)p->data[p->pos  ];
    *val |= (uint16_t)p->data[p->pos+1]<<8;
    p->pos += 2;
    return 1;
}

static int read_chars(ROPacket *p, unsigned char *str, int nb_chars)
{
    if (p->pos>p->maxlen-nb_chars)
        return 0;
    for (int i=0;i<nb_chars;i++)
        str[i] = p->data[p->pos++];
    return 1;
}

int opus_header_parse(const unsigned char *packet, int len, OpusHeader *h)
{
    char str[9];
    ROPacket p;
    unsigned char ch;
    uint16_t shortval;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    str[8] = 0;
    if (len<19)return 0;
    read_chars(&p, (unsigned char*)str, 8);
    if (memcmp(str, "OpusHead", 8)!=0)
        return 0;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->version = ch;
    if((h->version&240) != 0) /* Only major version 0 supported. */
        return 0;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->channels = ch;
    if (h->channels == 0)
        return 0;

    if (!read_uint16(&p, &shortval))
        return 0;
    h->preskip = shortval;

    if (!read_uint32(&p, &h->input_sample_rate))
        return 0;

    if (!read_uint16(&p, &shortval))
        return 0;
    h->gain = (short)shortval;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->channel_mapping = ch;

    if (h->channel_mapping != 0)
    {
        if (!read_chars(&p, &ch, 1))
            return 0;

        if (ch<1)
            return 0;
        h->nb_streams = ch;

        if (!read_chars(&p, &ch, 1))
            return 0;

        if (ch>h->nb_streams || (ch+h->nb_streams)>255)
            return 0;
        h->nb_coupled = ch;

        /* Multi-stream support */
        for (int i=0;i<h->channels;i++)
        {
            if (!read_chars(&p, &h->stream_map[i], 1))
                return 0;
            if (h->stream_map[i]>(h->nb_streams+h->nb_coupled) && h->stream_map[i]!=255)
                return 0;
        }
    } else {
        if(h->channels>2)
            return 0;
        h->nb_streams = 1;
        h->nb_coupled = h->channels>1;
        h->stream_map[0]=0;
        h->stream_map[1]=1;
    }
    /*For version 0/1 we know there won't be any more data
      so reject any that have data past the end.*/
    if ((h->version==0 || h->version==1) && p.pos != len)
        return 0;
    return 1;
}

/*
 Comments will be stored in the Vorbis style.
 It is described in the "Structure" section of
    http://www.xiph.org/ogg/vorbis/doc/v-comment.html

 However, Opus and other non-vorbis formats omit the "framing_bit".

The comment header is decoded as follows:
  1) [vendor_length] = unsigned little endian 32 bits integer
  2) [vendor_string] = UTF-8 vector as [vendor_length] octets
  3) [user_comment_list_length] = unsigned little endian 32 bits integer
  4) iterate [user_comment_list_length] times {
     5) [length] = unsigned little endian 32 bits integer
     6) this iteration's user comment = UTF-8 vector as [length] octets
  }
  7) done.
*/

static char *comment_init(size_t *length, const char *vendor)
{
    /*The 'vendor' field should be the actual encoding library used.*/
    if (!vendor)
        vendor = "unknown";
    size_t vendor_length = strlen(vendor);

    size_t user_comment_list_length = 0;
    size_t len = 8 + 4 + vendor_length + 4;
    char *p = malloc(len);
    if (p == NULL)
        return NULL;

    memcpy(p, "OpusTags", 8);
    SetDWLE(p + 8, vendor_length);
    memcpy(p + 12, vendor, vendor_length);
    SetDWLE(p + 12 + vendor_length, user_comment_list_length);

    *length = len;
    return p;
}

static int comment_add(char **comments, size_t *length, const char *tag,
                       const char *val)
{
    char *p = *comments;
    uint32_t vendor_length = GetDWLE(p + 8);
    size_t user_comment_list_length = GetDWLE(p + 8 + 4 + vendor_length);
    size_t tag_len = (tag ? strlen(tag) : 0);
    size_t val_len = strlen(val);
    size_t len = (*length) + 4 + tag_len + val_len;

    char *reaced = realloc(p, len);
    if (reaced == NULL)
        return 1;
    p = reaced;

    SetDWLE(p + *length, tag_len + val_len);          /* length of comment */
    if (tag) memcpy(p + *length + 4, tag, tag_len);         /* comment */
    memcpy(p + *length + 4 + tag_len, val, val_len);        /* comment */
    SetDWLE(p + 8 + 4 + vendor_length, user_comment_list_length + 1);
    *comments = p;
    *length = len;
    return 0;
}

/* adds padding so that metadata can be updated without rewriting the whole file */
static int comment_pad(char **comments, size_t *length)
{
    const unsigned padding = 512; /* default from opus-tools */

    if(SIZE_MAX - *length < padding + 255)
        return 1;

    char *p = *comments;
    /* Make sure there is at least "padding" worth of padding free, and
       round up to the maximum that fits in the current ogg segments. */
    size_t newlen = ((*length + padding) / 255 + 1) * 255 - 1;
    char *reaced = realloc(p, newlen);
    if (reaced == NULL)
        return 1;
    p = reaced;

    memset(p + *length, 0, newlen - *length);
    *comments = p;
    *length = newlen;
    return 0;
}

void opus_prepare_header(unsigned channels, unsigned rate, OpusHeader *header)
{
    header->version = 1;
    header->channels = channels;
    header->nb_streams = header->channels;
    header->nb_coupled = 0;
    header->input_sample_rate = rate;
    header->gain = 0; // 0dB
    header->channel_mapping = header->channels > 8 ? 255 :
                              header->channels > 2;
}

static int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len)
{
    Packet p;
    unsigned char ch;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    if (len<19)return 0;
    if (!write_chars(&p, (const unsigned char*)"OpusHead", 8))
        return 0;
    /* Version is 1 */
    ch = 1;
    if (!write_chars(&p, &ch, 1))
        return 0;

    ch = h->channels;
    if (!write_chars(&p, &ch, 1))
        return 0;

    if (!write_uint16(&p, h->preskip))
        return 0;

    if (!write_uint32(&p, h->input_sample_rate))
        return 0;

    if (!write_uint16(&p, h->gain))
        return 0;

    ch = h->channel_mapping;
    if (!write_chars(&p, &ch, 1))
        return 0;

    if (h->channel_mapping != 0)
    {
        ch = h->nb_streams;
        if (!write_chars(&p, &ch, 1))
            return 0;

        ch = h->nb_coupled;
        if (!write_chars(&p, &ch, 1))
            return 0;

        /* Multi-stream support */
        for (int i=0;i<h->channels;i++)
        {
            if (!write_chars(&p, &h->stream_map[i], 1))
                return 0;
        }
    }

    return p.pos;
}

int opus_write_header(uint8_t **p_extra, int *i_extra, OpusHeader *header, const char *vendor)
{
    unsigned char header_data[100];
    const int packet_size = opus_header_to_packet(header, header_data,
                                                  sizeof(header_data));

    const unsigned char *data[2];
    size_t size[2];

    data[0] = header_data;
    size[0] = packet_size;

    size_t comments_length;
    char *comments = comment_init(&comments_length, vendor);
    if (!comments)
        return 1;
    if (comment_add(&comments, &comments_length, "ENCODER=",
                    "VLC media player"))
    {
        free(comments);
        return 1;
    }

    if (comment_pad(&comments, &comments_length))
    {
        free(comments);
        return 1;
    }

    data[1] = (unsigned char *) comments;
    size[1] = comments_length;

    *i_extra = 0;
    *p_extra = NULL;

    for (unsigned i = 0; i < ARRAY_SIZE(data); ++i)
    {
        if (xiph_AppendHeaders(i_extra, (void **) p_extra, size[i], data[i]))
        {
            *i_extra = 0;
            free(*p_extra);
            *p_extra = NULL;
        }
    }

    free(comments);

    return 0;
}

