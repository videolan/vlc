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

#include <time.h>

#include <vlc_iso_lang.h>
#include <vlc_meta.h>

#include "../demux/mpeg/mpeg_parser_helpers.h"

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
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "faststart", NULL
};

static int Control(sout_mux_t *, int, va_list);
static int AddStream(sout_mux_t *, sout_input_t *);
static int DelStream(sout_mux_t *, sout_input_t *);
static int Mux      (sout_mux_t *);

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

typedef struct
{
    es_format_t   fmt;
    int           i_track_id;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4_entry_t  *entry;
    int64_t      i_length_neg;

    /* stats */
    int64_t      i_dts_start; /* applies to current segment only */
    int64_t      i_duration;
    mtime_t      i_starttime; /* the really first packet */
    bool         b_hasbframes;

    /* for later stco fix-up (fast start files) */
    uint64_t i_stco_pos;
    bool b_stco64;

    /* for spu */
    int64_t i_last_dts; /* applies to current segment only */

} mp4_stream_t;

struct sout_mux_sys_t
{
    bool b_mov;
    bool b_3gp;
    bool b_64_ext;
    bool b_fast_start;

    uint64_t i_mdat_pos;
    uint64_t i_pos;
    mtime_t  i_duration;

    int          i_nb_streams;
    mp4_stream_t **pp_streams;
};

typedef struct bo_t
{
    block_t    *b;
    size_t     len;
} bo_t;

static void bo_init     (bo_t *);
static void bo_add_8    (bo_t *, uint8_t);
static void bo_add_16be (bo_t *, uint16_t);
static void bo_add_24be (bo_t *, uint32_t);
static void bo_add_32be (bo_t *, uint32_t);
static void bo_add_64be (bo_t *, uint64_t);
static void bo_add_fourcc(bo_t *, const char *);
static void bo_add_mem  (bo_t *, int , uint8_t *);
static void bo_add_descr(bo_t *, uint8_t , uint32_t);

static void bo_fix_32be (bo_t *, int , uint32_t);

static bo_t *box_new     (const char *fcc);
static bo_t *box_full_new(const char *fcc, uint8_t v, uint32_t f);
static void  box_fix     (bo_t *box);
static void  box_gather  (bo_t *box, bo_t *box2);

static void box_send(sout_mux_t *p_mux,  bo_t *box);

static bo_t *GetMoovBox(sout_mux_t *p_mux);

static block_t *ConvertSUBT(block_t *);
static block_t *ConvertFromAnnexB(block_t *);

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
    p_sys->i_duration   = 0;

    if (!p_sys->b_mov) {
        /* Now add ftyp header */
        box = box_new("ftyp");
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
        bo_add_fourcc(box, "qt  ");
        box_fix(box);

        p_sys->i_pos += box->len;
        p_sys->i_mdat_pos = p_sys->i_pos;

        box_send(p_mux, box);
    }

    /* FIXME FIXME
     * Quicktime actually doesn't like the 64 bits extensions !!! */
    p_sys->b_64_ext = false;

    /* Now add mdat header */
    box = box_new("mdat");
    bo_add_64be  (box, 0); // enough to store an extended size

    p_sys->i_pos += box->len;

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
    bo_init(&bo);
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

    bo.b->i_buffer = bo.len;
    sout_AccessOutSeek(p_mux->p_access, p_sys->i_mdat_pos);
    sout_AccessOutWrite(p_mux->p_access, bo.b);

    /* Create MOOV header */
    uint64_t i_moov_pos = p_sys->i_pos;
    bo_t *moov = GetMoovBox(p_mux);

    /* Check we need to create "fast start" files */
    p_sys->b_fast_start = var_GetBool(p_this, SOUT_CFG_PREFIX "faststart");
    while (p_sys->b_fast_start) {
        /* Move data to the end of the file so we can fit the moov header
         * at the start */
        int64_t i_size = p_sys->i_pos - p_sys->i_mdat_pos;
        int i_moov_size = moov->len;

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

        /* Fix-up samples to chunks table in MOOV header */
        for (int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
            mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

            moov->len = p_stream->i_stco_pos;
            for (unsigned i = 0; i < p_stream->i_entry_count; ) {
                mp4_entry_t *entry = p_stream->entry;
                if (p_stream->b_stco64)
                    bo_add_64be(moov, entry[i].i_pos + i_moov_size);
                else
                    bo_add_32be(moov, entry[i].i_pos + i_moov_size);

                for (; i < p_stream->i_entry_count; i++)
                    if (i >= p_stream->i_entry_count - 1 ||
                        entry[i].i_pos + entry[i].i_size != entry[i+1].i_pos) {
                        i++;
                        break;
                    }
            }
        }

        moov->len = i_moov_size;
        i_moov_pos = p_sys->i_mdat_pos;
        p_sys->b_fast_start = false;
    }

    /* Write MOOV header */
    sout_AccessOutSeek(p_mux->p_access, i_moov_pos);
    box_send(p_mux, moov);

    /* Clean-up */
    for (int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        es_format_Clean(&p_stream->fmt);
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
    case VLC_CODEC_MP4A:
    case VLC_CODEC_MP4V:
    case VLC_CODEC_MPGA:
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
    p_stream->i_duration    = 0;
    p_stream->i_starttime   = p_sys->i_duration;
    p_stream->b_hasbframes  = false;

    p_stream->i_last_dts    = 0;

    p_input->p_sys          = p_stream;

    msg_Dbg(p_mux, "adding input");

    TAB_APPEND(p_sys->i_nb_streams, p_sys->pp_streams, p_stream);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static int DelStream(sout_mux_t *p_mux, sout_input_t *p_input)
{
    VLC_UNUSED(p_input);
    msg_Dbg(p_mux, "removing input");
    return VLC_SUCCESS;
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
                        msg_Dbg( p_mux, "video track %d fixup to %"PRId64" for sample %u",
                                 p_stream->i_track_id, p_data->i_length, p_stream->i_entry_count );
                    }
                    else if ( p_stream->fmt.i_cat == AUDIO_ES &&
                              p_stream->fmt.audio.i_rate &&
                              p_data->i_nb_samples )
                    {
                        p_data->i_length = CLOCK_FREQ * p_data->i_nb_samples /
                                           p_stream->fmt.audio.i_rate;
                        msg_Dbg( p_mux, "audio track %d fixup to %"PRId64" for sample %u",
                                 p_stream->i_track_id, p_data->i_length, p_stream->i_entry_count );
                    }
                    else if ( p_data->i_length <= 0 )
                    {
                        msg_Warn( p_mux, "unknown length for track %d sample %u",
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
        p_stream->i_duration += __MAX( 0, p_data->i_length );
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
            p_stream->i_duration = p_stream->i_starttime + ( p_stream->i_last_dts - p_stream->i_dts_start );
        }
    }

    /* Update the global segment/media duration */
    for ( int i=0; i<p_sys->i_nb_streams; i++ )
    {
        if ( p_sys->pp_streams[i]->i_duration > p_sys->i_duration )
            p_sys->i_duration = p_sys->pp_streams[i]->i_duration;
    }

    return(VLC_SUCCESS);
}

/*****************************************************************************
 *
 *****************************************************************************/
static block_t *ConvertSUBT(block_t *p_block)
{
    p_block = block_Realloc(p_block, 2, p_block->i_buffer);

    /* No trailling '\0' */
    if (p_block->i_buffer > 2 && p_block->p_buffer[p_block->i_buffer-1] == '\0')
        p_block->i_buffer--;

    p_block->p_buffer[0] = ((p_block->i_buffer - 2) >> 8)&0xff;
    p_block->p_buffer[1] = ((p_block->i_buffer - 2)     )&0xff;

    return p_block;
}

static block_t *ConvertFromAnnexB(block_t *p_block)
{
    uint8_t *last = p_block->p_buffer;  /* Assume it starts with 0x00000001 */
    uint8_t *dat  = &p_block->p_buffer[4];
    uint8_t *end = &p_block->p_buffer[p_block->i_buffer];


    /* Replace the 4 bytes start code with 4 bytes size,
     * FIXME are all startcodes 4 bytes ? (I don't think :(*/
    while (dat < end) {
        while (dat < end - 4) {
            if (!memcmp(dat, avc1_start_code, 4))
                break;
            dat++;
        }
        if (dat >= end - 4)
            dat = end;

        /* Fix size */
        int i_size = dat - &last[4];
        last[0] = (i_size >> 24)&0xff;
        last[1] = (i_size >> 16)&0xff;
        last[2] = (i_size >>  8)&0xff;
        last[3] = (i_size      )&0xff;

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

    if (p_stream->i_duration > 0)
        i_bitrate_avg = INT64_C(8000000) * i_bitrate_avg / p_stream->i_duration;
    else
        i_bitrate_avg = 0;
    if (i_bitrate_max <= 1)
        i_bitrate_max = 0x7fffffff;

    /* */
    int i_decoder_specific_info_size = (p_stream->fmt.i_extra > 0) ? 5 + p_stream->fmt.i_extra : 0;

    esds = box_full_new("esds", 0, 0);

    /* ES_Descr */
    bo_add_descr(esds, 0x03, 3 + 5 + 13 + i_decoder_specific_info_size + 5 + 1);
    bo_add_16be(esds, p_stream->i_track_id);
    bo_add_8   (esds, 0x1f);      // flags=0|streamPriority=0x1f

    /* DecoderConfigDescr */
    bo_add_descr(esds, 0x04, 13 + i_decoder_specific_info_size);

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
        bo_add_descr(esds, 0x05, p_stream->fmt.i_extra);

        for (int i = 0; i < p_stream->fmt.i_extra; i++)
            bo_add_8(esds, ((uint8_t*)p_stream->fmt.p_extra)[i]);
    }

    /* SL_Descr mandatory */
    bo_add_descr(esds, 0x06, 1);
    bo_add_8    (esds, 0x02);  // sl_predefined

    return esds;
}

static bo_t *GetWaveTag(mp4_stream_t *p_stream)
{
    bo_t *wave;
    bo_t *box;

    wave = box_new("wave");

    box = box_new("frma");
    bo_add_fourcc(box, "mp4a");
    box_gather(wave, box);

    box = box_new("mp4a");
    bo_add_32be(box, 0);
    box_gather(wave, box);

    box = GetESDS(p_stream);
    box_gather(wave, box);

    box = box_new("srcq");
    bo_add_32be(box, 0x40);
    box_gather(wave, box);

    /* wazza ? */
    bo_add_32be(wave, 8); /* new empty box */
    bo_add_32be(wave, 0); /* box label */

    return wave;
}

static bo_t *GetDamrTag(mp4_stream_t *p_stream)
{
    bo_t *damr;

    damr = box_new("damr");

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
    bo_t *d263;

    d263 = box_new("d263");

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
    (void) read_ue( &bs );

    *chroma_idc = read_ue(&bs);
    if (*chroma_idc == 3)
        bs_skip(&bs, 1);

    /* skip width and heigh */
    (void) read_ue( &bs );
    (void) read_ue( &bs );

    uint32_t conformance_window_flag = bs_read1(&bs);
    if (conformance_window_flag) {
        /* skip offsets*/
        (void) read_ue(&bs);
        (void) read_ue(&bs);
        (void) read_ue(&bs);
        (void) read_ue(&bs);
    }
    *bit_depth_luma_minus8 = read_ue(&bs);
    *bit_depth_chroma_minus8 = read_ue(&bs);
}

static bo_t *GetHvcCTag(mp4_stream_t *p_stream)
{
    /* Generate hvcC box matching iso/iec 14496-15 3rd edition */
    bo_t *hvcC = box_new("hvcC");
    if(!p_stream->fmt.i_extra)
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
    bo_t    *avcC = NULL;
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
 
    /* FIXME use better value */
    avcC = box_new("avcC");
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

    /* Requirements */
    for (int i_track = 0; i_track < p_sys->i_nb_streams; i_track++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_track];
        vlc_fourcc_t codec = p_stream->fmt.i_codec;

        if (codec == VLC_CODEC_MP4V || codec == VLC_CODEC_MP4A) {
            bo_t *box = box_new("\251req");
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
        /* String length */
        bo_add_16be(box, sizeof(PACKAGE_STRING " stream output") - 1);
        bo_add_16be(box, 0);
        bo_add_mem(box, sizeof(PACKAGE_STRING " stream output") - 1,
                    (uint8_t*)PACKAGE_STRING " stream output");
        box_gather(udta, box);
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
    vlc_fourcc_to_char(codec, fcc);

    if (codec == VLC_CODEC_MPGA) {
        if (p_sys->b_mov) {
            b_descr = false;
            memcpy(fcc, ".mp3", 4);
        } else
            memcpy(fcc, "mp4a", 4);
    }

    bo_t *soun = box_new(fcc);
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
        else
            box = GetESDS(p_stream);
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
    bo_fix_32be(stco, 12, i_chunk);
    msg_Dbg(p_mux, "created %d chunks (stco)", i_chunk);

    /* Fix stsc entry count */
    bo_fix_32be(stsc, 12, i_stsc_entries );

    /* add stts */
    bo_t *stts = box_full_new("stts", 0, 0);
    bo_add_32be(stts, 0);     // entry-count (fixed latter)

    uint32_t i_timescale;
    if (p_stream->fmt.i_cat == AUDIO_ES)
        i_timescale = p_stream->fmt.audio.i_rate;
    else
        i_timescale = CLOCK_FREQ;

    unsigned i_index = 0;
    for (unsigned i = 0; i < p_stream->i_entry_count; i_index++) {
        int     i_first = i;
        mtime_t i_delta = p_stream->entry[i].i_length;

        for (; i < p_stream->i_entry_count; ++i)
            if (i == p_stream->i_entry_count || p_stream->entry[i].i_length != i_delta)
                break;

        bo_add_32be(stts, i - i_first); // sample-count
        bo_add_32be(stts, i_delta * i_timescale / CLOCK_FREQ ); // sample-delta
    }
    bo_fix_32be(stts, 12, i_index);

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
            bo_add_32be(ctts, i_offset * i_timescale / CLOCK_FREQ ); // sample-offset
        }
        bo_fix_32be(ctts, 12, i_index);
    }

    bo_t *stsz = box_full_new("stsz", 0, 0);
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
    for (unsigned i = 0; i < p_stream->i_entry_count; i++)
        if (p_stream->entry[i].i_flags & BLOCK_FLAG_TYPE_I) {
            if (stss == NULL) {
                stss = box_full_new("stss", 0, 0);
                bo_add_32be(stss, 0); /* fixed later */
            }
            bo_add_32be(stss, 1 + i);
            i_index++;
        }

    if (stss)
        bo_fix_32be(stss, 12, i_index);

    /* Now gather all boxes into stbl */
    bo_t *stbl = box_new("stbl");

    box_gather(stbl, stsd);
    box_gather(stbl, stts);
    if (stss)
        box_gather(stbl, stss);
    if (ctts)
        box_gather(stbl, ctts);
    box_gather(stbl, stsc);
    box_gather(stbl, stsz);
    p_stream->i_stco_pos = stbl->len + 16;
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

    /* Create general info */
    for (int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
        i_movie_duration = __MAX(i_movie_duration, p_stream->i_duration);
    }
    msg_Dbg(p_mux, "movie duration %"PRId64"s", i_movie_duration / CLOCK_FREQ);

    i_movie_duration = i_movie_duration * i_movie_timescale / CLOCK_FREQ;

    /* *** add /moov/mvhd *** */
    if (!p_sys->b_64_ext) {
        mvhd = box_full_new("mvhd", 0, 0);
        bo_add_32be(mvhd, i_timestamp);   // creation time
        bo_add_32be(mvhd, i_timestamp);   // modification time
        bo_add_32be(mvhd, i_movie_timescale);  // timescale
        bo_add_32be(mvhd, i_movie_duration);  // duration
    } else {
        mvhd = box_full_new("mvhd", 1, 0);
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

    for (int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++) {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        uint32_t i_timescale;
        if (p_stream->fmt.i_cat == AUDIO_ES)
            i_timescale = p_stream->fmt.audio.i_rate;
        else
            i_timescale = CLOCK_FREQ;

        /* *** add /moov/trak *** */
        bo_t *trak = box_new("trak");

        /* *** add /moov/trak/tkhd *** */
        bo_t *tkhd;
        if (!p_sys->b_64_ext) {
            if (p_sys->b_mov)
                tkhd = box_full_new("tkhd", 0, 0x0f);
            else
                tkhd = box_full_new("tkhd", 0, 1);

            bo_add_32be(tkhd, i_timestamp);       // creation time
            bo_add_32be(tkhd, i_timestamp);       // modification time
            bo_add_32be(tkhd, p_stream->i_track_id);
            bo_add_32be(tkhd, 0);                     // reserved 0
            bo_add_32be(tkhd, p_stream->i_duration *
                         (int64_t)i_movie_timescale / CLOCK_FREQ); // duration
        } else {
            if (p_sys->b_mov)
                tkhd = box_full_new("tkhd", 1, 0x0f);
            else
                tkhd = box_full_new("tkhd", 1, 1);

            bo_add_64be(tkhd, i_timestamp);       // creation time
            bo_add_64be(tkhd, i_timestamp);       // modification time
            bo_add_32be(tkhd, p_stream->i_track_id);
            bo_add_32be(tkhd, 0);                     // reserved 0
            bo_add_64be(tkhd, p_stream->i_duration *
                         (int64_t)i_movie_timescale / CLOCK_FREQ); // duration
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
            for (int i = 0; i < p_sys->i_nb_streams; i++) {
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
        bo_t *edts = box_new("edts");
        bo_t *elst = box_full_new("elst", p_sys->b_64_ext ? 1 : 0, 0);
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
            bo_add_64be(elst, p_stream->i_duration *
                         i_movie_timescale / CLOCK_FREQ);
            bo_add_64be(elst, 0);
        } else {
            bo_add_32be(elst, p_stream->i_duration *
                         i_movie_timescale / CLOCK_FREQ);
            bo_add_32be(elst, 0);
        }
        bo_add_16be(elst, 1);
        bo_add_16be(elst, 0);

        box_gather(edts, elst);
        box_gather(trak, edts);

        /* *** add /moov/trak/mdia *** */
        bo_t *mdia = box_new("mdia");

        /* media header */
        bo_t *mdhd;
        if (!p_sys->b_64_ext) {
            mdhd = box_full_new("mdhd", 0, 0);
            bo_add_32be(mdhd, i_timestamp);   // creation time
            bo_add_32be(mdhd, i_timestamp);   // modification time
            bo_add_32be(mdhd, i_timescale);        // timescale
            bo_add_32be(mdhd, p_stream->i_duration * (int64_t)i_timescale /
                               CLOCK_FREQ);  // duration
        } else {
            mdhd = box_full_new("mdhd", 1, 0);
            bo_add_64be(mdhd, i_timestamp);   // creation time
            bo_add_64be(mdhd, i_timestamp);   // modification time
            bo_add_32be(mdhd, i_timescale);        // timescale
            bo_add_64be(mdhd, p_stream->i_duration * (int64_t)i_timescale /
                               CLOCK_FREQ);  // duration
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

        /* add smhd|vmhd */
        if (p_stream->fmt.i_cat == AUDIO_ES) {
            bo_t *smhd;

            smhd = box_full_new("smhd", 0, 0);
            bo_add_16be(smhd, 0);     // balance
            bo_add_16be(smhd, 0);     // reserved

            box_gather(minf, smhd);
        } else if (p_stream->fmt.i_cat == VIDEO_ES) {
            bo_t *vmhd;

            vmhd = box_full_new("vmhd", 0, 1);
            bo_add_16be(vmhd, 0);     // graphicsmode
            for (int i = 0; i < 3; i++)
                bo_add_16be(vmhd, 0); // opcolor

            box_gather(minf, vmhd);
        } else if (p_stream->fmt.i_cat == SPU_ES) {
            bo_t *gmhd = box_new("gmhd");
            bo_t *gmin = box_full_new("gmin", 0, 1);

            bo_add_16be(gmin, 0);     // graphicsmode
            for (int i = 0; i < 3; i++)
                bo_add_16be(gmin, 0); // opcolor
            bo_add_16be(gmin, 0);     // balance
            bo_add_16be(gmin, 0);     // reserved

            box_gather(gmhd, gmin);

            box_gather(minf, gmhd);
        }

        /* dinf */
        bo_t *dinf = box_new("dinf");
        bo_t *dref = box_full_new("dref", 0, 0);
        bo_add_32be(dref, 1);
        bo_t *url = box_full_new("url ", 0, 0x01);
        box_gather(dref, url);
        box_gather(dinf, dref);

        /* append dinf to mdia */
        box_gather(minf, dinf);

        /* add stbl */
        bo_t *stbl = GetStblBox(p_mux, p_stream);

        /* append stbl to minf */
        p_stream->i_stco_pos += minf->len;
        box_gather(minf, stbl);

        /* append minf to mdia */
        p_stream->i_stco_pos += mdia->len;
        box_gather(mdia, minf);

        /* append mdia to trak */
        p_stream->i_stco_pos += trak->len;
        box_gather(trak, mdia);

        /* append trak to moov */
        p_stream->i_stco_pos += moov->len;
        box_gather(moov, trak);
    }

    /* Add user data tags */
    box_gather(moov, GetUdtaTag(p_mux));

    box_fix(moov);
    return moov;
}

/****************************************************************************/

static void bo_init(bo_t *p_bo)
{
    p_bo->len = 0;
    p_bo->b = block_Alloc(1024);
}

static void bo_add_8(bo_t *p_bo, uint8_t i)
{
    if (p_bo->len >= p_bo->b->i_buffer)
        p_bo->b = block_Realloc(p_bo->b, 0, p_bo->b->i_buffer + 1024);

    p_bo->b->p_buffer[p_bo->len++] = i;
}

static void bo_add_16be(bo_t *p_bo, uint16_t i)
{
    bo_add_8(p_bo, ((i >> 8) &0xff));
    bo_add_8(p_bo, i &0xff);
}

static void bo_add_24be(bo_t *p_bo, uint32_t i)
{
    bo_add_8(p_bo, ((i >> 16) &0xff));
    bo_add_8(p_bo, ((i >> 8) &0xff));
    bo_add_8(p_bo, (  i &0xff));
}
static void bo_add_32be(bo_t *p_bo, uint32_t i)
{
    bo_add_16be(p_bo, ((i >> 16) &0xffff));
    bo_add_16be(p_bo, i &0xffff);
}

static void bo_fix_32be (bo_t *p_bo, int i_pos, uint32_t i)
{
    p_bo->b->p_buffer[i_pos    ] = (i >> 24)&0xff;
    p_bo->b->p_buffer[i_pos + 1] = (i >> 16)&0xff;
    p_bo->b->p_buffer[i_pos + 2] = (i >>  8)&0xff;
    p_bo->b->p_buffer[i_pos + 3] = (i      )&0xff;
}

static void bo_add_64be(bo_t *p_bo, uint64_t i)
{
    bo_add_32be(p_bo, ((i >> 32) &0xffffffff));
    bo_add_32be(p_bo, i &0xffffffff);
}

static void bo_add_fourcc(bo_t *p_bo, const char *fcc)
{
    bo_add_8(p_bo, fcc[0]);
    bo_add_8(p_bo, fcc[1]);
    bo_add_8(p_bo, fcc[2]);
    bo_add_8(p_bo, fcc[3]);
}

static void bo_add_mem(bo_t *p_bo, int i_size, uint8_t *p_mem)
{
    for (int i = 0; i < i_size; i++)
        bo_add_8(p_bo, p_mem[i]);
}

static void bo_add_descr(bo_t *p_bo, uint8_t tag, uint32_t size)
{
    bo_add_8(p_bo, tag);
    for (int i = 3; i>0; i--)
        bo_add_8(p_bo, (size>>(7*i)) | 0x80);
    bo_add_8(p_bo, size & 0x7F);
}

static bo_t *box_new(const char *fcc)
{
    bo_t *box = malloc(sizeof(*box));
    if (!box)
        return NULL;

    bo_init(box);

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

static void box_fix(bo_t *box)
{
    box->b->p_buffer[0] = box->len >> 24;
    box->b->p_buffer[1] = box->len >> 16;
    box->b->p_buffer[2] = box->len >>  8;
    box->b->p_buffer[3] = box->len;
}

static void box_gather (bo_t *box, bo_t *box2)
{
    box_fix(box2);
    box->b = block_Realloc(box->b, 0, box->len + box2->len);
    memcpy(&box->b->p_buffer[box->len], box2->b->p_buffer, box2->len);
    box->len += box2->len;
    block_Release(box2->b);
    free(box2);
}

static void box_send(sout_mux_t *p_mux,  bo_t *box)
{
    box->b->i_buffer = box->len;
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
