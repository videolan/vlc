/*****************************************************************************
 * mpc.c : MPC stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr.com>
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
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_codec.h>
#include <math.h>

#include <mpcdec/mpcdec.h>

/* TODO:
 *  - test stream version 4..6
 *  - test fixed float version
 *  - ...
 *
 *  XXX:
 *  It is done the ugly way (the demux does the decode stage... but it works
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define REPLAYGAIN_TYPE_TEXT N_("Replay Gain type" )
#define REPLAYGAIN_TYPE_LONGTEXT N_( "Musepack can have a title-specific " \
              "replay gain (volume control) or an album-specific one. "  \
              "Choose which type you want to use" )

static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

static int  pi_replaygain_type[] = { 0, 1, 2 };
static char *ppsz_replaygain_type[] = { N_("None"), N_("Title"), N_("Album") };

vlc_module_begin();
    set_shortname( "MPC" );
    set_description( _("MusePack demuxer") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability( "demux2", 145 );

    add_integer( "mpc-replaygain-type", 2, NULL,
                REPLAYGAIN_TYPE_TEXT, REPLAYGAIN_TYPE_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_replaygain_type, ppsz_replaygain_type, 0 );

    set_callbacks( Open, Close );
    add_shortcut( "mpc" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    /* */
    es_out_id_t *p_es;

    /* */
    mpc_decoder    decoder;
    mpc_reader     reader;
    mpc_streaminfo info;

    /* */
    vlc_meta_t     *p_meta;
    int64_t        i_position;
};

mpc_int32_t ReaderRead( void *p_private, void *dst, mpc_int32_t i_size );
mpc_bool_t  ReaderSeek( void *p_private, mpc_int32_t i_offset );
mpc_int32_t ReaderTell( void *p_private);
mpc_int32_t ReaderGetSize( void *p_private );
mpc_bool_t  ReaderCanSeek( void *p_private );

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    char        psz_info[4096];
    es_format_t fmt;
    uint8_t     *p_peek;
    module_t    *p_id3;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, "MP+", 3 ) )
    {
        /* for v4..6 we check extention file */
        const int i_version = (GetDWLE( p_peek ) >> 11)&0x3ff;

        if( i_version  < 4 || i_version > 6 )
            return VLC_EGENERIC;

        if( !p_demux->psz_demux || strcmp( p_demux->psz_demux, "mpc" ) )
        {
            /* Check file name extension */
            int i_len;
            if( !p_demux->psz_path )
                return VLC_EGENERIC;

            i_len = strlen( p_demux->psz_path );
            if( i_len < 4 ||
                ( strcasecmp( &p_demux->psz_path[i_len-4], ".mpc" ) &&
                  strcasecmp( &p_demux->psz_path[i_len-4], ".mp+" ) &&
                  strcasecmp( &p_demux->psz_path[i_len-4], ".mpp" ) ) )
                return VLC_EGENERIC;
        }
    }

    /* */
    p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof(demux_sys_t) );

    p_sys->i_position = 0;

    p_sys->reader.read = ReaderRead;
    p_sys->reader.seek = ReaderSeek;
    p_sys->reader.tell = ReaderTell;
    p_sys->reader.get_size = ReaderGetSize;
    p_sys->reader.canseek = ReaderCanSeek;
    p_sys->reader.data = p_demux;

    /* Load info */
    mpc_streaminfo_init( &p_sys->info );
    if( mpc_streaminfo_read( &p_sys->info, &p_sys->reader ) != ERROR_CODE_OK )
    {
        /* invalid file */
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* */
    mpc_decoder_setup( &p_sys->decoder, &p_sys->reader );
    if( !mpc_decoder_initialize( &p_sys->decoder, &p_sys->info ) )
    {
        /* */
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Handle reaply gain */
    if( p_sys->info.peak_title != 32767 )
    {
        int i_type = var_CreateGetInteger( p_demux, "mpc-replaygain-type" );
        int gain;
        int peak;

        if( i_type == 2 )       // album
        {
            gain = p_sys->info.gain_album;
            peak = p_sys->info.peak_album;
        }
        else if( i_type == 1 )  // title
        {
            gain = p_sys->info.gain_title;
            peak = p_sys->info.peak_title;
        }
        else
        {
            gain = 0;
            peak = 0;
        }

        if( gain )
        {
            double g = pow( 10, (double)gain / 2000.0 );
            double gmax = (double)32767.0 / (peak+1);
            if( g > gmax )
                g = gmax;

            msg_Dbg( p_demux, "Using reaply gain factor %f", g );
            mpc_decoder_scale_output( &p_sys->decoder, g );
        }
    }

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys;

    /* */
#ifndef MPC_FIXED_POINT
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'f', 'l', '3', '2' ) );
#else
#   ifdef WORDS_BIGENDIAN
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 's', '3', '2', 'b' ) );
#   else
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 's', '3', '2', 'l' ) );
#   endif
#endif
    fmt.audio.i_channels = p_sys->info.channels;
    fmt.audio.i_rate = p_sys->info.sample_freq;
    fmt.audio.i_blockalign = 4*fmt.audio.i_channels;
    fmt.audio.i_bitspersample = 32;
    fmt.i_bitrate = fmt.i_bitrate * fmt.audio.i_channels *
                    fmt.audio.i_bitspersample;
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );


    /* Parse possible id3 header */
    if( ( p_id3 = module_Need( p_demux, "id3", NULL, 0 ) ) )
    {
        p_sys->p_meta = (vlc_meta_t *)p_demux->p_private;
        p_demux->p_private = NULL;
        module_Unneed( p_demux, p_id3 );
    }

    if( !p_sys->p_meta )
        p_sys->p_meta = vlc_meta_New();

    sprintf( psz_info, "Musepack v%d", p_sys->info.stream_version );
    vlc_meta_Add( p_sys->p_meta, VLC_META_CODEC_NAME, psz_info );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_data;
    int i_ret;

    p_data = block_New( p_demux,
                        MPC_DECODER_BUFFER_LENGTH*sizeof(MPC_SAMPLE_FORMAT) );
    i_ret = mpc_decoder_decode( &p_sys->decoder,
                               (MPC_SAMPLE_FORMAT*)p_data->p_buffer,
                               NULL, NULL );
    if( i_ret <= 0 )
    {
        block_Release( p_data );
        return i_ret < 0 ? -1 : 0;
    }

    /* */
    p_data->i_buffer = i_ret * sizeof(MPC_SAMPLE_FORMAT) * p_sys->info.channels;
    p_data->i_dts = p_data->i_pts =
            1 + I64C(1000000) * p_sys->i_position / p_sys->info.sample_freq;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_data->i_dts );

    es_out_Send( p_demux->out, p_sys->p_es, p_data );

    /* */
    p_sys->i_position += i_ret;

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double   f, *pf;
    int64_t i64, *pi64;
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

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = I64C(1000000) * p_sys->info.pcm_samples /
                        p_sys->info.sample_freq;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->info.pcm_samples > 0 )
                *pf = (double) p_sys->i_position /
                      (double)p_sys->info.pcm_samples;
            else
                *pf = 0.0;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = I64C(1000000) * p_sys->i_position /
                        p_sys->info.sample_freq;
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            i64 = (int64_t)(f * p_sys->info.pcm_samples);
            if( mpc_decoder_seek_sample( &p_sys->decoder, i64 ) )
            {
                p_sys->i_position = i64;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( mpc_decoder_seek_sample( &p_sys->decoder, i64 ) )
            {
                p_sys->i_position = i64;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        default:
            return VLC_EGENERIC;
    }
}

mpc_int32_t ReaderRead( void *p_private, void *dst, mpc_int32_t i_size )
{
    demux_t *p_demux = (demux_t*)p_private;
    return stream_Read( p_demux->s, dst, i_size );
}

mpc_bool_t ReaderSeek( void *p_private, mpc_int32_t i_offset )
{
    demux_t *p_demux = (demux_t*)p_private;
    return !stream_Seek( p_demux->s, i_offset );
}

mpc_int32_t ReaderTell( void *p_private)
{
    demux_t *p_demux = (demux_t*)p_private;
    return stream_Tell( p_demux->s );
}

mpc_int32_t ReaderGetSize( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;
    return stream_Size( p_demux->s );
}

mpc_bool_t ReaderCanSeek( void *p_private )
{
    demux_t *p_demux = (demux_t*)p_private;
    vlc_bool_t b_canseek;

    stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_canseek );
    return b_canseek;
}

