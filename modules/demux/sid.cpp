/**
 * @file sid.cpp
 * @brief Sidplay demux module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
 * Copyright © 2010 Alan Fischer <alan@lightningtoads.com>
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
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* INT64_C and UINT64_C are only exposed to c++ if this is defined */
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#include <limits.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname ("sid")
    set_description ( N_("C64 sid demuxer") )
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_DEMUX)
    set_capability ("demux", 100)
    set_callbacks (Open, Close)
vlc_module_end ()

struct demux_sys_t
{
    sidplay2 *player;
    sid2_config_t config;
    sid2_info_t info;
    SidTune *tune;
    SidTuneInfo tuneInfo;

    int bytes_per_frame;
    int block_size;
    es_out_id_t *es;
    date_t pts;
};


static int Demux (demux_t *);
static int Control (demux_t *, int, va_list);

static int Open (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = NULL;
    es_format_t fmt;
    bool result = false;
    SidTune *tune = NULL;
    sidplay2 *player = NULL;
    ReSIDBuilder *builder = NULL;

    int64_t size = stream_Size (demux->s);
    if (size < 4 || size > LONG_MAX) /* We need to load the whole file for sidplay */
        return VLC_EGENERIC;

    const uint8_t *peek;
    if (stream_Peek (demux->s, &peek, 4) < 4)
        return VLC_EGENERIC;

    /* sidplay2 can read PSID and the newer RSID formats */
    if(memcmp(peek,"PSID",4)!=0 && memcmp(peek,"RSID",4)!=0)
        return VLC_EGENERIC;

    uint8_t *data = (uint8_t*) malloc(size);
    if (unlikely (data==NULL))
        goto error;

    if (stream_Read (demux->s,data,size) < size) {
        free (data);
        goto error;
    }

    tune = new SidTune(0);
    if (unlikely (tune==NULL)) {
        free (data);
        goto error;
    }

    result = tune->read (data, size);
    free (data);
    if (!result)
        goto error;

    player = new sidplay2();
    if (unlikely(player==NULL))
        goto error;

    sys = (demux_sys_t*) calloc (1, sizeof(demux_sys_t));
    if (unlikely(sys==NULL))
        goto error;

    sys->player = player;
    sys->tune = tune;

    tune->getInfo (sys->tuneInfo);

    sys->info = player->info();
    sys->config = player->config();

    builder = new ReSIDBuilder ("ReSID");
    if (unlikely(builder==NULL))
        goto error;

    builder->create (sys->info.maxsids);
    builder->sampling (sys->config.frequency);

    sys->config.sidEmulation = builder;
    sys->config.precision    = 16;
    sys->config.playback     = (sys->info.channels == 2 ? sid2_stereo : sid2_mono);

    player->config (sys->config);

    sys->bytes_per_frame = sys->info.channels * sys->config.precision / 8;
    sys->block_size = sys->config.frequency / 10 * sys->bytes_per_frame;

    es_format_Init (&fmt, AUDIO_ES, VLC_CODEC_S16N);

    fmt.audio.i_channels        = sys->info.channels;
    fmt.audio.i_bitspersample   = sys->config.precision;
    fmt.audio.i_rate            = sys->config.frequency;
    fmt.audio.i_bytes_per_frame = sys->bytes_per_frame;
    fmt.audio.i_frame_length    = fmt.audio.i_bytes_per_frame;
    fmt.audio.i_blockalign      = fmt.audio.i_bytes_per_frame;

    fmt.i_bitrate = fmt.audio.i_rate * fmt.audio.i_bytes_per_frame;

    sys->es = es_out_Add (demux->out, &fmt);

    date_Init (&sys->pts, fmt.audio.i_rate, 1);
    date_Set (&sys->pts, 0);

    sys->tune->selectSong (0);
    result = (sys->player->load (sys->tune) >=0 );
    sys->player->fastForward (100);
    if (!result)
        goto error;

    /* Callbacks */
    demux->pf_demux = Demux;
    demux->pf_control = Control;
    demux->p_sys = sys;

    return VLC_SUCCESS;

error:
    msg_Err (demux, "An error occurred during sid demuxing" );
    delete player;
    delete builder;
    delete tune;
    free (sys);
    return VLC_EGENERIC;
}


static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    delete sys->player;
    delete sys->config.sidEmulation;
    delete sys->tune;
    free (sys);
}

static int Demux (demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    block_t *block = block_Alloc( sys->block_size);
    if (unlikely(block==NULL))
        return 0;

    if (!sys->tune->getStatus()) {
        block_Release (block);
        return 0;
    }

    int i_read = sys->player->play ((void*)block->p_buffer, block->i_buffer);
    if (i_read <= 0) {
        block_Release (block);
        return 0;
    }
    block->i_buffer = i_read;
    block->i_pts = block->i_dts = VLC_TS_0 + date_Get (&sys->pts);

    es_out_Control (demux->out, ES_OUT_SET_PCR, block->i_pts);

    es_out_Send (demux->out, sys->es, block);

    date_Increment (&sys->pts, i_read / sys->bytes_per_frame);

    return 1;
}


static int Control (demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_TIME : {
            int64_t *v = va_arg (args, int64_t*);
            *v = sys->player->time() * sys->player->timebase() * (CLOCK_FREQ / 100);
            return VLC_SUCCESS;
        }

        case DEMUX_GET_META : {
            vlc_meta_t *p_meta = (vlc_meta_t *) va_arg (args, vlc_meta_t*);

            /* These are specified in the sid tune class as 0 = Title, 1 = Artist, 2 = Copyright/Publisher */
            vlc_meta_SetTitle( p_meta, sys->tuneInfo.infoString[0] );
            vlc_meta_SetArtist( p_meta, sys->tuneInfo.infoString[1] );
            vlc_meta_SetCopyright( p_meta, sys->tuneInfo.infoString[2] );

            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO :
            if ( sys->tuneInfo.songs > 1 ) {
                input_title_t ***ppp_title = (input_title_t***) va_arg (args, input_title_t***);
                int *pi_int    = (int*)va_arg( args, int* );

                *pi_int = sys->tuneInfo.songs;
                *ppp_title = (input_title_t**) malloc( sizeof (input_title_t**) * sys->tuneInfo.songs);

                for( int i = 0; i < sys->tuneInfo.songs; i++ ) {
                    (*ppp_title)[i] = vlc_input_title_New();
                }

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE : {
            int i_idx = (int) va_arg (args, int);
            sys->tune->selectSong (i_idx+1);
            bool result = (sys->player->load (sys->tune) >=0 );
            if (!result)
                return  VLC_EGENERIC;

            demux->info.i_title = i_idx;
            demux->info.i_update = INPUT_UPDATE_TITLE;
            msg_Dbg( demux, "set song %i", i_idx);

            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}
