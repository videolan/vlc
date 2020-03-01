/*****************************************************************************
 * mp4.c: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

#include <assert.h>
#include <time.h>

#include <vlc_iso_lang.h>
#include <vlc_meta.h>

#include "../../demux/mp4/libmp4.h"
#include "libmp4mux.h"
#include "../../packetizer/hxxx_nal.h"
#include "../av1_pack.h"
#include "../extradata.h"

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
static void CloseFrag  (vlc_object_t *);

#define SOUT_CFG_PREFIX "sout-mp4-"

vlc_module_begin ()
    set_description(N_("MP4/MOV muxer"))
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_MUX)
    set_shortname("MP4")

    add_bool(SOUT_CFG_PREFIX "faststart", false,
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
    set_callbacks(Open, CloseFrag)

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
    vlc_tick_t  i_time;
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
    mp4mux_trackinfo_t *tinfo;

    mux_extradata_builder_t *extrabuilder;

    /* index */
    vlc_tick_t   i_length_neg;

    /* applies to current segment only */
    vlc_tick_t   i_first_dts;
    vlc_tick_t   i_last_dts;
    vlc_tick_t   i_last_pts;

    /*** mp4frag ***/
    bool         b_hasiframes;

    uint32_t         i_current_run;
    mp4_fragentry_t *p_held_entry;
    mp4_fragqueue_t  read;
    mp4_fragqueue_t  towrite;
    vlc_tick_t       i_last_iframe_time;
    vlc_tick_t       i_written_duration;
    mp4_fragindex_t *p_indexentries;
    uint32_t         i_indexentriesmax;
    uint32_t         i_indexentries;
} mp4_stream_t;

typedef struct
{
    mp4mux_handle_t *muxh;
    bool b_3gp;
    bool b_fast_start;

    /* global */
    bool     b_header_sent;

    uint64_t i_mdat_pos;
    uint64_t i_pos;
    vlc_tick_t  i_read_duration;
    vlc_tick_t  i_start_dts;

    unsigned int   i_nb_streams;
    mp4_stream_t **pp_streams;


    /* mp4frag */
    vlc_tick_t     i_written_duration;
    uint32_t       i_mfhd_sequence;
} sout_mux_sys_t;

static void mp4_stream_Delete(mp4_stream_t *p_stream)
{
    if(p_stream->extrabuilder)
        mux_extradata_builder_Delete(p_stream->extrabuilder);

    /* mp4 frag */
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

    free(p_stream);
}

static mp4_stream_t *mp4_stream_New(void)
{
    mp4_stream_t *p_stream = calloc(1, sizeof(*p_stream));
    if(p_stream)
    {
        p_stream->i_first_dts = VLC_TICK_INVALID;
        p_stream->i_last_dts = VLC_TICK_INVALID;
        p_stream->i_last_pts = VLC_TICK_INVALID;
    }
    return p_stream;
}

static void box_send(sout_mux_t *p_mux,  bo_t *box);

static block_t *ConvertSUBT(block_t *);
static bool CreateCurrentEdit(mp4_stream_t *, vlc_tick_t, bool);
static int MuxStream(sout_mux_t *p_mux, sout_input_t *p_input, mp4_stream_t *p_stream);

static int WriteSlowStartHeader(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bo_t *box;

    if (!mp4mux_Is(p_sys->muxh, QUICKTIME))
    {
        /* Now add ftyp header */
        box = mp4mux_GetFtyp(p_sys->muxh);
        if(!box)
            return VLC_ENOMEM;

        p_sys->i_pos += bo_size(box);
        p_sys->i_mdat_pos = p_sys->i_pos;
        box_send(p_mux, box);
    }

    /* Now add mdat header */
    box = box_new("mdat");
    if(!box)
        return VLC_ENOMEM;

    bo_add_64be(box, 0); // enough to store an extended size

    if(box->b)
        p_sys->i_pos += bo_size(box);

    box_send(p_mux, box);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = malloc(sizeof(sout_mux_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    msg_Dbg(p_mux, "Mp4 muxer opened");
    config_ChainParse(p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg);

    enum mp4mux_options options = 0;
    if(p_mux->psz_mux)
    {
        if(!strcmp(p_mux->psz_mux, "mov"))
            options |= QUICKTIME;
        if(!strcmp(p_mux->psz_mux, "mp4frag") || !strcmp(p_mux->psz_mux, "mp4stream"))
            options |= FRAGMENTED;
    }

    p_sys->b_3gp = p_mux->psz_mux && !strcmp(p_mux->psz_mux, "3gp");

    p_sys->muxh = mp4mux_New(options);

    p_sys->i_pos        = 0;
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;
    p_sys->i_mdat_pos   = 0;
    p_sys->b_header_sent = false;

    p_sys->i_read_duration   = 0;
    p_sys->i_written_duration= 0;
    p_sys->i_start_dts = VLC_TICK_INVALID;
    p_sys->i_mfhd_sequence = 1;

    p_mux->p_sys        = p_sys;
    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = (options & FRAGMENTED) ? MuxFrag : Mux;

    if(p_sys->b_3gp)
    {
        mp4mux_SetBrand(p_sys->muxh, BRAND_3gp6, 0x0);
        mp4mux_AddExtraBrand(p_sys->muxh, BRAND_3gp4);
    }
    else
    {
        mp4mux_SetBrand(p_sys->muxh, BRAND_isom, 0x0);
    }

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
    bool b_64bitext = (p_sys->i_pos > UINT32_MAX);
    if(b_64bitext)
        mp4mux_Set64BitExt(p_sys->muxh);

    uint64_t i_moov_pos = p_sys->i_pos;
    bo_t *moov = mp4mux_GetMoov(p_sys->muxh, VLC_OBJECT(p_mux), 0);

    /* Check we need to create "fast start" files */
    p_sys->b_fast_start = var_GetBool(p_this, SOUT_CFG_PREFIX "faststart");
    while (p_sys->b_fast_start && moov && moov->b)
    {
        /* Move data to the end of the file so we can fit the moov header
         * at the start */
        uint64_t i_mdatsize = p_sys->i_pos - p_sys->i_mdat_pos;

        /* moving samples will need new moov with 64bit atoms ? */
        if(!b_64bitext && p_sys->i_pos + bo_size(moov) > UINT32_MAX)
        {
            mp4mux_Set64BitExt(p_sys->muxh);
            b_64bitext = true;
            /* generate a new moov */
            bo_t *moov64 = mp4mux_GetMoov(p_sys->muxh, VLC_OBJECT(p_mux), 0);
            if(moov64)
            {
                bo_free(moov);
                moov = moov64;
            }
        }
        /* We now know our final MOOV size */

        /* Fix-up samples to chunks table in MOOV header to they point to next MDAT location */
        mp4mux_ShiftSamples(p_sys->muxh, bo_size(moov));
        msg_Dbg(p_this,"Moving data by %"PRIu64, (uint64_t)bo_size(moov));
        bo_t *shifted = mp4mux_GetMoov(p_sys->muxh, VLC_OBJECT(p_mux), 0);
        if(!shifted)
        {
            /* fail */
            p_sys->b_fast_start = false;
            continue;
        }
        assert(bo_size(shifted) == bo_size(moov));
        bo_free(moov);
        moov = shifted;

        /* Make space, move MDAT data by moov size towards the end */
        while (i_mdatsize > 0)
        {
            size_t i_chunk = __MIN(32768, i_mdatsize);
            block_t *p_buf = block_Alloc(i_chunk);
            sout_AccessOutSeek(p_mux->p_access,
                                p_sys->i_mdat_pos + i_mdatsize - i_chunk);
            ssize_t i_read = sout_AccessOutRead(p_mux->p_access, p_buf);
            if (i_read < 0 || (size_t) i_read < i_chunk) {
                msg_Warn(p_this, "read() not supported by access output, "
                          "won't create a fast start file");
                p_sys->b_fast_start = false;
                block_Release(p_buf);
                break;
            }
            sout_AccessOutSeek(p_mux->p_access, p_sys->i_mdat_pos + i_mdatsize +
                               bo_size(moov) - i_chunk);
            sout_AccessOutWrite(p_mux->p_access, p_buf);
            i_mdatsize -= i_chunk;
        }

        if (!p_sys->b_fast_start) /* failed above */
            continue;

        /* Update pos pointers */
        i_moov_pos = p_sys->i_mdat_pos;
        p_sys->i_mdat_pos += bo_size(moov);

        p_sys->b_fast_start = false;
    }

    /* Write MOOV header */
    sout_AccessOutSeek(p_mux->p_access, i_moov_pos);
    if (moov != NULL)
        box_send(p_mux, moov);

cleanup:
    /* Clean-up */
    for (unsigned int i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++)
        mp4_stream_Delete(p_sys->pp_streams[i_trak]);
    TAB_CLEAN(p_sys->i_nb_streams, p_sys->pp_streams);
    mp4mux_Delete(p_sys->muxh);
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
        pb_bool = va_arg(args, bool *);
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

    if(!mp4mux_CanMux(VLC_OBJECT(p_mux), p_input->p_fmt,
                      mp4mux_Is(p_sys->muxh, QUICKTIME) ? BRAND_qt__ : BRAND_isom,
                      mp4mux_Is(p_sys->muxh, FRAGMENTED)))
    {
        msg_Err(p_mux, "unsupported codec %4.4s in mp4",
                 (char*)&p_input->p_fmt->i_codec);
        return VLC_EGENERIC;
    }

    if(!(p_stream = mp4_stream_New()))
        return VLC_ENOMEM;

    uint32_t i_track_timescale = CLOCK_FREQ;
    es_format_t trackfmt;
    es_format_Init(&trackfmt, p_input->p_fmt->i_cat, p_input->p_fmt->i_codec);
    es_format_Copy(&trackfmt, p_input->p_fmt);

    switch( p_input->p_fmt->i_cat )
    {
    case AUDIO_ES:
        if(!trackfmt.audio.i_rate)
        {
            msg_Warn( p_mux, "no audio rate given for stream %d, assuming 48KHz",
                      p_sys->i_nb_streams );
            trackfmt.audio.i_rate = 48000;
        }
        i_track_timescale = trackfmt.audio.i_rate;
        break;
    case VIDEO_ES:
        if( !trackfmt.video.i_frame_rate ||
            !trackfmt.video.i_frame_rate_base )
        {
            msg_Warn( p_mux, "Missing frame rate for stream %d, assuming 25fps",
                      p_sys->i_nb_streams );
            trackfmt.video.i_frame_rate = 25;
            trackfmt.video.i_frame_rate_base = 1;
        }

        i_track_timescale = trackfmt.video.i_frame_rate *
                            trackfmt.video.i_frame_rate_base;

        if( i_track_timescale > CLOCK_FREQ )
            i_track_timescale = CLOCK_FREQ;
        else if( i_track_timescale < 90000 )
            i_track_timescale = 90000;
        break;
    default:
        break;
    }

    p_stream->tinfo = mp4mux_track_Add(p_sys->muxh, p_sys->i_nb_streams + 1,
                                       &trackfmt, i_track_timescale);
    es_format_Clean(&trackfmt);
    if(!p_stream->tinfo)
    {
        free(p_stream);
        return VLC_ENOMEM;
    }

    p_stream->extrabuilder = mux_extradata_builder_New(p_input->p_fmt->i_codec,
                                                       EXTRADATA_ISOBMFF);

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
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    mp4_stream_t *p_stream = (mp4_stream_t*)p_input->p_sys;

    if(!mp4mux_Is(p_sys->muxh, FRAGMENTED))
    {
        while(block_FifoCount(p_input->p_fifo) > 0 &&
              MuxStream(p_mux, p_input, p_stream) == VLC_SUCCESS) {};

        if(CreateCurrentEdit(p_stream, p_sys->i_start_dts, false))
            mp4mux_track_DebugEdits(VLC_OBJECT(p_mux), p_stream->tinfo);
    }

    msg_Dbg(p_mux, "removing input");
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static bool CreateCurrentEdit(mp4_stream_t *p_stream, vlc_tick_t i_mux_start_dts,
                              bool b_fragmented)
{
    const mp4mux_edit_t *p_lastedit = mp4mux_track_GetLastEdit(p_stream->tinfo);

    /* Never more than first empty edit for fragmented */
    if(p_lastedit != NULL && b_fragmented)
        return true;

    const mp4mux_sample_t *p_lastsample = mp4mux_track_GetLastSample(p_stream->tinfo);
    if(p_lastsample == NULL)
        return true;

    mp4mux_edit_t newedit;

    if(p_lastedit == NULL)
    {
        newedit.i_start_time = 0;
        newedit.i_start_offset = __MAX(0, p_stream->i_first_dts - i_mux_start_dts);
    }
    else
    {
        newedit.i_start_time = __MAX(0, p_lastedit->i_start_time + p_lastedit->i_duration);
        newedit.i_start_offset = 0;
    }

    if(b_fragmented)
    {
        newedit.i_duration = 0;
    }
    else
    {
        if(p_stream->i_last_pts != VLC_TICK_INVALID)
            newedit.i_duration = p_stream->i_last_pts - p_stream->i_first_dts;
        else
            newedit.i_duration = p_stream->i_last_dts - p_stream->i_first_dts;

        newedit.i_duration += p_lastsample->i_length;
    }

    return mp4mux_track_AddEdit(p_stream->tinfo, &newedit);
}

static block_t * BlockDequeue(sout_input_t *p_input, mp4_stream_t *p_stream)
{
    block_t *p_block = block_FifoGet(p_input->p_fifo);
    if(unlikely(!p_block))
        return NULL;

    /* Create on the fly extradata as packetizer is not in the loop */
    if(p_stream->extrabuilder && !mp4mux_track_HasSamplePriv(p_stream->tinfo))
    {
         mux_extradata_builder_Feed(p_stream->extrabuilder,
                                    p_block->p_buffer, p_block->i_buffer);
         const uint8_t *p_extra;
         size_t i_extra = mux_extradata_builder_Get(p_stream->extrabuilder, &p_extra);
         if(i_extra)
            mp4mux_track_SetSamplePriv(p_stream->tinfo, p_extra, i_extra);
    }

    switch(mp4mux_track_GetFmt(p_stream->tinfo)->i_codec)
    {
        case VLC_CODEC_AV1:
            p_block = AV1_Pack_Sample(p_block);
            break;
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
            p_block = hxxx_AnnexB_to_xVC(p_block, 4);
            break;
        case VLC_CODEC_SUBT:
            p_block = ConvertSUBT(p_block);
            break;
        default:
            break;
    }

    return p_block;
}

static inline vlc_tick_t dts_fb_pts( const block_t *p_data )
{
    return p_data->i_dts != VLC_TICK_INVALID ? p_data->i_dts: p_data->i_pts;
}

static int MuxStream(sout_mux_t *p_mux, sout_input_t *p_input, mp4_stream_t *p_stream)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    block_t *p_data = BlockDequeue(p_input, p_stream);
    if(!p_data)
        return VLC_SUCCESS;

    /* Reset reference dts in case of discontinuity (ex: gather sout) */
    if (p_data->i_flags & BLOCK_FLAG_DISCONTINUITY &&
        mp4mux_track_GetLastSample(p_stream->tinfo) != NULL)
    {
        if(p_stream->i_first_dts != VLC_TICK_INVALID)
        {
            if(!CreateCurrentEdit(p_stream, p_sys->i_start_dts,
                                  mp4mux_Is(p_sys->muxh, FRAGMENTED)))
            {
                block_Release( p_data );
                return VLC_ENOMEM;
            }
        }

        p_stream->i_length_neg = 0;
        p_stream->i_first_dts = VLC_TICK_INVALID;
        p_stream->i_last_dts = VLC_TICK_INVALID;
        p_stream->i_last_pts = VLC_TICK_INVALID;
    }

    /* Set current segment ranges */
    if( p_stream->i_first_dts == VLC_TICK_INVALID )
    {
        p_stream->i_first_dts = dts_fb_pts( p_data );
        if( p_sys->i_start_dts == VLC_TICK_INVALID )
            p_sys->i_start_dts = p_stream->i_first_dts;
    }

    if (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat != SPU_ES)
    {
        /* Fix length of the sample */
        if (block_FifoCount(p_input->p_fifo) > 0)
        {
            block_t *p_next = block_FifoShow(p_input->p_fifo);
            if ( p_next->i_flags & BLOCK_FLAG_DISCONTINUITY )
            { /* we have no way to know real length except by decoding */
                if ( mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES )
                {
                    p_data->i_length = vlc_tick_from_samples(
                            mp4mux_track_GetFmt(p_stream->tinfo)->video.i_frame_rate_base,
                            mp4mux_track_GetFmt(p_stream->tinfo)->video.i_frame_rate );
                    if( p_data->i_flags & BLOCK_FLAG_SINGLE_FIELD )
                        p_data->i_length >>= 1;
                    msg_Dbg( p_mux, "video track %u fixup to %"PRId64" for sample %u",
                             mp4mux_track_GetID(p_stream->tinfo), p_data->i_length,
                             mp4mux_track_GetSampleCount(p_stream->tinfo) );
                }
                else if ( mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == AUDIO_ES &&
                          mp4mux_track_GetFmt(p_stream->tinfo)->audio.i_rate &&
                          p_data->i_nb_samples )
                {
                    p_data->i_length = vlc_tick_from_samples(p_data->i_nb_samples,
                            mp4mux_track_GetFmt(p_stream->tinfo)->audio.i_rate);
                    msg_Dbg( p_mux, "audio track %u fixup to %"PRId64" for sample %u",
                             mp4mux_track_GetID(p_stream->tinfo), p_data->i_length,
                             mp4mux_track_GetSampleCount(p_stream->tinfo) );
                }
                else if ( p_data->i_length <= 0 )
                {
                    msg_Warn( p_mux, "unknown length for track %u sample %u",
                              mp4mux_track_GetID(p_stream->tinfo),
                              mp4mux_track_GetSampleCount(p_stream->tinfo) );
                    p_data->i_length = 1;
                }
            }
            else
            {
                vlc_tick_t i_diff  = dts_fb_pts( p_next ) - dts_fb_pts( p_data );
                if (i_diff < VLC_TICK_FROM_SEC(1)) /* protection */
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
    else /* SPU_ES */
    {
        const mp4mux_sample_t *p_lastsample = mp4mux_track_GetLastSample(p_stream->tinfo);
        if (p_lastsample != NULL && p_lastsample->i_length == 0)
        {
            mp4mux_sample_t updated = *p_lastsample;
            /* length of previous spu, stored in spu clearer */
            int64_t i_length = dts_fb_pts( p_data ) - p_stream->i_last_dts;
            if(i_length < 0)
                i_length = 0;
            /* Fix entry */
            updated.i_length = i_length;
            mp4mux_track_UpdateLastSample(p_stream->tinfo, &updated);
        }
    }

    /* Flag interlacing on first block */
    if(mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES &&
       (p_data->i_flags & BLOCK_FLAG_INTERLACED_MASK) &&
       mp4mux_track_GetInterlacing(p_stream->tinfo) == INTERLACING_NONE)
    {
        if(p_data->i_flags & BLOCK_FLAG_SINGLE_FIELD)
            mp4mux_track_SetInterlacing(p_stream->tinfo, INTERLACING_SINGLE_FIELD);
        else if(p_data->i_flags & BLOCK_FLAG_TOP_FIELD_FIRST)
            mp4mux_track_SetInterlacing(p_stream->tinfo, INTERLACING_TOPBOTTOM);
        else
            mp4mux_track_SetInterlacing(p_stream->tinfo, INTERLACING_BOTTOMTOP);
    }

    /* Update (Not earlier for SPU!) */
    p_stream->i_last_dts = dts_fb_pts( p_data );
    if( p_data->i_pts > p_stream->i_last_pts )
        p_stream->i_last_pts = p_data->i_pts;

    /* add index entry */
    mp4mux_sample_t sample;
    sample.i_pos    = p_sys->i_pos;
    sample.i_size   = p_data->i_buffer;

    if ( p_data->i_dts != VLC_TICK_INVALID && p_data->i_pts > p_data->i_dts )
        sample.i_pts_dts = p_data->i_pts - p_data->i_dts;
    else
        sample.i_pts_dts = 0;

    sample.i_length = p_data->i_length;
    sample.i_flags  = p_data->i_flags;

    /* update */
    p_stream->i_last_dts = dts_fb_pts( p_data );

    /* write data */
    if(mp4mux_track_AddSample(p_stream->tinfo, &sample))
    {
        p_sys->i_pos += p_data->i_buffer;
        sout_AccessOutWrite(p_mux->p_access, p_data);
    }

    /* Add SPU clearing tag (duration tb fixed on next SPU or stream end )*/
    if ( mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == SPU_ES && sample.i_length > 0 )
    {
        block_t *p_empty = NULL;
        if(mp4mux_track_GetFmt(p_stream->tinfo)->i_codec == VLC_CODEC_SUBT||
           mp4mux_track_GetFmt(p_stream->tinfo)->i_codec == VLC_CODEC_QTXT||
           mp4mux_track_GetFmt(p_stream->tinfo)->i_codec == VLC_CODEC_TX3G)
        {
            p_empty = block_Alloc(3);
            if(p_empty)
            {
                /* Write a " " */
                p_empty->p_buffer[0] = 0;
                p_empty->p_buffer[1] = 1;
                p_empty->p_buffer[2] = ' ';
            }
        }
        else if(mp4mux_track_GetFmt(p_stream->tinfo)->i_codec == VLC_CODEC_TTML)
        {
            p_empty = block_Alloc(40);
            if(p_empty)
                memcpy(p_empty->p_buffer, "<tt><body><div><p></p></div></body></tt>", 40);
        }
        else if(mp4mux_track_GetFmt(p_stream->tinfo)->i_codec == VLC_CODEC_WEBVTT)
        {
            p_empty = block_Alloc(8);
            if(p_empty)
                memcpy(p_empty->p_buffer, "\x00\x00\x00\x08vtte", 8);
        }

        /* point to start of our empty */
        p_stream->i_last_dts += sample.i_length;

        if(p_empty)
        {
            /* Append a idx entry */
            /* XXX: No need to grow the entry here */
            mp4mux_sample_t closersample;
            closersample.i_pos    = p_sys->i_pos;
            closersample.i_size   = p_empty->i_buffer;
            closersample.i_pts_dts= 0;
            closersample.i_length = 0; /* will add dts diff later*/
            closersample.i_flags  = 0;

            if(mp4mux_track_AddSample(p_stream->tinfo, &closersample))
            {
                p_sys->i_pos += p_empty->i_buffer;
                sout_AccessOutWrite(p_mux->p_access, p_empty);
            }
        }
    }

    /* Update the global segment/media duration */
    if( mp4mux_track_GetDuration(p_stream->tinfo) > p_sys->i_read_duration )
        p_sys->i_read_duration = mp4mux_track_GetDuration(p_stream->tinfo);

    return VLC_SUCCESS;
}

static int Mux(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_ret = VLC_SUCCESS;

    if(!p_sys->b_header_sent)
    {
        i_ret = WriteSlowStartHeader(p_mux);
        if(i_ret != VLC_SUCCESS)
            return i_ret;
        p_sys->b_header_sent = true;
    }

    do
    {
        int i_stream = sout_MuxGetStream(p_mux, 2, NULL);
        if (i_stream < 0)
            break;

        sout_input_t *p_input  = p_mux->pp_inputs[i_stream];
        mp4_stream_t *p_stream = (mp4_stream_t*)p_input->p_sys;

        i_ret = MuxStream(p_mux, p_input, p_stream);
    } while( i_ret == VLC_SUCCESS );

    return i_ret;
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

static void box_send(sout_mux_t *p_mux,  bo_t *box)
{
    assert(box != NULL);
    if (box->b)
        sout_AccessOutWrite(p_mux->p_access, box->b);
    free(box);
}

/***************************************************************************
    MP4 Live submodule
****************************************************************************/
#define FRAGMENT_LENGTH  VLC_TICK_FROM_MS(1500)

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
                             const vlc_tick_t i_time)
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

    vlc_tick_t i_last_entry_time;
    if (p_stream->i_indexentries)
        i_last_entry_time = p_stream->p_indexentries[p_stream->i_indexentries - 1].i_time;
    else
        i_last_entry_time = 0;

    if (p_entries && i_time - i_last_entry_time >= VLC_TICK_FROM_SEC(2))
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
                        vlc_tick_t i_barrier_time, const uint64_t i_write_pos)
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
        vlc_tick_t i_time = p_stream->i_written_duration;
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
                p_stream->read.p_first->p_block->i_length !=
                    mp4mux_track_GetDefaultSampleDuration(p_stream->tinfo) &&
                p_stream->read.p_first->p_block->i_length)
                    i_tfhd_flags |= MP4_TFHD_DFLT_SAMPLE_DURATION;

            /* Current segment have all same size value, different than trex's default */
            if (b_allsamesize &&
                p_stream->read.p_first->p_block->i_buffer !=
                    mp4mux_track_GetDefaultSampleSize(p_stream->tinfo) &&
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
        bo_add_32be(tfhd, mp4mux_track_GetID(p_stream->tinfo));

        /* set the local sample duration default */
        if (i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_DURATION)
            bo_add_32be(tfhd, samples_from_vlc_tick(p_stream->read.p_first->p_block->i_length,
                                                    mp4mux_track_GetTimescale(p_stream->tinfo)));

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
        bo_add_64be(tfdt, samples_from_vlc_tick(p_stream->i_written_duration,
                                                mp4mux_track_GetTimescale(p_stream->tinfo)) );
        box_gather(traf, tfdt);

        /* *** add /moof/traf/trun *** */
        if (p_stream->read.p_first)
        {
            uint32_t i_trun_flags = 0x0;

            if (p_stream->b_hasiframes && !(p_stream->read.p_first->p_block->i_flags & BLOCK_FLAG_TYPE_I))
                i_trun_flags |= MP4_TRUN_FIRST_FLAGS;

            if (!b_allsamelength ||
                ( !(i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_DURATION) &&
                    mp4mux_track_GetDefaultSampleDuration(p_stream->tinfo) == 0 ))
                i_trun_flags |= MP4_TRUN_SAMPLE_DURATION;

            if (!b_allsamesize ||
                ( !(i_tfhd_flags & MP4_TFHD_DFLT_SAMPLE_SIZE) &&
                  mp4mux_track_GetDefaultSampleSize(p_stream->tinfo) == 0 ))
                i_trun_flags |= MP4_TRUN_SAMPLE_SIZE;

            if (mp4mux_track_HasBFrames(p_stream->tinfo))
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
            vlc_tick_t i_run_time = p_stream->i_written_duration;
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
                i_fixupoffset = bo_size(moof) + bo_size(traf) + bo_size(trun);
                bo_add_32be(trun, 0xdeadbeef); // data offset
            }

            if (i_trun_flags & MP4_TRUN_FIRST_FLAGS)
                bo_add_32be(trun, 1<<16); // flag as non keyframe

            while(p_stream->read.p_first && i_entry_count)
            {
                DEQUEUE_ENTRY(p_stream->read, p_entry);

                if (i_trun_flags & MP4_TRUN_SAMPLE_DURATION)
                    bo_add_32be(trun, samples_from_vlc_tick(p_entry->p_block->i_length,
                                                            mp4mux_track_GetTimescale(p_stream->tinfo))); // sample duration

                if (i_trun_flags & MP4_TRUN_SAMPLE_SIZE)
                    bo_add_32be(trun, p_entry->p_block->i_buffer); // sample size

                if (i_trun_flags & MP4_TRUN_SAMPLE_TIME_OFFSET)
                {
                    vlc_tick_t i_diff = 0;
                    if ( p_entry->p_block->i_dts  != VLC_TICK_INVALID &&
                         p_entry->p_block->i_pts > p_entry->p_block->i_dts )
                    {
                        i_diff = p_entry->p_block->i_pts - p_entry->p_block->i_dts;
                    }
                    bo_add_32be(trun, samples_from_vlc_tick(i_diff, mp4mux_track_GetTimescale(p_stream->tinfo))); // ctts
                }

                *pi_mdat_total_size += p_entry->p_block->i_buffer;

                ENQUEUE_ENTRY(p_stream->towrite, p_entry);
                i_entry_count--;
                i_sample++;

                /* Add keyframe entry if needed */
                if (p_stream->b_hasiframes && (p_entry->p_block->i_flags & BLOCK_FLAG_TYPE_I) &&
                    (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES ||
                     mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == AUDIO_ES))
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

    box_fix(moof, bo_size(moof));

    /* do tfhd base data offset fixup */
    if (i_fixupoffset)
    {
        /* mdat will follow moof */
        bo_set_32be(moof, i_fixupoffset, bo_size(moof) + 8);
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
    assert(bo_size(mdat)==8);
    box_fix(mdat, bo_size(mdat) + i_total_size);
    p_sys->i_pos += bo_size(mdat);
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
            bo_add_32be(tfra, mp4mux_track_GetID(p_stream->tinfo));
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

    if(p_sys->i_pos >= (((uint64_t)0x1) << 32))
        mp4mux_Set64BitExt(p_sys->muxh);

    /* Now add ftyp header */
    bo_t *ftyp = mp4mux_GetFtyp(p_sys->muxh);
    if(!ftyp)
        return;

    bo_t *moov = mp4mux_GetMoov(p_sys->muxh, VLC_OBJECT(p_mux), 0);

    /* merge into a single block */
    box_gather(ftyp, moov);

    /* add header flag for streaming server */
    ftyp->b->i_flags |= BLOCK_FLAG_HEADER;
    p_sys->i_pos += bo_size(ftyp);
    box_send(p_mux, ftyp);
    p_sys->b_header_sent = true;
}

static void WriteFragments(sout_mux_t *p_mux, bool b_flush)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;
    bo_t *moof = NULL;
    vlc_tick_t i_barrier_time = p_sys->i_written_duration + FRAGMENT_LENGTH;
    size_t i_mdat_size = 0;
    bool b_has_samples = false;

    if(!p_sys->b_header_sent)
    {
        for (unsigned int j = 0; j < p_sys->i_nb_streams; j++)
        {
            mp4_stream_t *p_stream = p_sys->pp_streams[j];
            if(CreateCurrentEdit(p_stream, p_sys->i_start_dts, true))
                mp4mux_track_DebugEdits(VLC_OBJECT(p_mux), p_stream->tinfo);
        }
    }

    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
    {
        const mp4_stream_t *p_stream = p_sys->pp_streams[i];
        if (p_stream->read.p_first)
        {
            b_has_samples = true;

            /* set a barrier so we try to align to keyframe */
            if (p_stream->b_hasiframes &&
                    p_stream->i_last_iframe_time > p_stream->i_written_duration &&
                    (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES ||
                     mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == AUDIO_ES) )
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
        p_sys->i_pos += bo_size(moof);
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
    if ( mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES &&
         mp4mux_track_GetFmt(p_stream->tinfo)->video.i_frame_rate )
    {
        p_entrydata->i_length = vlc_tick_from_samples(
                mp4mux_track_GetFmt(p_stream->tinfo)->video.i_frame_rate_base,
                mp4mux_track_GetFmt(p_stream->tinfo)->video.i_frame_rate);
        msg_Dbg(p_mux, "video track %d fixup to %"PRId64" for sample %u",
                mp4mux_track_GetID(p_stream->tinfo), p_entrydata->i_length,
                mp4mux_track_GetSampleCount(p_stream->tinfo) - 1);
    }
    else if (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == AUDIO_ES &&
             mp4mux_track_GetFmt(p_stream->tinfo)->audio.i_rate &&
             p_entrydata->i_nb_samples && mp4mux_track_GetFmt(p_stream->tinfo)->audio.i_rate)
    {
        p_entrydata->i_length = vlc_tick_from_samples(p_entrydata->i_nb_samples,
                mp4mux_track_GetFmt(p_stream->tinfo)->audio.i_rate);
        msg_Dbg(p_mux, "audio track %d fixup to %"PRId64" for sample %u",
                mp4mux_track_GetID(p_stream->tinfo), p_entrydata->i_length,
                mp4mux_track_GetSampleCount(p_stream->tinfo) - 1);
    }
    else
    {
        msg_Warn(p_mux, "unknown length for track %d sample %u",
                 mp4mux_track_GetID(p_stream->tinfo),
                 mp4mux_track_GetSampleCount(p_stream->tinfo) - 1);
        p_entrydata->i_length = 1;
    }
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
            if (mfro)
            {
                if (mfra->b)
                {
                    box_fix(mfra, bo_size(mfra));
                    bo_add_32be(mfro, bo_size(mfra) + MP4_MFRO_BOXSIZE);
                }
                box_gather(mfra, mfro);
            }
            box_send(p_mux, mfra);
        }
    }

    for (unsigned int i = 0; i < p_sys->i_nb_streams; i++)
        mp4_stream_Delete(p_sys->pp_streams[i]);
    TAB_CLEAN(p_sys->i_nb_streams, p_sys->pp_streams);
    mp4mux_Delete(p_sys->muxh);
    free(p_sys);
}

static int MuxFrag(sout_mux_t *p_mux)
{
    sout_mux_sys_t *p_sys = (sout_mux_sys_t*) p_mux->p_sys;

    int i_stream = sout_MuxGetStream(p_mux, 1, NULL);
    if (i_stream < 0)
        return VLC_SUCCESS;

    sout_input_t *p_input  = p_mux->pp_inputs[i_stream];
    mp4_stream_t *p_stream = (mp4_stream_t*) p_input->p_sys;
    block_t *p_currentblock = BlockDequeue(p_input, p_stream);
    if( !p_currentblock )
        return VLC_SUCCESS;

    /* Set time ranges */
    if( p_stream->i_first_dts == VLC_TICK_INVALID )
    {
        p_stream->i_first_dts = p_currentblock->i_dts;
        if( p_sys->i_start_dts == VLC_TICK_INVALID )
            p_sys->i_start_dts = p_currentblock->i_dts;
    }

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
            mp4mux_track_GetDuration(p_stream->tinfo) - p_sys->i_written_duration < FRAGMENT_LENGTH)
        {
            /* Flag the last iframe time, we'll use it as boundary so it will start
               next fragment */
            p_stream->i_last_iframe_time = mp4mux_track_GetDuration(p_stream->tinfo);
        }

        /* update buffered time */
        mp4mux_track_ForceDuration(p_stream->tinfo,
                                 mp4mux_track_GetDuration(p_stream->tinfo) +
                                 __MAX(0, p_heldblock->i_length));
    }


    /* set temp entry */
    p_stream->p_held_entry = malloc(sizeof(mp4_fragentry_t));
    if (unlikely(!p_stream->p_held_entry))
        return VLC_ENOMEM;

    p_stream->p_held_entry->p_block  = p_currentblock;
    p_stream->p_held_entry->i_run    = p_stream->i_current_run;
    p_stream->p_held_entry->p_next   = NULL;

    if (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat == VIDEO_ES )
    {
        if (!p_stream->b_hasiframes && (p_currentblock->i_flags & BLOCK_FLAG_TYPE_I))
            p_stream->b_hasiframes = true;

        if (!mp4mux_track_HasBFrames(p_stream->tinfo) &&
            p_currentblock->i_dts != VLC_TICK_INVALID &&
            p_currentblock->i_pts > p_currentblock->i_dts)
                mp4mux_track_SetHasBFrames(p_stream->tinfo);
    }

    /* Update the global fragment/media duration */
    vlc_tick_t i_min_read_duration = mp4mux_track_GetDuration(p_stream->tinfo);
    vlc_tick_t i_min_written_duration = p_stream->i_written_duration;
    for (unsigned int i=0; i<p_sys->i_nb_streams; i++)
    {
        const mp4_stream_t *p_s = p_sys->pp_streams[i];
        if (mp4mux_track_GetFmt(p_stream->tinfo)->i_cat != VIDEO_ES &&
            mp4mux_track_GetFmt(p_stream->tinfo)->i_cat != AUDIO_ES)
            continue;
        if (mp4mux_track_GetDuration(p_s->tinfo) < i_min_read_duration)
            i_min_read_duration = mp4mux_track_GetDuration(p_s->tinfo);

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
