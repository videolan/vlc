/**
 * @file rtpfmt.c
 */
/*****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 * Copyright © 2007-2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_aout.h> /* aout_FormatPrepare() */

#include "rtp.h"
#include "sdp.h"

/*
 * Generic packet handlers
 */

static void codec_destroy(struct vlc_rtp_pt *pt, void *data)
{
    (void) pt;
    vlc_rtp_es_destroy(data);
}

/* Send a packet to ES */
static void codec_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                         const struct vlc_rtp_pktinfo *restrict info)
{
    (void) pt; (void) info;
    block->i_dts = VLC_TICK_INVALID;
    vlc_rtp_es_send(data, block);
}

/*
 * Static payload types handler
 */

/* PT=3
 * GSM
 */
static void *gsm_init(struct vlc_rtp_pt *pt)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_GSM);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return vlc_rtp_pt_request_es(pt, &fmt);
}

static const struct vlc_rtp_pt_operations rtp_audio_gsm = {
    NULL, gsm_init, codec_destroy, codec_decode,
};

/* PT=12
 * QCELP
 */
static void *qcelp_init(struct vlc_rtp_pt *pt)
{
    es_format_t fmt;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_QCELP);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = AOUT_CHAN_CENTER;
    return vlc_rtp_pt_request_es(pt, &fmt);
}

static const struct vlc_rtp_pt_operations rtp_audio_qcelp = {
    NULL, qcelp_init, codec_destroy, codec_decode,
};

/* Not using SDP, we need to guess the payload format used */
/* see http://www.iana.org/assignments/rtp-parameters */
void rtp_autodetect(vlc_object_t *obj, rtp_session_t *session,
                    const struct vlc_rtp_pt_owner *restrict owner)
{
    char type[] = "audio", proto[] = "RTP/AVP";
    char format[] = "0 3 8 10 11 12 14";
    struct vlc_sdp_media media = {
        .type = type, .port_count = 1, .proto = proto, .format = format };

    vlc_rtp_add_media_types(obj, session, &media, owner);
    strcpy(type, "video");
    strcpy(format, "32 33");
    vlc_rtp_add_media_types(obj, session, &media, owner);
}

/*
 * Dynamic payload type handlers
 */

static struct vlc_rtp_pt *vlc_rtp_pt_create(vlc_object_t *obj,
                                            const struct vlc_sdp_pt *desc,
                                 const struct vlc_rtp_pt_owner *restrict owner)
{
    if (desc->clock_rate == 0) {
        /* Dynamic payload type not defined in the SDP */
        errno = EINVAL;
        return NULL;
    }

    struct vlc_rtp_pt *pt = malloc(sizeof (*pt));
    if (unlikely(pt == NULL))
        return NULL;

    pt->owner = *owner;
    pt->frequency = desc->clock_rate;
    pt->channel_count = desc->channel_count;

    if (vlc_rtp_pt_instantiate(obj, pt, desc) == 0)
        return pt;

    pt->ops = NULL;

    if (strcmp(desc->media->type, "audio") == 0) {
        if (strcmp(desc->name, "GSM") == 0)
            pt->ops = &rtp_audio_gsm;
        else if (strcmp(desc->name, "QCELP") == 0)
            pt->ops = &rtp_audio_qcelp;
    }

    if (pt->ops == NULL) {
        msg_Err(obj, "unsupported media type %s/%s", desc->media->type,
                desc->name);
        free(pt);
        errno = ENOTSUP;
        pt = NULL;
    }

    return pt;
}

void vlc_rtp_pt_release(struct vlc_rtp_pt *pt)
{
    if (pt->ops->release != NULL)
        pt->ops->release(pt);
    free(pt);
}

struct vlc_sdp_pt_default {
    unsigned char number;
    char subtype[6];
    unsigned char channel_count;
    unsigned int clock_rate;
};

/**
 * Sets the static payload types.
 */
static void vlc_rtp_set_default_types(struct vlc_sdp_pt *restrict types,
                                      const struct vlc_sdp_media *media)
{
    /* Implicit payload type mappings (RFC3551 section 6) */
    static const struct vlc_sdp_pt_default audio_defaults[] = {
        {  0, "PCMU",  1,  8000, },
        {  3, "GSM",   1,  8000, },
        {  4, "G723",  1,  8000, },
        {  5, "DIV4",  1,  8000, },
        {  6, "DIV4",  1, 16000, },
        {  7, "LPC",   1,  8000, },
        {  8, "PCMA",  1,  8000, },
        {  9, "G722",  1,  8000, },
        { 10, "L16",   2, 44100, },
        { 11, "L16",   1, 44100, },
        { 12, "QCELP", 1,  8000, },
        { 13, "CN",    1,  8000, },
        { 14, "MPA",   0, 90000 },
        { 15, "G728",  1,  8000, },
        { 16, "DIV4",  1, 11025, },
        { 17, "DIV4",  1, 22050, },
        { 18, "G729",  1,  8000, },
        { 33, "MP2T",  0, 90000, },
    };
    static const struct vlc_sdp_pt_default video_defaults[] = {
        { 25, "CelB",  0, 90000, },
        { 26, "JPEG",  0, 90000, },
        { 28, "nv",    0, 90000, },
        { 31, "H261",  0, 90000, },
        { 32, "MPV",   0, 90000, },
        { 33, "MP2T",  0, 90000, },
        { 34, "H263",  0, 90000, },
    };
    const struct vlc_sdp_pt_default *defs = NULL;
    size_t def_size = 0;

    if (strcmp(media->type, "audio") == 0) {
        defs = audio_defaults;
        def_size = ARRAY_SIZE(audio_defaults);
    } else if (strcmp(media->type, "video") == 0) {
        defs = video_defaults;
        def_size = ARRAY_SIZE(video_defaults);
    }

    for (size_t i = 0; i < def_size; i++) {
        const struct vlc_sdp_pt_default *def = defs + i;
        struct vlc_sdp_pt *pt = types + def->number;

        pt->media = media;
        strcpy(pt->name, def->subtype);
        pt->clock_rate = def->clock_rate;
        pt->channel_count = def->channel_count;
    }
}

/**
 * Registers all payload types declared in an SDP media.
 */
int vlc_rtp_add_media_types(vlc_object_t *obj, rtp_session_t *session,
                            const struct vlc_sdp_media *media,
                            const struct vlc_rtp_pt_owner *restrict owner)
{
    struct vlc_sdp_pt types[128] = { };

    vlc_rtp_set_default_types(types, media);

    /* Parse the a=rtpmap and extract a=fmtp lines */
    for (const struct vlc_sdp_attr *a = media->attrs; a != NULL; a = a->next) {
        if (strcmp(a->name, "rtpmap") == 0) {
            unsigned char number, channels;
            char name[16];
            unsigned int frequency;

            switch (sscanf(a->value, "%hhu %15[^/]/%u/%hhu", &number, name,
                           &frequency, &channels)) {
                case 3:
                    channels = 0;
                    /* fall through */
                case 4:
                    if (number < ARRAY_SIZE(types)) {
                        types[number].media = media;
                        strcpy(types[number].name, name);
                        types[number].clock_rate = frequency;
                        types[number].channel_count = channels;
                    }
                    break;
            }

        } else if (strcmp(a->name, "fmtp") == 0) {
            unsigned char number;
            int offset;

            if (sscanf(a->value, "%hhu %n", &number, &offset) == 1
             && number < ARRAY_SIZE(types))
                types[number].parameters = a->value + offset;
        }
    }

    const char *numbers = media->format; /* space-separated list of PTs */
    char *end;
    int errors = 0;

    for (;;) {
        unsigned long number = strtoul(numbers, &end, 10);

        if (end == numbers) {
            if (*end != '\0')
                return -EINVAL;
            break; /* garbage or end of the line */
        }

        numbers = end + strspn(end, " "); /* next PT number */

        if (number >= ARRAY_SIZE(types))
            continue;

        struct vlc_sdp_pt *const type = types + number;

        if (type->media == NULL) /* not defined or already used */
            continue;

        msg_Dbg(obj, "payload type %lu: %s/%s, %u Hz", number,
                media->type, type->name, type->clock_rate);
        if (type->channel_count > 0)
            msg_Dbg(obj, " - %hhu channel(s)", type->channel_count);
        if (type->parameters != NULL)
            msg_Dbg(obj, " - parameters: %s", type->parameters);

        struct vlc_rtp_pt *pt = vlc_rtp_pt_create(obj, type, owner);
        if (pt != NULL) {
            pt->number = number;
            if (rtp_add_type(session, pt))
                vlc_rtp_pt_release(pt);
        } else
            errors++;

        type->media = NULL; /* Prevent duplicate PT numbers. */
    }

    return errors;
}

static void es_dummy_destroy(struct vlc_rtp_es *es)
{
    assert(es == vlc_rtp_es_dummy);
}

static void es_dummy_decode(struct vlc_rtp_es *es, block_t *block)
{
    assert(es == vlc_rtp_es_dummy);
    block_Release(block);
}

static const struct vlc_rtp_es_operations vlc_rtp_es_dummy_ops = {
    es_dummy_destroy, es_dummy_decode,
};

static struct vlc_rtp_es vlc_rtp_es_dummy_instance = {
    &vlc_rtp_es_dummy_ops,
};

struct vlc_rtp_es *const vlc_rtp_es_dummy = &vlc_rtp_es_dummy_instance;
