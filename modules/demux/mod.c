/*****************************************************************************
 * mod.c: MOD file demuxer (using libmodplug)
 *****************************************************************************
 * Copyright (C) 2004-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 * Konstanty Bialkowski <konstanty@ieee.org>
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
#include <vlc_demux.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <assert.h>

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

vlc_module_begin ()
    set_shortname( "MOD")
    set_description( N_("MOD demuxer (libmodplug)" ) )
    set_capability( "demux", 10 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "mod-noisereduction", true, N_("Noise reduction"),
              NOISE_LONGTEXT, false )

    add_bool( "mod-reverb", false, N_("Reverb"),
              REVERB_LONGTEXT, false )
    add_integer_with_range( "mod-reverb-level", 0, 0, 100,
             N_("Reverberation level"), REVERB_LEVEL_LONGTEXT, true )
    add_integer_with_range( "mod-reverb-delay", 40, 0, 1000,
             N_("Reverberation delay"), REVERB_DELAY_LONGTEXT, true )

    add_bool( "mod-megabass", false, N_("Mega bass"),
                    MEGABASS_LONGTEXT, false )
    add_integer_with_range( "mod-megabass-level", 0, 0, 100,
              N_("Mega bass level"), MEGABASS_LEVEL_LONGTEXT, true )
    add_integer_with_range( "mod-megabass-range", 10, 10, 100,
              N_("Mega bass cutoff"), MEGABASS_RANGE_LONGTEXT, true )

    add_bool( "mod-surround", false, N_("Surround"), N_("Surround"),
               false )
    add_integer_with_range( "mod-surround-level", 0, 0, 100,
              N_("Surround level"), SURROUND_LEVEL_LONGTEXT, true )
    add_integer_with_range( "mod-surround-delay", 5, 0, 1000,
              N_("Surround delay (ms)"), SURROUND_DELAY_LONGTEXT, true )

    set_callbacks( Open, Close )
    add_shortcut( "mod" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static vlc_mutex_t libmodplug_lock = VLC_STATIC_MUTEX;

struct demux_sys_t
{
    es_format_t  fmt;
    es_out_id_t *es;

    date_t      pts;
    int64_t     i_length;

    int         i_data;
    uint8_t     *p_data;
    ModPlugFile *f;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int Validate( demux_t *p_demux, const char *psz_ext );

/* We load the complete file in memory, put a higher bound
 * of 500 Mo (which is really big anyway) */
#define MOD_MAX_FILE_SIZE (500*1000*1000)

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    ModPlug_Settings settings;

    /* We accept file based on extension match */
    if( !p_demux->b_force )
    {
        const char *psz_ext = p_demux->psz_file ? strrchr( p_demux->psz_file, '.' )
                                                : NULL;
        if( psz_ext )
            psz_ext++;

        if( Validate( p_demux, psz_ext ) )
        {
            msg_Dbg( p_demux, "MOD validation failed (ext=%s)", psz_ext ? psz_ext : "");
            return VLC_EGENERIC;
        }
    }

    const int64_t i_size = stream_Size( p_demux->s );
    if( i_size <= 0 || i_size >= MOD_MAX_FILE_SIZE )
        return VLC_EGENERIC;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    msg_Dbg( p_demux, "loading complete file (could be long)" );
    p_sys->i_data = i_size;
    p_sys->p_data = malloc( p_sys->i_data );
    if( p_sys->p_data )
        p_sys->i_data = stream_Read( p_demux->s, p_sys->p_data, p_sys->i_data );
    if( p_sys->i_data <= 0 || !p_sys->p_data )
    {
        msg_Err( p_demux, "failed to read the complete file" );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Configure modplug before loading the file */
    vlc_mutex_lock( &libmodplug_lock );
    ModPlug_GetSettings( &settings );
    settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
    settings.mChannels = 2;
    settings.mBits = 16;
    settings.mFrequency = 44100;
    settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;

    if( var_InheritBool( p_demux, "mod-noisereduction" ) )
        settings.mFlags |= MODPLUG_ENABLE_NOISE_REDUCTION;

    if( var_InheritBool( p_demux, "mod-reverb" ) )
        settings.mFlags |= MODPLUG_ENABLE_REVERB;
    settings.mReverbDepth = var_InheritInteger( p_demux, "mod-reverb-level" );
    settings.mReverbDelay = var_InheritInteger( p_demux, "mod-reverb-delay" );

    if( var_InheritBool( p_demux, "mod-megabass" ) )
        settings.mFlags |= MODPLUG_ENABLE_MEGABASS;
    settings.mBassAmount = var_InheritInteger( p_demux, "mod-megabass-level" );
    settings.mBassRange = var_InheritInteger( p_demux, "mod-megabass-range" );

    if( var_InheritBool( p_demux, "mod-surround" ) )
        settings.mFlags |= MODPLUG_ENABLE_SURROUND;
    settings.mSurroundDepth = var_InheritInteger( p_demux, "mod-surround-level" );
    settings.mSurroundDelay = var_InheritInteger( p_demux, "mod-surround-delay" );

    ModPlug_SetSettings( &settings );

    p_sys->f = ModPlug_Load( p_sys->p_data, p_sys->i_data );
    vlc_mutex_unlock( &libmodplug_lock );

    if( !p_sys->f )
    {
        msg_Err( p_demux, "failed to understand the file" );
        /* we try to seek to recover for other plugin */
        stream_Seek( p_demux->s, 0 );
        free( p_sys->p_data );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* init time */
    date_Init( &p_sys->pts, settings.mFrequency, 1 );
    date_Set( &p_sys->pts, 0 );
    p_sys->i_length = ModPlug_GetLength( p_sys->f ) * INT64_C(1000);

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
    const int i_bk = ( p_sys->fmt.audio.i_bitspersample / 8 ) *
                       p_sys->fmt.audio.i_channels;

    p_frame = block_Alloc( p_sys->fmt.audio.i_rate / 10 * i_bk );
    if( !p_frame )
        return -1;

    const int i_read = ModPlug_Read( p_sys->f, p_frame->p_buffer, p_frame->i_buffer );
    if( i_read <= 0 )
    {
        /* EOF */
        block_Release( p_frame );
        return 0;
    }
    p_frame->i_buffer = i_read;
    p_frame->i_dts =
    p_frame->i_pts = VLC_TS_0 + date_Get( &p_sys->pts );

    /* Set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_frame->i_pts );

    /* Send data */
    es_out_Send( p_demux->out, p_sys->es, p_frame );

    date_Increment( &p_sys->pts, i_read / i_bk );

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
            double current = date_Get( &p_sys->pts );
            double length = p_sys->i_length;
            *pf = current / length;
            return VLC_SUCCESS;
        }
        return VLC_EGENERIC;

    case DEMUX_SET_POSITION:
        f = (double) va_arg( args, double );

        i64 = f * p_sys->i_length;
        if( i64 >= 0 && i64 <= p_sys->i_length )
        {
            ModPlug_Seek( p_sys->f, i64 / 1000 );
            date_Set( &p_sys->pts, i64 );

            return VLC_SUCCESS;
        }
        return VLC_EGENERIC;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = date_Get( &p_sys->pts );
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
            date_Set( &p_sys->pts, i64 );

            return VLC_SUCCESS;
        }
        return VLC_EGENERIC;

    case DEMUX_HAS_UNSUPPORTED_META:
    {
        bool *pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = false; /* FIXME I am not sure of this one */
        return VLC_SUCCESS;
    }
    case DEMUX_GET_META:
    {
        vlc_meta_t *p_meta = (vlc_meta_t *)va_arg( args, vlc_meta_t* );
        unsigned i_num_samples = ModPlug_NumSamples( p_sys->f ),
                 i_num_instruments = ModPlug_NumInstruments( p_sys->f );
        unsigned i_num_patterns = ModPlug_NumPatterns( p_sys->f ),
                 i_num_channels = ModPlug_NumChannels( p_sys->f );
        //      unsigned modType = ModPlug_GetModuleType( p_sys->f );

        char psz_temp[2048]; /* 32 * 240 max, but only need start  */
        char *psz_module_info, *psz_instrument_info;
        unsigned i_temp_index = 0;
        const char *psz_name = ModPlug_GetName( p_sys->f );
        if( psz_name && *psz_name && IsUTF8( psz_name ) )
            vlc_meta_SetTitle( p_meta, psz_name );

        /* Comment field from artist - not in every type of MOD */
        psz_name = ModPlug_GetMessage( p_sys->f );
        if( psz_name && *psz_name && IsUTF8( psz_name ) )
            vlc_meta_SetDescription( p_meta, psz_name );

        /* Instruments only in newer MODs - so don't show if 0 */
        if( asprintf( &psz_instrument_info, ", %i Instruments",
                      i_num_instruments ) >= 0 )
        {
            if( asprintf( &psz_module_info,
                          "%i Channels, %i Patterns\n"
                          "%i Samples%s\n",
                          i_num_channels, i_num_patterns, i_num_samples,
                          ( i_num_instruments ? psz_instrument_info : "" ) ) >= 0 )
            {
                vlc_meta_AddExtra( p_meta, "Module Information",
                                   psz_module_info );
                free( psz_module_info );
            }

            free( psz_instrument_info );
        }

        /* Make list of instruments (XM, IT, etc) */
        if( i_num_instruments )
        {
            i_temp_index = 0;
            for( unsigned i = 0; i < i_num_instruments && i_temp_index < sizeof(psz_temp); i++ )
            {
                char lBuffer[33];
                ModPlug_InstrumentName( p_sys->f, i, lBuffer );
                if ( !lBuffer[0] || !IsUTF8( lBuffer ) ) continue;
                i_temp_index += snprintf( &psz_temp[i_temp_index], sizeof(psz_temp) - i_temp_index, "%s\n", lBuffer );
            }

            vlc_meta_AddExtra( p_meta, "Instruments", psz_temp );
        }

        /* Make list of samples */
        for( unsigned int i = 0; i < i_num_samples && i_temp_index < sizeof(psz_temp); i++ )
        {
            char psz_buffer[33];
            ModPlug_SampleName( p_sys->f, i, psz_buffer );
            if ( !psz_buffer[0] || !IsUTF8( psz_buffer ) ) continue;
            i_temp_index += snprintf( &psz_temp[i_temp_index], sizeof(psz_temp) - i_temp_index, "%s\n", psz_buffer );
        }

        vlc_meta_AddExtra( p_meta, "Samples", psz_temp );

        return VLC_SUCCESS;
    }

    case DEMUX_GET_FPS: /* meaningless */
    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Validate: try to ensure it is really a mod file.
 * The tests are not robust enough to replace extension checks in the general
 * cases.
 * TODO: maybe it should return a score, which will be used to bypass the
 * extension checks when high enough.
 *****************************************************************************/
static int Validate( demux_t *p_demux, const char *psz_ext )
{
    static const struct
    {
        int i_offset;
        const char *psz_marker;
    } p_marker[] = {
        {  0, "ziRCONia" },             /* MMCMP files */
        {  0, "Extended Module" },      /* XM */
        { 44, "SCRM" },                 /* S3M */
        {  0, "IMPM" },                 /* IT */
        {  0, "GF1PATCH110" },          /* PAT */
        { 20, "!SCREAM!" },             /* STM */
        { 20, "!Scream!" },             /* STM */
        { 20, "BMOD2STM" },             /* STM */
        {  0, "MMD0" },                 /* MED v0 */
        {  0, "MMD1" },                 /* MED v1 */
        {  0, "MMD2" },                 /* MED v2 */
        {  0, "MMD3" },                 /* MED v3 */
        {  0, "MTM" },                  /* MTM */
        {  0, "DMDL" },                 /* MDL */
        {  0, "DBM0" },                 /* DBM */
        {  0, "if" },                   /* 669 */
        {  0, "JN" },                   /* 669 */
        {  0, "FAR\xfe" },              /* FAR */
        {  0, "Extreme" },              /* AMS */
        {  0, "OKTASONGCMOD" },         /* OKT */
        { 44, "PTMF" },                 /* PTM */
        {  0, "MAS_UTrack_V00" },       /* Ult */
        {  0, "DDMF" },                 /* DMF */
        {  8, "DSMFSONG" },             /* DSM */
        {  0, "\xc1\x83\x2a\x9e" },     /* UMX */
        {  0, "ASYLUM Music Format V1.0" }, /* AMF Type 0 */
        {  0, "AMF" },                  /* AMF */
        {  0, "PSM\xfe" },              /* PSM */
        {  0, "PSM " },                 /* PSM */
        {  0, "MT20" },                 /* MT2 */

        { 1080, "M.K." },               /* MOD */
        { 1080, "M!K!" },
        { 1080, "M&K!" },
        { 1080, "N.T." },
        { 1080, "CD81" },
        { 1080, "OKTA" },
        { 1080, "16CN" },
        { 1080, "32CN" },
        { 1080, "FLT4" },
        { 1080, "FLT8" },
        { 1080, "6CHN" },
        { 1080, "8CHN" },
        { 1080, "FLT" },
        { 1080, "TDZ" },
        { 1081, "CHN" },
        { 1082, "CH" },

        {  -1, NULL }
    };
    static const char *ppsz_mod_ext[] =
    {
        "mod", "s3m", "xm",  "it",  "669", "amf", "ams", "dbm", "dmf", "dsm",
        "far", "mdl", "med", "mtm", "okt", "ptm", "stm", "ult", "umx", "mt2",
        "psm", "abc", NULL
    };
    bool has_valid_extension = false;
    if( psz_ext )
    {
        for( int i = 0; ppsz_mod_ext[i] != NULL; i++ )
        {
            has_valid_extension |= !strcasecmp( psz_ext, ppsz_mod_ext[i] );
            if( has_valid_extension )
                break;
        }
    }

    const uint8_t *p_peek;
    const int i_peek = stream_Peek( p_demux->s, &p_peek, 2048 );
    if( i_peek < 4 )
        return VLC_EGENERIC;

    for( int i = 0; p_marker[i].i_offset >= 0; i++ )
    {
        const char *psz_marker = p_marker[i].psz_marker;
        const int i_size = strlen( psz_marker );
        const int i_offset = p_marker[i].i_offset;

        if( i_peek < i_offset + i_size )
            continue;

        if( !memcmp( &p_peek[i_offset], psz_marker, i_size ) )
        {
            if( i_size >= 4 || has_valid_extension )
                return VLC_SUCCESS;
        }
    }

    /* The only two format left untested are ABC and MOD(old version)
     * ant they are difficult to test :( */

    /* Check for ABC
     * TODO i_peek = 2048 is too big for such files */
    if( psz_ext && !strcasecmp( psz_ext, "abc" ) )
    {
        bool b_k = false;
        bool b_tx = false;

        for( int i = 0; i < i_peek-1; i++ )
        {
            b_k |= p_peek[i+0] == 'K' && p_peek[i+1] == ':';
            b_tx |= ( p_peek[i+0] == 'X' || p_peek[i+0] == 'T') && p_peek[i+1] == ':';
        }
        if( !b_k || !b_tx )
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }

    /* Check for MOD */
    if( psz_ext && !strcasecmp( psz_ext, "mod" ) && i_peek >= 20 + 15 * 30 )
    {
        /* Check that the name is correctly null padded */
        const uint8_t *p = memchr( p_peek, '\0', 20 );
        if( p )
        {
            for( ; p < &p_peek[20]; p++ )
            {
                if( *p )
                    return VLC_EGENERIC;
            }
        }

        for( int i = 0; i < 15; i++ )
        {
            const uint8_t *p_sample = &p_peek[20 + i*30];

            /* Check correct null padding */
            const uint8_t *p = memchr( &p_sample[0], '\0', 22 );
            if( p )
            {
                for( ; p < &p_sample[22]; p++ )
                {
                    if( *p )
                        return VLC_EGENERIC;
                }
            }

            if( p_sample[25] > 64 ) /* Volume value */
                return VLC_EGENERIC;
        }
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}
