/*****************************************************************************
 * mod.c: MOD file demuxer (using libmodplug)
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_demux.h>

#include <libmodplug/modplug.h>

/* TODO:
 *  - extend demux control to query meta data (demuxer should NEVER touch
 *      playlist itself)
 *  - FIXME test endian of samples
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define NOISE_LONGTEXT N_("Enable noise reduction algorithm.")
#define REVERB_LONGTEXT N_("Enable reverberation" )
#define REVERB_LEVEL_LONGTEXT N_( "Reverberation level (from 0 " \
                "to 100, default value is 0)." )
#define REVERB_DELAY_LONGTEXT N_("Reverberation delay, in ms." \
                " Usual values are from to 40 to 200ms." )
#define MEGABASS_LONGTEXT N_( "Enable megabass mode" )
#define MEGABASS_LEVEL_LONGTEXT N_("Megabass mode level (from 0 to 100, " \
                "default value is 0)." )
#define MEGABASS_RANGE_LONGTEXT N_("Megabass mode cutoff frequency, in Hz. " \
                "This is the maximum frequency for which the megabass " \
                "effect applies. Valid values are from 10 to 100 Hz." )
#define SURROUND_LEVEL_LONGTEXT N_( "Surround effect level (from 0 to 100, " \
                "default value is 0)." )
#define SURROUND_DELAY_LONGTEXT N_("Surround delay, in ms. Usual values are " \
                "from 5 to 40 ms." )

vlc_module_begin();
    set_shortname( "MOD");
    set_description( N_("MOD demuxer (libmodplug)" ) );
    set_capability( "demux", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_bool( "mod-noisereduction", true, NULL, N_("Noise reduction"),
              NOISE_LONGTEXT, false );

    add_bool( "mod-reverb", false, NULL, N_("Reverb"),
              REVERB_LONGTEXT, false );
    add_integer_with_range( "mod-reverb-level", 0, 0, 100, NULL,
             N_("Reverberation level"), REVERB_LEVEL_LONGTEXT, true );
    add_integer_with_range( "mod-reverb-delay", 40, 0, 1000, NULL,
             N_("Reverberation delay"), REVERB_DELAY_LONGTEXT, true );

    add_bool( "mod-megabass", false, NULL, N_("Mega bass"),
                    MEGABASS_LONGTEXT, false );
    add_integer_with_range( "mod-megabass-level", 0, 0, 100, NULL,
              N_("Mega bass level"), MEGABASS_LEVEL_LONGTEXT, true );
    add_integer_with_range( "mod-megabass-range", 10, 10, 100, NULL,
              N_("Mega bass cutoff"), MEGABASS_RANGE_LONGTEXT, true );

    add_bool( "mod-surround", false, NULL, N_("Surround"), N_("Surround"),
               false );
    add_integer_with_range( "mod-surround-level", 0, 0, 100, NULL,
              N_("Surround level"), SURROUND_LEVEL_LONGTEXT, true );
    add_integer_with_range( "mod-surround-delay", 5, 0, 1000, NULL,
              N_("Surround delay (ms)"), SURROUND_DELAY_LONGTEXT, true );

    set_callbacks( Open, Close );
    add_shortcut( "mod" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    es_format_t  fmt;
    es_out_id_t *es;

    int64_t     i_time;
    int64_t     i_length;

    int         i_data;
    uint8_t     *p_data;
    ModPlugFile *f;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static const char* mod_ext[] =
{
    "mod", "s3m", "xm",  "it",  "669", "amf", "ams", "dbm", "dmf", "dsm",
    "far", "mdl", "med", "mtm", "okt", "ptm", "stm", "ult", "umx", "mt2",
    "psm", NULL
};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    ModPlug_Settings settings;
    vlc_value_t val;

    /* We accept file based on extension match */
    if( !p_demux->b_force )
    {
        char *ext;
        int i;
        if( ( ext = strrchr( p_demux->psz_path, '.' ) ) == NULL ||
            stream_Size( p_demux->s ) == 0 ) return VLC_EGENERIC;

        ext++;  /* skip . */
        for( i = 0; mod_ext[i] != NULL; i++ )
        {
            if( !strcasecmp( ext, mod_ext[i] ) )
            {
                break;
            }
        }
        if( mod_ext[i] == NULL ) return VLC_EGENERIC;
        msg_Dbg( p_demux, "running MOD demuxer (ext=%s)", mod_ext[i] );
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );

    msg_Dbg( p_demux, "loading complete file (could be long)" );
    p_sys->i_data = stream_Size( p_demux->s );
    p_sys->p_data = malloc( p_sys->i_data );
    p_sys->i_data = stream_Read( p_demux->s, p_sys->p_data, p_sys->i_data );
    if( p_sys->i_data <= 0 )
    {
        msg_Err( p_demux, "failed to read the complete file" );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }
    /* Create our config variable */
    var_Create( p_demux, "mod-noisereduction", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-reverb", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-reverb-level", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-reverb-delay", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-megabass", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-megabass-level", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-megabass-range", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-surround", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-surround-level", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_demux, "mod-surround-delay", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    /* Configure modplug before loading the file */
    ModPlug_GetSettings( &settings );
    settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    settings.mChannels = 2;
    settings.mBits = 16;
    settings.mFrequency = 44100;
    settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;

    var_Get( p_demux, "mod-noisereduction", &val );
    if( val.b_bool) settings.mFlags |= MODPLUG_ENABLE_NOISE_REDUCTION;

    var_Get( p_demux, "mod-reverb", &val );
    if( val.b_bool) settings.mFlags |= MODPLUG_ENABLE_REVERB;
    var_Get( p_demux, "mod-reverb-level", &val );
    settings.mReverbDepth = val.i_int;
    var_Get( p_demux, "mod-reverb-delay", &val );
    settings.mReverbDelay = val.i_int;

    var_Get( p_demux, "mod-megabass", &val );
    if( val.b_bool) settings.mFlags |= MODPLUG_ENABLE_MEGABASS;
    var_Get( p_demux, "mod-megabass-level", &val );
    settings.mBassAmount = val.i_int;
    var_Get( p_demux, "mod-megabass-range", &val );
    settings.mBassRange = val.i_int;

    var_Get( p_demux, "mod-surround", &val );
    if( val.b_bool) settings.mFlags |= MODPLUG_ENABLE_SURROUND;
    var_Get( p_demux, "mod-surround-level", &val );
    settings.mSurroundDepth = val.i_int;
    var_Get( p_demux, "mod-surround-delay", &val );
    settings.mSurroundDelay = val.i_int;

    ModPlug_SetSettings( &settings );

    if( ( p_sys->f = ModPlug_Load( p_sys->p_data, p_sys->i_data ) ) == NULL )
    {
        msg_Err( p_demux, "failed to understand the file" );
        /* we try to seek to recover for other plugin */
        stream_Seek( p_demux->s, 0 );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* init time */
    p_sys->i_time  = 1;
    p_sys->i_length = ModPlug_GetLength( p_sys->f ) * (int64_t)1000;

    msg_Dbg( p_demux, "MOD loaded name=%s lenght=%"PRId64"ms",
             ModPlug_GetName( p_sys->f ),
             p_sys->i_length );

#ifdef WORDS_BIGENDIAN
    es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 't', 'w', 'o', 's' ) );
#else
    es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 'a', 'r', 'a', 'w' ) );
#endif
    p_sys->fmt.audio.i_rate = settings.mFrequency;
    p_sys->fmt.audio.i_channels = settings.mChannels;
    p_sys->fmt.audio.i_bitspersample = settings.mBits;
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

    ModPlug_Unload( p_sys->f );
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
    int         i_read;

    p_frame = block_New( p_demux, p_sys->fmt.audio.i_rate / 10 * i_bk );

    i_read = ModPlug_Read( p_sys->f, p_frame->p_buffer, p_frame->i_buffer );
    if( i_read <= 0 )
    {
        /* EOF */
        block_Release( p_frame );
        return 0;
    }
    p_frame->i_buffer = i_read;

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

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            if( p_sys->i_length > 0 )
            {
                *pf = (double)p_sys->i_time / (double)p_sys->i_length;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

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

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;

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

        case DEMUX_GET_FPS: /* meaningless */
        default:
            return VLC_EGENERIC;
    }
}

