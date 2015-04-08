/*****************************************************************************
 * mp4.c: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006 VLC authors and VideoLAN
 * $Id$
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <vlc_bits.h>

#include <time.h>

#include <vlc_iso_lang.h>
#include <vlc_meta.h>

#include "../demux/mpeg/mpeg_parser_helpers.h"
#include "../demux/mp4/libmp4.h"

#include "assert.h"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FASTSTART_TEXT N_("Create \"Fast Start\" files")
#define FASTSTART_LONGTEXT N_(\
    "Create \"Fast Start\" files. " \
    "\"Fast Start\" files are optimized for downloads and allow the user " \
    "to start previewing the file while it is downloading.")

static int  Open   (vlc_object_t *);
static void Close  (vlc_object_t *);
static int  OpenFrag   (vlc_object_t *);
static void CloseFrag  (vlc_object_t *);

#define SOUT_CFG_PREFIX "sout-mp4-"

vlc_module_begin ()
    set_description(N_("MP4/MOV muxer"))
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_MUX)
    set_shortname("MP4")

    add_bool(SOUT_CFG_PREFIX "faststart", true,
              FASTSTART_TEXT, FASTSTART_LONGTEXT,
              true)
    set_capability("sout mux", 5)
    add_shortcut("mp4", "mov", "3gp")
    set_callbacks(Open, Close)

add_submodule ()
    set_description(N_("Fragmented and streamable MP4 muxer"))
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_MUX)
    set_shortname("MP4 Frag")
    add_shortcut("mp4frag", "mp4stream")
    set_capability("sout mux", 0)
    set_callbacks(OpenFrag, CloseFrag)

vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "faststart", NULL
};

static int Control(sout_mux_t *, int, va_list);
static int AddStream(sout_mux_t *, sout_input_t *);
static void DelStream(sout_mux_t *, sout_input_t *);
static int Mux      (sout_mux_t *);
static int MuxFrag  (sout_mux_t *);

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    uint64_t i_pos;
    int      i_size;

    mtime_t  i_pts_dts;
    mtime_t  i_length;
    unsigned int i_flags;
} mp4_entry_t;

typedef struct mp4_fragentry_t mp4_fragentry_t;

struct mp4_fragentry_t
{
    block_t  *p_block;
    uint32_t  i_run;
    mp4_fragentry_t *p_next;
};

typedef struct mp4_fragindex_t
{
    uint64_t i_moofoffset;
    mtime_t  i_time;
    uint8_t  i_traf;
    uint8_t  i_trun;
    uint32_t i_sample;
} mp4_fragindex_t;

typedef struct mp4_fragqueue_t
{
    mp4_fragentry_t *p_first;
    mp4_fragentry_t *p_last;
} mp4_fragqueue_t;

typedef struct
{
    es_format_t   fmt;
    unsigned int  i_track_id;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4_entry_t  *entry;
    int64_t      i_length_neg;

    /* stats */
    int64_t      i_dts_start; /* applies to current segment only */
    int64_t      i_read_duration;
    uint32_t     i_timescale;
    mtime_t      i_starttime; /* the really first packet */
    bool         b_hasbframes;

    /* XXX: needed for other codecs too, see lavf */
    block_t      *a52_frame;

    /* for later stco fix-up (fast start files) */
    uint64_t i_stco_pos;
    bool b_stco64;

    /* for spu */
    int64_t i_last_dts; /* applies to current segment only */
    int64_t i_last_length;

    /*** mp4frag ***/
    bool         b_hasiframes;
    uint32_t     i_trex_length;
    uint32_t     i_trex_size;
    uint32_t     i_tfhd_flags;

    uint32_t         i_current_run;
    mp4_fragentry_t *p_held_entry;
    mp4_fragqueue_t  read;
    mp4_fragqueue_t  towrite;
    mtime_t          i_last_iframe_time;
    mtime_t          i_written_duration;
    mp4_fragindex_t *p_indexentries;
    uint32_t         i_indexentriesmax;
    uint32_t         i_indexentries;
} mp4_stream_t;

struct sout_mux_sys_t
{
    bool b_mov;
    bool b_3gp;
    bool b_64_ext;
    bool b_fast_start;

    uint64_t i_mdat_pos;
    uint64_t i_pos;
    mtime_t  i_read_duration;

    unsigned int   i_nb_streams;
    mp4_stream_t **pp_streams;

    /* mp4frag */
    bool           b_fragmented;
    bool           b_header_sent;
    mtime_t        i_written_duration;
    uint32_t       i_mfhd_sequence;
};

static bo_t *box_new     (const char *fcc);
static bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f);
static void  box_fix     (bo_t *box, uint32_t);
static void  box_gather  (bo_t *box, bo_t *box2);

static inline void bo_add_mp4_tag_descr(bo_t *box, uint8_t tag, uint32_t size)
{
    bo_add_8(box, tag);
    for (int i = 3; i>0; i--)
        bo_add_8(box, (size>>(7*i)) | 0x80);
    bo_add_8(box, size & 0x7F);
}

static void box_send(sout_mux_t *p_mux,  bo_t *box);

static bo_t *GetMoovBox(sout_mux_t *p_mux);

static block_t *ConvertSUBT(block_t *);
static block_t *ConvertFromAnnexB(block_t *);

static const char avc1_short_start_code[3] = { 0, 0, 1 };
static const char avc1_start_code[4] = { 0, 0, 0, 1 };

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;
    bo_t            *box;

    msg_Dbg(p_mux, "Mp4 muxer opened");
    config_ChainParse(p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg);

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys = malloc(sizeof(sout_mux_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;
    p_sys->i_pos        = 0;
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;
    p_sys->i_mdat_pos   = 0;
    p_sys->b_mov        = p_mux->psz_mux && !strcmp(p_mux->psz_mux, "mov");
    p_sys->b_3gp        = p_mux->psz_mux && !strcmp(p_mux->psz_mux, "3gp");
    p_sys->i_read_duration   = 0;
    p_sys->b_fragmented = false;

    if (!p_sys->b_mov) {
        /* Now add ftyp header */
        box = box_new("ftyp");
        if(!box)
        {
            free(p_sys);
            return VLC_ENOMEM;
        }
        if (p_sys->b_3gp)
            bo_add_fourcc(box, "3gp6");
        else
            bo_add_fourcc(box, "isom");
        bo_add_32be  (box, 0);
        if (p_sys->b_3gp)
            bo_add_fourcc(box, "3gp4");
        else
            bo_add_fourcc(box, "mp41");
        bo_add_fourcc(box, "avc1");
        if(box->b)
        {
            box_fix(box, box->b->i_buffer);
            p_sys->i_pos += box->b->i_buffer;
            p_sys->i_mdat_pos = p_sys->i_pos;

            box_send(p_mux, box);
        }
    }

    /* FIXME FIXME
     * Quicktime actually doesn't like the 64 bits extensions !!! */
    p_sys->b_64_ext = false;

    /* Now add mdat header */
    box = box_new("mdat");
    if(!box)
    {
        free(p_sys);
        return VLC_ENOMEM;
    }
    bo_add_64be  (box, 0); // enough to store an extended size

    if(box->b)
        p_sys->i_pos += box->b->i_buffer;

    box_send(p_mux, box);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    msg_Dbg(p_mux, "Close");

    /* Update mdat size */
    bo_t bo;
    if (!bo_init(&bo, 16))
        goto cleanup;
    if (p_sys->i_pos - p_sys->i_mdat_pos >= (((uint64_t)1)<<32)) {
        /* Extended size */
        bo_add_32be  (&bo, 1);
        bo_add_fourcc(&bo, "mdat");
        bo_add_64be  (&bo, p_sys->i_pos - p_sys->i_mdat_pos);
    } else {
        bo_add_32be  (&bo, 8);
        bo_add_fourcc(&bo, "wide");
        bo_add_32be  (&bo, p_sys->i_pos - p_sys->i_mdat_pos - 8);
        bo_add_fourcc(&bo, "mdat");
    }

    sout_AccessOutSeek(p_mux->p_access, p_sys->i_mdat_pos);
    sout_AccessOutWrite(p_mux->p_access, bo.b);

    /* Create MOOV header */
    uint64_t i_moov_pos = p_sys->i_pos;
    bo_t *moov = GetMoovBox(p_mux);

    /* Check we need to create "fast start" files */
    p_sys->b_fast_start = var_GetBool(p_this, SOUT_CFG_PREFIX "faststart");
    while (p_sys->b_fast_start && moov && moov->b) {
        /* Move data to the end of the file so we can fit the moov header
         * at the start */
        int64_t i_size = p_sys->i_pos - p_sys->i_mdat_pos;
        int i_moov_size = moov->b->i_buffer;

        while (i_size > 0) {
            int64_t i_chunk = __MIN(32768, i_size);
            block_t *p_buf = block_Alloc(i_chunk);
            sout_AccessOutSeek(p_mux->p_access,
                                p_sys->i_mdat_pos + i_size - i_chunk);
            if (sout_AccessOutRead(p_mux->p_access, p_buf) < i_chunk) {
                msg_Warn(p_this, "read() not supported by access output, "
                          "won't create a fast start file");
                p_sys->b_fast_start = false;
                block_Release(p_buf);
                break;
            }
            sout_AccessOutSeek(p_mux->p_access, p_sys->i_mdat_pos + i_size +
                                i_moov_size - i_chunk);
            sout_AccessOutWrite(p_mux->p_access, p_buf);
            i_size -= i_chunk;
        }

        if (!p_sys->b_fast_start)
            break;

        /* Update pos pointers */
        i_moov_pos = p_sys->i_mdat_pos;
        p_sys->i_mdat_pos += moov->b->i_buffer;

        /* Fix-up samples to chunks table in MOOV header */
        for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
            mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
            unsigned i_written = 0;
            for (unsigned i = 0; i < p_stream->i_entry_count; ) {
                mp4_entry_t *entry = p_stream->entry;
                if (p_stream->b_stco64)
                    bo_set_64be(moov, p_stream->i_stco_pos + i_written++ * 8, entry[i].i_pos + p_sys->i_mdat_pos - i_moov_pos);
                else
                    bo_set_32be(moov, p_stream->i_stco_pos + i_written++ * 4, entry[i].i_pos + p_sys->i_mdat_pos - i_moov_pos);

                for (; i < p_stream->i_entry_count; i++)
                    if (i >= p_stream->i_entry_count - 1 ||
                        entry[i].i_pos + entry[i].i_size != entry[i+1].i_pos) {
                        i++;
                        break;
                    }
            }
        }

        p_sys->b_fast_start = false;
    }

    /* Write MOOV header */
    sout_AccessOutSeek(p_mux->p_access, i_moov_pos);
    box_send(p_mux, moov);

cleanup:
    /* Clean-up */
    for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        es_format_Clean(&p_stream->fmt);
        if (p_stream->a52_frame)
            block_Release(p_stream->a52_frame);
        free(p_stream->entry);
        free(p_stream);
    }
    if (p_sys->i_nb_streams)
        free(p_sys->pp_streams);
    free(p_sys);
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control(sout_mux_t *p_mux, int i_query, va_list args)
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;

    switch(i_query)
    {
    case MUX_CAN_ADD_STREAM_WHILE_MUXING:
        pb_bool = (bool*)va_arg(args, bool *);
        *pb_bool = false;
        return VLC_SUCCESS;

    case MUX_GET_ADD_STREAM_WAIT:
        pb_bool = (bool*)va_arg(args, bool *);
        *pb_bool = true;
        return VLC_SUCCESS;

    case MUX_GET_MIME:   /* Not needed, as not streamable */
    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * AddStream:
 *****************************************************************************/
static int AddStream(sout_mux_t *p_mux, sout_input_t *p_input)
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    mp4_stream_t    *p_stream;

    switch(p_input->p_fmt->i_codec)
    {
    case VLC_CODEC_A52:
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
    case VLC_CODEC_H264:
    case VLC_CODEC_HEVC:
    case VLC_CODEC_AMR_NB:
    case VLC_CODEC_AMR_WB:
    case VLC_CODEC_YV12:
    case VLC_CODEC_YUYV:
        break;
    case VLC_CODEC_SUBT:
        msg_Warn(p_mux, "subtitle track added like in .mov (even when creating .mp4)");
        break;
    default:
        msg_Err(p_mux, "unsupported codec %4.4s in mp4",
                 (char*)&p_input->p_fmt->i_codec);
        return VLC_EGENERIC;
    }

    p_stream = malloc(sizeof(mp4_stream_t));
    if (!p_stream)
        return VLC_ENOMEM;
    es_format_Copy(&p_stream->fmt, p_input->p_fmt);
    p_stream->i_track_id    = p_sys->i_nb_streams + 1;
    p_stream->i_length_neg  = 0;
    p_stream->i_entry_count = 0;
    p_stream->i_entry_max   = 1000;
    p_stream->entry         =
        calloc(p_stream->i_entry_max, sizeof(mp4_entry_t));
    p_stream->i_dts_start   = 0;
    p_stream->i_read_duration    = 0;
    p_stream->a52_frame = NULL;
    switch( p_stream->fmt.i_cat )
    {
    case AUDIO_ES:
        if(!p_stream->fmt.audio.i_rate)
        {
            msg_Warn( p_mux, "no audio rate given for stream %d, assuming 48KHz",
                      p_sys->i_nb_streams );
            p_stream->fmt.audio.i_rate = 48000;
        }
        p_stream->i_timescale = p_stream->fmt.audio.i_rate;
        break;
    case VIDEO_ES:
        if( !p_stream->fmt.video.i_frame_rate ||
            !p_stream->fmt.video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate for stream %d, assuming 25fps",
                      p_sys->i_nb_streams );
            p_stream->fmt.video.i_frame_rate = 25;
            p_stream->fmt.video.i_frame_rate_base = 1;
        }
        p_stream->i_timescale = p_stream->fmt.video.i_frame_rate * 1000 /
                                p_stream->fmt.video.i_frame_rate_base;
        break;
    default:
        p_stream->i_timescale = CLOCK_FREQ;
        break;
    }

    p_stream->i_starttime   = p_sys->i_read_duration;
    p_stream->b_hasbframes  = false;

    p_stream->i_last_dts    = 0;
    p_stream->i_last_length = 0;

    p_stream->b_hasiframes  = false;
    p_stream->i_trex_length = 0;
    p_stream->i_trex_size   = 0;

    p_stream->i_current_run = 0;
    p_stream->read.p_first  = NULL;
    p_stream->read.p_last   = NULL;
    p_stream->towrite.p_first = NULL;
    p_stream->towrite.p_last  = NULL;
    p_stream->p_held_entry    = NULL;
    p_stream->i_last_iframe_time = 0;
    p_stream->i_written_duration = 0;
    p_stream->p_indexentries     = NULL;
    p_stream->i_indexentriesmax  = 0;
    p_stream->i_indexentries     = 0;

    p_input->p_sys          = p_stream;

    msg_Dbg(p_mux, "adding input");

    TAB_APPEND(p_sys->i_nb_streams, p_sys->pp_streams, p_stream);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static void DelStream(sout_mux_t *p_mux, sout_input_t *p_input)
{
    VLC_UNUSED(p_input);
    msg_Dbg(p_mux, "removing input");
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static int Mux(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    for (;;) {
        int i_stream = sout_MuxGetStream(p_mux, 2, NULL);
        if (i_stream < 0)
            return(VLC_SUCCESS);

        sout_input_t *p_input  = p_mux->pp_inputs[i_stream];
        mp4_stream_t *p_stream = (mp4_stream_t*)p_input->p_sys;

        block_t *p_data;
        do {
            p_data = block_FifoGet(p_input->p_fifo);
            if (p_stream->fmt.i_codec == VLC_CODEC_H264 ||
                p_stream->fmt.i_codec == VLC_CODEC_HEVC)
                p_data = ConvertFromAnnexB(p_data);
            else if (p_stream->fmt.i_codec == VLC_CODEC_SUBT)
                p_data = ConvertSUBT(p_data);
            else if (p_stream->fmt.i_codec == VLC_CODEC_A52 ||
                     p_stream->fmt.i_codec == VLC_CODEC_EAC3) {
                if (p_stream->a52_frame == NULL && p_data->i_buffer >= 8)
                    p_stream->a52_frame = block_Duplicate(p_data);
            }
        } while (!p_data);

        /* Reset reference dts in case of discontinuity (ex: gather sout) */
        if ( p_stream->i_entry_count == 0 || p_data->i_flags & BLOCK_FLAG_DISCONTINUITY )
        {
            p_stream->i_dts_start = p_data->i_dts;
            p_stream->i_last_dts = p_data->i_dts;
            p_stream->i_length_neg = 0;
        }

        if (p_stream->fmt.i_cat != SPU_ES) {
            /* Fix length of the sample */
            if (block_FifoCount(p_input->p_fifo) > 0) {
                block_t *p_next = block_FifoShow(p_input->p_fifo);
                if ( p_next->i_flags & BLOCK_FLAG_DISCONTINUITY )
                { /* we have no way to know real length except by decoding */
                    if ( p_stream->fmt.i_cat == VIDEO_ES )
                    {
                        p_data->i_length = CLOCK_FREQ *
                                           p_stream->fmt.video.i_frame_rate_base /
                                           p_stream->fmt.video.i_frame_rate;
                        msg_Dbg( p_mux, "video track %u fixup to %"PRId64" for sample %u",
                                 p_stream->i_track_id, p_data->i_length, p_stream->i_entry_count );
                    }
                    else if ( p_stream->fmt.i_cat == AUDIO_ES &&
                              p_stream->fmt.audio.i_rate &&
                              p_data->i_nb_samples )
                    {
                        p_data->i_length = CLOCK_FREQ * p_data->i_nb_samples /
                                           p_stream->fmt.audio.i_rate;
                        msg_Dbg( p_mux, "audio track %u fixup to %"PRId64" for sample %u",
                                 p_stream->i_track_id, p_data->i_length, p_stream->i_entry_count );
                    }
                    else if ( p_data->i_length <= 0 )
                    {
                        msg_Warn( p_mux, "unknown length for track %u sample %u",
                                  p_stream->i_track_id, p_stream->i_entry_count );
                        p_data->i_length = 1;
                    }
                }
                else
                {
                    int64_t i_diff  = p_next->i_dts - p_data->i_dts;
                    if (i_diff < CLOCK_FREQ) /* protection */
                        p_data->i_length = i_diff;
                }
            }
            if (p_data->i_length <= 0) {
                msg_Warn(p_mux, "i_length <= 0");
                p_stream->i_length_neg += p_data->i_length - 1;
                p_data->i_length = 1;
            } else if (p_stream->i_length_neg < 0) {
                int64_t i_recover = __MIN(p_data->i_length / 4, - p_stream->i_length_neg);

                p_data->i_length -= i_recover;
                p_stream->i_length_neg += i_recover;
            }
        }

        if (p_stream->fmt.i_cat == SPU_ES && p_stream->i_entry_count > 0) {
            int64_t i_length = p_data->i_dts - p_stream->i_last_dts;

            if (i_length <= 0) /* FIXME handle this broken case */
                i_length = 1;

            /* Fix last entry */
            if (p_stream->entry[p_stream->i_entry_count-1].i_length <= 0)
                p_stream->entry[p_stream->i_entry_count-1].i_length = i_length;
        }

        /* add index entry */
        mp4_entry_t *e = &p_stream->entry[p_stream->i_entry_count];
        e->i_pos    = p_sys->i_pos;
        e->i_size   = p_data->i_buffer;

        if ( p_data->i_dts > VLC_TS_INVALID && p_data->i_pts > p_data->i_dts )
        {
            e->i_pts_dts = p_data->i_pts - p_data->i_dts;
            if ( !p_stream->b_hasbframes )
                p_stream->b_hasbframes = true;
        }
        else e->i_pts_dts = 0;

        e->i_length = p_data->i_length;
        e->i_flags  = p_data->i_flags;

        p_stream->i_entry_count++;
        /* XXX: -1 to always have 2 entry for easy adding of empty SPU */
        if (p_stream->i_entry_count >= p_stream->i_entry_max - 1) {
            p_stream->i_entry_max += 1000;
            p_stream->entry = xrealloc(p_stream->entry,
                         p_stream->i_entry_max * sizeof(mp4_entry_t));
        }

        /* update */
        p_stream->i_read_duration += __MAX( 0, p_data->i_length );
        p_stream->i_last_length = p_data->i_length;
        p_sys->i_pos += p_data->i_buffer;

        /* Save the DTS for SPU */
        p_stream->i_last_dts = p_data->i_dts;

        /* write data */
        sout_AccessOutWrite(p_mux->p_access, p_data);

        /* close subtitle with empty frame */
        if (p_stream->fmt.i_cat == SPU_ES) {
            int64_t i_length = p_stream->entry[p_stream->i_entry_count-1].i_length;

            if ( i_length != 0 && (p_data = block_Alloc(3)) ) {
                /* TODO */
                msg_Dbg(p_mux, "writing an empty sub") ;

                /* Append a idx entry */
                mp4_entry_t *e = &p_stream->entry[p_stream->i_entry_count];
                e->i_pos    = p_sys->i_pos;
                e->i_size   = 3;
                e->i_pts_dts= 0;
                e->i_length = 0;
                e->i_flags  = 0;

                /* XXX: No need to grow the entry here */
                p_stream->i_entry_count++;

                /* Fix last dts */
                p_stream->i_last_dts += i_length;

                /* Write a " " */
                p_data->i_dts = p_stream->i_last_dts;
                p_data->i_dts = p_data->i_pts;
                p_data->p_buffer[0] = 0;
                p_data->p_buffer[1] = 1;
                p_data->p_buffer[2] = ' ';

                p_sys->i_pos += p_data->i_buffer;

                sout_AccessOutWrite(p_mux->p_access, p_data);
            }

            /* Fix duration = current segment starttime + duration within */
            p_stream->i_read_duration = p_stream->i_starttime + ( p_stream->i_last_dts - p_stream->i_dts_start );
        }
    }

    /* Update the global segment/media duration */
    for ( unsigned int i=0; i<p_sys->i_nb_streams; i++ )
    {
        if ( p_sys->pp_streams[i]->i_read_duration > p_sys->i_read_duration )
            p_sys->i_read_duration = p_sys->pp_streams[i]->i_read_duration;
    }

    return(VLC_SUCCESS);
}

/*****************************************************************************
 *
 *****************************************************************************/
static block_t *ConvertSUBT(block_t *p_block)
{
    p_block = block_Realloc(p_block, 2, p_block->i_buffer);
    if( !p_block )
        return NULL;
    /* No trailling '\0' */
    if (p_block->i_buffer > 2 && p_block->p_buffer[p_block->i_buffer-1] == '\0')
        p_block->i_buffer--;

    p_block->p_buffer[0] = ((p_block->i_buffer - 2) >> 8)&0xff;
    p_block->p_buffer[1] = ((p_block->i_buffer - 2)     )&0xff;

    return p_block;
}

static block_t *ConvertFromAnnexB(block_t *p_block)
{
    if(p_block->i_buffer < 4)
    {
        block_Release(p_block);
        return NULL;
    }

    if(memcmp(p_block->p_buffer, avc1_start_code, 4))
    {
        if(!memcmp(p_block->p_buffer, avc1_short_start_code, 3))
        {
            p_block = block_Realloc(p_block, 1, p_block->i_buffer);
            if( !p_block )
                return NULL;
        }
        else /* No startcode on start */
        {
            block_Release(p_block);
            return NULL;
        }
    }

    uint8_t *last = p_block->p_buffer;
    uint8_t *dat  = &p_block->p_buffer[4];
    uint8_t *end = &p_block->p_buffer[p_block->i_buffer];

    /* Replace the 4 bytes start code with 4 bytes size */
    while (dat < end) {
        while (dat < end - 4) {
            if (!memcmp(dat, avc1_start_code, 4))
            {
                break;
            }
            else if(!memcmp(dat, avc1_short_start_code, 3))
            {
                /* save offsets as we don't know if realloc will replace buffer */
                size_t i_last = last - p_block->p_buffer;
                size_t i_dat = dat - p_block->p_buffer;
                size_t i_end = end - p_block->p_buffer;

                p_block = block_Realloc(p_block, 0, p_block->i_buffer + 1);
                if( !p_block )
                    return NULL;

                /* restore offsets */
                last = &p_block->p_buffer[i_last];
                dat = &p_block->p_buffer[i_dat];
                end = &p_block->p_buffer[i_end];

                /* Shift data */
                memmove(&dat[4], &dat[3], end - &dat[3]);
                end++;
                break;
            }
            dat++;
        }
        if (dat >= end - 4)
            dat = end;

        /* Fix size */
        SetDWBE(last, dat - &last[4]);

        /* Skip blocks with SPS/PPS */
        //if ((last[4]&0x1f) == 7 || (last[4]&0x1f) == 8)
        //    ; // FIXME Find a way to skip dat without frelling everything
        last = dat;
        dat += 4;
    }
    return p_block;
}

static bo_t *GetESDS(mp4_stream_t *p_stream)
{
    bo_t *esds;
    int64_t i_bitrate_avg = 0;
    int64_t i_bitrate_max = 0;

    /* Compute avg/max bitrate */
    for (unsigned i = 0; i < p_stream->i_entry_count; i++) {
        i_bitrate_avg += p_stream->entry[i].i_size;
        if (p_stream->entry[i].i_length > 0) {
            int64_t i_bitrate = INT64_C(8000000) * p_stream->entry[i].i_size / p_stream->entry[i].i_length;
            if (i_bitrate > i_bitrate_max)
                i_bitrate_max = i_bitrate;
        }
    }

    if (p_stream->i_read_duration > 0)
        i_bitrate_avg = INT64_C(8000000) * i_bitrate_avg / p_stream->i_read_duration;
    else
        i_bitrate_avg = 0;
    if (i_bitrate_max <= 1)
        i_bitrate_max = 0x7fffffff;

    /* */
    int i_decoder_specific_info_size = (p_stream->fmt.i_extra > 0) ? 5 + p_stream->fmt.i_extra : 0;

    esds = box_full_new("esds", 0, 0);
    if(!esds)
        return NULL;

    /* ES_Descr */
    bo_add_mp4_tag_descr(esds, 0x03, 3 + 5 + 13 + i_decoder_specific_info_size + 5 + 1);
    bo_add_16be(esds, p_stream->i_track_id);
    bo_add_8   (esds, 0x1f);      // flags=0|streamPriority=0x1f

    /* DecoderConfigDescr */
    bo_add_mp4_tag_descr(esds, 0x04, 13 + i_decoder_specific_info_size);

    int  i_object_type_indication;
    switch(p_stream->fmt.i_codec)
    {
    case VLC_CODEC_MP4V:
        i_object_type_indication = 0x20;
        break;
    case VLC_CODEC_MP2V:
        /* MPEG-I=0x6b, MPEG-II = 0x60 -> 0x65 */
        i_object_type_indication = 0x65;
        break;
    case VLC_CODEC_MP1V:
        /* MPEG-I=0x6b, MPEG-II = 0x60 -> 0x65 */
        i_object_type_indication = 0x6b;
        break;
    case VLC_CODEC_MP4A:
        /* FIXME for mpeg2-aac == 0x66->0x68 */
        i_object_type_indication = 0x40;
        break;
    case VLC_CODEC_MPGA:
        i_object_type_indication =
            p_stream->fmt.audio.i_rate < 32000 ? 0x69 : 0x6b;
        break;
    default:
        i_object_type_indication = 0x00;
        break;
    }
    int i_stream_type = p_stream->fmt.i_cat == VIDEO_ES ? 0x04 : 0x05;

    bo_add_8   (esds, i_object_type_indication);
    bo_add_8   (esds, (i_stream_type << 2) | 1);
    bo_add_24be(esds, 1024 * 1024);       // bufferSizeDB
    bo_add_32be(esds, i_bitrate_max);     // maxBitrate
    bo_add_32be(esds, i_bitrate_avg);     // avgBitrate

    if (p_stream->fmt.i_extra > 0) {
        /* DecoderSpecificInfo */
        bo_add_mp4_tag_descr(esds, 0x05, p_stream->fmt.i_extra);

        for (int i = 0; i < p_stream->fmt.i_extra; i++)
            bo_add_8(esds, ((uint8_t*)p_stream->fmt.p_extra)[i]);
    }

    /* SL_Descr mandatory */
    bo_add_mp4_tag_descr(esds, 0x06, 1);
    bo_add_8    (esds, 0x02);  // sl_predefined

    return esds;
}

static bo_t *GetWaveTag(mp4_stream_t *p_stream)
{
    bo_t *wave;
    bo_t *box;

    wave = box_new("wave");
    if(wave)
    {
        box = box_new("frma");
        if(box)
        {
            bo_add_fourcc(box, "mp4a");
            box_gather(wave, box);
        }

        box = box_new("mp4a");
        if(box)
        {
            bo_add_32be(box, 0);
            box_gather(wave, box);
        }

        box = GetESDS(p_stream);
        box_gather(wave, box);

        box = box_new("srcq");
        if(box)
        {
            bo_add_32be(box, 0x40);
            box_gather(wave, box);
        }

        /* wazza ? */
        bo_add_32be(wave, 8); /* new empty box */
        bo_add_32be(wave, 0); /* box label */
    }
    return wave;
}

static bo_t *GetDec3Tag(mp4_stream_t *p_stream)
{
    if (!p_stream->a52_frame)
        return NULL;

    bs_t s;
    bs_init(&s, p_stream->a52_frame->p_buffer, sizeof(p_stream->a52_frame->i_buffer));
    bs_skip(&s, 16); // syncword

    uint8_t fscod, bsid, bsmod, acmod, lfeon, strmtyp;

    bsmod = 0;

    strmtyp = bs_read(&s, 2);

    if (strmtyp & 0x1) // dependant or reserved stream
        return NULL;

    if (bs_read(&s, 3) != 0x0) // substreamid: we don't support more than 1 stream
        return NULL;

    int numblkscod;
    bs_skip(&s, 11); // frmsizecod
    fscod = bs_read(&s, 2);
    if (fscod == 0x03) {
        bs_skip(&s, 2); // fscod2
        numblkscod = 3;
    } else {
        numblkscod = bs_read(&s, 2);
    }

    acmod = bs_read(&s, 3);
    lfeon = bs_read1(&s);

    bsid = bs_read(&s, 5);

    bs_skip(&s, 5); // dialnorm
    if (bs_read1(&s)) // compre
        bs_skip(&s, 5); // compr

    if (acmod == 0) {
        bs_skip(&s, 5); // dialnorm2
        if (bs_read1(&s)) // compr2e
            bs_skip(&s, 8); // compr2
    }

    if (strmtyp == 0x1) // dependant stream XXX: unsupported
        if (bs_read1(&s)) // chanmape
            bs_skip(&s, 16); // chanmap

    /* we have to skip mixing info to read bsmod */
    if (bs_read1(&s)) { // mixmdate
        if (acmod > 0x2) // 2+ channels
            bs_skip(&s, 2); // dmixmod
        if ((acmod & 0x1) && (acmod > 0x2)) // 3 front channels
            bs_skip(&s, 3 + 3); // ltrtcmixlev + lorocmixlev
        if (acmod & 0x4) // surround channel
            bs_skip(&s, 3 + 3); // ltrsurmixlev + lorosurmixlev
        if (lfeon)
            if (bs_read1(&s))
                bs_skip(&s, 5); // lfemixlevcod
        if (strmtyp == 0) { // independant stream
            if (bs_read1(&s)) // pgmscle
                bs_skip(&s, 6); // pgmscl
            if (acmod == 0x0) // dual mono
                if (bs_read1(&s)) // pgmscl2e
                    bs_skip(&s, 6); // pgmscl2
            if (bs_read1(&s)) // extpgmscle
                bs_skip(&s, 6); // extpgmscl
            uint8_t mixdef = bs_read(&s, 2);
            if (mixdef == 0x1)
                bs_skip(&s, 5);
            else if (mixdef == 0x2)
                bs_skip(&s, 12);
            else if (mixdef == 0x3) {
                uint8_t mixdeflen = bs_read(&s, 5);
                bs_skip(&s, 8 * (mixdeflen + 2));
            }
            if (acmod < 0x2) { // mono or dual mono
                if (bs_read1(&s)) // paninfoe
                    bs_skip(&s, 14); // paninfo
                if (acmod == 0) // dual mono
                    if (bs_read1(&s)) // paninfo2e
                        bs_skip(&s, 14); // paninfo2
            }
            if (bs_read1(&s)) { // frmmixcfginfoe
                static const int blocks[4] = { 1, 2, 3, 6 };
                int number_of_blocks = blocks[numblkscod];
                if (number_of_blocks == 1)
                    bs_skip(&s, 5); // blkmixcfginfo[0]
                else for (int i = 0; i < number_of_blocks; i++)
                    if (bs_read1(&s)) // blkmixcfginfoe
                        bs_skip(&s, 5); // blkmixcfginfo[i]
            }
        }
    }

    if (bs_read1(&s)) // infomdate
        bsmod = bs_read(&s, 3);

    uint8_t mp4_eac3_header[5];
    bs_init(&s, mp4_eac3_header, sizeof(mp4_eac3_header));

    int data_rate = p_stream->fmt.i_bitrate / 1000;
    bs_write(&s, 13, data_rate);
    bs_write(&s, 3, 0); // num_ind_sub - 1
    bs_write(&s, 2, fscod);
    bs_write(&s, 5, bsid);
    bs_write(&s, 5, bsmod);
    bs_write(&s, 3, acmod);
    bs_write(&s, 1, lfeon);
    bs_write(&s, 3, 0); // reserved
    bs_write(&s, 4, 0); // num_dep_sub
    bs_write(&s, 1, 0); // reserved

    bo_t *dec3 = box_new("dec3");
    if(dec3)
        bo_add_mem(dec3, sizeof(mp4_eac3_header), mp4_eac3_header);

    return dec3;
}

static bo_t *GetDac3Tag(mp4_stream_t *p_stream)
{
    if (!p_stream->a52_frame)
        return NULL;

    bo_t *dac3 = box_new("dac3");
    if(!dac3)
        return NULL;

    bs_t s;
    bs_init(&s, p_stream->a52_frame->p_buffer, sizeof(p_stream->a52_frame->i_buffer));

    uint8_t fscod, bsid, bsmod, acmod, lfeon, frmsizecod;

    bs_skip(&s, 16 + 16); // syncword + crc

    fscod = bs_read(&s, 2);
    frmsizecod = bs_read(&s, 6);
    bsid = bs_read(&s, 5);
    bsmod = bs_read(&s, 3);
    acmod = bs_read(&s, 3);
    if (acmod == 2)
        bs_skip(&s, 2); // dsurmod
    else {
        if ((acmod & 1) && acmod != 1)
            bs_skip(&s, 2); // cmixlev
        if (acmod & 4)
            bs_skip(&s, 2); // surmixlev
    }

    lfeon = bs_read1(&s);

    uint8_t mp4_a52_header[3];
    bs_init(&s, mp4_a52_header, sizeof(mp4_a52_header));

    bs_write(&s, 2, fscod);
    bs_write(&s, 5, bsid);
    bs_write(&s, 3, bsmod);
    bs_write(&s, 3, acmod);
    bs_write(&s, 1, lfeon);
    bs_write(&s, 5, frmsizecod >> 1); // bit_rate_code
    bs_write(&s, 5, 0); // reserved

    bo_add_mem(dac3, sizeof(mp4_a52_header), mp4_a52_header);

    return dac3;
}

static bo_t *GetDamrTag(mp4_stream_t *p_stream)
{
    bo_t *damr = box_new("damr");
    if(!damr)
        return NULL;

    bo_add_fourcc(damr, "REFC");
    bo_add_8(damr, 0);

    if (p_stream->fmt.i_codec == VLC_CODEC_AMR_NB)
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

static void hevcParseVPS(uint8_t * p_buffer, size_t i_buffer, uint8_t *general,
                         uint8_t * numTemporalLayer, bool * temporalIdNested)
{
    const size_t i_decoded_nal_size = 512;
    uint8_t p_dec_nal[i_decoded_nal_size];
    size_t i_size = (i_buffer < i_decoded_nal_size)?i_buffer:i_decoded_nal_size;
    nal_decode(p_buffer, p_dec_nal, i_size);

    /* first two bytes are the NAL header, 3rd and 4th are:
        vps_video_parameter_set_id(4)
        vps_reserved_3_2bis(2)
        vps_max_layers_minus1(6)
        vps_max_sub_layers_minus1(3)
        vps_temporal_id_nesting_flags
    */
    *numTemporalLayer =  ((p_dec_nal[3] & 0x0E) >> 1) + 1;
    *temporalIdNested = (bool)(p_dec_nal[3] & 0x01);

    /* 5th & 6th are reserved 0xffff */
    /* copy the first 12 bytes of profile tier */
    memcpy(general, &p_dec_nal[6], 12);
}

static void hevcParseSPS(uint8_t * p_buffer, size_t i_buffer, uint8_t * chroma_idc,
                         uint8_t *bit_depth_luma_minus8, uint8_t *bit_depth_chroma_minus8)
{
    const size_t i_decoded_nal_size = 512;
    uint8_t p_dec_nal[i_decoded_nal_size];
    size_t i_size = (i_buffer < i_decoded_nal_size)?i_buffer-2:i_decoded_nal_size;
    nal_decode(p_buffer+2, p_dec_nal, i_size);
    bs_t bs;
    bs_init(&bs, p_dec_nal, i_size);

    /* skip vps id */
    bs_skip(&bs, 4);
    uint32_t sps_max_sublayer_minus1 = bs_read(&bs, 3);

    /* skip nesting flag */
    bs_skip(&bs, 1);

    hevc_skip_profile_tiers_level(&bs, sps_max_sublayer_minus1);

    /* skip sps id */
    (void) bs_read_ue( &bs );

    *chroma_idc = bs_read_ue(&bs);
    if (*chroma_idc == 3)
        bs_skip(&bs, 1);

    /* skip width and heigh */
    (void) bs_read_ue( &bs );
    (void) bs_read_ue( &bs );

    uint32_t conformance_window_flag = bs_read1(&bs);
    if (conformance_window_flag) {
        /* skip offsets*/
        (void) bs_read_ue(&bs);
        (void) bs_read_ue(&bs);
        (void) bs_read_ue(&bs);
        (void) bs_read_ue(&bs);
    }
    *bit_depth_luma_minus8 = bs_read_ue(&bs);
    *bit_depth_chroma_minus8 = bs_read_ue(&bs);
}

static bo_t *GetHvcCTag(mp4_stream_t *p_stream)
{
    /* Generate hvcC box matching iso/iec 14496-15 3rd edition */
    bo_t *hvcC = box_new("hvcC");
    if(!hvcC || !p_stream->fmt.i_extra)
        return hvcC;

    struct nal {
        size_t i_buffer;
        uint8_t * p_buffer;
    };

    /* According to the specification HEVC stream can have
     * 16 vps id and an "unlimited" number of sps and pps id using ue(v) id*/
    struct nal p_vps[16], *p_sps = NULL, *p_pps = NULL, *p_sei = NULL,
               *p_nal = NULL;
    size_t i_vps = 0, i_sps = 0, i_pps = 0, i_sei = 0;
    uint8_t i_num_arrays = 0;

    uint8_t * p_buffer = p_stream->fmt.p_extra;
    size_t i_buffer = p_stream->fmt.i_extra;

    uint8_t general_configuration[12] = {0};
    uint8_t i_numTemporalLayer = 0;
    uint8_t i_chroma_idc = 1;
    uint8_t i_bit_depth_luma_minus8 = 0;
    uint8_t i_bit_depth_chroma_minus8 = 0;
    bool b_temporalIdNested = false;

    uint32_t cmp = 0xFFFFFFFF;
    while (i_buffer) {
        /* look for start code 0X0000001 */
        while (i_buffer) {
            cmp = (cmp << 8) | *p_buffer;
            if((cmp ^ UINT32_C(0x100)) <= UINT32_C(0xFF))
                break;
            p_buffer++;
            i_buffer--;
        }
        if (p_nal)
            p_nal->i_buffer = p_buffer - p_nal->p_buffer - ((i_buffer)?3:0);

        switch (*p_buffer & 0x72) {
            /* VPS */
        case 0x40:
            p_nal = &p_vps[i_vps++];
            p_nal->p_buffer = p_buffer;
            /* Only keep the general profile from the first VPS
             * if there are several (this shouldn't happen so soon) */
            if (i_vps == 1) {
                hevcParseVPS(p_buffer, i_buffer, general_configuration,
                             &i_numTemporalLayer, &b_temporalIdNested);
                i_num_arrays++;
            }
            break;
            /* SPS */
        case 0x42: {
            struct nal * p_tmp =  realloc(p_sps, sizeof(struct nal) * (i_sps + 1));
            if (!p_tmp)
                break;
            p_sps = p_tmp;
            p_nal = &p_sps[i_sps++];
            p_nal->p_buffer = p_buffer;
            if (i_sps == 1 && i_buffer > 15) {
                /* Get Chroma_idc and bitdepths */
                hevcParseSPS(p_buffer, i_buffer, &i_chroma_idc,
                             &i_bit_depth_luma_minus8, &i_bit_depth_chroma_minus8);
                i_num_arrays++;
            }
            break;
            }
        /* PPS */
        case 0x44: {
            struct nal * p_tmp =  realloc(p_pps, sizeof(struct nal) * (i_pps + 1));
            if (!p_tmp)
                break;
            p_pps = p_tmp;
            p_nal = &p_pps[i_pps++];
            p_nal->p_buffer = p_buffer;
            if (i_pps == 1)
                i_num_arrays++;
            break;
            }
        /* SEI */
        case 0x4E:
        case 0x50: {
            struct nal * p_tmp =  realloc(p_sei, sizeof(struct nal) * (i_sei + 1));
            if (!p_tmp)
                break;
            p_sei = p_tmp;
            p_nal = &p_sei[i_sei++];
            p_nal->p_buffer = p_buffer;
            if(i_sei == 1)
                i_num_arrays++;
            break;
        }
        default:
            p_nal = NULL;
            break;
        }
    }
    bo_add_8(hvcC, 0x01);
    bo_add_mem(hvcC, 12, general_configuration);
    /* Don't set min spatial segmentation */
    bo_add_16be(hvcC, 0xF000);
    /* Don't set parallelism type since segmentation isn't set */
    bo_add_8(hvcC, 0xFC);
    bo_add_8(hvcC, (0xFC | (i_chroma_idc & 0x03)));
    bo_add_8(hvcC, (0xF8 | (i_bit_depth_luma_minus8 & 0x07)));
    bo_add_8(hvcC, (0xF8 | (i_bit_depth_chroma_minus8 & 0x07)));

    /* Don't set framerate */
    bo_add_16be(hvcC, 0x0000);
    /* Force NAL size of 4 bytes that replace the startcode */
    bo_add_8(hvcC, (((i_numTemporalLayer & 0x07) << 3) |
                    (b_temporalIdNested << 2) | 0x03));
    bo_add_8(hvcC, i_num_arrays);

    if (i_vps)
    {
        /* Write VPS without forcing array_completeness */
        bo_add_8(hvcC, 32);
        bo_add_16be(hvcC, i_vps);
        for (size_t i = 0; i < i_vps; i++) {
            p_nal = &p_vps[i];
            bo_add_16be(hvcC, p_nal->i_buffer);
            bo_add_mem(hvcC, p_nal->i_buffer, p_nal->p_buffer);
        }
    }

    if (i_sps) {
        /* Write SPS without forcing array_completeness */
        bo_add_8(hvcC, 33);
        bo_add_16be(hvcC, i_sps);
        for (size_t i = 0; i < i_sps; i++) {
            p_nal = &p_sps[i];
            bo_add_16be(hvcC, p_nal->i_buffer);
            bo_add_mem(hvcC, p_nal->i_buffer, p_nal->p_buffer);
        }
    }

    if (i_pps) {
        /* Write PPS without forcing array_completeness */
        bo_add_8(hvcC, 34);
        bo_add_16be(hvcC, i_pps);
        for (size_t i = 0; i < i_pps; i++) {
            p_nal = &p_pps[i];
            bo_add_16be(hvcC, p_nal->i_buffer);
            bo_add_mem(hvcC, p_nal->i_buffer, p_nal->p_buffer);
        }
    }

    if (i_sei) {
        /* Write SEI without forcing array_completeness */
        bo_add_8(hvcC, 39);
        bo_add_16be(hvcC, i_sei);
        for (size_t i = 0; i < i_sei; i++) {
            p_nal = &p_sei[i];
            bo_add_16be(hvcC, p_nal->i_buffer);
            bo_add_mem(hvcC, p_nal->i_buffer, p_nal->p_buffer);
        }
    }
    return hvcC;
}

static bo_t *GetAvcCTag(mp4_stream_t *p_stream)
{
    bo_t    *avcC = box_new("avcC");/* FIXME use better value */
    if(!avcC)
        return NULL;
    uint8_t *p_sps = NULL;
    uint8_t *p_pps = NULL;
    int     i_sps_size = 0;
    int     i_pps_size = 0;

    if (p_stream->fmt.i_extra > 0) {
        /* FIXME: take into account multiple sps/pps */
        uint8_t *p_buffer = p_stream->fmt.p_extra;
        int     i_buffer = p_stream->fmt.i_extra;

        while (i_buffer > 3) {
            while (memcmp(p_buffer, &avc1_start_code[1], 3)) {
                 i_buffer--;
                 p_buffer++;
            }
            const int i_nal_type = p_buffer[3]&0x1f;
            int i_startcode = 0;

            for (int i_offset = 1; i_offset+2 < i_buffer ; i_offset++)
                if (!memcmp(&p_buffer[i_offset], &avc1_start_code[1], 3)) {
                    /* we found another startcode */
                    i_startcode = i_offset;
                    while (p_buffer[i_startcode-1] == 0 && i_startcode > 0)
                        i_startcode--;
                    break;
                }

            int i_size = i_startcode ? i_startcode : i_buffer;

            if (i_nal_type == 7) {
                p_sps = &p_buffer[3];
                i_sps_size = i_size - 3;
            }
            if (i_nal_type == 8) {
                p_pps = &p_buffer[3];
                i_pps_size = i_size - 3;
            }
            i_buffer -= i_size;
            p_buffer += i_size;
        }
    }

    bo_add_8(avcC, 1);      /* configuration version */
    bo_add_8(avcC, i_sps_size ? p_sps[1] : 77);
    bo_add_8(avcC, i_sps_size ? p_sps[2] : 64);
    bo_add_8(avcC, i_sps_size ? p_sps[3] : 30);       /* level, 5.1 */
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

    return avcC;
}

/* TODO: No idea about these values */
static bo_t *GetSVQ3Tag(mp4_stream_t *p_stream)
{
    bo_t *smi = box_new("SMI ");
    if(!smi)
        return NULL;

    if (p_stream->fmt.i_extra > 0x4e) {
        uint8_t *p_end = &((uint8_t*)p_stream->fmt.p_extra)[p_stream->fmt.i_extra];
        uint8_t *p     = &((uint8_t*)p_stream->fmt.p_extra)[0x46];

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

static bo_t *GetUdtaTag(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bo_t *udta = box_new("udta");
    if (!udta)
        return NULL;

    /* Requirements */
    for (unsigned int i_track = 0; i_track < p_sys->i_nb_streams; i_track++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_track];
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

static bo_t *GetSounBox(sout_mux_t *p_mux, mp4_stream_t *p_stream)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bool b_descr = true;
    vlc_fourcc_t codec = p_stream->fmt.i_codec;
    char fcc[4];

    if (codec == VLC_CODEC_MPGA) {
        if (p_sys->b_mov) {
            b_descr = false;
            memcpy(fcc, ".mp3", 4);
        } else
            memcpy(fcc, "mp4a", 4);
    } else if (codec == VLC_CODEC_A52) {
        memcpy(fcc, "ac-3", 4);
    } else if (codec == VLC_CODEC_EAC3) {
        memcpy(fcc, "ec-3", 4);
    } else
        vlc_fourcc_to_char(codec, fcc);

    bo_t *soun = box_new(fcc);
    if(!soun)
        return NULL;
    for (int i = 0; i < 6; i++)
        bo_add_8(soun, 0);        // reserved;
    bo_add_16be(soun, 1);         // data-reference-index

    /* SoundDescription */
    if (p_sys->b_mov && codec == VLC_CODEC_MP4A)
        bo_add_16be(soun, 1);     // version 1;
    else
        bo_add_16be(soun, 0);     // version 0;
    bo_add_16be(soun, 0);         // revision level (0)
    bo_add_32be(soun, 0);         // vendor
    // channel-count
    bo_add_16be(soun, p_stream->fmt.audio.i_channels);
    // sample size
    bo_add_16be(soun, p_stream->fmt.audio.i_bitspersample ?
                 p_stream->fmt.audio.i_bitspersample : 16);
    bo_add_16be(soun, -2);        // compression id
    bo_add_16be(soun, 0);         // packet size (0)
    bo_add_16be(soun, p_stream->fmt.audio.i_rate); // sampleratehi
    bo_add_16be(soun, 0);                             // sampleratelo

    /* Extended data for SoundDescription V1 */
    if (p_sys->b_mov && p_stream->fmt.i_codec == VLC_CODEC_MP4A) {
        /* samples per packet */
        bo_add_32be(soun, p_stream->fmt.audio.i_frame_length);
        bo_add_32be(soun, 1536); /* bytes per packet */
        bo_add_32be(soun, 2);    /* bytes per frame */
        /* bytes per sample */
        bo_add_32be(soun, 2 /*p_stream->fmt.audio.i_bitspersample/8 */);
    }

    /* Add an ES Descriptor */
    if (b_descr) {
        bo_t *box;

        if (p_sys->b_mov && codec == VLC_CODEC_MP4A)
            box = GetWaveTag(p_stream);
        else if (codec == VLC_CODEC_AMR_NB)
            box = GetDamrTag(p_stream);
        else if (codec == VLC_CODEC_A52)
            box = GetDac3Tag(p_stream);
        else if (codec == VLC_CODEC_EAC3)
            box = GetDec3Tag(p_stream);
        else
            box = GetESDS(p_stream);

        if (box)
            box_gather(soun, box);
    }

    return soun;
}

static bo_t *GetVideBox(mp4_stream_t *p_stream)
{
    char fcc[4];

    switch(p_stream->fmt.i_codec)
    {
    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGV: memcpy(fcc, "mp4v", 4); break;
    case VLC_CODEC_MJPG: memcpy(fcc, "mjpa", 4); break;
    case VLC_CODEC_SVQ1: memcpy(fcc, "SVQ1", 4); break;
    case VLC_CODEC_SVQ3: memcpy(fcc, "SVQ3", 4); break;
    case VLC_CODEC_H263: memcpy(fcc, "s263", 4); break;
    case VLC_CODEC_H264: memcpy(fcc, "avc1", 4); break;
    case VLC_CODEC_HEVC: memcpy(fcc, "hvc1", 4); break;
    case VLC_CODEC_YV12: memcpy(fcc, "yv12", 4); break;
    case VLC_CODEC_YUYV: memcpy(fcc, "yuy2", 4); break;
    default:
        vlc_fourcc_to_char(p_stream->fmt.i_codec, fcc);
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

    bo_add_16be(vide, p_stream->fmt.video.i_width);  // i_width
    bo_add_16be(vide, p_stream->fmt.video.i_height); // i_height

    bo_add_32be(vide, 0x00480000);                // h 72dpi
    bo_add_32be(vide, 0x00480000);                // v 72dpi

    bo_add_32be(vide, 0);         // data size, always 0
    bo_add_16be(vide, 1);         // frames count per sample

    // compressor name;
    for (int i = 0; i < 32; i++)
        bo_add_8(vide, 0);

    bo_add_16be(vide, 0x18);      // depth
    bo_add_16be(vide, 0xffff);    // predefined

    /* add an ES Descriptor */
    switch(p_stream->fmt.i_codec)
    {
    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGV:
        box_gather(vide, GetESDS(p_stream));
        break;

    case VLC_CODEC_H263:
        box_gather(vide, GetD263Tag());
        break;

    case VLC_CODEC_SVQ3:
        box_gather(vide, GetSVQ3Tag(p_stream));
        break;

    case VLC_CODEC_H264:
        box_gather(vide, GetAvcCTag(p_stream));
        break;

    case VLC_CODEC_HEVC:
        box_gather(vide, GetHvcCTag(p_stream));
        break;
    }

    return vide;
}

static bo_t *GetTextBox(void)
{
    bo_t *text = box_new("text");
    if(!text)
        return NULL;

    for (int i = 0; i < 6; i++)
        bo_add_8(text, 0);        // reserved;
    bo_add_16be(text, 1);         // data-reference-index

    bo_add_32be(text, 0);         // display flags
    bo_add_32be(text, 0);         // justification
    for (int i = 0; i < 3; i++)
        bo_add_16be(text, 0);     // back ground color

    bo_add_16be(text, 0);         // box text
    bo_add_16be(text, 0);         // box text
    bo_add_16be(text, 0);         // box text
    bo_add_16be(text, 0);         // box text

    bo_add_64be(text, 0);         // reserved
    for (int i = 0; i < 3; i++)
        bo_add_16be(text, 0xff);  // foreground color

    bo_add_8 (text, 9);
    bo_add_mem(text, 9, (uint8_t*)"Helvetica");

    return text;
}

static bo_t *GetStblBox(sout_mux_t *p_mux, mp4_stream_t *p_stream)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    /* sample description */
    bo_t *stsd = box_full_new("stsd", 0, 0);
    if(!stsd)
        return NULL;
    bo_add_32be(stsd, 1);
    if (p_stream->fmt.i_cat == AUDIO_ES)
        box_gather(stsd, GetSounBox(p_mux, p_stream));
    else if (p_stream->fmt.i_cat == VIDEO_ES)
        box_gather(stsd, GetVideBox(p_stream));
    else if (p_stream->fmt.i_cat == SPU_ES)
        box_gather(stsd, GetTextBox());

    /* chunk offset table */
    bo_t *stco;
    if (p_sys->i_pos >= (((uint64_t)0x1) << 32)) {
        /* 64 bits version */
        p_stream->b_stco64 = true;
        stco = box_full_new("co64", 0, 0);
    } else {
        /* 32 bits version */
        p_stream->b_stco64 = false;
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
    bo_add_32be(stsc, 0);     // entry-count (fixed latter)

    unsigned i_chunk = 0;
    unsigned i_stsc_last_val = 0, i_stsc_entries = 0;
    for (unsigned i = 0; i < p_stream->i_entry_count; i_chunk++) {
        mp4_entry_t *entry = p_stream->entry;
        int i_first = i;

        if (p_stream->b_stco64)
            bo_add_64be(stco, entry[i].i_pos);
        else
            bo_add_32be(stco, entry[i].i_pos);

        for (; i < p_stream->i_entry_count; i++)
            if (i >= p_stream->i_entry_count - 1 ||
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
    msg_Dbg(p_mux, "created %d chunks (stco)", i_chunk);

    /* Fix stsc entry count */
    bo_swap_32be(stsc, 12, i_stsc_entries );

    /* add stts */
    bo_t *stts = box_full_new("stts", 0, 0);
    if(!stts)
    {
        bo_free(stsd);
        bo_free(stco);
        return NULL;
    }
    bo_add_32be(stts, 0);     // entry-count (fixed latter)

    unsigned i_index = 0;
    for (unsigned i = 0; i < p_stream->i_entry_count; i_index++) {
        int     i_first = i;
        mtime_t i_delta = p_stream->entry[i].i_length;

        for (; i < p_stream->i_entry_count; ++i)
            if (i == p_stream->i_entry_count || p_stream->entry[i].i_length != i_delta)
                break;

        bo_add_32be(stts, i - i_first); // sample-count
        bo_add_32be(stts, (uint64_t)i_delta  * p_stream->i_timescale / CLOCK_FREQ); // sample-delta
    }
    bo_swap_32be(stts, 12, i_index);

    /* composition time handling */
    bo_t *ctts = NULL;
    if ( p_stream->b_hasbframes && (ctts = box_full_new("ctts", 0, 0)) )
    {
        bo_add_32be(ctts, 0);
        i_index = 0;
        for (unsigned i = 0; i < p_stream->i_entry_count; i_index++)
        {
            int     i_first = i;
            mtime_t i_offset = p_stream->entry[i].i_pts_dts;

            for (; i < p_stream->i_entry_count; ++i)
                if (i == p_stream->i_entry_count || p_stream->entry[i].i_pts_dts != i_offset)
                    break;

            bo_add_32be(ctts, i - i_first); // sample-count
            bo_add_32be(ctts, i_offset * p_stream->i_timescale / CLOCK_FREQ ); // sample-offset
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
    for (unsigned i = 0; i < p_stream->i_entry_count; i++)
    {
        if ( i == 0 )
            i_size = p_stream->entry[i].i_size;
        else if ( p_stream->entry[i].i_size != i_size )
        {
            i_size = 0;
            break;
        }
    }
    bo_add_32be(stsz, i_size);                         // sample-size
    bo_add_32be(stsz, p_stream->i_entry_count);       // sample-count
    if ( i_size == 0 ) // all samples have different size
    {
        for (unsigned i = 0; i < p_stream->i_entry_count; i++)
            bo_add_32be(stsz, p_stream->entry[i].i_size); // sample-size
    }

    /* create stss table */
    bo_t *stss = NULL;
    i_index = 0;
    if ( p_stream->fmt.i_cat == VIDEO_ES || p_stream->fmt.i_cat == AUDIO_ES )
    {
        mtime_t i_interval = -1;
        for (unsigned i = 0; i < p_stream->i_entry_count; i++)
        {
            if ( i_interval != -1 )
            {
                i_interval += p_stream->entry[i].i_length + p_stream->entry[i].i_pts_dts;
                if ( i_interval < CLOCK_FREQ * 2 )
                    continue;
            }

            if (p_stream->entry[i].i_flags & BLOCK_FLAG_TYPE_I) {
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
    p_stream->i_stco_pos = stbl->b->i_buffer + 16;
    box_gather(stbl, stco);

    return stbl;
}

static int64_t get_timestamp(void);

static void matrix_apply_rotation(es_format_t *fmt, uint32_t mvhd_matrix[9])
{
    enum video_orientation_t orientation = ORIENT_NORMAL;
    if (fmt->i_cat == VIDEO_ES)
        orientation = fmt->video.orientation;

#define ATAN(a, b) do { mvhd_matrix[1] = (a) << 16; \
    mvhd_matrix[0] = (b) << 16; \
    } while(0)

    switch (orientation) {
    case ORIENT_ROTATED_90:  ATAN( 1,  0); break;
    case ORIENT_ROTATED_180: ATAN( 0, -1); break;
    case ORIENT_ROTATED_270: ATAN( -1, 0); break;
    default:                 ATAN( 0,  1); break;
    }

    mvhd_matrix[3] = mvhd_matrix[0] ? 0 : 0x10000;
    mvhd_matrix[4] = mvhd_matrix[1] ? 0 : 0x10000;
}

static bo_t *GetMoovBox(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    bo_t            *moov, *mvhd;

    uint32_t        i_movie_timescale = 90000;
    int64_t         i_movie_duration  = 0;
    int64_t         i_timestamp = get_timestamp();

    moov = box_new("moov");
    if(!moov)
        return NULL;
    /* Create general info */
    if ( !p_sys->b_fragmented )
    {
        for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
            mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
            i_movie_duration = __MAX(i_movie_duration, p_stream->i_read_duration);
        }
        msg_Dbg(p_mux, "movie duration %"PRId64"s", i_movie_duration / CLOCK_FREQ);

        i_movie_duration = i_movie_duration * i_movie_timescale / CLOCK_FREQ;
    }
    else
        i_movie_duration = 0;

    /* *** add /moov/mvhd *** */
    if (!p_sys->b_64_ext) {
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
    bo_add_32be(mvhd, p_sys->i_nb_streams + 1); // next-track-id

    box_gather(moov, mvhd);

    for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        mtime_t i_stream_duration;
        if ( !p_sys->b_fragmented )
            i_stream_duration = p_stream->i_read_duration * i_movie_timescale / CLOCK_FREQ;
        else
            i_stream_duration = 0;

        /* *** add /moov/trak *** */
        bo_t *trak = box_new("trak");
        if(!trak)
            continue;

        /* *** add /moov/trak/tkhd *** */
        bo_t *tkhd;
        if (!p_sys->b_64_ext) {
            if (p_sys->b_mov)
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
            if (p_sys->b_mov)
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
            int i_width = p_stream->fmt.video.i_width << 16;
            if (p_stream->fmt.video.i_sar_num > 0 && p_stream->fmt.video.i_sar_den > 0) {
                i_width = (int64_t)p_stream->fmt.video.i_sar_num *
                          ((int64_t)p_stream->fmt.video.i_width << 16) /
                          p_stream->fmt.video.i_sar_den;
            }
            // width (presentation)
            bo_add_32be(tkhd, i_width);
            // height(presentation)
            bo_add_32be(tkhd, p_stream->fmt.video.i_height << 16);
        } else {
            int i_width = 320 << 16;
            int i_height = 200;
            for (unsigned int i = 0; i < p_sys->i_nb_streams; i++) {
                mp4_stream_t *tk = p_sys->pp_streams[i];
                if (tk->fmt.i_cat == VIDEO_ES) {
                    if (tk->fmt.video.i_sar_num > 0 &&
                        tk->fmt.video.i_sar_den > 0)
                        i_width = (int64_t)tk->fmt.video.i_sar_num *
                                  ((int64_t)tk->fmt.video.i_width << 16) /
                                  tk->fmt.video.i_sar_den;
                    else
                        i_width = tk->fmt.video.i_width << 16;
                    i_height = tk->fmt.video.i_height;
                    break;
                }
            }
            bo_add_32be(tkhd, i_width);     // width (presentation)
            bo_add_32be(tkhd, i_height << 16);    // height(presentation)
        }

        box_gather(trak, tkhd);

        /* *** add /moov/trak/edts and elst */
        if ( !p_sys->b_fragmented )
        {
            bo_t *elst = box_full_new("elst", p_sys->b_64_ext ? 1 : 0, 0);
            if(elst)
            {
                if (p_stream->i_starttime > 0) {
                    bo_add_32be(elst, 2);

                    if (p_sys->b_64_ext) {
                        bo_add_64be(elst, p_stream->i_starttime *
                                    i_movie_timescale / CLOCK_FREQ);
                        bo_add_64be(elst, -1);
                    } else {
                        bo_add_32be(elst, p_stream->i_starttime *
                                    i_movie_timescale / CLOCK_FREQ);
                        bo_add_32be(elst, -1);
                    }
                    bo_add_16be(elst, 1);
                    bo_add_16be(elst, 0);
                } else {
                    bo_add_32be(elst, 1);
                }
                if (p_sys->b_64_ext) {
                    bo_add_64be(elst, p_stream->i_read_duration *
                                i_movie_timescale / CLOCK_FREQ);
                    bo_add_64be(elst, 0);
                } else {
                    bo_add_32be(elst, p_stream->i_read_duration *
                                i_movie_timescale / CLOCK_FREQ);
                    bo_add_32be(elst, 0);
                }
                bo_add_16be(elst, 1);
                bo_add_16be(elst, 0);

                bo_t *edts = box_new("edts");
                if(edts)
                {
                    box_gather(edts, elst);
                    box_gather(trak, edts);
                }
                else bo_free(elst);
            }
        }

        /* *** add /moov/trak/mdia *** */
        bo_t *mdia = box_new("mdia");
        if(!mdia)
        {
            bo_free(trak);
            continue;
        }

        /* media header */
        bo_t *mdhd;
        if (!p_sys->b_64_ext) {
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
            bo_add_32be(mdhd, i_stream_duration);  // duration
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
            bo_add_64be(mdhd, i_stream_duration);  // duration
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

        if (p_sys->b_mov)
            bo_add_fourcc(hdlr, "mhlr");         // media handler
        else
            bo_add_32be(hdlr, 0);

        if (p_stream->fmt.i_cat == AUDIO_ES)
            bo_add_fourcc(hdlr, "soun");
        else if (p_stream->fmt.i_cat == VIDEO_ES)
            bo_add_fourcc(hdlr, "vide");
        else if (p_stream->fmt.i_cat == SPU_ES)
            bo_add_fourcc(hdlr, "text");

        bo_add_32be(hdlr, 0);         // reserved
        bo_add_32be(hdlr, 0);         // reserved
        bo_add_32be(hdlr, 0);         // reserved

        if (p_sys->b_mov)
            bo_add_8(hdlr, 12);   /* Pascal string for .mov */

        if (p_stream->fmt.i_cat == AUDIO_ES)
            bo_add_mem(hdlr, 12, (uint8_t*)"SoundHandler");
        else if (p_stream->fmt.i_cat == VIDEO_ES)
            bo_add_mem(hdlr, 12, (uint8_t*)"VideoHandler");
        else
            bo_add_mem(hdlr, 12, (uint8_t*)"Text Handler");

        if (!p_sys->b_mov)
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
            else bo_free(dinf);
        }

        /* add stbl */
        bo_t *stbl;
        if ( p_sys->b_fragmented )
        {
            uint32_t i_backup = p_stream->i_entry_count;
            p_stream->i_entry_count = 0;
            stbl = GetStblBox(p_mux, p_stream);
            p_stream->i_entry_count = i_backup;
        }
        else
            stbl = GetStblBox(p_mux, p_stream);

        /* append stbl to minf */
        p_stream->i_stco_pos += minf->b->i_buffer;
        box_gather(minf, stbl);

        /* append minf to mdia */
        p_stream->i_stco_pos += mdia->b->i_buffer;
        box_gather(mdia, minf);

        /* append mdia to trak */
        p_stream->i_stco_pos += trak->b->i_buffer;
        box_gather(trak, mdia);

        /* append trak to moov */
        p_stream->i_stco_pos += moov->b->i_buffer;
        box_gather(moov, trak);
    }

    /* Add user data tags */
    box_gather(moov, GetUdtaTag(p_mux));

    if ( p_sys->b_fragmented )
    {
        bo_t *mvex = box_new("mvex");
        for (unsigned int i_trak = 0; mvex && i_trak < p_sys->i_nb_streams; i_trak++)
        {
            mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

            /* Try to find some defaults */
            if ( p_stream->i_entry_count )
            {
                // FIXME: find highest occurence
                p_stream->i_trex_length = p_stream->entry[0].i_length;
                p_stream->i_trex_size = p_stream->entry[0].i_size;
            }

            /* *** add /mvex/trex *** */
            bo_t *trex = box_full_new("trex", 0, 0);
            bo_add_32be(trex, p_stream->i_track_id);
            bo_add_32be(trex, 1); // sample desc index
            bo_add_32be(trex, (uint64_t)p_stream->i_trex_length * p_stream->i_timescale / CLOCK_FREQ); // sample duration
            bo_add_32be(trex, p_stream->i_trex_size); // sample size
            bo_add_32be(trex, 0); // sample flags
            box_gather(mvex, trex);
        }
        box_gather(moov, mvex);
    }

    if(moov->b)
        box_fix(moov, moov->b->i_buffer);
    return moov;
}

/****************************************************************************/

static bo_t *box_new(const char *fcc)
{
    bo_t *box = malloc(sizeof(*box));
    if (!box)
        return NULL;

    bo_init(box, 1024);

    bo_add_32be  (box, 0);
    bo_add_fourcc(box, fcc);

    return box;
}

static bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f)
{
    bo_t *box = box_new(fcc);
    if (!box)
        return NULL;

    bo_add_8     (box, v);
    bo_add_24be  (box, f);

    return box;
}

static void box_fix(bo_t *box, uint32_t i_size)
{
    bo_set_32be(box, 0, i_size);
}

static void box_gather (bo_t *box, bo_t *box2)
{
    if(box2 && box2->b && box && box->b)
    {
        box_fix(box2, box2->b->i_buffer);
        size_t i_offset = box->b->i_buffer;
        box->b = block_Realloc(box->b, 0, box->b->i_buffer + box2->b->i_buffer);
        memcpy(&box->b->p_buffer[i_offset], box2->b->p_buffer, box2->b->i_buffer);
    }
    bo_free(box2);
}

static void box_send(sout_mux_t *p_mux,  bo_t *box)
{
    if(box && box->b)
        sout_AccessOutWrite(p_mux->p_access, box->b);
    free(box);
}

static int64_t get_timestamp(void)
{
    int64_t i_timestamp = time(NULL);

    i_timestamp += 2082844800; // MOV/MP4 start date is 1/1/1904
    // 208284480 is (((1970 - 1904) * 365) + 17) * 24 * 60 * 60

    return i_timestamp;
}

/***************************************************************************
    MP4 Live submodule
****************************************************************************/
#define FRAGMENT_LENGTH  (CLOCK_FREQ * 3/2)

#define ENQUEUE_ENTRY(object, entry) \
    do {\
        if (object.p_last)\
            object.p_last->p_next = entry;\
        object.p_last = entry;\
        if (!object.p_first)\
            object.p_first = entry;\
    } while(0)

#define DEQUEUE_ENTRY(object, entry) \
    do {\
        entry = object.p_first;\
        if (object.p_last == entry)\
            object.p_last = NULL;\
        object.p_first = object.p_first->p_next;\
        entry->p_next = NULL;\
    } while(0)

/* Creates mfra/traf index entries */
static void AddKeyframeEntry(mp4_stream_t *p_stream, const uint64_t i_moof_pos,
                             const uint8_t i_traf, const uint32_t i_sample,
                             const mtime_t i_time)
{
    /* alloc or realloc */
    mp4_fragindex_t *p_entries = p_stream->p_indexentries;
    if (p_stream->i_indexentries >= p_stream->i_indexentriesmax)
    {
        p_stream->i_indexentriesmax += 256;
        p_entries = xrealloc(p_stream->p_indexentries,
                             p_stream->i_indexentriesmax * sizeof(mp4_fragindex_t));
        if (p_entries) /* realloc can fail */
            p_stream->p_indexentries = p_entries;
    }

    mtime_t i_last_entry_time;
    if (p_stream->i_indexentries)
        i_last_entry_time = p_stream->p_indexentries[p_stream->i_indexentries - 1].i_time;
    else
        i_last_entry_time = 0;

    if (p_entries && i_time - i_last_entry_time >= CLOCK_FREQ * 2)
    {
        mp4_fragindex_t *p_indexentry = &p_stream->p_indexentries[p_stream->i_indexentries];
        p_indexentry->i_time = i_time;
        p_indexentry->i_moofoffset = i_moof_pos;
        p_indexentry->i_sample = i_sample;
        p_indexentry->i_traf = i_traf;
        p_indexentry->i_trun = 1;
        p_stream->i_indexentries++;
    }
}

/* Creates moof box and traf/trun information.
 * Single run per traf is absolutely not optimal as interleaving should be done
 * using runs and not limiting moof size, but creating an relative offset only
 * requires base_offset_is_moof and then comply to late iso brand spec which
 * breaks clients. */
static bo_t *GetMoofBox(sout_mux_t *p_mux, size_t *pi_mdat_total_size,
                        mtime_t i_barrier_time, const uint64_t i_write_pos)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    bo_t            *moof, *mfhd;
    size_t           i_fixupoffset = 0;

    *pi_mdat_total_size = 0;

    moof = box_new("moof");
    if(!moof)
        return NULL;

    /* *** add /moof/mfhd *** */

    mfhd = box_full_new("mfhd", 0, 0);
    if(!mfhd)
    {
        bo_free(moof);
        return NULL;
    }
    bo_add_32be(mfhd, p_sys->i_mfhd_sequence++);   // sequence number

    box_gather(moof, mfhd);

    for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++)
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        /* *** add /moof/traf *** */
        bo_t *traf = box_new("traf");
        if(!traf)
            continue;
        uint32_t i_sample = 0;
        mtime_t i_time = p_stream->i_written_duration;
        bool b_allsamesize = true;
        bool b_allsamelength = true;
        if ( p_stream->read.p_first )
        {
            mp4_fragentry_t *p_entry = p_stream->read.p_first->p_next;
            while (p_entry && (b_allsamelength || b_allsamesize))
            {
                /* compare against queue head */
                b_allsamelength &= ( p_entry->p_block->i_length == p_stream->read.p_first->p_block->i_length );
                b_allsamesize &= ( p_entry->p_block->i_buffer == p_stream->read.p_first->p_block->i_buffer );
                p_entry = p_entry->p_next;
            }
        }

        uint32_t i_tfhd_flags = 0x0;
        if (p_stream->read.p_first)
        {
            /* Current segment have all same duration value, different than trex's default */
            if (b_allsamelength &&
                p_stream->read.p_first->p_block->i_length != p_stream->i_trex_length &&
                p_stream->read.p_first->p_block->i_length)
                    i_tfhd_flags |= MP4_TFHD_DFLT_SAMPLE_DURATION;

            /* Current segment have all same size value, different than trex's default */
            if (b_allsamesize &&
                p_stream->read.p_first->p_block->i_buffer != p_stream->i_trex_size &&
                p_stream->read.p_first->p_block->i_buffer)
                    i_tfhd_flags |= MP4_TFHD_DFLT_SAMPLE_SIZE;
        }
        else
        {
            /* We have no samples */
            i_tfhd_flags |= MP4_TFHD_DURATION_IS_EMPTY;
        }

        /* *** add /moof/traf/tfhd *** */
        bo_t *tfhd = box_full_new("tfhd", 0, i_tfhd_flags);
        if(!tfhd)
        {
            bo_free(traf);
            continue;
        }
        bo_add_32be(tfhd, p_stream->i_track_id);

        /* set the local sample duration default */
        if (i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_DURATION)
            bo_add_32be(tfhd, p_stream->read.p_first->p_block->i_length * p_stream->i_timescale / CLOCK_FREQ);

        /* set the local sample size default */
        if (i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_SIZE)
            bo_add_32be(tfhd, p_stream->read.p_first->p_block->i_buffer);

        box_gather(traf, tfhd);

        /* *** add /moof/traf/tfdt *** */
        bo_t *tfdt = box_full_new("tfdt", 1, 0);
        if(!tfdt)
        {
            bo_free(traf);
            continue;
        }
        bo_add_64be(tfdt, p_stream->i_written_duration * p_stream->i_timescale / CLOCK_FREQ );
        box_gather(traf, tfdt);

        /* *** add /moof/traf/trun *** */
        if (p_stream->read.p_first)
        {
            uint32_t i_trun_flags = 0x0;

            if (p_stream->b_hasiframes && !(p_stream->read.p_first->p_block->i_flags & BLOCK_FLAG_TYPE_I))
                i_trun_flags |= MP4_TRUN_FIRST_FLAGS;

            if (!b_allsamelength ||
                ( !(i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_DURATION) && p_stream->i_trex_length == 0 ))
                i_trun_flags |= MP4_TRUN_SAMPLE_DURATION;

            if (!b_allsamesize ||
                ( !(i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_SIZE) && p_stream->i_trex_size == 0 ))
                i_trun_flags |= MP4_TRUN_SAMPLE_SIZE;

            if (p_stream->b_hasbframes)
                i_trun_flags |= MP4_TRUN_SAMPLE_TIME_OFFSET;

            if (i_fixupoffset == 0)
                i_trun_flags |= MP4_TRUN_DATA_OFFSET;

            bo_t *trun = box_full_new("trun", 0, i_trun_flags);
            if(!trun)
            {
                bo_free(traf);
                continue;
            }

            /* count entries */
            uint32_t i_entry_count = 0;
            mtime_t i_run_time = p_stream->i_written_duration;
            mp4_fragentry_t *p_entry = p_stream->read.p_first;
            while(p_entry)
            {
                if ( i_barrier_time && i_run_time + p_entry->p_block->i_length > i_barrier_time )
                    break;
                i_entry_count++;
                i_run_time += p_entry->p_block->i_length;
                p_entry = p_entry->p_next;
            }
            bo_add_32be(trun, i_entry_count); // sample count

            if (i_trun_flags & MP4_TRUN_DATA_OFFSET)
            {
                i_fixupoffset = moof->b->i_buffer + traf->b->i_buffer + trun->b->i_buffer;
                bo_add_32be(trun, 0xdeadbeef); // data offset
            }

            if (i_trun_flags & MP4_TRUN_FIRST_FLAGS)
                bo_add_32be(trun, 1<<16); // flag as non keyframe

            while(p_stream->read.p_first && i_entry_count)
            {
                DEQUEUE_ENTRY(p_stream->read, p_entry);

                if (i_trun_flags & MP4_TRUN_SAMPLE_DURATION)
                    bo_add_32be(trun, p_entry->p_block->i_length * p_stream->i_timescale / CLOCK_FREQ); // sample duration

                if (i_trun_flags & MP4_TRUN_SAMPLE_SIZE)
                    bo_add_32be(trun, p_entry->p_block->i_buffer); // sample size

                if (i_trun_flags & MP4_TRUN_SAMPLE_TIME_OFFSET)
                {
                    uint32_t i_diff = 0;
                    if ( p_entry->p_block->i_dts  > VLC_TS_INVALID &&
                         p_entry->p_block->i_pts > p_entry->p_block->i_dts )
                    {
                        i_diff = p_entry->p_block->i_pts - p_entry->p_block->i_dts;
                    }
                    bo_add_32be(trun, i_diff * p_stream->i_timescale / CLOCK_FREQ); // ctts
                }

                *pi_mdat_total_size += p_entry->p_block->i_buffer;

                ENQUEUE_ENTRY(p_stream->towrite, p_entry);
                i_entry_count--;
                i_sample++;

                /* Add keyframe entry if needed */
                if (p_stream->b_hasiframes && (p_entry->p_block->i_flags & BLOCK_FLAG_TYPE_I) &&
                    (p_stream->fmt.i_cat == VIDEO_ES || p_stream->fmt.i_cat == AUDIO_ES))
                {
                    AddKeyframeEntry(p_stream, i_write_pos, i_trak, i_sample, i_time);
                }

                i_time += p_entry->p_block->i_length;
            }

            box_gather(traf, trun);
        }

        box_gather(moof, traf);
    }

    if(!moof->b)
    {
        bo_free(moof);
        return NULL;
    }

    box_fix(moof, moof->b->i_buffer);

    /* do tfhd base data offset fixup */
    if (i_fixupoffset)
    {
        /* mdat will follow moof */
        bo_set_32be(moof, i_fixupoffset, moof->b->i_buffer + 8);
    }

    /* set iframe flag, so the streaming server always starts from moof */
    moof->b->i_flags |= BLOCK_FLAG_TYPE_I;

    return moof;
}

static void WriteFragmentMDAT(sout_mux_t *p_mux, size_t i_total_size)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    /* Now add mdat header */
    bo_t *mdat = box_new("mdat");
    if(!mdat)
        return;
    /* force update of real size */
    assert(mdat->b->i_buffer==8);
    box_fix(mdat, mdat->b->i_buffer + i_total_size);
    p_sys->i_pos += mdat->b->i_buffer;
    /* only write header */
    sout_AccessOutWrite(p_mux->p_access, mdat->b);
    free(mdat);
    /* Header and its size are written and good, now write content */
    for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++)
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        while(p_stream->towrite.p_first)
        {
            mp4_fragentry_t *p_entry = p_stream->towrite.p_first;
            p_sys->i_pos += p_entry->p_block->i_buffer;
            p_stream->i_written_duration += p_entry->p_block->i_length;

            p_entry->p_block->i_flags &= ~BLOCK_FLAG_TYPE_I; // clear flag for http stream
            sout_AccessOutWrite(p_mux->p_access, p_entry->p_block);

            p_stream->towrite.p_first = p_entry->p_next;
            free(p_entry);
            if (!p_stream->towrite.p_first)
                p_stream->towrite.p_last = NULL;
        }
    }
}

static bo_t *GetMfraBox(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;
    bo_t *mfra = NULL;
    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i];
        if (p_stream->i_indexentries)
        {
            bo_t *tfra = box_full_new("tfra", 0, 0x0);
            if (!tfra) continue;
            bo_add_32be(tfra, p_stream->i_track_id);
            bo_add_32be(tfra, 0x3); // reserved + lengths (1,1,4)=>(0,0,3)
            bo_add_32be(tfra, p_stream->i_indexentries);
            for(uint32_t i_index=0; i_index<p_stream->i_indexentries; i_index++)
            {
                const mp4_fragindex_t *p_indexentry = &p_stream->p_indexentries[i_index];
                bo_add_32be(tfra, p_indexentry->i_time);
                bo_add_32be(tfra, p_indexentry->i_moofoffset);
                assert(sizeof(p_indexentry->i_traf)==1); /* guard against sys changes */
                assert(sizeof(p_indexentry->i_trun)==1);
                assert(sizeof(p_indexentry->i_sample)==4);
                bo_add_8(tfra, p_indexentry->i_traf);
                bo_add_8(tfra, p_indexentry->i_trun);
                bo_add_32be(tfra, p_indexentry->i_sample);
            }

            if (!mfra && !(mfra = box_new("mfra")))
            {
                bo_free(tfra);
                return NULL;
            }

            box_gather(mfra,tfra);
        }
    }
    return mfra;
}

static void FlushHeader(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;

    /* Now add ftyp header */
    bo_t *ftyp = box_new("ftyp");
    if(!ftyp)
        return;
    bo_add_fourcc(ftyp, "isom");
    bo_add_32be  (ftyp, 0); // minor version
    if(ftyp->b)
        box_fix(ftyp, ftyp->b->i_buffer);

    bo_t *moov = GetMoovBox(p_mux);

    /* merge into a single block */
    box_gather(ftyp, moov);

    /* add header flag for streaming server */
    ftyp->b->i_flags |= BLOCK_FLAG_HEADER;
    p_sys->i_pos += ftyp->b->i_buffer;
    box_send(p_mux, ftyp);
    p_sys->b_header_sent = true;
}

static int OpenFrag(vlc_object_t *p_this)
{
    sout_mux_t *p_mux = (sout_mux_t*) p_this;
    sout_mux_sys_t *p_sys = malloc(sizeof(sout_mux_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_mux->p_sys = (sout_mux_sys_t *) p_sys;
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = MuxFrag;

    /* unused */
    p_sys->b_mov        = false;
    p_sys->b_3gp        = false;
    p_sys->b_64_ext     = false;
    /* !unused */

    p_sys->i_pos        = 0;
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;
    p_sys->i_mdat_pos   = 0;
    p_sys->i_read_duration   = 0;
    p_sys->i_written_duration= 0;

    p_sys->b_header_sent = false;
    p_sys->b_fragmented  = true;
    p_sys->i_mfhd_sequence = 1;

    return VLC_SUCCESS;
}

static void WriteFragments(sout_mux_t *p_mux, bool b_flush)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;
    bo_t *moof = NULL;
    mtime_t i_barrier_time = p_sys->i_written_duration + FRAGMENT_LENGTH;
    size_t i_mdat_size = 0;
    bool b_has_samples = false;

    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
    {
        const mp4_stream_t *p_stream = p_sys->pp_streams[i];
        if (p_stream->read.p_first)
        {
            b_has_samples = true;

            /* set a barrier so we try to align to keyframe */
            if (p_stream->b_hasiframes &&
                    p_stream->i_last_iframe_time > p_stream->i_written_duration &&
                    (p_stream->fmt.i_cat == VIDEO_ES ||
                     p_stream->fmt.i_cat == AUDIO_ES) )
            {
                i_barrier_time = __MIN(i_barrier_time, p_stream->i_last_iframe_time);
            }
        }
    }

    if (!p_sys->b_header_sent)
        FlushHeader(p_mux);

    if (b_has_samples)
        moof = GetMoofBox(p_mux, &i_mdat_size, (b_flush)?0:i_barrier_time, p_sys->i_pos);

    if (moof && i_mdat_size == 0)
    {
        block_Release(moof->b);
        FREENULL(moof);
    }

    if (moof)
    {
        msg_Dbg(p_mux, "writing moof @ %"PRId64, p_sys->i_pos);
        p_sys->i_pos += moof->b->i_buffer;
        assert(moof->b->i_flags & BLOCK_FLAG_TYPE_I); /* http sout */
        box_send(p_mux, moof);
        msg_Dbg(p_mux, "writing mdat @ %"PRId64, p_sys->i_pos);
        WriteFragmentMDAT(p_mux, i_mdat_size);

        /* update iframe point */
        for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
        {
            mp4_stream_t *p_stream = p_sys->pp_streams[i];
            p_stream->i_last_iframe_time = 0;
        }
    }
}

/* Do an entry length fixup using only its own info.
 * This is the end boundary case. */
static void LengthLocalFixup(sout_mux_t *p_mux, const mp4_stream_t *p_stream, block_t *p_entrydata)
{
    if ( p_stream->fmt.i_cat == VIDEO_ES )
    {
        p_entrydata->i_length = CLOCK_FREQ *
                p_stream->fmt.video.i_frame_rate_base /
                p_stream->fmt.video.i_frame_rate;
        msg_Dbg(p_mux, "video track %d fixup to %"PRId64" for sample %u",
                p_stream->i_track_id, p_entrydata->i_length, p_stream->i_entry_count - 1);
    }
    else if (p_stream->fmt.i_cat == AUDIO_ES &&
             p_stream->fmt.audio.i_rate &&
             p_entrydata->i_nb_samples)
    {
        p_entrydata->i_length = CLOCK_FREQ * p_entrydata->i_nb_samples /
                p_stream->fmt.audio.i_rate;
        msg_Dbg(p_mux, "audio track %d fixup to %"PRId64" for sample %u",
                p_stream->i_track_id, p_entrydata->i_length, p_stream->i_entry_count - 1);
    }
    else
    {
        msg_Warn(p_mux, "unknown length for track %d sample %u",
                 p_stream->i_track_id, p_stream->i_entry_count - 1);
        p_entrydata->i_length = 1;
    }
}

static void CleanupFrag(sout_mux_sys_t *p_sys)
{
    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i];
        if (p_stream->p_held_entry)
        {
            block_Release(p_stream->p_held_entry->p_block);
            free(p_stream->p_held_entry);
        }
        while(p_stream->read.p_first)
        {
            mp4_fragentry_t *p_next = p_stream->read.p_first->p_next;
            block_Release(p_stream->read.p_first->p_block);
            free(p_stream->read.p_first);
            p_stream->read.p_first = p_next;
        }
        while(p_stream->towrite.p_first)
        {
            mp4_fragentry_t *p_next = p_stream->towrite.p_first->p_next;
            block_Release(p_stream->towrite.p_first->p_block);
            free(p_stream->towrite.p_first);
            p_stream->towrite.p_first = p_next;
        }
        free(p_stream->p_indexentries);
    }
    free(p_sys);
}

static void CloseFrag(vlc_object_t *p_this)
{
    sout_mux_t *p_mux = (sout_mux_t *) p_this;
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;

    /* Flush remaining entries */
    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i];
        if (p_stream->p_held_entry)
        {
            if (p_stream->p_held_entry->p_block->i_length < 1)
                LengthLocalFixup(p_mux, p_stream, p_stream->p_held_entry->p_block);
            ENQUEUE_ENTRY(p_stream->read, p_stream->p_held_entry);
            p_stream->p_held_entry = NULL;
        }
    }

    /* and force creating a fragment from it */
    WriteFragments(p_mux, true);

    /* Write indexes, but only for non streamed content
       as they refer to moof by absolute position */
    if (!strcmp(p_mux->psz_mux, "mp4frag"))
    {
        bo_t *mfra = GetMfraBox(p_mux);
        if (mfra)
        {
            bo_t *mfro = box_full_new("mfro", 0, 0x0);
            if (mfro && mfra->b)
            {
                box_fix(mfra, mfra->b->i_buffer);
                bo_add_32be(mfro, mfra->b->i_buffer + MP4_MFRO_BOXSIZE);
                box_gather(mfra, mfro);
            }
            box_send(p_mux, mfra);
        }
    }

    CleanupFrag(p_sys);
}

static int MuxFrag(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;

    int i_stream = sout_MuxGetStream(p_mux, 1, NULL);
    if (i_stream < 0)
        return VLC_SUCCESS;
    sout_input_t *p_input  = p_mux->pp_inputs[i_stream];
    mp4_stream_t *p_stream = (mp4_stream_t*) p_input->p_sys;
    block_t *p_currentblock = block_FifoGet(p_input->p_fifo);

    /* do block conversion */
    switch(p_stream->fmt.i_codec)
    {
    case VLC_CODEC_H264:
    case VLC_CODEC_HEVC:
        p_currentblock = ConvertFromAnnexB(p_currentblock);
        break;
    case VLC_CODEC_SUBT:
        p_currentblock = ConvertSUBT(p_currentblock);
        break;
    default:
        break;
    }

    if( !p_currentblock )
        return VLC_ENOMEM;

    /* If we have a previous entry for outgoing queue */
    if (p_stream->p_held_entry)
    {
        block_t *p_heldblock = p_stream->p_held_entry->p_block;

        /* Fix previous block length from current */
        if (p_heldblock->i_length < 1)
        {

            /* Fix using dts if not on a boundary */
            if ((p_currentblock->i_flags & BLOCK_FLAG_DISCONTINUITY) == 0)
                p_heldblock->i_length = p_currentblock->i_dts - p_heldblock->i_dts;

            if (p_heldblock->i_length < 1)
                LengthLocalFixup(p_mux, p_stream, p_heldblock);
        }

        /* enqueue */
        ENQUEUE_ENTRY(p_stream->read, p_stream->p_held_entry);
        p_stream->p_held_entry = NULL;

        if (p_stream->b_hasiframes && (p_heldblock->i_flags & BLOCK_FLAG_TYPE_I) &&
            p_stream->i_read_duration - p_sys->i_written_duration < FRAGMENT_LENGTH)
        {
            /* Flag the last iframe time, we'll use it as boundary so it will start
               next fragment */
            p_stream->i_last_iframe_time = p_stream->i_read_duration;
        }

        /* update buffered time */
        p_stream->i_read_duration += __MAX(0, p_heldblock->i_length);
    }


    /* set temp entry */
    p_stream->p_held_entry = malloc(sizeof(mp4_fragentry_t));
    if (unlikely(!p_stream->p_held_entry))
        return VLC_ENOMEM;

    p_stream->p_held_entry->p_block  = p_currentblock;
    p_stream->p_held_entry->i_run    = p_stream->i_current_run;
    p_stream->p_held_entry->p_next   = NULL;

    if (p_stream->fmt.i_cat == VIDEO_ES )
    {
        if (!p_stream->b_hasiframes && (p_currentblock->i_flags & BLOCK_FLAG_TYPE_I))
            p_stream->b_hasiframes = true;

        if (!p_stream->b_hasbframes && p_currentblock->i_dts > VLC_TS_INVALID &&
            p_currentblock->i_pts > p_currentblock->i_dts)
            p_stream->b_hasbframes = true;
    }

    /* Update the global fragment/media duration */
    mtime_t i_min_read_duration = p_stream->i_read_duration;
    mtime_t i_min_written_duration = p_stream->i_written_duration;
    for (unsigned int i=0; i<p_sys->i_nb_streams; i++)
    {
        const mp4_stream_t *p_s = p_sys->pp_streams[i];
        if (p_s->fmt.i_cat != VIDEO_ES && p_s->fmt.i_cat != AUDIO_ES)
            continue;
        if (p_s->i_read_duration < i_min_read_duration)
            i_min_read_duration = p_s->i_read_duration;

        if (p_s->i_written_duration < i_min_written_duration)
            i_min_written_duration = p_s->i_written_duration;
    }
    p_sys->i_read_duration = i_min_read_duration;
    p_sys->i_written_duration = i_min_written_duration;

    /* we have prerolled enough to know all streams, and have enough date to create a fragment */
    if (p_stream->read.p_first && p_sys->i_read_duration - p_sys->i_written_duration >= FRAGMENT_LENGTH)
        WriteFragments(p_mux, false);

    return VLC_SUCCESS;
}
