/*****************************************************************************
 * gme.cpp: Game Music files demuxer (using Game_Music_Emu)
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jean Sreng <fox@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "Nsf_Emu.h"
#include "Gbs_Emu.h"
#include "Vgm_Emu.h"
#include "Spc_Emu.h"
#include "Gym_Emu.h"

#ifdef HAVE_ZLIB_H
    #include "zlib.h"
#endif

using namespace std;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_shortname( "GME");
    set_description( N_("GME demuxer (Game_Music_Emu)" ) );
    set_capability( "demux", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "gme" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

enum EmuType_e
{
    EMU_NSF     = 0,
    EMU_GBS     = 1,
    EMU_VGM     = 2,
    EMU_SPC     = 3,
    EMU_GYM     = 4
};

static const char* type_str[] =
{
    "NSF (Nes)", "GBS (Gameboy)", "VGM (Master System/Game Gear/Genesis)", "SPC (Super Nes)", "GYM (Genesis)"
};

struct demux_sys_t
{
    es_format_t       fmt;
    es_out_id_t      *es;

    int64_t           i_time;
    int64_t           i_length;

    int               i_data;
    uint8_t          *p_data;
    int               i_type;
    int               i_tracks;
    Music_Emu        *p_musicemu;
    Emu_Mem_Reader   *p_reader;
    vlc_meta_t       *p_meta;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

#ifdef HAVE_ZLIB_H
static void inflate_gzbuf(uint8_t * p_buffer, size_t i_size, uint8_t ** pp_obuffer, size_t * pi_osize);
#endif

static const char* gme_ext[] =
{
    "nsf", "nsfe", "gbs", "vgm", "vgz", "spc", "gym", NULL
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    char        *ext;
    int         i;
    vlc_value_t val;
 
    /* We accept file based on extention match */
    if( !p_demux->b_force )
    {
        if( ( ext = strrchr( p_demux->psz_path, '.' ) ) == NULL ||
            stream_Size( p_demux->s ) == 0 ) return VLC_EGENERIC;

        ext++;  /* skip . */
        for( i = 0; gme_ext[i] != NULL; i++ )
        {
            if( !strcasecmp( ext, gme_ext[i] ) )
            {
                break;
            }
        }
        if( gme_ext[i] == NULL ) return VLC_EGENERIC;
        msg_Dbg( p_demux, "running GME demuxer (ext=%s)", gme_ext[i] );
    }

#ifndef HAVE_ZLIB_H
    if (i == 4) /* gzipped vgm */
    {
        msg_Dbg( p_demux, "zlib unvailable, unable to read gzipped vgz file" );
        return VLC_EGENERIC;
    }
#endif

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = (demux_sys_t *)malloc( sizeof( demux_sys_t ) );

    msg_Dbg( p_demux, "loading complete file (could be long)" );
    p_sys->i_data = stream_Size( p_demux->s );
    p_sys->p_data = (uint8_t *)malloc( p_sys->i_data );
    p_sys->i_data = stream_Read( p_demux->s, p_sys->p_data, p_sys->i_data );
    if( p_sys->i_data <= 0 )
    {
        msg_Err( p_demux, "failed to read the complete file" );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Prepare emulator */
 
#ifdef HAVE_ZLIB_H
    if (i == 4) /* gzipped vgm */
    {
        uint8_t * p_outbuffer;
        size_t i_outsize;

        inflate_gzbuf( p_sys->p_data, p_sys->i_data, &p_outbuffer, &i_outsize );
 
        if (p_outbuffer == NULL)
        {
            msg_Err( p_demux, "failed to understand the file : unable to inflate vgz file" );
            /* we try to seek to recover for other plugin */
            stream_Seek( p_demux->s, 0 );
            free( p_sys->p_data );
            free( p_sys );
            return VLC_EGENERIC;
        }

        free(p_sys->p_data);

        p_sys->p_data = p_outbuffer;
        p_sys->i_data = i_outsize;
    }
#endif

    p_sys->p_reader = new Emu_Mem_Reader( p_sys->p_data, p_sys->i_data );
 
    switch(i)
    {
        case 0:
        case 1:
            p_sys->i_type = EMU_NSF;
            break;
        case 2:
            p_sys->i_type = EMU_GBS;
            break;
        case 3:
        case 4:
            p_sys->i_type = EMU_VGM;
            break;
        case 5:
            p_sys->i_type = EMU_SPC;
            break;
        case 6:
            p_sys->i_type = EMU_GYM;
            break;
    }

    /* Emulator specific initialization */

#define INIT_EMU(type) \
        type##_Emu::header_t header; \
        type##_Emu * p_emu = new type##_Emu; \
        p_emu->init( 44100 ); \
        p_sys->p_musicemu = p_emu; \
        p_sys->p_reader->read( &header, sizeof(header) ); \
        p_error = p_emu->load( header, *(p_sys->p_reader) );

    p_sys->p_meta = vlc_meta_New();

    char psz_temp[512];
 
    /// \todo Reinstate meta codec name
    //SET_META( VLC_META_CODEC_NAME, type_str[p_sys->i_type])
 
    const char * p_error;

    switch(p_sys->i_type)
    {
        case EMU_NSF:
        {
            INIT_EMU(Nsf)
            if (p_error == NULL)
            {
                vlc_meta_SetTitle( p_meta, header.game );
                vlc_meta_SetArtist( p_meta, header.author );
                vlc_meta_SetCopyright( p_meta, header.copyright );
                p_sys->i_tracks = p_emu->track_count();
            }
        }
        break;
        case EMU_GBS:
        {
            INIT_EMU(Gbs)
            if (p_error == NULL)
            {
                vlc_meta_SetTitle( p_meta, header.game );
                vlc_meta_SetArtist( p_meta, header.author );
                vlc_meta_SetCopyright( p_meta, header.copyright );
                p_sys->i_tracks = p_emu->track_count();
            }
        }
        break;
        case EMU_VGM:
        {
            INIT_EMU(Vgm)
            if (p_error == NULL)
            {
                p_sys->i_tracks = p_emu->track_count();
            }
       }
        break;
        case EMU_SPC:
        {
            INIT_EMU(Spc)
            if (p_error == NULL)
            {
                snprintf( psz_temp, 511, "%s (%s)", header.song, header.game );
                vlc_meta_SetTitle( p_meta, psz_temp );
                vlc_meta_SetArtist( p_meta, header.author );
                p_sys->i_tracks = p_emu->track_count();
            }
      }
        break;
        case EMU_GYM:
        {
            INIT_EMU(Gym)
            if (p_error == NULL)
            {
                snprintf( psz_temp, 511, "%s (%s)", header.song, header.game );
                vlc_meta_SetTitle( p_meta, psz_temp );
                vlc_meta_SetCopyright( p_meta, header.copyright );
                p_sys->i_tracks = p_emu->track_count();
            }
     }
        break;
    }

    if( p_error != NULL )
    {
        msg_Err( p_demux, "failed to understand the file : %s", p_error );
        /* we try to seek to recover for other plugin */
        stream_Seek( p_demux->s, 0 );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }

   /* init time */
    p_sys->i_time  = 1;
    p_sys->i_length = 314 * (int64_t)1000;

    msg_Dbg( p_demux, "GME loaded type=%s title=%s tracks=%i", type_str[p_sys->i_type],
              vlc_meta_GetValue( p_sys->p_meta, VLC_META_TITLE ), p_sys->i_tracks );

    p_sys->p_musicemu->start_track( 0 );

#ifdef WORDS_BIGENDIAN
    es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 't', 'w', 'o', 's' ) );
#else
    es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 'a', 'r', 'a', 'w' ) );
#endif
    p_sys->fmt.audio.i_rate = 44100;
    p_sys->fmt.audio.i_channels = 2;
    p_sys->fmt.audio.i_bitspersample = 16;
    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );
 
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    delete p_sys->p_musicemu;
    delete p_sys->p_reader;

    free( p_sys->p_data );
    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_frame;
    int         i_bk = ( p_sys->fmt.audio.i_bitspersample / 8 ) *
                       p_sys->fmt.audio.i_channels;
    const unsigned int i_buf = p_sys->fmt.audio.i_rate / 10 * i_bk;
    const unsigned int i_emubuf = i_buf / sizeof(Music_Emu::sample_t);
    const char * p_error;
    Music_Emu::sample_t p_emubuf [i_emubuf];

    p_frame = block_New( p_demux, i_buf );

    p_sys->p_musicemu->play( i_emubuf, p_emubuf );
 
    /*
    if( p_error != NULL )
    {
        msg_Dbg( p_demux, "stop playing : %s", p_error );
        block_Release( p_frame );
        return 0;
    }
    */

    /* Copy emulator output to frame */
    for (int i = 0; i<i_buf; i++) p_frame->p_buffer[i] = ((uint8_t *)p_emubuf)[i];

    /* Set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_time );

    /* We should use p_frame->i_buffer */
    p_sys->i_time += (int64_t)1000000 * p_frame->i_buffer / i_bk / p_sys->fmt.audio.i_rate;

    /* Send data */
    p_frame->i_dts = p_frame->i_pts = p_sys->i_time;
    es_out_Send( p_demux->out, p_sys->es, p_frame );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *pi64;
    int i_idx;
    vlc_meta_t **pp_meta;

switch( i_query )
    {
        case DEMUX_GET_META:
            pp_meta = (vlc_meta_t **)va_arg( args, vlc_meta_t** );
            if( p_sys->p_meta )
                *pp_meta = vlc_meta_Duplicate( p_sys->p_meta );
            else
                *pp_meta = NULL;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            if( p_sys->i_length > 0 )
            {
                *pf = (double)p_sys->i_time / (double)p_sys->i_length;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
/*
        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );

            i64 = f * p_sys->i_length;
            if( i64 >= 0 && i64 <= p_sys->i_length )
            {
                ModPlug_Seek( p_sys->f, i64 / 1000 );
                p_sys->i_time = i64 + 1;
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
*/
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;
/*
        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );

            if( i64 >= 0 && i64 <= p_sys->i_length )
            {
                ModPlug_Seek( p_sys->f, i64 / 1000 );
                p_sys->i_time = i64 + 1;
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
*/
        case DEMUX_GET_TITLE_INFO:
            if( p_sys->i_tracks > 1 )
            {
                input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
                int *pi_int    = (int*)va_arg( args, int* );

                *pi_int = p_sys->i_tracks;
                *ppp_title = (input_title_t**)malloc( sizeof( input_title_t**) * p_sys->i_tracks );

                for( int i = 0; i < p_sys->i_tracks; i++ )
                {
                    char psz_temp[16];
                    snprintf(psz_temp, 15, "Track %i", i);
                    (*ppp_title)[i] = vlc_input_title_New();
                    (*ppp_title)[i]->psz_name = strdup(psz_temp);
                }

                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;


        case DEMUX_SET_TITLE:
            i_idx = (int)va_arg( args, int );
            p_sys->p_musicemu->start_track( i_idx );
            p_demux->info.i_title = i_idx;
            p_demux->info.i_update = INPUT_UPDATE_TITLE;
            msg_Dbg( p_demux, "set title %i", i_idx);
            return VLC_SUCCESS;

        case DEMUX_GET_FPS: /* meaningless */
        default:
            return VLC_EGENERIC;
    }

}

#ifdef HAVE_ZLIB_H
static void inflate_gzbuf(uint8_t * p_buffer, size_t i_size, uint8_t ** pp_obuffer, size_t * pi_osize)
{
    z_stream z_str;
    int err;
    size_t offset, out_size;
    uint8_t * out_buffer;

    (*pp_obuffer) = NULL;
    (*pi_osize) = 0;

    memset(&z_str, 0, sizeof(z_str));

    out_size = i_size * 2;
    out_buffer = (uint8_t*)malloc(out_size);

    z_str.next_in   = (unsigned char*)p_buffer;
    z_str.avail_in  = i_size;
    z_str.next_out  = out_buffer;
    z_str.avail_out = out_size;

    if ((err = inflateInit2(&z_str, 31)) != Z_OK) /* gzip format */
    {
        free(out_buffer);
        return;
    }

    while ((err = inflate(&z_str, Z_FINISH)) != Z_STREAM_END)
    {
        switch(err)
        {
        case Z_OK:
            break;
        case Z_BUF_ERROR:
            offset = z_str.next_out - out_buffer;
            out_size *= 2;
            out_buffer = (uint8_t *)realloc(out_buffer, out_size);
            z_str.next_out  = out_buffer + offset;
            z_str.avail_out = out_size - offset;
            break;
        default:
            inflateEnd(&z_str);
            free(out_buffer);
            return;
        }
    }

    (*pi_osize) = out_size - z_str.avail_out;

    inflateEnd(&z_str);
 
    out_buffer = (uint8_t *)realloc(out_buffer, *pi_osize);
    (*pp_obuffer) = out_buffer;
}
#endif
