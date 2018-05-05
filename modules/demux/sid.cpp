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

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#include <limits.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>

#include <new>

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

namespace {

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

    int last_title;
    bool title_changed;
};

} // namespace

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
    if (vlc_stream_Peek (demux->s, &peek, 4) < 4)
        return VLC_EGENERIC;

    /* sidplay2 can read PSID and the newer RSID formats */
    if(memcmp(peek,"PSID",4)!=0 && memcmp(peek,"RSID",4)!=0)
        return VLC_EGENERIC;

    uint8_t *data = (uint8_t*) malloc(size);
    if (unlikely (data==NULL))
        goto error;

    if (vlc_stream_Read (demux->s,data,size) < size) {
        free (data);
        goto error;
    }

    tune = new (std::nothrow) SidTune(0);
    if (unlikely (tune==NULL)) {
        free (data);
        goto error;
    }

    result = tune->read (data, size);
    free (data);
    if (!result)
        goto error;

    player = new (std::nothrow) sidplay2();
    if (unlikely(player==NULL))
        goto error;

    sys = reinterpret_cast<demux_sys_t *>(calloc(1, sizeof(demux_sys_t)));
    if (unlikely(sys==NULL))
        goto error;

    sys->player = player;
    sys->tune = tune;

    tune->getInfo (sys->tuneInfo);

    sys->info = player->info();
    sys->config = player->config();

    builder = new (std::nothrow) ReSIDBuilder ("ReSID");
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
    date_Set(&sys->pts, VLC_TICK_0);

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
    demux_sys_t *sys = reinterpret_cast<demux_sys_t *>(demux->p_sys);

    delete sys->player;
    delete sys->config.sidEmulation;
    delete sys->tune;
    free (sys);
}

static int Demux (demux_t *demux)
{
    demux_sys_t *sys = reinterpret_cast<demux_sys_t *>(demux->p_sys);

    block_t *block = block_Alloc( sys->block_size);
    if (unlikely(block==NULL))
        return VLC_DEMUXER_EOF;

    if (!sys->tune->getStatus()) {
        block_Release (block);
        return VLC_DEMUXER_EOF;
    }

    int i_read = sys->player->play ((void*)block->p_buffer, block->i_buffer);
    if (i_read <= 0) {
        block_Release (block);
        return VLC_DEMUXER_EOF;
    }
    block->i_buffer = i_read;
    block->i_pts = block->i_dts = date_Get (&sys->pts);

    es_out_SetPCR (demux->out, block->i_pts);

    es_out_Send (demux->out, sys->es, block);

    date_Increment (&sys->pts, i_read / sys->bytes_per_frame);

    return VLC_DEMUXER_SUCCESS;
}


static int Control (demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = reinterpret_cast<demux_sys_t *>(demux->p_sys);

    switch (query)
    {
        case DEMUX_GET_TIME : {
            /* FIXME resolution in 100ns? */
            *va_arg (args, vlc_tick_t*) =
                sys->player->time() * sys->player->timebase() * VLC_TICK_FROM_MS(10);
            return VLC_SUCCESS;
        }

        case DEMUX_GET_META : {
            vlc_meta_t *p_meta = va_arg (args, vlc_meta_t *);

            /* These are specified in the sid tune class as 0 = Title, 1 = Artist, 2 = Copyright/Publisher */
            vlc_meta_SetTitle( p_meta, sys->tuneInfo.infoString[0] );
            vlc_meta_SetArtist( p_meta, sys->tuneInfo.infoString[1] );
            vlc_meta_SetCopyright( p_meta, sys->tuneInfo.infoString[2] );

            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO :
            if ( sys->tuneInfo.songs > 1 ) {
                input_title_t ***ppp_title = va_arg (args, input_title_t ***);
                int *pi_int = va_arg( args, int* );

                *pi_int = sys->tuneInfo.songs;
                *ppp_title = (input_title_t**) vlc_alloc( sys->tuneInfo.songs, sizeof (input_title_t*));

                for( int i = 0; i < sys->tuneInfo.songs; i++ ) {
                    (*ppp_title)[i] = vlc_input_title_New();
                }

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE : {
            int i_idx = va_arg (args, int);
            sys->tune->selectSong (i_idx+1);
            bool result = (sys->player->load (sys->tune) >=0 );
            if (!result)
                return  VLC_EGENERIC;

            sys->last_title = i_idx;
            sys->title_changed = true;
            msg_Dbg( demux, "set song %i", i_idx);

            return VLC_SUCCESS;
        }

        case DEMUX_TEST_AND_CLEAR_FLAGS: {
            unsigned *restrict flags = va_arg(args, unsigned *);

            if ((*flags & INPUT_UPDATE_TITLE) && sys->title_changed) {
                *flags = INPUT_UPDATE_TITLE;
                sys->title_changed = false;
            } else
                *flags = 0;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE:
            *va_arg(args, int *) = sys->last_title;
            return VLC_SUCCESS;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( demux->s, 0, -1, 0,
                                          sys->bytes_per_frame, query, args );
    }

    return VLC_EGENERIC;
}
