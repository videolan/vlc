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

static int write_uint32(Packet *p, ogg_uint32_t val)
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

static int write_uint16(Packet *p, ogg_uint16_t val)
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

static int read_uint32(ROPacket *p, ogg_uint32_t *val)
{
    if (p->pos>p->maxlen-4)
        return 0;
    *val =  (ogg_uint32_t)p->data[p->pos  ];
    *val |= (ogg_uint32_t)p->data[p->pos+1]<< 8;
    *val |= (ogg_uint32_t)p->data[p->pos+2]<<16;
    *val |= (ogg_uint32_t)p->data[p->pos+3]<<24;
    p->pos += 4;
    return 1;
}

static int read_uint16(ROPacket *p, ogg_uint16_t *val)
{
    if (p->pos>p->maxlen-2)
        return 0;
    *val =  (ogg_uint16_t)p->data[p->pos  ];
    *val |= (ogg_uint16_t)p->data[p->pos+1]<<8;
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
    ogg_uint16_t shortval;

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

int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len)
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
