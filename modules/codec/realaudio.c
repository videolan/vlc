/*****************************************************************************
 * realaudio.c: a realaudio decoder that uses the realaudio library/dll
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
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
#include <vlc_aout.h>
#include <vlc_codec.h>

#ifdef LOADER
/* Need the w32dll loader from mplayer */
#   include <wine/winerror.h>
#   include <ldt_keeper.h>
#   include <wine/windef.h>

void *WINAPI LoadLibraryA( char *name );
void *WINAPI GetProcAddress( void *handle, char *func );
int WINAPI FreeLibrary( void *handle );
#endif

#ifndef WINAPI
#   define WINAPI
#endif

#if defined(HAVE_DL_DLOPEN)
#   if defined(HAVE_DLFCN_H) /* Linux, BSD, Hurd */
#       include <dlfcn.h>
#   endif
#   if defined(HAVE_SYS_DL_H)
#       include <sys/dl.h>
#   endif
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( N_("RealAudio library decoder") );
    set_capability( "decoder", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDll( decoder_t * );
static int  OpenNativeDll( decoder_t *, char *, char * );
static int  OpenWin32Dll( decoder_t *, char *, char * );
static void CloseDll( decoder_t * );

static aout_buffer_t *Decode( decoder_t *, block_t ** );

struct decoder_sys_t
{
    audio_date_t end_date;

    /* Output buffer */
    char *p_out;
    unsigned int i_out;

    /* Codec params */
    void *context;
    short int i_codec_flavor;

    void *dll;
    unsigned long (*raCloseCodec)(void*);
    unsigned long (*raDecode)(void*, char*, unsigned long, char*,
                              unsigned int*, long);
    unsigned long (*raFlush)(unsigned long, unsigned long, unsigned long);
    unsigned long (*raFreeDecoder)(void*);
    void*         (*raGetFlavorProperty)(void*, unsigned long,
                                         unsigned long, int*);
    unsigned long (*raInitDecoder)(void*, void*);
    unsigned long (*raOpenCodec)(void*);
    unsigned long (*raOpenCodec2)(void*, void*);
    unsigned long (*raSetFlavor)(void*, unsigned long);
    void          (*raSetDLLAccessPath)(char*);
    void          (*raSetPwd)(char*, char*);

#if defined(LOADER)
    ldt_fs_t *ldt_fs;
#endif

    void *win32_dll;
    unsigned long (WINAPI *wraCloseCodec)(void*);
    unsigned long (WINAPI *wraDecode)(void*, char*, unsigned long, char*,
                                      unsigned int*, long);
    unsigned long (WINAPI *wraFlush)(unsigned long, unsigned long,
                                     unsigned long);
    unsigned long (WINAPI *wraFreeDecoder)(void*);
    void*         (WINAPI *wraGetFlavorProperty)(void*, unsigned long,
                                                 unsigned long, int*);
    unsigned long (WINAPI *wraInitDecoder)(void*, void*);
    unsigned long (WINAPI *wraOpenCodec)(void*);
    unsigned long (WINAPI *wraOpenCodec2)(void*, void*);
    unsigned long (WINAPI *wraSetFlavor)(void*, unsigned long);
    void          (WINAPI *wraSetDLLAccessPath)(char*);
    void          (WINAPI *wraSetPwd)(char*, char*);
};

/* linux dlls doesn't need packing */
typedef struct /*__attribute__((__packed__))*/ {
    int samplerate;
    short bits;
    short channels;
    short quality;
    /* 2bytes padding here, by gcc */
    int bits_per_frame;
    int packetsize;
    int extradata_len;
    void* extradata;
} ra_init_t;

/* windows dlls need packed structs (no padding) */
typedef struct __attribute__((__packed__)) {
    int samplerate;
    short bits;
    short channels;
    short quality;
    int bits_per_frame;
    int packetsize;
    int extradata_len;
    void* extradata;
} wra_init_t;

void *__builtin_new(unsigned long size) {return malloc(size);}
void __builtin_delete(void *p) {free(p);}

static const int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
    case VLC_FOURCC('c','o','o','k'):
    case VLC_FOURCC('a','t','r','c'):
    case VLC_FOURCC('s','i','p','r'):
        break;

    default:
        return VLC_EGENERIC;
    }

    /* Channel detection */
    if( p_dec->fmt_in.audio.i_channels <= 0 ||
        p_dec->fmt_in.audio.i_channels > 6 )
    {
        msg_Err( p_dec, "invalid number of channels (not between 1 and 6): %i",
                 p_dec->fmt_in.audio.i_channels );
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof(decoder_sys_t) );

    /* Flavor for SIPR codecs */
    p_sys->i_codec_flavor = -1;
    if( p_dec->fmt_in.i_codec == VLC_FOURCC('s','i','p','r') )
    {
        p_sys->i_codec_flavor = p_dec->fmt_in.audio.i_flavor;
        msg_Dbg( p_dec, "Got sipr flavor %d", p_sys->i_codec_flavor );
    }

    if( OpenDll( p_dec ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

#ifdef LOADER
    if( p_sys->win32_dll ) Close( p_this );
#endif

    es_format_Init( &p_dec->fmt_out, AUDIO_ES, AOUT_FMT_S16_NE );
    p_dec->fmt_out.audio.i_rate = p_dec->fmt_in.audio.i_rate;
    p_dec->fmt_out.audio.i_channels = p_dec->fmt_in.audio.i_channels;
    p_dec->fmt_out.audio.i_bitspersample =
        p_dec->fmt_in.audio.i_bitspersample;
    p_dec->fmt_out.audio.i_physical_channels =
    p_dec->fmt_out.audio.i_original_channels =
        pi_channels_maps[p_dec->fmt_out.audio.i_channels];

    aout_DateInit( &p_sys->end_date, p_dec->fmt_out.audio.i_rate );
    aout_DateSet( &p_sys->end_date, 0 );

    p_dec->pf_decode_audio = Decode;

    p_sys->p_out = malloc( 4096 * 10 );
    if( !p_sys->p_out )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->i_out = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    CloseDll( p_dec );
    free( p_dec->p_sys->p_out );
    free( p_dec->p_sys );
}

/*****************************************************************************
 * OpenDll:
 *****************************************************************************/
static int OpenDll( decoder_t *p_dec )
{
    char *psz_dll;
    int i, i_result;

    /** Find the good path for the dlls.**/
    char *ppsz_path[] =
    {
      ".",
#ifndef WIN32
      "/usr/local/RealPlayer8/Codecs",
      "/usr/RealPlayer8/Codecs",
      "/usr/lib/RealPlayer8/Codecs",
      "/opt/RealPlayer8/Codecs",
      "/usr/lib/RealPlayer9/users/Real/Codecs",
      "/usr/lib/RealPlayer10/codecs",
      "/usr/lib/RealPlayer10GOLD/codecs",
      "/usr/lib/helix/player/codecs",
      "/usr/lib64/RealPlayer8/Codecs",
      "/usr/lib64/RealPlayer9/users/Real/Codecs",
      "/usr/lib64/RealPlayer10/codecs",
      "/usr/lib64/RealPlayer10GOLD/codecs",
      "/usr/lib/win32",
      "/usr/lib/codecs",
      "/usr/local/lib/codecs",
#endif
      NULL,
      NULL,
      NULL
    };

#ifdef WIN32
    char psz_win32_real_codecs[MAX_PATH + 1];
    char psz_win32_helix_codecs[MAX_PATH + 1];

    {
        HKEY h_key;
        DWORD i_type, i_data = MAX_PATH + 1, i_index = 1;
        char *p_data;

        p_data = psz_win32_real_codecs;
        if( RegOpenKeyEx( HKEY_CLASSES_ROOT,
                          _T("Software\\RealNetworks\\Preferences\\DT_Codecs"),
                          0, KEY_READ, &h_key ) == ERROR_SUCCESS )
        {
             if( RegQueryValueEx( h_key, _T(""), 0, &i_type,
                                  (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS &&
                 i_type == REG_SZ )
             {
                 int i_len = strlen( p_data );
                 if( i_len && p_data[i_len-1] == '\\' ) p_data[i_len-1] = 0;
                 ppsz_path[i_index++] = p_data;
                 msg_Err( p_dec, "Real: %s", p_data );
             }
             RegCloseKey( h_key );
        }

        p_data = psz_win32_helix_codecs;
        if( RegOpenKeyEx( HKEY_CLASSES_ROOT,
                          _T("Helix\\HelixSDK\\10.0\\Preferences\\DT_Codecs"),
                          0, KEY_READ, &h_key ) == ERROR_SUCCESS )
        {
             if( RegQueryValueEx( h_key, _T(""), 0, &i_type,
                                  (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS &&
                 i_type == REG_SZ )
             {
                 int i_len = strlen( p_data );
                 if( i_len && p_data[i_len-1] == '\\' ) p_data[i_len-1] = 0;
                 ppsz_path[i_index++] = p_data;
                 msg_Err( p_dec, "Real: %s", p_data );
             }
             RegCloseKey( h_key );
        }
    }
#endif


    /** Try the native libraries first **/
#ifndef WIN32
    for( i = 0; ppsz_path[i]; i++ )
    {
        /* Old format */
        if( asprintf( &psz_dll, "%s/%4.4s.so.6.0", ppsz_path[i],
                  (char *)&p_dec->fmt_in.i_codec ) != -1 )
        {
            i_result = OpenNativeDll( p_dec, ppsz_path[i], psz_dll );
            free( psz_dll );
            if( i_result == VLC_SUCCESS ) return VLC_SUCCESS;
        }

        /* New format */
        if( asprintf( &psz_dll, "%s/%4.4s.so", ppsz_path[i],
                  (char *)&p_dec->fmt_in.i_codec ) != -1 )
        {
            i_result = OpenNativeDll( p_dec, ppsz_path[i], psz_dll );
            free( psz_dll );
            if( i_result == VLC_SUCCESS ) return VLC_SUCCESS;
        }
    }
#endif

    /** Or use the WIN32 dlls **/
#if defined(LOADER) || defined(WIN32)
    for( i = 0; ppsz_path[i]; i++ )
    {
        /* New format */
        if( asprintf( &psz_dll, "%s\\%4.4s.dll", ppsz_path[i],
                  (char *)&p_dec->fmt_in.i_codec ) != -1 )
        {
            i_result = OpenWin32Dll( p_dec, ppsz_path[i], psz_dll );
            free( psz_dll );
            if( i_result == VLC_SUCCESS ) return VLC_SUCCESS;
        }

        /* Old format */
        if( asprintf( &psz_dll, "%s\\%4.4s3260.dll", ppsz_path[i],
                  (char *)&p_dec->fmt_in.i_codec ) != -1 )
        {
            i_result = OpenWin32Dll( p_dec, ppsz_path[i], psz_dll );
            free( psz_dll );
            if( i_result == VLC_SUCCESS ) return VLC_SUCCESS;
        }
    }
#endif

    return VLC_EGENERIC;
}

static int OpenNativeDll( decoder_t *p_dec, char *psz_path, char *psz_dll )
{
#if defined(HAVE_DL_DLOPEN)
    decoder_sys_t *p_sys = p_dec->p_sys;
    void *handle = 0, *context = 0;
    unsigned int i_result;
    void *p_prop;
    int i_prop;

    ra_init_t init_data =
    {
        p_dec->fmt_in.audio.i_rate,
        p_dec->fmt_in.audio.i_bitspersample,
        p_dec->fmt_in.audio.i_channels,
        100, /* quality */
        p_dec->fmt_in.audio.i_blockalign, /* subpacket size */
        p_dec->fmt_in.audio.i_blockalign, /* coded frame size */
        p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra
    };

    msg_Dbg( p_dec, "opening library '%s'", psz_dll );
    if( !(handle = dlopen( psz_dll, RTLD_LAZY )) )
    {
        msg_Dbg( p_dec, "couldn't load library '%s' (%s)",
                 psz_dll, dlerror() );
        return VLC_EGENERIC;
    }

    p_sys->raCloseCodec = dlsym( handle, "RACloseCodec" );
    p_sys->raDecode = dlsym( handle, "RADecode" );
    p_sys->raFlush = dlsym( handle, "RAFlush" );
    p_sys->raFreeDecoder = dlsym( handle, "RAFreeDecoder" );
    p_sys->raGetFlavorProperty = dlsym( handle, "RAGetFlavorProperty" );
    p_sys->raOpenCodec = dlsym( handle, "RAOpenCodec" );
    p_sys->raOpenCodec2 = dlsym( handle, "RAOpenCodec2" );
    p_sys->raInitDecoder = dlsym( handle, "RAInitDecoder" );
    p_sys->raSetFlavor = dlsym( handle, "RASetFlavor" );
    p_sys->raSetDLLAccessPath = dlsym( handle, "SetDLLAccessPath" );
    p_sys->raSetPwd = dlsym( handle, "RASetPwd" ); // optional, used by SIPR

    if( !(p_sys->raOpenCodec || p_sys->raOpenCodec2) ||
        !p_sys->raCloseCodec || !p_sys->raInitDecoder ||
        !p_sys->raDecode || !p_sys->raFreeDecoder ||
        !p_sys->raGetFlavorProperty || !p_sys->raSetFlavor
        /* || !p_sys->raFlush || !p_sys->raSetDLLAccessPath */ )
    {
        goto error_native;
    }

    if( p_sys->raOpenCodec2 )
        i_result = p_sys->raOpenCodec2( &context, psz_path );
    else
        i_result = p_sys->raOpenCodec( &context );

    if( i_result )
    {
        msg_Err( p_dec, "decoder open failed, error code: 0x%x", i_result );
        goto error_native;
    }

    i_result = p_sys->raInitDecoder( context, &init_data );
    if( i_result )
    {
        msg_Err( p_dec, "decoder init failed, error code: 0x%x", i_result );
        goto error_native;
    }

    if( p_sys->i_codec_flavor >= 0 )
    {
        i_result = p_sys->raSetFlavor( context, p_sys->i_codec_flavor );
        if( i_result )
        {
            msg_Err( p_dec, "decoder flavor setup failed, error code: 0x%x",
                     i_result );
            goto error_native;
        }

        p_prop = p_sys->raGetFlavorProperty( context, p_sys->i_codec_flavor,
                                             0, &i_prop );
        msg_Dbg( p_dec, "audio codec: [%d] %s",
                 p_sys->i_codec_flavor, (char *)p_prop );

        p_prop = p_sys->raGetFlavorProperty( context, p_sys->i_codec_flavor,
                                             1, &i_prop );
        if( p_prop )
        {
            int i_bps = ((*((int*)p_prop))+4)/8;
            msg_Dbg( p_dec, "audio bitrate: %5.3f kbit/s (%d bps)",
                     (*((int*)p_prop))*0.001f, i_bps );
        }
    }

    p_sys->context = context;
    p_sys->dll = handle;
    return VLC_SUCCESS;

 error_native:
    if( context ) p_sys->raFreeDecoder( context );
    if( context ) p_sys->raCloseCodec( context );
    dlclose( handle );
#endif

    return VLC_EGENERIC;
}

static int OpenWin32Dll( decoder_t *p_dec, char *psz_path, char *psz_dll )
{
#if defined(LOADER) || defined(WIN32)
    decoder_sys_t *p_sys = p_dec->p_sys;
    void *handle = 0, *context = 0;
    unsigned int i_result;
    void *p_prop;
    int i_prop;

    wra_init_t init_data =
    {
        p_dec->fmt_in.audio.i_rate,
        p_dec->fmt_in.audio.i_bitspersample,
        p_dec->fmt_in.audio.i_channels,
        100, /* quality */
        p_dec->fmt_in.audio.i_blockalign, /* subpacket size */
        p_dec->fmt_in.audio.i_blockalign, /* coded frame size */
        p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra
    };

    msg_Dbg( p_dec, "opening win32 dll '%s'", psz_dll );

#ifdef LOADER
    Setup_LDT_Keeper();
#endif

    if( !(handle = LoadLibraryA( psz_dll )) )
    {
        msg_Dbg( p_dec, "couldn't load dll '%s'", psz_dll );
        return VLC_EGENERIC;
    }

    p_sys->wraCloseCodec = GetProcAddress( handle, "RACloseCodec" );
    p_sys->wraDecode = GetProcAddress( handle, "RADecode" );
    p_sys->wraFlush = GetProcAddress( handle, "RAFlush" );
    p_sys->wraFreeDecoder = GetProcAddress( handle, "RAFreeDecoder" );
    p_sys->wraGetFlavorProperty =
        GetProcAddress( handle, "RAGetFlavorProperty" );
    p_sys->wraOpenCodec = GetProcAddress( handle, "RAOpenCodec" );
    p_sys->wraOpenCodec2 = GetProcAddress( handle, "RAOpenCodec2" );
    p_sys->wraInitDecoder = GetProcAddress( handle, "RAInitDecoder" );
    p_sys->wraSetFlavor = GetProcAddress( handle, "RASetFlavor" );
    p_sys->wraSetDLLAccessPath = GetProcAddress( handle, "SetDLLAccessPath" );
    p_sys->wraSetPwd =
        GetProcAddress( handle, "RASetPwd" ); // optional, used by SIPR

    if( !(p_sys->wraOpenCodec || p_sys->wraOpenCodec2) ||
        !p_sys->wraCloseCodec || !p_sys->wraInitDecoder ||
        !p_sys->wraDecode || !p_sys->wraFreeDecoder ||
        !p_sys->wraGetFlavorProperty || !p_sys->wraSetFlavor
        /* || !p_sys->wraFlush || !p_sys->wraSetDLLAccessPath */ )
    {
        FreeLibrary( handle );
        return VLC_EGENERIC;
    }

    if( p_sys->wraOpenCodec2 )
        i_result = p_sys->wraOpenCodec2( &context, psz_path );
    else
        i_result = p_sys->wraOpenCodec( &context );

    if( i_result )
    {
        msg_Err( p_dec, "decoder open failed, error code: 0x%x", i_result );
        goto error_win32;
    }

    i_result = p_sys->wraInitDecoder( context, &init_data );
    if( i_result )
    {
        msg_Err( p_dec, "decoder init failed, error code: 0x%x", i_result );
        goto error_win32;
    }

    if( p_sys->i_codec_flavor >= 0 )
    {
        i_result = p_sys->wraSetFlavor( context, p_sys->i_codec_flavor );
        if( i_result )
        {
            msg_Err( p_dec, "decoder flavor setup failed, error code: 0x%x",
                     i_result );
            goto error_win32;
        }

        p_prop = p_sys->wraGetFlavorProperty( context, p_sys->i_codec_flavor,
                                              0, &i_prop );
        msg_Dbg( p_dec, "audio codec: [%d] %s",
                 p_sys->i_codec_flavor, (char *)p_prop );

        p_prop = p_sys->wraGetFlavorProperty( context, p_sys->i_codec_flavor,
                                              1, &i_prop );
        if( p_prop )
        {
            int i_bps = ((*((int*)p_prop))+4)/8;
            msg_Dbg( p_dec, "audio bitrate: %5.3f kbit/s (%d bps)",
                     (*((int*)p_prop))*0.001f, i_bps );
        }
    }

    p_sys->context = context;
    p_sys->win32_dll = handle;
    return VLC_SUCCESS;

 error_win32:
    if( context ) p_sys->wraFreeDecoder( context );
    if( context ) p_sys->wraCloseCodec( context );
    FreeLibrary( handle );
#endif

    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDll:
 *****************************************************************************/
static void CloseDll( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->context && p_sys->dll )
    {
        p_sys->raFreeDecoder( p_sys->context );
        p_sys->raCloseCodec( p_sys->context );
    }

    if( p_sys->context && p_sys->win32_dll )
    {
        p_sys->wraFreeDecoder( p_sys->context );
        p_sys->wraCloseCodec( p_sys->context );
    }

#if defined(HAVE_DL_DLOPEN)
    if( p_sys->dll ) dlclose( p_sys->dll );
#endif

#if defined(LOADER) || defined(WIN32)
    if( p_sys->win32_dll ) FreeLibrary( p_sys->win32_dll );

#if 0 //def LOADER /* Segfaults */
    Restore_LDT_Keeper( p_sys->ldt_fs );
    msg_Dbg( p_dec, "Restore_LDT_Keeper" );
#endif
#endif

    p_sys->dll = 0;
    p_sys->win32_dll = 0;
    p_sys->context = 0;
}

/*****************************************************************************
 * DecodeAudio:
 *****************************************************************************/
static aout_buffer_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_aout_buffer = 0;
    unsigned int i_result;
    int i_samples;
    block_t *p_block;

#ifdef LOADER
    if( !p_sys->win32_dll && !p_sys->dll )
    {
        /* We must do open and close in the same thread (unless we do
         * Setup_LDT_Keeper in the main thread before all others */
        if( OpenDll( p_dec ) != VLC_SUCCESS )
        {
            /* Fatal */
            p_dec->b_error = true;
            return NULL;
        }
    }
#endif

    if( pp_block == NULL || *pp_block == NULL ) return NULL;
    p_block = *pp_block;

    if( p_sys->dll )
        i_result = p_sys->raDecode( p_sys->context, (char *)p_block->p_buffer,
                                    (unsigned long)p_block->i_buffer,
                                    p_sys->p_out, &p_sys->i_out, -1 );
    else
        i_result = p_sys->wraDecode( p_sys->context, (char *)p_block->p_buffer,
                                     (unsigned long)p_block->i_buffer,
                                     p_sys->p_out, &p_sys->i_out, -1 );

#if 0
    msg_Err( p_dec, "decoded: %i samples (%i)",
             p_sys->i_out * 8 / p_dec->fmt_out.audio.i_bitspersample /
             p_dec->fmt_out.audio.i_channels, i_result );
#endif

    /* Date management */
    if( p_block->i_pts > 0 &&
        p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

    if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return NULL;
    }

    i_samples = p_sys->i_out * 8 /
        p_dec->fmt_out.audio.i_bitspersample /p_dec->fmt_out.audio.i_channels;

    p_aout_buffer =
        p_dec->pf_aout_buffer_new( p_dec, i_samples );
    if( p_aout_buffer )
    {
        memcpy( p_aout_buffer->p_buffer, p_sys->p_out, p_sys->i_out );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date =
            aout_DateIncrement( &p_sys->end_date, i_samples );
    }

    block_Release( p_block );
    *pp_block = 0;
    return p_aout_buffer;
}
