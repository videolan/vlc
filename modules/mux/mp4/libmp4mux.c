/*****************************************************************************
 * libmp4mux.c: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006, 20115 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin at videolan dot org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libmp4mux.h"
#include "../../demux/mp4/libmp4.h" /* flags */
#include "../../packetizer/hevc_nal.h"
#include "../../packetizer/h264_nal.h" /* h264_AnnexB_get_spspps */
#include "../../packetizer/hxxx_nal.h"
#include "../../packetizer/iso_color_tables.h"

#include <vlc_es.h>
#include <vlc_iso_lang.h>
#include <vlc_bits.h>
#include <vlc_arrays.h>
#include <vlc_text_style.h>
#include <assert.h>
#include <time.h>

struct mp4mux_trackinfo_t
{
    unsigned i_track_id;
    es_format_t   fmt;

    /* index */
    unsigned int i_samples_count;
    unsigned int i_samples_max;
    mp4mux_sample_t *samples;

    /* XXX: needed for other codecs too, see lavf */
    struct
    {
        size_t   i_data;
        uint8_t *p_data;
    } sample_priv;

    /* stats */
    vlc_tick_t   i_read_duration;
    uint32_t     i_timescale;
    bool         b_hasbframes;
    enum mp4mux_interlacing e_interlace;

    /* frags */
    vlc_tick_t   i_trex_default_length;
    uint32_t     i_trex_default_size;

    /* edit list */
    unsigned int i_edits_count;
    mp4mux_edit_t *p_edits;

};

struct mp4mux_handle_t
{
    unsigned options;
    vlc_array_t tracks;
    struct
    {
        vlc_fourcc_t i_major;
        uint32_t     i_minor;
        DECL_ARRAY(vlc_fourcc_t) extra;
    } brands;
};

static void mp4mux_AddExtraBrandForFormat(mp4mux_handle_t *h, const es_format_t *fmt)
{
    switch(fmt->i_codec)
    {
        case VLC_CODEC_H264:
            mp4mux_AddExtraBrand(h, BRAND_avc1);
            break;
        case VLC_CODEC_HEVC:
            mp4mux_AddExtraBrand(h, BRAND_hevc);
            break;
        case VLC_CODEC_AV1:
            mp4mux_AddExtraBrand(h, BRAND_av01);
            mp4mux_AddExtraBrand(h, BRAND_iso6);
            break;
        case VLC_CODEC_MP3:
        case VLC_CODEC_MPGA:
        case VLC_CODEC_MP4V:
        case VLC_CODEC_DIV1:
        case VLC_CODEC_DIV2:
        case VLC_CODEC_DIV3:
        case VLC_CODEC_H263:
            mp4mux_AddExtraBrand(h, BRAND_mp41);
            break;
        case VLC_CODEC_MP4A:
            mp4mux_AddExtraBrand(h, BRAND_mp41);
            if(vlc_array_count(&h->tracks) == 1)
                mp4mux_AddExtraBrand(h, BRAND_M4A);
            break;
        default:
            break;
    }
}

static bool mp4mux_trackinfo_Init(mp4mux_trackinfo_t *p_stream, unsigned i_id,
                                  uint32_t i_timescale)
{
    memset(p_stream, 0, sizeof(*p_stream));
    p_stream->i_track_id = i_id;

    p_stream->i_timescale   = i_timescale;
    es_format_Init(&p_stream->fmt, 0, 0);

    return true;
}

static void mp4mux_trackinfo_Clear(mp4mux_trackinfo_t *p_stream)
{
    es_format_Clean(&p_stream->fmt);
    mp4mux_track_SetSamplePriv(p_stream, NULL, 0);
    free(p_stream->samples);
    free(p_stream->p_edits);
}

mp4mux_trackinfo_t * mp4mux_track_Add(mp4mux_handle_t *h, unsigned id,
                                      const es_format_t *fmt, uint32_t timescale)
{
    if(unlikely(id == 0))
        return NULL;
    mp4mux_trackinfo_t *t = malloc(sizeof(*t));
    if(!t || !mp4mux_trackinfo_Init(t, 0, 0))
    {
        free(t);
        return NULL;
    }
    t->i_track_id = id;
    t->i_timescale = timescale;
    es_format_Init(&t->fmt, fmt->i_cat, fmt->i_codec);
    es_format_Copy(&t->fmt, fmt);
    vlc_array_append(&h->tracks, t);
    mp4mux_AddExtraBrandForFormat(h, fmt);
    return t;
}

bool mp4mux_track_AddEdit(mp4mux_trackinfo_t *t, const mp4mux_edit_t *p_newedit)
{
    if(t->i_edits_count + 1 < t->i_edits_count)
        return false;
    mp4mux_edit_t *p_realloc = vlc_reallocarray( t->p_edits,
                                                 t->i_edits_count + 1,
                                                 sizeof(*p_realloc) );
    if(unlikely(!p_realloc))
        return false;

    t->p_edits = p_realloc;
    t->p_edits[t->i_edits_count++] = *p_newedit;

    return true;
}

const mp4mux_edit_t *mp4mux_track_GetLastEdit(const mp4mux_trackinfo_t *t)
{
    if(t->i_edits_count)
        return &t->p_edits[t->i_edits_count - 1];
    else return NULL;
}

void mp4mux_track_DebugEdits(vlc_object_t *obj, const mp4mux_trackinfo_t *t)
{
    for( unsigned i=0; i<t->i_edits_count; i++ )
    {
        msg_Dbg(obj, "tk %d elst media time %" PRId64 " duration %" PRIu64 " offset %" PRId64 ,
                t->i_track_id,
                t->p_edits[i].i_start_time,
                t->p_edits[i].i_duration,
                t->p_edits[i].i_start_offset);
    }
}

bool mp4mux_track_AddSample(mp4mux_trackinfo_t *t, const mp4mux_sample_t *entry)
{
    /* XXX: -1 to always have 2 entry for easy adding of empty SPU */
    if (t->i_samples_count + 2 >= t->i_samples_max)
    {
        if(t->i_samples_max + 1000 < t->i_samples_max)
            return false;
        mp4mux_sample_t *p_realloc = vlc_reallocarray(t->samples,
                                                      t->i_samples_max + 1000,
                                                      sizeof(*p_realloc));
        if(!p_realloc)
            return false;
        t->samples = p_realloc;
        t->i_samples_max += 1000;
    }
    t->samples[t->i_samples_count++] = *entry;
    if(!t->b_hasbframes && entry->i_pts_dts != 0)
        t->b_hasbframes = true;
    t->i_read_duration += __MAX(0, entry->i_length);
    return true;
}

const mp4mux_sample_t *mp4mux_track_GetLastSample(const mp4mux_trackinfo_t *t)
{
    if(t->i_samples_count)
        return &t->samples[t->i_samples_count - 1];
    else return NULL;
}

unsigned mp4mux_track_GetSampleCount(const mp4mux_trackinfo_t *t)
{
    return t->i_samples_count;
}

void mp4mux_track_UpdateLastSample(mp4mux_trackinfo_t *t,
                                   const mp4mux_sample_t *entry)
{
    if(t->i_samples_count)
    {
        mp4mux_sample_t *e = &t->samples[t->i_samples_count - 1];
        t->i_read_duration -= e->i_length;
        t->i_read_duration += entry->i_length;
        *e = *entry;
    }
}

vlc_tick_t mp4mux_track_GetDefaultSampleDuration(const mp4mux_trackinfo_t *t)
{
    return t->i_trex_default_length;
}

uint32_t mp4mux_track_GetDefaultSampleSize(const mp4mux_trackinfo_t *t)
{
    return t->i_trex_default_size;
}

const es_format_t * mp4mux_track_GetFmt(const mp4mux_trackinfo_t *t)
{
    return &t->fmt;
}

bool mp4mux_track_HasBFrames(const mp4mux_trackinfo_t *t)
{
    return t->b_hasbframes;
}

void mp4mux_track_SetHasBFrames(mp4mux_trackinfo_t *t)
{
    t->b_hasbframes = true;
}

uint32_t mp4mux_track_GetTimescale(const mp4mux_trackinfo_t *t)
{
    return t->i_timescale;
}

vlc_tick_t mp4mux_track_GetDuration(const mp4mux_trackinfo_t *t)
{
    return t->i_read_duration;
}

void mp4mux_track_ForceDuration(mp4mux_trackinfo_t *t, vlc_tick_t d)
{
    t->i_read_duration = d;
}

uint32_t mp4mux_track_GetID(const mp4mux_trackinfo_t *t)
{
    return t->i_track_id;
}

void mp4mux_track_SetInterlacing(mp4mux_trackinfo_t *t, enum mp4mux_interlacing i)
{
    t->e_interlace = i;
}

enum mp4mux_interlacing mp4mux_track_GetInterlacing(const mp4mux_trackinfo_t *t)
{
    return t->e_interlace;
}

void mp4mux_track_SetSamplePriv(mp4mux_trackinfo_t *t,
                                const uint8_t *p_data, size_t i_data)
{
    if(t->sample_priv.p_data)
    {
        free(t->sample_priv.p_data);
        t->sample_priv.p_data = NULL;
        t->sample_priv.i_data = 0;
    }

    if(p_data && i_data)
    {
        t->sample_priv.p_data = malloc(i_data);
        if(i_data)
        {
            memcpy(t->sample_priv.p_data, p_data, i_data);
            t->sample_priv.i_data = i_data;
        }
    }
}

bool mp4mux_track_HasSamplePriv(const mp4mux_trackinfo_t *t)
{
    return t->sample_priv.i_data != 0;
}

void mp4mux_ShiftSamples(mp4mux_handle_t *h, int64_t offset)
{
    for(size_t i_track = 0; i_track < vlc_array_count(&h->tracks); i_track++)
    {
        mp4mux_trackinfo_t *t = vlc_array_item_at_index(&h->tracks, i_track);
        for (unsigned i = 0; i < t->i_samples_count; i++)
        {
            mp4mux_sample_t *sample = t->samples;
            sample[i].i_pos += offset;
        }
    }
}

mp4mux_handle_t * mp4mux_New(enum mp4mux_options options)
{
    mp4mux_handle_t *h = malloc(sizeof(*h));
    vlc_array_init(&h->tracks);
    ARRAY_INIT(h->brands.extra);
    h->brands.i_major = 0;
    h->brands.i_minor = 0;
    h->options = options;
    return h;
}

void mp4mux_Delete(mp4mux_handle_t *h)
{
    for(size_t i=0; i<vlc_array_count(&h->tracks); i++)
    {
        mp4mux_trackinfo_t *t = vlc_array_item_at_index(&h->tracks, i);
        mp4mux_trackinfo_Clear(t);
        free(t);
    }
    vlc_array_clear(&h->tracks);
    ARRAY_RESET(h->brands.extra);
    free(h);
}

void mp4mux_Set64BitExt(mp4mux_handle_t *h)
{
    /* FIXME FIXME
     * Quicktime actually doesn't like the 64 bits extensions !!! */
    if(h->options & QUICKTIME)
        return;

    h->options |= USE64BITEXT;
}

bool mp4mux_Is(mp4mux_handle_t *h, enum mp4mux_options o)
{
    return h->options & o;
}

void mp4mux_SetBrand(mp4mux_handle_t *h, vlc_fourcc_t i_major, uint32_t i_minor)
{
    h->brands.i_major = i_major;
    h->brands.i_minor = i_minor;
    mp4mux_AddExtraBrand(h, i_major);
}

void mp4mux_AddExtraBrand(mp4mux_handle_t *h, vlc_fourcc_t b)
{
    for(int i=0; i<h->brands.extra.i_size; i++)
        if(h->brands.extra.p_elems[i] == b)
            return;
    ARRAY_APPEND(h->brands.extra, b);
}

bo_t *box_new(const char *fcc)
{
    bo_t *box = malloc(sizeof(*box));
    if (!box)
        return NULL;

    if(!bo_init(box, 1024))
    {
        bo_free(box);
        return NULL;
    }

    bo_add_32be  (box, 0);
    bo_add_fourcc(box, fcc);

    return box;
}

bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f)
{
    bo_t *box = box_new(fcc);
    if (!box)
        return NULL;

    bo_add_8     (box, v);
    bo_add_24be  (box, f);

    return box;
}

void box_fix(bo_t *box, uint32_t i_size)
{
    bo_set_32be(box, 0, i_size);
}

void box_gather (bo_t *box, bo_t *box2)
{
    if(box2 && box2->b && box && box->b)
    {
        box_fix(box2, bo_size( box2 ));
        size_t i_offset = bo_size( box );
        box->b = block_Realloc(box->b, 0, box->b->i_buffer + box2->b->i_buffer);
        if(likely(box->b))
            memcpy(&box->b->p_buffer[i_offset], box2->b->p_buffer, box2->b->i_buffer);
    }
    bo_free(box2);
}

static inline void bo_add_mp4_tag_descr(bo_t *box, uint8_t tag, uint32_t size)
{
    bo_add_8(box, tag);
    for (int i = 3; i>0; i--)
        bo_add_8(box, (size>>(7*i)) | 0x80);
    bo_add_8(box, size & 0x7F);
}

static int64_t get_timestamp(void)
{
    int64_t i_timestamp = time(NULL);

    i_timestamp += 2082844800; // MOV/MP4 start date is 1/1/1904
    // 208284480 is (((1970 - 1904) * 365) + 17) * 24 * 60 * 60

    return i_timestamp;
}

/****************************************************************************/

static void matrix_apply_rotation(es_format_t *fmt, uint32_t mvhd_matrix[9])
{
    enum video_orientation_t orientation = ORIENT_NORMAL;
    if (fmt->i_cat == VIDEO_ES)
        orientation = fmt->video.orientation;

#define ATAN(a, b) \
    do { \
        mvhd_matrix[1] = ((uint32_t)(a)) << 16; \
        mvhd_matrix[0] = ((uint32_t)(b)) << 16; \
    } while(0)

    switch (orientation) {
    case ORIENT_ROTATED_90:  ATAN( 1,  0); break;
    case ORIENT_ROTATED_180: ATAN( 0, -1); break;
    case ORIENT_ROTATED_270: ATAN(-1,  0); break;
    default:                 ATAN( 0,  1); break;
    }

    mvhd_matrix[3] = mvhd_matrix[0] ? 0 : 0x10000;
    mvhd_matrix[4] = mvhd_matrix[1] ? 0 : 0x10000;
}

static void AddEdit(bo_t *elst,
                    int64_t i_movie_scaled_duration,
                    int64_t i_media_scaled_time,
                    bool b_64_ext)
{
    if(b_64_ext)
    {
        bo_add_64be(elst, i_movie_scaled_duration);
        bo_add_64be(elst, i_media_scaled_time);
    }
    else
    {
        bo_add_32be(elst, i_movie_scaled_duration);
        bo_add_32be(elst, i_media_scaled_time);
    }
    bo_add_16be(elst, 1);
    bo_add_16be(elst, 0);
}

static bo_t *GetEDTS( mp4mux_trackinfo_t *p_track, uint32_t i_movietimescale, bool b_64_ext)
{
    if(p_track->i_edits_count == 0)
        return NULL;

    bo_t *edts = box_new("edts");
    bo_t *elst = box_full_new("elst", b_64_ext ? 1 : 0, 0);
    if(!elst || !edts)
    {
        bo_free(elst);
        bo_free(edts);
        return NULL;
    }

    uint32_t i_total_edits = p_track->i_edits_count;
    for(unsigned i=0; i<p_track->i_edits_count; i++)
    {
        /* !WARN! media time must start sample time 0, we need a -1 edit for start offsets */
        if(p_track->p_edits[i].i_start_offset != 0)
            i_total_edits++;
    }

    bo_add_32be(elst, i_total_edits);

    for(unsigned i=0; i<p_track->i_edits_count; i++)
    {
        if(p_track->p_edits[i].i_start_offset != 0)
        {
            AddEdit(elst,
                    samples_from_vlc_tick(p_track->p_edits[i].i_start_offset, i_movietimescale),
                    -1,
                    b_64_ext);
        }

        /* !WARN AGAIN! Uses different Timescales ! */
        AddEdit(elst,
                samples_from_vlc_tick(p_track->p_edits[i].i_duration, i_movietimescale),
                samples_from_vlc_tick(p_track->p_edits[i].i_start_time, p_track->i_timescale),
                b_64_ext);
    }

    box_gather(edts, elst);
    return edts;
}

static bo_t *GetESDS(mp4mux_trackinfo_t *p_track)
{
    bo_t *esds;
    const uint8_t *p_extradata = NULL;
    int i_extradata = 0;
    uint8_t *p_extradata_allocated = NULL;

    switch(p_track->fmt.i_codec)
    {
        case VLC_CODEC_SPU:
            if(p_track->fmt.subs.spu.palette[0] == SPU_PALETTE_DEFINED)
            {
#ifndef WORDS_BIGENDIAN
                p_extradata = p_extradata_allocated = malloc(16*4);
                if(p_extradata_allocated)
                {
                    for(int i=0; i<16; i++)
                        SetDWBE(&p_extradata_allocated[i*4], p_track->fmt.subs.spu.palette[i+1]);
                    i_extradata = 16*4;
                }
#else
                p_extradata = (const uint8_t *) &p_track->fmt.subs.spu.palette[1];
                i_extradata = 16 * sizeof(p_track->fmt.subs.spu.palette[1]);
#endif
            }
            break;
        default:
            p_extradata = p_track->fmt.p_extra;
            i_extradata = p_track->fmt.i_extra;
            break;
    }

    /* */
    int i_decoder_specific_info_size = (i_extradata > 0) ? 5 + i_extradata : 0;

    esds = box_full_new("esds", 0, 0);
    if(!esds)
    {
        free(p_extradata_allocated);
        return NULL;
    }

    /* Compute Max bitrate */
    int64_t i_bitrate_avg = 0;
    int64_t i_bitrate_max = 0;
    /* Compute avg/max bitrate */
    for (unsigned i = 0; i < p_track->i_samples_count; i++) {
        i_bitrate_avg += p_track->samples[i].i_size;
        if (p_track->samples[i].i_length > 0) {
            int64_t i_bitrate = CLOCK_FREQ * 8 * p_track->samples[i].i_size / p_track->samples[i].i_length;
            if (i_bitrate > i_bitrate_max)
                i_bitrate_max = i_bitrate;
        }
    }

    if (p_track->i_read_duration > 0)
        i_bitrate_avg = CLOCK_FREQ * 8 * i_bitrate_avg / p_track->i_read_duration;
    else
        i_bitrate_avg = 0;
    if (i_bitrate_max <= 1)
        i_bitrate_max = 0x7fffffff;

    /* ES_Descriptor ISO/IEC 14496-1 */
    bo_add_mp4_tag_descr(esds, 0x03, 3 + 5 + 13 + i_decoder_specific_info_size + 5 + 1);
    bo_add_16be(esds, p_track->i_track_id);
    bo_add_8   (esds, 0x1f);      // flags=0|streamPriority=0x1f

    /* DecoderConfigDescr ISO/IEC 14496-1
        http://mp4ra.org/#/object_types */
    bo_add_mp4_tag_descr(esds, 0x04, 13 + i_decoder_specific_info_size);

    uint8_t i_object_profile_indication;
    switch(p_track->fmt.i_codec)
    {
    case VLC_CODEC_MP4V:
        i_object_profile_indication = 0x20; /* Visual 14496-2 */
        break;
    case VLC_CODEC_MPGV:
        if(p_track->fmt.i_original_fourcc == VLC_CODEC_MP1V)
        {
            i_object_profile_indication = 0x6a; /* Visual ISO/IEC 11172-2  */
            break;
        }
        /* fallthrough */
    case VLC_CODEC_MP2V:
        /* MPEG-I=0x6b, MPEG-II = 0x60 -> 0x65 */
        i_object_profile_indication = 0x61; /* Visual 13818-2 Main Profile */
        break;
    case VLC_CODEC_MP1V:
        i_object_profile_indication = 0x6a; /* Visual ISO/IEC 11172-2  */
        break;
    case VLC_CODEC_MP4A:
        /* FIXME for mpeg2-aac == 0x66->0x68 */
        i_object_profile_indication = 0x40; /* Audio 14496-3 */
        break;
    case VLC_CODEC_MP3:
    case VLC_CODEC_MPGA:
        i_object_profile_indication =
            p_track->fmt.audio.i_rate < 32000 ? 0x69 /* Audio 13818-3 */
                                              : 0x6b;/* Audio 11172-3 */
        break;
    case VLC_CODEC_DTS:
        i_object_profile_indication = 0xa9; /* Core Substream */
        break;
    case VLC_CODEC_SPU:
        i_object_profile_indication = 0xe0; /* NeroDigital SPU mapping */
        break;
    default:
        i_object_profile_indication = 0xFE; /* No profile specified */
        break;
    }

    uint8_t i_stream_type;
    switch(p_track->fmt.i_cat)
    {
        case VIDEO_ES:
            i_stream_type = 0x04;
            break;
        case AUDIO_ES:
            i_stream_type = 0x05;
            break;
        case SPU_ES:
            i_stream_type = 0x0D;
            break;
        default:
            i_stream_type = 0x20; /* Private */
            break;
    }

    bo_add_8   (esds, i_object_profile_indication);
    bo_add_8   (esds, (i_stream_type << 2) | 1);
    bo_add_24be(esds, 1024 * 1024);       // bufferSizeDB
    bo_add_32be(esds, i_bitrate_max);     // maxBitrate
    bo_add_32be(esds, i_bitrate_avg);     // avgBitrate

    if (i_extradata > 0) {
        /* DecoderSpecificInfo */
        bo_add_mp4_tag_descr(esds, 0x05, i_extradata);

        for (int i = 0; i < i_extradata; i++)
            bo_add_8(esds, p_extradata[i]);

        free(p_extradata_allocated);
    }

    /* SL_Descr mandatory */
    bo_add_mp4_tag_descr(esds, 0x06, 1);
    bo_add_8    (esds, 0x02);  // sl_predefined

    return esds;
}

static bo_t *GetEndaTag(bool b_le)
{
    bo_t *enda = box_new("enda");
    if(enda)
        bo_add_16be(enda, !!b_le);
    return enda;
}

static bo_t *GetFrmaTag(const char format[4])
{
    bo_t *frma = box_new("frma");
    if(frma)
        bo_add_mem(frma, 4, format);
    return frma;
}

static bo_t *GetWaveTag(const char *fcc, bo_t *extraboxes[], size_t i_extraboxes)
{
    bo_t *wave;
    bo_t *box;

    wave = box_new("wave");
    if(wave)
    {
        box = GetFrmaTag(fcc);
        box_gather(wave, box);

        for(size_t i=0; i<i_extraboxes; i++)
            box_gather(wave, extraboxes[i]);

        /* The last atom in the siDecompressionParam atom must be a terminator atom. */
        bo_add_32be(wave, 8); /* new empty box */
        bo_add_32be(wave, 0); /* box label */
    }
    return wave;
}

static bo_t *GetDamrTag(es_format_t *p_fmt)
{
    bo_t *damr = box_new("damr");
    if(!damr)
        return NULL;

    bo_add_fourcc(damr, "REFC");
    bo_add_8(damr, 0);

    if (p_fmt->i_codec == VLC_CODEC_AMR_NB)
        bo_add_16be(damr, 0x81ff); /* Mode set (all modes for AMR_NB) */
    else
        bo_add_16be(damr, 0x83ff); /* Mode set (all modes for AMR_WB) */
    bo_add_16be(damr, 0x1); /* Mode change period (no restriction) */

    return damr;
}

static bo_t *GetD263Tag(void)
{
    bo_t *d263 = box_new("d263");
    if(!d263)
        return NULL;

    bo_add_fourcc(d263, "VLC ");
    bo_add_16be(d263, 0xa);
    bo_add_8(d263, 0);

    return d263;
}

static bo_t *GetHvcCTag(const uint8_t *p_extra, size_t i_extra,
                        bool b_completeness)
{

    /* Generate hvcC box matching iso/iec 14496-15 3rd edition */
    bo_t *hvcC = box_new("hvcC");
    if(!hvcC || !i_extra)
        return hvcC;

    /* Extradata is already an HEVCDecoderConfigurationRecord */
    if(hevc_ishvcC(p_extra, i_extra))
    {
        (void) bo_add_mem(hvcC, i_extra, p_extra);
        return hvcC;
    }

    struct hevc_dcr_params params = { };
    const uint8_t *p_nal;
    size_t i_nal;

    hxxx_iterator_ctx_t it;
    hxxx_iterator_init(&it, p_extra, i_extra, 0);
    while(hxxx_annexb_iterate_next(&it, &p_nal, &i_nal))
    {
        switch (hevc_getNALType(p_nal))
        {
            case HEVC_NAL_VPS:
                if(params.i_vps_count != HEVC_DCR_VPS_COUNT)
                {
                    params.p_vps[params.i_vps_count] = p_nal;
                    params.rgi_vps[params.i_vps_count] = i_nal;
                    params.i_vps_count++;
                }
                break;
            case HEVC_NAL_SPS:
                if(params.i_sps_count != HEVC_DCR_SPS_COUNT)
                {
                    params.p_sps[params.i_sps_count] = p_nal;
                    params.rgi_sps[params.i_sps_count] = i_nal;
                    params.i_sps_count++;
                }
                break;
            case HEVC_NAL_PPS:
                if(params.i_pps_count != HEVC_DCR_PPS_COUNT)
                {
                    params.p_pps[params.i_pps_count] = p_nal;
                    params.rgi_pps[params.i_pps_count] = i_nal;
                    params.i_pps_count++;
                }
                break;
            case HEVC_NAL_PREF_SEI:
                if(params.i_seipref_count != HEVC_DCR_SEI_COUNT)
                {
                    params.p_seipref[params.i_seipref_count] = p_nal;
                    params.rgi_seipref[params.i_seipref_count] = i_nal;
                    params.i_seipref_count++;
                }
                break;
            case HEVC_NAL_SUFF_SEI:
                if(params.i_seisuff_count != HEVC_DCR_SEI_COUNT)
                {
                    params.p_seisuff[params.i_seisuff_count] = p_nal;
                    params.rgi_seisuff[params.i_seisuff_count] = i_nal;
                    params.i_seisuff_count++;
                }
                break;

            default:
                break;
        }
    }

    size_t i_dcr;
    uint8_t *p_dcr = hevc_create_dcr(&params, 4, b_completeness, &i_dcr);
    if(!p_dcr)
    {
        bo_free(hvcC);
        return NULL;
    }

    bo_add_mem(hvcC, i_dcr, p_dcr);
    free(p_dcr);

    return hvcC;
}

static bo_t *GetWaveFormatExTag(es_format_t *p_fmt, const char *tag)
{
    bo_t *box = box_new(tag);
    if(!box)
        return NULL;

    uint16_t wFormatTag;
    fourcc_to_wf_tag(p_fmt->i_codec, &wFormatTag);
    bo_add_16le(box, wFormatTag); //wFormatTag
    bo_add_16le(box, p_fmt->audio.i_channels); //nChannels
    bo_add_32le(box, p_fmt->audio.i_rate); //nSamplesPerSec
    bo_add_32le(box, p_fmt->i_bitrate / 8); //nAvgBytesPerSec
    bo_add_16le(box, p_fmt->audio.i_blockalign); //nBlockAlign
    bo_add_16le(box, p_fmt->audio.i_bitspersample);  //wBitsPerSample
    bo_add_16le(box, p_fmt->i_extra); //cbSize

    bo_add_mem(box, p_fmt->i_extra, p_fmt->p_extra);

    return box;
}

static bo_t *GetxxxxTag(const uint8_t *p_extra, size_t i_extra,
                        const char *tag)
{
    bo_t *box = box_new(tag);
    if(!box)
        return NULL;
    bo_add_mem(box, i_extra, p_extra);
    return box;
}

static bo_t *GetFielBox(enum mp4mux_interlacing i)
{
    bo_t *p_box = box_new("fiel");
    if(p_box)
    {
        const uint16_t values[] =
        {
            [INTERLACING_NONE]         = 0x0100,
            [INTERLACING_SINGLE_FIELD] = 0x0200,
            [INTERLACING_TOPBOTTOM]    = 0x0201,
            [INTERLACING_BOTTOMTOP   ] = 0x0206,
        };
        bo_add_16be(p_box, values[i]);
    }
    return p_box;
}

static bo_t *GetColrBox(const video_format_t *p_vfmt, bool b_mov)
{
    bo_t *p_box = box_new("colr");
    if(p_box)
    {
        bo_add_mem(p_box, 4, b_mov ? "nclc" : "nclx");
        bo_add_16be(p_box, vlc_primaries_to_iso_23001_8_cp(p_vfmt->primaries));
        bo_add_16be(p_box, vlc_xfer_to_iso_23001_8_tc(p_vfmt->transfer));
        bo_add_16be(p_box, vlc_coeffs_to_iso_23001_8_mc(p_vfmt->space));
        bo_add_8(p_box, p_vfmt->color_range == COLOR_RANGE_FULL ? 0x80 : 0x00);
    }
    return p_box;
}

static bo_t *GetMdcv(const video_format_t *p_vfmt)
{
    if(!p_vfmt->mastering.max_luminance)
        return NULL;
    bo_t *p_box = box_new("mdcv");
    if(p_box)
    {
        for(int i=0; i<6; i++)
            bo_add_16be(p_box, p_vfmt->mastering.primaries[i]);
        bo_add_16be(p_box, p_vfmt->mastering.white_point[0]);
        bo_add_16be(p_box, p_vfmt->mastering.white_point[1]);
        bo_add_32be(p_box, p_vfmt->mastering.max_luminance);
        bo_add_32be(p_box, p_vfmt->mastering.min_luminance);
    }
    return p_box;
}

static bo_t *GetClli(const video_format_t *p_vfmt)
{
    if(!p_vfmt->lighting.MaxFALL)
        return NULL;
    bo_t *p_box = box_new("clli");
    if(p_box)
    {
        bo_add_16be(p_box, p_vfmt->lighting.MaxCLL);
        bo_add_16be(p_box, p_vfmt->lighting.MaxFALL);
    }
    return p_box;
}

static bo_t *GetAvcCTag(const uint8_t *p_extra, size_t i_extra)
{
    bo_t    *avcC = box_new("avcC");/* FIXME use better value */
    if(!avcC)
        return NULL;
    const uint8_t *p_sps, *p_pps, *p_ext;
    size_t i_sps_size, i_pps_size, i_ext_size;

    if(! h264_AnnexB_get_spspps(p_extra, i_extra,
                        &p_sps, &i_sps_size,
                        &p_pps, &i_pps_size,
                        &p_ext, &i_ext_size ) )
    {
        p_sps = p_pps = p_ext = NULL;
        i_sps_size = i_pps_size = i_ext_size = 0;
    }

    bo_add_8(avcC, 1);      /* configuration version */
    bo_add_8(avcC, i_sps_size > 3 ? p_sps[1] : PROFILE_H264_MAIN);
    bo_add_8(avcC, i_sps_size > 3 ? p_sps[2] : 64);
    bo_add_8(avcC, i_sps_size > 3 ? p_sps[3] : 30);       /* level, 5.1 */
    bo_add_8(avcC, 0xff);   /* 0b11111100 | lengthsize = 0x11 */

    bo_add_8(avcC, 0xe0 | (i_sps_size > 0 ? 1 : 0));   /* 0b11100000 | sps_count */
    if (i_sps_size > 0) {
        bo_add_16be(avcC, i_sps_size);
        bo_add_mem(avcC, i_sps_size, p_sps);
    }

    bo_add_8(avcC, (i_pps_size > 0 ? 1 : 0));   /* pps_count */
    if (i_pps_size > 0) {
        bo_add_16be(avcC, i_pps_size);
        bo_add_mem(avcC, i_pps_size, p_pps);
    }

    if( i_sps_size > 3 &&
       (p_sps[1] == PROFILE_H264_HIGH ||
        p_sps[1] == PROFILE_H264_HIGH_10 ||
        p_sps[1] == PROFILE_H264_HIGH_422 ||
        p_sps[1] == PROFILE_H264_HIGH_444 ||
        p_sps[1] == PROFILE_H264_HIGH_444_PREDICTIVE) )
    {
        h264_sequence_parameter_set_t *p_spsdata = h264_decode_sps( p_sps, i_sps_size, true );
        if( p_spsdata )
        {
            uint8_t data[3];
            if( h264_get_chroma_luma( p_spsdata, &data[0], &data[1], &data[2]) )
            {
                bo_add_8(avcC, 0xFC | data[0]);
                bo_add_8(avcC, 0xF8 | (data[1] - 8));
                bo_add_8(avcC, 0xF8 | (data[2] - 8));
                bo_add_8(avcC, (i_ext_size > 0 ? 1 : 0));
                if (i_ext_size > 0) {
                    bo_add_16be(avcC, i_ext_size);
                    bo_add_mem(avcC, i_ext_size, p_ext);
                }
            }
            h264_release_sps( p_spsdata );
        }
    }

    return avcC;
}

/* TODO: No idea about these values */
static bo_t *GetSVQ3Tag(const uint8_t *p_extra, size_t i_extra)
{
    bo_t *smi = box_new("SMI ");
    if(!smi)
        return NULL;

    if (i_extra > 0x4e) {
        const uint8_t *p_end = &p_extra[i_extra];
        const uint8_t *p     = &p_extra[0x46];

        while (p + 8 < p_end) {
            int i_size = GetDWBE(p);
            if (i_size <= 1) /* FIXME handle 1 as long size */
                break;
            if (!strncmp((const char *)&p[4], "SMI ", 4)) {
                bo_add_mem(smi, p_end - p - 8, &p[8]);
                return smi;
            }
            p += i_size;
        }
    }

    /* Create a dummy one in fallback */
    bo_add_fourcc(smi, "SEQH");
    bo_add_32be(smi, 0x5);
    bo_add_32be(smi, 0xe2c0211d);
    bo_add_8(smi, 0xc0);

    return smi;
}

static bo_t *GetUdtaTag(mp4mux_handle_t *muxh)
{
    bo_t *udta = box_new("udta");
    if (!udta)
        return NULL;

    /* Requirements */
    for (unsigned int i = 0; i < vlc_array_count(&muxh->tracks); i++) {
        mp4mux_trackinfo_t *p_stream = vlc_array_item_at_index(&muxh->tracks, i);
        vlc_fourcc_t codec = p_stream->fmt.i_codec;

        if (codec == VLC_CODEC_MP4V || codec == VLC_CODEC_MP4A) {
            bo_t *box = box_new("\251req");
            if(!box)
                break;
            /* String length */
            bo_add_16be(box, sizeof("QuickTime 6.0 or greater") - 1);
            bo_add_16be(box, 0);
            bo_add_mem(box, sizeof("QuickTime 6.0 or greater") - 1,
                        (uint8_t *)"QuickTime 6.0 or greater");
            box_gather(udta, box);
            break;
        }
    }

    /* Encoder */
    {
        bo_t *box = box_new("\251enc");
        if(box)
        {
            /* String length */
            bo_add_16be(box, sizeof(PACKAGE_STRING " stream output") - 1);
            bo_add_16be(box, 0);
            bo_add_mem(box, sizeof(PACKAGE_STRING " stream output") - 1,
                        (uint8_t*)PACKAGE_STRING " stream output");
            box_gather(udta, box);
        }
    }
#if 0
    /* Misc atoms */
    vlc_meta_t *p_meta = p_mux->p_sout->p_meta;
    if (p_meta) {
#define ADD_META_BOX(type, box_string) { \
        bo_t *box = NULL;  \
        if (vlc_meta_Get(p_meta, vlc_meta_##type)) \
            box = box_new("\251" box_string); \
        if (box) { \
            bo_add_16be(box, strlen(vlc_meta_Get(p_meta, vlc_meta_##type))); \
            bo_add_16be(box, 0); \
            bo_add_mem(box, strlen(vlc_meta_Get(p_meta, vlc_meta_##type)), \
                        (uint8_t*)(vlc_meta_Get(p_meta, vlc_meta_##type))); \
            box_gather(udta, box); \
        } }

        ADD_META_BOX(Title, "nam");
        ADD_META_BOX(Artist, "ART");
        ADD_META_BOX(Genre, "gen");
        ADD_META_BOX(Copyright, "cpy");
        ADD_META_BOX(Description, "des");
        ADD_META_BOX(Date, "day");
        ADD_META_BOX(URL, "url");
#undef ADD_META_BOX
    }
#endif
    return udta;
}

static bo_t *GetSratBox(uint32_t i_sample_rate)
{
    bo_t *srat = box_full_new("srat", 0, 0);
    if(srat)
        bo_add_32be(srat, i_sample_rate);
    return srat;
}

static bo_t *GetSounBox(vlc_object_t *p_obj, mp4mux_trackinfo_t *p_track, bool b_mov)
{
    VLC_UNUSED(p_obj);

    bool b_descr = false;
    const vlc_fourcc_t codec = p_track->fmt.i_codec;
    const audio_format_t *afmt = &p_track->fmt.audio;
    char fcc[4];
    bo_t *specificbox = NULL;
    uint16_t i_qt_version = 1;
    uint16_t i_compression_id = -2;
    uint32_t i_uncompressed_bps = 0;
    bo_t *srat = NULL;

    /* codec specific extradata */
    const uint8_t *p_extradata = p_track->fmt.p_extra;
    size_t i_extradata = p_track->fmt.i_extra;
    if(p_track->sample_priv.i_data)
    {
        p_extradata = p_track->sample_priv.p_data;
        i_extradata = p_track->sample_priv.i_data;
    }

    switch(codec)
    {
        case VLC_CODEC_A52:
            memcpy(fcc, "ac-3", 4);
            if(i_extradata >= 3)
            {
                specificbox = GetxxxxTag(p_extradata, i_extradata, "dac3");
                if(b_mov)
                    specificbox = GetWaveTag(fcc, &specificbox, 1);
            }
            break;
        case VLC_CODEC_AMR_NB:
            memcpy(fcc, "samr", 4);
            specificbox = GetDamrTag(&p_track->fmt);
            if(b_mov)
                specificbox = GetWaveTag(fcc, &specificbox, 1);
            break;
        case VLC_CODEC_DTS:
            if(b_mov)
            {
                memcpy(fcc, "dtsc", 4);
            } else {
                memcpy(fcc, "mp4a", 4);
                b_descr = true;
            }
            break;
        case VLC_CODEC_EAC3:
            memcpy(fcc, "ec-3", 4);
            if(i_extradata >= 5)
            {
                specificbox = GetxxxxTag(p_extradata, i_extradata, "dec3");
                if(b_mov)
                    specificbox = GetWaveTag(fcc, &specificbox, 1);
            }
            break;
        case VLC_CODEC_MP4A:
            memcpy(fcc, "mp4a", 4);
            if(b_mov)
            {
                bo_t *extraboxes[2] = {NULL};
                extraboxes[0] = box_new("mp4a");
                if(extraboxes[0])
                    bo_add_32be(extraboxes[0], 0);
                extraboxes[1] = GetESDS(p_track);
                specificbox = GetWaveTag("mp4a", extraboxes, 2);
            } else b_descr = true;
            break;
        case VLC_CODEC_MPGA:
        case VLC_CODEC_MP3:
            if (b_mov) {
                /* mpeg audio in mov */
                memcpy(fcc, ".mp3", 4);
                i_qt_version = 0;
            } else {
                memcpy(fcc, "mp4a", 4);
                b_descr = true;
            }
            break;
        case VLC_CODEC_WMAP:
            memcpy(fcc, "wma ", 4);
            specificbox = GetWaveFormatExTag(&p_track->fmt, "wfex");
            break;
        // v0 only Qt codecs
        case VLC_CODEC_U8:
            i_uncompressed_bps = 8;
            i_qt_version = 0;
            memcpy(fcc, "raw ", 4);
            break;
        case VLC_CODEC_ALAW:
            i_qt_version = 0;
            memcpy(fcc, "alaw", 4);
            break;
        case VLC_CODEC_MULAW:
            i_qt_version = 0;
            memcpy(fcc, "ulaw", 4);
            break;
        case VLC_CODEC_S16B:
            i_uncompressed_bps = 16;
            i_qt_version = 0;
            memcpy(fcc, "twos", 4);
            break;
        // v1
        case VLC_CODEC_S16L:
            i_uncompressed_bps = 16;
            memcpy(fcc, "sowt", 4);
            break;
        case VLC_CODEC_S24B: /* only v1 uncompressed audio */
        case VLC_CODEC_S24L:
        case VLC_CODEC_S32B:
        case VLC_CODEC_S32L:
        case VLC_CODEC_F32B:
        case VLC_CODEC_F32L:
        case VLC_CODEC_F64B:
        case VLC_CODEC_F64L:
        {
            i_uncompressed_bps = aout_BitsPerSample(codec);
            const char * uncompressedcc;
            if(i_uncompressed_bps <= 32)
            {
                if(i_uncompressed_bps == 24)
                    uncompressedcc = "in24";
                else if((VLC_FOURCC('f',0,0,0) & codec) != VLC_FOURCC('f',0,0,0))
                    uncompressedcc = "in32";
                else
                    uncompressedcc = "fl32";
            }
            else uncompressedcc = "fl64";
            memcpy(fcc, uncompressedcc, 4);
            const bool b_le = ((codec & VLC_FOURCC(0,0,0,'b')) != VLC_FOURCC(0,0,0,'b'));
            bo_t *extraboxes[1] = { GetEndaTag(b_le) };
            specificbox = GetWaveTag(fcc, extraboxes, 1);
            break;
        }
        default:
            vlc_fourcc_to_char(codec, fcc);
            break;
    }

    bo_t *soun = box_new(fcc);
    if(!soun)
        return NULL;
    for (int i = 0; i < 6; i++)
        bo_add_8(soun, 0);        // reserved;
    bo_add_16be(soun, 1);         // data-reference-index

    /* SoundDescription */
    bo_add_16be(soun, i_qt_version);
    bo_add_16be(soun, 0);         // revision level (0)
    bo_add_32be(soun, 0);         // vendor
    // channel-count
    if(i_qt_version == 0 && afmt->i_channels == 1)
        bo_add_16be(soun, 1);
    else
        bo_add_16be(soun, 2);
    // sample size
    if(i_qt_version == 0 && i_uncompressed_bps == 8)
        bo_add_16be(soun, 8);
    else
        bo_add_16be(soun, 16);
    // compression id
    if(!b_mov)
        bo_add_16be(soun, 0);
    else
        bo_add_16be(soun, i_compression_id);
    bo_add_16be(soun, 0);         // packet size (0)

    if(!b_mov && i_qt_version > 0 &&
       p_track->fmt.audio.i_rate >= (1<<16))
    {
        bo_add_32be(soun, 1<<16);
        srat = GetSratBox(p_track->fmt.audio.i_rate);
    }
    else
    {
        bo_add_16be(soun, p_track->fmt.audio.i_rate); // sampleratehi
        bo_add_16be(soun, 0);                         // sampleratelo
    }

    /* Extended data for SoundDescription V1 */
    if (i_qt_version == 1)
    {
        const uint32_t i_samples_per_packet = (i_uncompressed_bps && i_uncompressed_bps < 16)
                                            ? 1
                                            : afmt->i_frame_length;
        const uint32_t i_bytes_per_packet = afmt->i_channels
                                          ? afmt->i_frame_length / afmt->i_channels
                                          : 0;
        bo_add_32be(soun, i_samples_per_packet);
        bo_add_32be(soun, i_bytes_per_packet);
        bo_add_32be(soun, p_track->fmt.audio.i_bytes_per_frame); /* bytes per frame */
        bo_add_32be(soun, 2); /* bytes per sample */
    }

    box_gather(soun, specificbox);

    /* Add an ES Descriptor */
    if (b_descr)
       box_gather(soun, GetESDS(p_track));

    box_gather(soun, srat);

    return soun;
}

static bo_t *GetVideBox(vlc_object_t *p_obj, mp4mux_trackinfo_t *p_track, bool b_mov)
{
    VLC_UNUSED(p_obj);

    char fcc[4];
    bool b_colr = false, b_fiel = false;

    static_assert(VLC_CODEC_YUV4 == VLC_FOURCC('y','u','v','4'), "incorrect fcc for yuv4");
    static_assert(VLC_CODEC_V210 == VLC_FOURCC('v','2','1','0'), "incorrect fcc for v210");
    static_assert(VLC_CODEC_V308 == VLC_FOURCC('v','3','0','8'), "incorrect fcc for v308");
    static_assert(VLC_CODEC_V408 == VLC_FOURCC('v','4','0','8'), "incorrect fcc for v408");
    static_assert(VLC_CODEC_V410 == VLC_FOURCC('v','4','1','0'), "incorrect fcc for v410");

    switch(p_track->fmt.i_codec)
    {
    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGV: memcpy(fcc, "mp4v", 4); break;
    case VLC_CODEC_MJPG: memcpy(fcc, "mjpa", 4); break;
    case VLC_CODEC_SVQ1: memcpy(fcc, "SVQ1", 4); break;
    case VLC_CODEC_SVQ3: memcpy(fcc, "SVQ3", 4); break;
    case VLC_CODEC_H263: memcpy(fcc, "s263", 4); break;
    case VLC_CODEC_H264: memcpy(fcc, "avc1", 4); break;
    case VLC_CODEC_VC1 : memcpy(fcc, "vc-1", 4); break;
    /* FIXME: find a way to know if no non-VCL units are in the stream (->hvc1)
     * see 14496-15 8.4.1.1.1 */
    case VLC_CODEC_HEVC: memcpy(fcc, "hev1", 4); break;
    case VLC_CODEC_YV12: memcpy(fcc, "yv12", 4); b_colr = b_fiel = true; break;
    case VLC_CODEC_YUYV: memcpy(fcc, "YUY2", 4); b_colr = b_fiel= true; break;
    case VLC_CODEC_UYVY: memcpy(fcc, "2vuy", 4); b_colr = b_fiel= true; break;
    case VLC_CODEC_YUV4:
    case VLC_CODEC_V210:
    case VLC_CODEC_V308:
    case VLC_CODEC_V408:
    case VLC_CODEC_V410:
            vlc_fourcc_to_char(p_track->fmt.i_codec, fcc);
            b_colr = b_fiel = true;
            break;
    default:
        vlc_fourcc_to_char(p_track->fmt.i_codec, fcc);
        break;
    }

    bo_t *vide = box_new(fcc);
    if(!vide)
        return NULL;
    for (int i = 0; i < 6; i++)
        bo_add_8(vide, 0);        // reserved;
    bo_add_16be(vide, 1);         // data-reference-index

    bo_add_16be(vide, 0);         // predefined;
    bo_add_16be(vide, 0);         // reserved;
    for (int i = 0; i < 3; i++)
        bo_add_32be(vide, 0);     // predefined;

    bo_add_16be(vide, p_track->fmt.video.i_visible_width);  // i_width
    bo_add_16be(vide, p_track->fmt.video.i_visible_height); // i_height

    bo_add_32be(vide, 0x00480000);                // h 72dpi
    bo_add_32be(vide, 0x00480000);                // v 72dpi

    bo_add_32be(vide, 0);         // data size, always 0
    bo_add_16be(vide, 1);         // frames count per sample

    // compressor name;
    uint8_t compressor_name[32] = {0};
    switch (p_track->fmt.i_codec)
    {
        case VLC_CODEC_AV1:
            memcpy(compressor_name, "\012AOM Coding", 11);
            break;
        default:
            break;
    }
    bo_add_mem(vide, 32, compressor_name);

    bo_add_16be(vide, 0x18);      // depth
    bo_add_16be(vide, 0xffff);    // predefined

    /* codec specific extradata */
    const uint8_t *p_extradata = p_track->fmt.p_extra;
    size_t i_extradata = p_track->fmt.i_extra;
    if(p_track->sample_priv.i_data)
    {
        p_extradata = p_track->sample_priv.p_data;
        i_extradata = p_track->sample_priv.i_data;
    }

    /* add an ES Descriptor */
    switch(p_track->fmt.i_codec)
    {
    case VLC_CODEC_AV1:
        box_gather(vide, GetxxxxTag(p_extradata, i_extradata, "av1C"));
        b_colr = true;
        break;

    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGV:
        box_gather(vide, GetESDS(p_track));
        break;

    case VLC_CODEC_H263:
        box_gather(vide, GetD263Tag());
        break;

    case VLC_CODEC_SVQ3:
        box_gather(vide, GetSVQ3Tag(p_extradata, i_extradata));
        break;

    case VLC_CODEC_H264:
        box_gather(vide, GetAvcCTag(p_extradata, i_extradata));
        break;

    case VLC_CODEC_VC1:
        box_gather(vide, GetxxxxTag(p_extradata, i_extradata, "dvc1"));
            break;

    case VLC_CODEC_HEVC:
        /* Write HvcC without forcing VPS/SPS/PPS/SEI array_completeness */
        box_gather(vide, GetHvcCTag(p_extradata, i_extradata, false));
        break;
    }

    if(b_colr)
        box_gather(vide, GetColrBox(&p_track->fmt.video, b_mov));
    if(b_fiel)
        box_gather(vide, GetFielBox(p_track->e_interlace));

    box_gather(vide, GetMdcv(&p_track->fmt.video));
    box_gather(vide, GetClli(&p_track->fmt.video));

    return vide;
}

static bo_t *GetTextBox(vlc_object_t *p_obj, mp4mux_trackinfo_t *p_track, bool b_mov)
{
    VLC_UNUSED(p_obj);
    if(p_track->fmt.i_codec == VLC_CODEC_QTXT)
    {
        bo_t *text = box_new("text");
        if(!text)
            return NULL;

        /* Sample Entry Header */
        for (int i = 0; i < 6; i++)
            bo_add_8(text, 0);        // reserved;
        bo_add_16be(text, 1);         // data-reference-index

        if(p_track->fmt.i_extra >= 44)
        {
            /* Copy the original sample description format */
            bo_add_mem(text, p_track->fmt.i_extra, p_track->fmt.p_extra);
        }
        else
        {
            for (int i = 0; i < 6; i++)
                bo_add_8(text, 0);        // reserved;
            bo_add_16be(text, 1);         // data-reference-index

            bo_add_32be(text, 0);         // display flags
            bo_add_32be(text, 0);         // justification
            for (int i = 0; i < 3; i++)
                bo_add_16be(text, 0);     // background color

            bo_add_64be(text, 0);         // box text
            bo_add_64be(text, 0);         // reserved

            bo_add_16be(text, 0);         // font-number
            bo_add_16be(text, 0);         // font-face
            bo_add_8(text, 0);            // reserved
            bo_add_16be(text, 0);         // reserved

            for (int i = 0; i < 3; i++)
                bo_add_16be(text, 0xff);  // foreground color

            bo_add_8(text, 5);
            bo_add_mem(text, 5,  (void*)"Serif");
        }
        return text;
    }
    else if(p_track->fmt.i_codec == VLC_CODEC_SUBT ||
            p_track->fmt.i_codec == VLC_CODEC_TX3G)
    {
        bo_t *tx3g = box_new("tx3g");
        if(!tx3g)
            return NULL;

        /* Sample Entry Header */
        for (int i = 0; i < 6; i++)
            bo_add_8(tx3g, 0);        // reserved;
        bo_add_16be(tx3g, 1);         // data-reference-index

        if(p_track->fmt.i_codec == VLC_CODEC_TX3G &&
           p_track->fmt.i_extra >= 32)
        {
            /* Copy the original sample description format */
            bo_add_mem(tx3g, p_track->fmt.i_extra, p_track->fmt.p_extra);
        }
        else /* Build TTXT(tx3g) sample desc */
        {
            /* tx3g sample description */
            bo_add_32be(tx3g, 0);         // display flags
            bo_add_16be(tx3g, 0);         // justification

            bo_add_32be(tx3g, 0);         // background color

            /* BoxRecord */
            bo_add_64be(tx3g, 0);

            /* StyleRecord*/
            bo_add_16be(tx3g, 0);         // startChar
            bo_add_16be(tx3g, 0);         // endChar
            bo_add_16be(tx3g, 0);         // default font ID
            bo_add_8(tx3g, 0);            // face style flags
            bo_add_8(tx3g, STYLE_DEFAULT_FONT_SIZE);  // font size
            bo_add_32be(tx3g, 0xFFFFFFFFU);// foreground color

            /* FontTableBox */
            bo_t *ftab = box_new("ftab");
            if(ftab)
            {
                bo_add_16be(ftab, b_mov ? 2 : 3); // Entry Count
                /* Font Record */
                bo_add_8(ftab, 5);
                bo_add_mem(ftab, 5,  (void*)"Serif");
                bo_add_8(ftab, 10);
                bo_add_mem(ftab, 10, (void*) (b_mov ? "Sans-Serif" : "Sans-serif"));
                if(!b_mov) /* qt only allows "Serif" and "Sans-Serif" */
                {
                    bo_add_8(ftab, 9);
                    bo_add_mem(ftab, 9,  (void*)"Monospace");
                }

                box_gather(tx3g, ftab);
            }
        }

        return tx3g;
    }
    else if(p_track->fmt.i_codec == VLC_CODEC_WEBVTT)
    {
        bo_t *wvtt = box_new("wvtt");
        if(!wvtt)
            return NULL;

        /* Sample Entry Header */
        for (int i = 0; i < 6; i++)
            bo_add_8(wvtt, 0);        // reserved;
        bo_add_16be(wvtt, 1);         // data-reference-index

        bo_t *ftab = box_new("vttc");
        box_gather(wvtt, ftab);

        return wvtt;
    }
    else if(p_track->fmt.i_codec == VLC_CODEC_TTML)
    {
        bo_t *stpp = box_new("stpp");
        if(!stpp)
            return NULL;

        /* Sample Entry Header */
        for (int i = 0; i < 6; i++)
            bo_add_8(stpp, 0);        // reserved;
        bo_add_16be(stpp, 1);         // data-reference-index

        return stpp;
    }
    else if(p_track->fmt.i_codec == VLC_CODEC_SPU)
    {
        bo_t *mp4s = box_full_new("mp4s", 0, 0);
        bo_add_32be(mp4s, 1); // index
        box_gather(mp4s, GetESDS(p_track));
        return mp4s;
    }

    return NULL;
}

static int64_t GetScaledEntryDuration( const mp4mux_sample_t *p_entry, uint32_t i_timescale,
                                       vlc_tick_t *pi_total_mtime, int64_t *pi_total_scaled )
{
    const vlc_tick_t i_totalscaledtototalmtime = vlc_tick_from_samples(*pi_total_scaled, i_timescale);
    const vlc_tick_t i_diff = *pi_total_mtime - i_totalscaledtototalmtime;

    /* Ensure to compensate the drift due to loss from time, and from scale, conversions */
    int64_t i_scaled = samples_from_vlc_tick(p_entry->i_length + i_diff, i_timescale);
    *pi_total_mtime += p_entry->i_length;
    *pi_total_scaled += i_scaled;

    return i_scaled;
}

static bo_t *GetStblBox(vlc_object_t *p_obj, mp4mux_trackinfo_t *p_track, bool b_mov, bool b_stco64)
{
    /* sample description */
    bo_t *stsd = box_full_new("stsd", 0, 0);
    if(!stsd)
        return NULL;
    bo_add_32be(stsd, 1);
    if (p_track->fmt.i_cat == AUDIO_ES)
        box_gather(stsd, GetSounBox(p_obj, p_track, b_mov));
    else if (p_track->fmt.i_cat == VIDEO_ES)
        box_gather(stsd, GetVideBox(p_obj, p_track, b_mov));
    else if (p_track->fmt.i_cat == SPU_ES)
        box_gather(stsd, GetTextBox(p_obj, p_track, b_mov));

    /* chunk offset table */
    bo_t *stco;

    if (b_stco64) {
        /* 64 bits version */
        stco = box_full_new("co64", 0, 0);
    } else {
        /* 32 bits version */
        stco = box_full_new("stco", 0, 0);
    }
    if(!stco)
    {
        bo_free(stsd);
        return NULL;
    }
    bo_add_32be(stco, 0);     // entry-count (fixed latter)

    /* sample to chunk table */
    bo_t *stsc = box_full_new("stsc", 0, 0);
    if(!stsc)
    {
        bo_free(stco);
        bo_free(stsd);
        return NULL;
    }
    bo_add_32be(stsc, 0);     // entry-count (fixed latter)

    unsigned i_chunk = 0;
    unsigned i_stsc_last_val = 0, i_stsc_entries = 0;
    for (unsigned i = 0; i < p_track->i_samples_count; i_chunk++) {
        mp4mux_sample_t *entry = p_track->samples;
        int i_first = i;

        if (b_stco64)
            bo_add_64be(stco, entry[i].i_pos);
        else
            bo_add_32be(stco, entry[i].i_pos);

        for (; i < p_track->i_samples_count; i++)
            if (i >= p_track->i_samples_count - 1 ||
                    entry[i].i_pos + entry[i].i_size != entry[i+1].i_pos) {
                i++;
                break;
            }

        /* Add entry to the stsc table */
        if (i_stsc_last_val != i - i_first) {
            bo_add_32be(stsc, 1 + i_chunk);   // first-chunk
            bo_add_32be(stsc, i - i_first) ;  // samples-per-chunk
            bo_add_32be(stsc, 1);             // sample-descr-index
            i_stsc_last_val = i - i_first;
            i_stsc_entries++;
        }
    }

    /* Fix stco entry count */
    bo_swap_32be(stco, 12, i_chunk);
    if(p_obj)
        msg_Dbg(p_obj, "created %d chunks (stco)", i_chunk);

    /* Fix stsc entry count */
    bo_swap_32be(stsc, 12, i_stsc_entries );

    /* add stts */
    bo_t *stts = box_full_new("stts", 0, 0);
    if(!stts)
    {
        bo_free(stsd);
        bo_free(stco);
        bo_free(stsc);
        return NULL;
    }
    bo_add_32be(stts, 0);     // entry-count (fixed latter)

    vlc_tick_t i_total_mtime = 0;
    int64_t i_total_scaled = 0;
    unsigned i_index = 0;
    for (unsigned i = 0; i < p_track->i_samples_count; i_index++) {
        int     i_first = i;

        int64_t i_scaled = GetScaledEntryDuration(&p_track->samples[i], p_track->i_timescale,
                                                  &i_total_mtime, &i_total_scaled);
        for (unsigned j=i+1; j < p_track->i_samples_count; j++)
        {
            vlc_tick_t i_total_mtime_next = i_total_mtime;
            int64_t i_total_scaled_next = i_total_scaled;
            int64_t i_scalednext = GetScaledEntryDuration(&p_track->samples[j], p_track->i_timescale,
                                                          &i_total_mtime_next, &i_total_scaled_next);
            if( i_scalednext != i_scaled )
                break;

            i_total_mtime = i_total_mtime_next;
            i_total_scaled = i_total_scaled_next;
            i = j;
        }

        bo_add_32be(stts, ++i - i_first); // sample-count
        bo_add_32be(stts, i_scaled); // sample-delta
    }
    bo_swap_32be(stts, 12, i_index);

    //msg_Dbg(p_obj, "total sout duration %"PRId64" reconverted from scaled %"PRId64,
    //                i_total_mtime, vlc_tick_from_samples(i_total_scaled, p_track->i_timescale) );

    /* composition time handling */
    bo_t *ctts = NULL;
    if ( p_track->b_hasbframes && (ctts = box_full_new("ctts", 0, 0)) )
    {
        bo_add_32be(ctts, 0);
        i_index = 0;
        for (unsigned i = 0; i < p_track->i_samples_count; i_index++)
        {
            int     i_first = i;
            vlc_tick_t i_offset = p_track->samples[i].i_pts_dts;

            for (; i < p_track->i_samples_count; ++i)
                if (i == p_track->i_samples_count || p_track->samples[i].i_pts_dts != i_offset)
                    break;

            bo_add_32be(ctts, i - i_first); // sample-count
            bo_add_32be(ctts, samples_from_vlc_tick(i_offset, p_track->i_timescale) ); // sample-offset
        }
        bo_swap_32be(ctts, 12, i_index);
    }

    bo_t *stsz = box_full_new("stsz", 0, 0);
    if(!stsz)
    {
        bo_free(stsd);
        bo_free(stco);
        bo_free(stts);
        return NULL;
    }
    int i_size = 0;
    for (unsigned i = 0; i < p_track->i_samples_count; i++)
    {
        if ( i == 0 )
            i_size = p_track->samples[i].i_size;
        else if ( p_track->samples[i].i_size != i_size )
        {
            i_size = 0;
            break;
        }
    }
    bo_add_32be(stsz, i_size);                         // sample-size
    bo_add_32be(stsz, p_track->i_samples_count);       // sample-count
    if ( i_size == 0 ) // all samples have different size
    {
        for (unsigned i = 0; i < p_track->i_samples_count; i++)
            bo_add_32be(stsz, p_track->samples[i].i_size); // sample-size
    }

    /* create stss table */
    bo_t *stss = NULL;
    i_index = 0;
    if ( p_track->fmt.i_cat == VIDEO_ES || p_track->fmt.i_cat == AUDIO_ES )
    {
        vlc_tick_t i_interval = -1;
        for (unsigned i = 0; i < p_track->i_samples_count; i++)
        {
            if ( i_interval != -1 )
            {
                i_interval += p_track->samples[i].i_length + p_track->samples[i].i_pts_dts;
                if ( i_interval < VLC_TICK_FROM_SEC(2) )
                    continue;
            }

            if (p_track->samples[i].i_flags & BLOCK_FLAG_TYPE_I) {
                if (stss == NULL) {
                    stss = box_full_new("stss", 0, 0);
                    if(!stss)
                        break;
                    bo_add_32be(stss, 0); /* fixed later */
                }
                bo_add_32be(stss, 1 + i);
                i_index++;
                i_interval = 0;
            }
        }
    }

    if (stss)
        bo_swap_32be(stss, 12, i_index);

    /* Now gather all boxes into stbl */
    bo_t *stbl = box_new("stbl");
    if(!stbl)
    {
        bo_free(stsd);
        bo_free(stco);
        bo_free(stts);
        bo_free(stsz);
        bo_free(stss);
        bo_free(ctts);
        return NULL;
    }
    box_gather(stbl, stsd);
    box_gather(stbl, stts);
    if (stss)
        box_gather(stbl, stss);
    if (ctts)
        box_gather(stbl, ctts);
    box_gather(stbl, stsc);
    box_gather(stbl, stsz);
    box_gather(stbl, stco);

    return stbl;
}

static unsigned ApplyARtoWidth(const video_format_t *vfmt)
{
    if (vfmt->i_sar_num > 0 && vfmt->i_sar_den > 0)
    {
        return (int64_t)vfmt->i_sar_num *
               (int64_t)vfmt->i_visible_width / vfmt->i_sar_den;
    }
    else return vfmt->i_visible_width;
}

bo_t * mp4mux_GetMoov(mp4mux_handle_t *h, vlc_object_t *p_obj, vlc_tick_t i_duration)
{
    bo_t            *moov, *mvhd;

    uint32_t        i_movie_timescale = 90000;
    int64_t         i_timestamp = get_timestamp();

    /* Important for smooth streaming where its (not muxed here) media time offsets
     * are in timescale == track timescale */
    if( vlc_array_count(&h->tracks) == 1 )
        i_movie_timescale = ((mp4mux_trackinfo_t *)vlc_array_item_at_index(&h->tracks, 0))->i_timescale;

    moov = box_new("moov");
    if(!moov)
        return NULL;
    /* Create general info */
    if( i_duration == 0 && (h->options & FRAGMENTED) == 0 )
    {
        for (unsigned int i = 0; i < vlc_array_count(&h->tracks); i++) {
            mp4mux_trackinfo_t *p_stream = vlc_array_item_at_index(&h->tracks, 0);
            i_duration = __MAX(i_duration, p_stream->i_read_duration);
        }
        if(p_obj)
            msg_Dbg(p_obj, "movie duration %"PRId64"s", SEC_FROM_VLC_TICK(i_duration));
    }
    int64_t i_movie_duration = samples_from_vlc_tick(i_duration, i_movie_timescale);

    /* *** add /moov/mvhd *** */
    if ((h->options & USE64BITEXT) == 0) {
        mvhd = box_full_new("mvhd", 0, 0);
        if(!mvhd)
        {
            bo_free(moov);
            return NULL;
        }
        bo_add_32be(mvhd, i_timestamp);   // creation time
        bo_add_32be(mvhd, i_timestamp);   // modification time
        bo_add_32be(mvhd, i_movie_timescale);  // timescale
        bo_add_32be(mvhd, i_movie_duration);  // duration
    } else {
        mvhd = box_full_new("mvhd", 1, 0);
        if(!mvhd)
        {
            bo_free(moov);
            return NULL;
        }
        bo_add_64be(mvhd, i_timestamp);   // creation time
        bo_add_64be(mvhd, i_timestamp);   // modification time
        bo_add_32be(mvhd, i_movie_timescale);  // timescale
        bo_add_64be(mvhd, i_movie_duration);  // duration
    }
    bo_add_32be(mvhd, 0x10000);           // rate
    bo_add_16be(mvhd, 0x100);             // volume
    bo_add_16be(mvhd, 0);                 // reserved
    for (int i = 0; i < 2; i++)
        bo_add_32be(mvhd, 0);             // reserved

    uint32_t mvhd_matrix[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000 };

    for (int i = 0; i < 9; i++)
        bo_add_32be(mvhd, mvhd_matrix[i]);// matrix
    for (int i = 0; i < 6; i++)
        bo_add_32be(mvhd, 0);             // pre-defined

    /* Next available track id */
    const mp4mux_trackinfo_t *lasttrack = vlc_array_count(&h->tracks)
                                        ? vlc_array_item_at_index(&h->tracks, vlc_array_count(&h->tracks) - 1)
                                        : NULL;
    uint32_t i_next_track_id = 1;
    if(lasttrack)
    {
        if(lasttrack->i_track_id < 0xFFFFFFFFU)
            i_next_track_id += lasttrack->i_track_id;
        else
            i_next_track_id = 0xFFFFFFFFU;
    }
    bo_add_32be(mvhd, i_next_track_id); // next-track-id

    box_gather(moov, mvhd);

    for (unsigned int i_trak = 0; i_trak < vlc_array_count(&h->tracks); i_trak++) {
        mp4mux_trackinfo_t *p_stream = vlc_array_item_at_index(&h->tracks, i_trak);

        int64_t i_stream_duration;
        if ( (h->options & FRAGMENTED) == 0 )
            i_stream_duration = samples_from_vlc_tick(p_stream->i_read_duration, i_movie_timescale);
        else
            i_stream_duration = 0;

        /* *** add /moov/trak *** */
        bo_t *trak = box_new("trak");
        if(!trak)
            continue;

        /* *** add /moov/trak/tkhd *** */
        bo_t *tkhd;
        if ((h->options & USE64BITEXT) == 0) {
            if (h->options & QUICKTIME)
                tkhd = box_full_new("tkhd", 0, 0x0f);
            else
                tkhd = box_full_new("tkhd", 0, 1);
            if(!tkhd)
            {
                bo_free(trak);
                continue;
            }
            bo_add_32be(tkhd, i_timestamp);       // creation time
            bo_add_32be(tkhd, i_timestamp);       // modification time
            bo_add_32be(tkhd, p_stream->i_track_id);
            bo_add_32be(tkhd, 0);                     // reserved 0
            bo_add_32be(tkhd, i_stream_duration); // duration
        } else {
            if (h->options & QUICKTIME)
                tkhd = box_full_new("tkhd", 1, 0x0f);
            else
                tkhd = box_full_new("tkhd", 1, 1);
            if(!tkhd)
            {
                bo_free(trak);
                continue;
            }
            bo_add_64be(tkhd, i_timestamp);       // creation time
            bo_add_64be(tkhd, i_timestamp);       // modification time
            bo_add_32be(tkhd, p_stream->i_track_id);
            bo_add_32be(tkhd, 0);                     // reserved 0
            bo_add_64be(tkhd, i_stream_duration); // duration
        }

        for (int i = 0; i < 2; i++)
            bo_add_32be(tkhd, 0);                 // reserved
        bo_add_16be(tkhd, 0);                     // layer
        bo_add_16be(tkhd, 0);                     // pre-defined
        // volume
        bo_add_16be(tkhd, p_stream->fmt.i_cat == AUDIO_ES ? 0x100 : 0);
        bo_add_16be(tkhd, 0);                     // reserved
        matrix_apply_rotation(&p_stream->fmt, mvhd_matrix);
        for (int i = 0; i < 9; i++)
            bo_add_32be(tkhd, mvhd_matrix[i]);    // matrix
        if (p_stream->fmt.i_cat == AUDIO_ES) {
            bo_add_32be(tkhd, 0);                 // width (presentation)
            bo_add_32be(tkhd, 0);                 // height(presentation)
        } else if (p_stream->fmt.i_cat == VIDEO_ES) {
            // width (presentation)
            bo_add_32be(tkhd, ApplyARtoWidth(&p_stream->fmt.video) << 16);
            // height(presentation)
            bo_add_32be(tkhd, p_stream->fmt.video.i_visible_height << 16);
        } else {
            unsigned i_width = 320;
            unsigned i_height = 200;
            /* Find video track for SPU representation */
            for (unsigned int i = 0; i < vlc_array_count(&h->tracks); i++)
            {
                const mp4mux_trackinfo_t *tk = vlc_array_item_at_index(&h->tracks, i);
                if (tk->fmt.i_cat != VIDEO_ES)
                    continue;
                i_width = ApplyARtoWidth(&tk->fmt.video);
                i_height = tk->fmt.video.i_visible_height;
                break;
            }
            bo_add_32be(tkhd, i_width << 16);     // width (presentation)
            bo_add_32be(tkhd, i_height << 16);    // height(presentation)
        }

        box_gather(trak, tkhd);

        /* *** add /moov/trak/edts and elst */
        bo_t *edts = GetEDTS(p_stream, i_movie_timescale, h->options & USE64BITEXT);
        if(edts)
            box_gather(trak, edts);

        /* *** add /moov/trak/mdia *** */
        bo_t *mdia = box_new("mdia");
        if(!mdia)
        {
            bo_free(trak);
            continue;
        }

        /* media header */
        bo_t *mdhd;
        if ((h->options & USE64BITEXT) == 0) {
            mdhd = box_full_new("mdhd", 0, 0);
            if(!mdhd)
            {
                bo_free(mdia);
                bo_free(trak);
                continue;
            }
            bo_add_32be(mdhd, i_timestamp);   // creation time
            bo_add_32be(mdhd, i_timestamp);   // modification time
            bo_add_32be(mdhd, p_stream->i_timescale); // timescale
            bo_add_32be(mdhd, i_stream_duration * p_stream->i_timescale / i_movie_timescale);  // duration
        } else {
            mdhd = box_full_new("mdhd", 1, 0);
            if(!mdhd)
            {
                bo_free(mdia);
                bo_free(trak);
                continue;
            }
            bo_add_64be(mdhd, i_timestamp);   // creation time
            bo_add_64be(mdhd, i_timestamp);   // modification time
            bo_add_32be(mdhd, p_stream->i_timescale); // timescale
            bo_add_64be(mdhd, i_stream_duration * p_stream->i_timescale / i_movie_timescale);  // duration
        }

        if (p_stream->fmt.psz_language) {
            char *psz = p_stream->fmt.psz_language;
            const iso639_lang_t *pl = NULL;
            uint16_t lang = 0x0;

            if (strlen(psz) == 2)
                pl = GetLang_1(psz);
            else if (strlen(psz) == 3) {
                pl = GetLang_2B(psz);
                if (!strcmp(pl->psz_iso639_1, "??"))
                    pl = GetLang_2T(psz);
            }

            if (pl && strcmp(pl->psz_iso639_1, "??"))
                lang = ((pl->psz_iso639_2T[0] - 0x60) << 10) |
                       ((pl->psz_iso639_2T[1] - 0x60) <<  5) |
                       ((pl->psz_iso639_2T[2] - 0x60));
            bo_add_16be(mdhd, lang);          // language
        } else
            bo_add_16be(mdhd, 0   );          // language
        bo_add_16be(mdhd, 0   );              // predefined
        box_gather(mdia, mdhd);

        /* handler reference */
        bo_t *hdlr = box_full_new("hdlr", 0, 0);
        if(!hdlr)
        {
            bo_free(mdia);
            bo_free(trak);
            continue;
        }

        if (h->options & QUICKTIME)
            bo_add_fourcc(hdlr, "mhlr");         // media handler
        else
            bo_add_32be(hdlr, 0);

        if (p_stream->fmt.i_cat == AUDIO_ES)
            bo_add_fourcc(hdlr, "soun");
        else if (p_stream->fmt.i_cat == VIDEO_ES)
            bo_add_fourcc(hdlr, "vide");
        else if (p_stream->fmt.i_cat == SPU_ES)
        {
            /* text/tx3g 3GPP */
            /* sbtl/tx3g Apple subs */
            /* text/text Apple textmedia */
            if(p_stream->fmt.i_codec == VLC_CODEC_TX3G)
                bo_add_fourcc(hdlr, (h->options & QUICKTIME) ? "sbtl" : "text");
            else if(p_stream->fmt.i_codec == VLC_CODEC_TTML)
                bo_add_fourcc(hdlr, "sbtl");
            else if(p_stream->fmt.i_codec == VLC_CODEC_SPU)
                bo_add_fourcc(hdlr, "subp");
            else
                bo_add_fourcc(hdlr, "text");
        }

        bo_add_32be(hdlr, 0);         // reserved
        bo_add_32be(hdlr, 0);         // reserved
        bo_add_32be(hdlr, 0);         // reserved

        if (h->options & QUICKTIME)
            bo_add_8(hdlr, 12);   /* Pascal string for .mov */

        if (p_stream->fmt.i_cat == AUDIO_ES)
            bo_add_mem(hdlr, 12, (uint8_t*)"SoundHandler");
        else if (p_stream->fmt.i_cat == VIDEO_ES)
            bo_add_mem(hdlr, 12, (uint8_t*)"VideoHandler");
        else
        {
            if( p_stream->fmt.i_codec == VLC_CODEC_SPU )
            {
                char language[13] = { 0 };
                if( p_stream->fmt.psz_language )
                    strncpy( language, p_stream->fmt.psz_language, 12 );
                bo_add_mem(hdlr, 12, language);
            }
            else
                bo_add_mem(hdlr, 12, (uint8_t*)"Text Handler");
        }

        if ((h->options & QUICKTIME) == 0)
            bo_add_8(hdlr, 0);   /* asciiz string for .mp4, yes that's BRAIN DAMAGED F**K MP4 */

        box_gather(mdia, hdlr);

        /* minf*/
        bo_t *minf = box_new("minf");
        if(!minf)
        {
            bo_free(mdia);
            bo_free(trak);
            continue;
        }

        /* add smhd|vmhd */
        if (p_stream->fmt.i_cat == AUDIO_ES) {
            bo_t *smhd = box_full_new("smhd", 0, 0);
            if(smhd)
            {
                bo_add_16be(smhd, 0);     // balance
                bo_add_16be(smhd, 0);     // reserved

                box_gather(minf, smhd);
            }
        } else if (p_stream->fmt.i_cat == VIDEO_ES) {
            bo_t *vmhd = box_full_new("vmhd", 0, 1);
            if(vmhd)
            {
                bo_add_16be(vmhd, 0);     // graphicsmode
                for (int i = 0; i < 3; i++)
                    bo_add_16be(vmhd, 0); // opcolor
                box_gather(minf, vmhd);
            }
        } else if (p_stream->fmt.i_cat == SPU_ES) {
            if((h->options & QUICKTIME) &&
               (p_stream->fmt.i_codec == VLC_CODEC_SUBT||
                p_stream->fmt.i_codec == VLC_CODEC_TX3G||
                p_stream->fmt.i_codec == VLC_CODEC_QTXT))
            {
                bo_t *gmin = box_full_new("gmin", 0, 1);
                if(gmin)
                {
                    bo_add_16be(gmin, 0);     // graphicsmode
                    for (int i = 0; i < 3; i++)
                        bo_add_16be(gmin, 0); // opcolor
                    bo_add_16be(gmin, 0);     // balance
                    bo_add_16be(gmin, 0);     // reserved

                    bo_t *gmhd = box_new("gmhd");
                    if(gmhd)
                    {
                        box_gather(gmhd, gmin);
                        box_gather(minf, gmhd);
                    }
                    else bo_free(gmin);
                }
            }
        }

        /* dinf */
        bo_t *dref = box_full_new("dref", 0, 0);
        if(dref)
        {
            bo_add_32be(dref, 1);

            bo_t *url = box_full_new("url ", 0, 0x01);
            if(url)
                box_gather(dref, url);

            bo_t *dinf = box_new("dinf");
            if(dinf)
            {
                box_gather(dinf, dref);

                /* append dinf to mdia */
                box_gather(minf, dinf);
            }
            else bo_free(dref);
        }

        /* add stbl */
        bo_t *stbl;
        if (h->options & FRAGMENTED)
        {
            uint32_t i_backup = p_stream->i_samples_count;
            p_stream->i_samples_count = 0;
            stbl = GetStblBox(p_obj, p_stream, h->options & QUICKTIME, h->options & USE64BITEXT);
            p_stream->i_samples_count = i_backup;
        }
        else
            stbl = GetStblBox(p_obj, p_stream, h->options & QUICKTIME, h->options & USE64BITEXT);

        /* append stbl to minf */
        box_gather(minf, stbl);

        /* append minf to mdia */
        box_gather(mdia, minf);

        /* append mdia to trak */
        box_gather(trak, mdia);

        /* append trak to moov */
        box_gather(moov, trak);
    }

    /* Add user data tags */
    box_gather(moov, GetUdtaTag(h));

    if ( h->options & FRAGMENTED )
    {
        bo_t *mvex = box_new("mvex");
        if( mvex )
        {
            if( i_movie_duration )
            {
                bo_t *mehd = box_full_new("mehd", (h->options & USE64BITEXT) ? 1 : 0, 0);
                if(mehd)
                {
                    if((h->options & USE64BITEXT))
                        bo_add_64be(mehd, i_movie_duration);
                    else
                        bo_add_32be(mehd, i_movie_duration);
                    box_gather(mvex, mehd);
                }
            }

            for (unsigned int i = 0; mvex && i < vlc_array_count(&h->tracks); i++)
            {
                mp4mux_trackinfo_t *p_stream = vlc_array_item_at_index(&h->tracks, i);

                /* Try to find some defaults */
                if ( p_stream->i_samples_count )
                {
                    // FIXME: find highest occurence
                    p_stream->i_trex_default_length = p_stream->samples[0].i_length;
                    p_stream->i_trex_default_size = p_stream->samples[0].i_size;
                }
                else
                {
                    p_stream->i_trex_default_length = 1;
                    p_stream->i_trex_default_size = 1;
                }

                /* *** add /mvex/trex *** */
                bo_t *trex = box_full_new("trex", 0, 0);
                bo_add_32be(trex, p_stream->i_track_id);
                bo_add_32be(trex, 1); // sample desc index
                bo_add_32be(trex, samples_from_vlc_tick(p_stream->i_trex_default_length, p_stream->i_timescale)); // sample duration
                bo_add_32be(trex, p_stream->i_trex_default_size); // sample size
                bo_add_32be(trex, 0); // sample flags
                box_gather(mvex, trex);
            }
            box_gather(moov, mvex);
        }
    }

    if(moov->b)
        box_fix(moov, bo_size(moov));
    return moov;
}

bo_t *mp4mux_GetFtyp(const mp4mux_handle_t *h)
{
    bo_t *box = box_new("ftyp");
    if(box)
    {
        bo_add_fourcc(box, &h->brands.i_major);
        bo_add_32be  (box, h->brands.i_minor);
        for(int i=0; i<h->brands.extra.i_size; i++)
            bo_add_fourcc(box, &h->brands.extra.p_elems[i]);
        if(!box->b)
        {
            free(box);
            return NULL;
        }
        box_fix(box, bo_size(box));
    }
    return box;
}

bool mp4mux_CanMux(vlc_object_t *p_obj, const es_format_t *p_fmt,
                   vlc_fourcc_t i_brand, bool b_fragmented)
{
    switch(p_fmt->i_codec)
    {
    case VLC_CODEC_U8:
    case VLC_CODEC_S16L:
    case VLC_CODEC_S16B:
    case VLC_CODEC_S24L:
    case VLC_CODEC_S24B:
    case VLC_CODEC_S32L:
    case VLC_CODEC_S32B:
    case VLC_CODEC_F32L:
    case VLC_CODEC_F32B:
    case VLC_CODEC_F64L:
    case VLC_CODEC_F64B:
    case VLC_CODEC_MULAW:
    case VLC_CODEC_ALAW:

    case VLC_CODEC_A52:
    case VLC_CODEC_DTS:
    case VLC_CODEC_EAC3:
    case VLC_CODEC_MP4A:
    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGA:
    case VLC_CODEC_MP3:
    case VLC_CODEC_MPGV:
    case VLC_CODEC_MP2V:
    case VLC_CODEC_MP1V:
    case VLC_CODEC_MJPG:
    case VLC_CODEC_MJPGB:
    case VLC_CODEC_SVQ1:
    case VLC_CODEC_SVQ3:
    case VLC_CODEC_H263:
    case VLC_CODEC_AMR_NB:
    case VLC_CODEC_AMR_WB:
    case VLC_CODEC_YUV4:
    case VLC_CODEC_YV12:
    case VLC_CODEC_UYVY:
    case VLC_CODEC_YUYV:
    case VLC_CODEC_V210:
    case VLC_CODEC_V308:
    case VLC_CODEC_V408:
    case VLC_CODEC_V410:
    case VLC_CODEC_VC1:
    case VLC_CODEC_WMAP:
    case VLC_CODEC_AV1:
        break;
    case VLC_CODEC_H264:
        if(!p_fmt->i_extra && p_obj)
            msg_Warn(p_obj, "H264 muxing from AnnexB source will set an incorrect default profile");
        break;
    case VLC_CODEC_HEVC:
        if(!p_fmt->i_extra)
        {
            if(p_obj)
                msg_Err(p_obj, "HEVC muxing from AnnexB source is unsupported");
            return false;
        }
        break;
    case VLC_CODEC_SPU:
            return i_brand != BRAND_qt__;
    case VLC_CODEC_SUBT:
        if(p_obj)
            msg_Warn(p_obj, "subtitle track added like in .mov (even when creating .mp4)");
        return !b_fragmented;
    case VLC_CODEC_TTML:
        /* Special case with smooth headers where we need to force frag TTML */
        /* TTML currently not supported in sout, until we can keep original timestamps */
            return i_brand == BRAND_smoo;
    case VLC_CODEC_QTXT:
    case VLC_CODEC_TX3G:
    case VLC_CODEC_WEBVTT:
        return !b_fragmented;
    default:
        return false;
    }
    return true;
}
