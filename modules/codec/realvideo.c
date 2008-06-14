/*****************************************************************************
 * realvideo.c: a realvideo decoder that uses the real's dll
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2008 Wang Bo
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
#include <vlc_vout.h>
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

typedef struct cmsg_data_s
{
    uint32_t data1;
    uint32_t data2;
    uint32_t* dimensions;
} cmsg_data_t;

typedef struct transform_in_s
{
    uint32_t len;
    uint32_t unknown1;
    uint32_t chunks;
    uint32_t* extra;
    uint32_t unknown2;
    uint32_t timestamp;
} transform_in_t;

// copypaste from demux_real.c - it should match to get it working!
typedef struct dp_hdr_s
{
    uint32_t chunks;    // number of chunks
    uint32_t timestamp;    // timestamp from packet header
    uint32_t len;    // length of actual data
    uint32_t chunktab;    // offset to chunk offset array
} dp_hdr_t;

/* we need exact positions */
struct rv_init_t
{
    short unk1;
    short w;
    short h;
    short unk3;
    int unk2;
    unsigned int * subformat;
    int unk5;
    unsigned int * format;
} rv_init_t;

struct decoder_sys_t
{
    /* library */
#ifdef LOADER
    ldt_fs_t    *ldt_fs;
#endif
    void        *handle;
    void         *rv_handle;
    int          inited;
    char         *plane;
};

int dll_type = 1;

static unsigned long (*rvyuv_custom_message)(cmsg_data_t* ,void*);
static unsigned long (*rvyuv_free)(void*);
static unsigned long (*rvyuv_init)(void*, void*); // initdata,context
static unsigned long (*rvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#ifdef WIN32
static unsigned long WINAPI (*wrvyuv_custom_message)(cmsg_data_t* ,void*);
static unsigned long WINAPI (*wrvyuv_free)(void*);
static unsigned long WINAPI (*wrvyuv_init)(void*, void*); // initdata,context
static unsigned long WINAPI (*wrvyuv_transform)(char*, char*,transform_in_t*,unsigned int*,void*);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

//static int  OpenPacketizer( vlc_object_t * );
static picture_t *DecodeVideo( decoder_t *, block_t ** );

vlc_module_begin();
    set_description( N_("RealVideo library decoder") );
    set_capability( "decoder", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#ifdef WIN32
static void * load_syms(decoder_t *p_dec, const char *path) 
{
    void *handle;

    msg_Dbg( p_dec, "opening win32 dll '%s'\n", path);
#ifdef LOADER
    Setup_LDT_Keeper();
#endif
    handle = LoadLibraryA(path);
    msg_Dbg( p_dec, "win32 real codec handle=%p  \n",handle);
    if (!handle)
    {
        msg_Err( p_dec, "Error loading dll\n");
        return NULL;
    }

    wrvyuv_custom_message = GetProcAddress(handle, "RV20toYUV420CustomMessage");
    wrvyuv_free = GetProcAddress(handle, "RV20toYUV420Free");
    wrvyuv_init = GetProcAddress(handle, "RV20toYUV420Init");
    wrvyuv_transform = GetProcAddress(handle, "RV20toYUV420Transform");

    if (wrvyuv_custom_message && wrvyuv_free && wrvyuv_init && wrvyuv_transform)
    {
        dll_type = 1;
        return handle;
    }
    msg_Err( p_dec, "Error resolving symbols! (version incompatibility?)\n");
    FreeLibrary(handle);
    return NULL; // error
}
#else
static void * load_syms_linux(decoder_t *p_dec, const char *path) 
{
    void *handle;

    msg_Dbg( p_dec, "opening shared obj '%s'\n", path);

    handle = dlopen (path, RTLD_LAZY);
    if (!handle) 
    {
        msg_Err( p_dec,"Error: %s\n",dlerror());
        return NULL;
    }

    rvyuv_custom_message = dlsym(handle, "RV20toYUV420CustomMessage");
    rvyuv_free = dlsym(handle, "RV20toYUV420Free");
    rvyuv_init = dlsym(handle, "RV20toYUV420Init");
    rvyuv_transform = dlsym(handle, "RV20toYUV420Transform");

    if(rvyuv_custom_message && rvyuv_free && rvyuv_init && rvyuv_transform)
    {
        dll_type = 0;
        return handle;
    }

    msg_Err( p_dec,"Error resolving symbols! (version incompatibility?)\n");
    dlclose(handle);
    return 0;
}
#endif

static int InitVideo(decoder_t *p_dec)
{
    int result;
    struct rv_init_t init_data;
    char fcc[4];
    vlc_mutex_t  *lock;
    char *g_decode_path;

    int  i_vide = p_dec->fmt_in.i_extra;
    unsigned int *p_vide = p_dec->fmt_in.p_extra;
    decoder_sys_t *p_sys = malloc( sizeof( decoder_sys_t ) );
    memset(p_sys,0,sizeof( decoder_sys_t ) );

    if( i_vide < 8 )
    {
            msg_Err( p_dec, "missing extra info" );
            free( p_sys );
            return VLC_EGENERIC;
    }
    if (p_sys->plane) free(p_sys->plane);
    p_sys->plane = malloc (p_dec->fmt_in.video.i_width*p_dec->fmt_in.video.i_height*3/2 + 1024 );
    if (NULL == p_sys->plane)
    {
        msg_Err( p_dec, "cannot alloc plane buffer" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys;
    p_dec->pf_decode_video = DecodeVideo;

    memcpy( fcc, &p_dec->fmt_in.i_codec, 4 );
    init_data.unk1 = 11;
    init_data.w = p_dec->fmt_in.video.i_width ;
    init_data.h = p_dec->fmt_in.video.i_height ;
    init_data.unk3 = 0;
    init_data.unk2 = 0;
    init_data.subformat = (unsigned int*)p_vide[0];
    init_data.unk5 = 1;
    init_data.format = (unsigned int*)p_vide[1];

    /* first try to load linux dlls, if failed and we're supporting win32 dlls,
       then try to load the windows ones */
    bool b_so_opened = false;

#ifdef WIN32
    g_decode_path="plugins\\drv43260.dll";

    if( (p_sys->rv_handle = load_syms(p_dec, g_decode_path)) )
        b_so_opened = true;
#else
    static const char psz_paths[] =
    {
        "/usr/lib/win32\0"
        "/usr/lib/codecs\0"
        "/usr/local/RealPlayer8/Codecs\0"
        "/usr/RealPlayer8/Codecs\0"
        "/usr/lib/RealPlayer8/Codecs\0"
        "/opt/RealPlayer8/Codecs\0"
        "/usr/lib/RealPlayer9/users/Real/Codecs\0"
        "/usr/lib/RealPlayer10/codecs\0"
        "/usr/lib/RealPlayer10GOLD/codecs\0"
        "/usr/lib/helix/player/codecs\0"
        "/usr/lib64/RealPlayer8/Codecs\0"
        "/usr/lib64/RealPlayer9/users/Real/Codecs\0"
        "/usr/lib64/RealPlayer10/codecs\0"
        "/usr/lib64/RealPlayer10GOLD/codecs\0"
        "/usr/local/lib/codecs\0"
        "\0"
    };

    for( size_t i = 0; psz_paths[i]; i += strlen( psz_paths + i ) + 1 )
    {
        if( asprintf( &g_decode_path, "%s/drv4.so.6.0", psz_paths + i ) != -1 )
        {
            p_sys->rv_handle = load_syms_linux(p_dec, g_decode_path);
            free( g_decode_path );
        }
        if( p_sys->rv_handle )
        {
            b_so_opened = true;
            break;
        }

        if( asprintf( &g_decode_path, "%s/drv3.so.6.0", psz_paths + i ) != -1 )
        {
            p_sys->rv_handle = load_syms_linux(p_dec, g_decode_path);
            free( g_decode_path );
        }
        if( p_sys->rv_handle )
        {
            b_so_opened = true;
            break;
        }

        msg_Dbg( p_dec, "Cannot load real decoder library: %s", g_decode_path);
    }
#endif

    if(!b_so_opened )
    {
        msg_Err( p_dec, "Cannot any real decoder library" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    lock = var_AcquireMutex( "rm_mutex" );
    if ( lock == NULL )
        return VLC_EGENERIC;

    p_sys->handle=NULL;
    #ifdef WIN32
    if (dll_type == 1)
        result=(*wrvyuv_init)(&init_data, &p_sys->handle);
    else
    #endif
        result=(*rvyuv_init)(&init_data, &p_sys->handle);
    if (result)
    {
        msg_Err( p_dec, "Cannot Init real decoder library: %s",  g_decode_path);
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* setup rv30 codec (codec sub-type and image dimensions): */
    /*if ( p_dec->fmt_in.i_codec == VLC_FOURCC('R','V','3','0') )*/
    if (p_vide[1]>=0x20200002)
    {
        int i, cmsg_cnt;
        uint32_t cmsg24[16]={p_dec->fmt_in.video.i_width,p_dec->fmt_in.video.i_height};
        cmsg_data_t cmsg_data={0x24,1+(p_vide[1]&7), &cmsg24[0]};
        cmsg_cnt = (p_vide[1]&7)*2;
        if (i_vide - 8 < cmsg_cnt) {
                    cmsg_cnt = i_vide - 8;
        }
        for (i = 0; i < cmsg_cnt; i++)
            cmsg24[2+i] = p_vide[8+i]*4;
        #ifdef WIN32
        if (dll_type == 1)
            (*wrvyuv_custom_message)(&cmsg_data,p_sys->handle);
        else
        #endif
            (*rvyuv_custom_message)(&cmsg_data,p_sys->handle);
    }
    /*
    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_FOURCC( 'Y','V','1','2' ));
    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_FOURCC( 'Y','U','Y','2' ));
     */
    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_FOURCC( 'I', '4', '2', '0'));
     
    p_dec->fmt_out.video.i_width = p_dec->fmt_in.video.i_width;
    p_dec->fmt_out.video.i_height= p_dec->fmt_in.video.i_height;
    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * p_dec->fmt_in.video.i_width / p_dec->fmt_in.video.i_height;
    p_sys->inited = 0;

    vlc_mutex_unlock( lock );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    /* create a mutex */
    var_Create( p_this->p_libvlc, "rm_mutex", VLC_VAR_MUTEX );

    switch ( p_dec->fmt_in.i_codec )
    {
    case VLC_FOURCC('R','V','1','0'): 
    case VLC_FOURCC('R','V','2','0'): 
    case VLC_FOURCC('R','V','3','0'):
    case VLC_FOURCC('R','V','4','0'): 
        p_dec->p_sys = NULL;
        p_dec->pf_decode_video = DecodeVideo;
        return InitVideo(p_dec);

    default:
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_mutex_t   *lock;

    /* get lock, avoid segfault */
    lock = var_AcquireMutex( "rm_mutex" );

    #ifdef WIN32
    if (dll_type == 1)
    {
        if (wrvyuv_free)
            wrvyuv_free(p_sys->handle);
    }
    else
    #endif
        if (rvyuv_free)
            rvyuv_free(p_sys->handle);
#ifdef WIN32
    if (dll_type == 1)
    {
        if (p_sys->rv_handle)
            FreeLibrary(p_sys->rv_handle);
    }
    else
#endif
        p_sys->rv_handle=NULL;

    if (p_sys->plane)
    {
        free(p_sys->plane);
        p_sys->plane = NULL;
    }

    msg_Dbg( p_dec, "FreeLibrary ok." );
#ifdef LOADER
    Restore_LDT_Keeper( p_sys->ldt_fs );
    msg_Dbg( p_dec, "Restore_LDT_Keeper" );
#endif
    p_sys->inited = 0;

    if ( lock )
    vlc_mutex_unlock( lock );

    if ( p_sys )
        free( p_sys );
}

/*****************************************************************************
 * DecodeVideo:
 *****************************************************************************/
static picture_t *DecodeVideo( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_mutex_t   *lock;
    block_t       *p_block;
    picture_t     *p_pic;
    mtime_t       i_pts;
    int           result;

    /* We must do open and close in the same thread (unless we do
     * Setup_LDT_Keeper in the main thread before all others */
    if ( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    i_pts = p_block->i_pts ? p_block->i_pts : p_block->i_dts;

    lock = var_AcquireMutex( "rm_mutex" );
    if ( lock == NULL )
        return NULL;

    p_pic = p_dec->pf_vout_buffer_new( p_dec );

    if ( p_pic )
    {
        unsigned int transform_out[5];
        dp_hdr_t dp_hdr;
        transform_in_t transform_in;
        uint32_t pkg_len = ((uint32_t*)p_block->p_buffer)[0];
        unsigned char* dp_data=((unsigned char*)p_block->p_buffer)+8;
        uint32_t* extra=(uint32_t*)(((char*)p_block->p_buffer)+8+pkg_len);
        uint32_t img_size;


        dp_hdr.len = pkg_len;
        dp_hdr.chunktab = 8 + pkg_len;
        dp_hdr.chunks = ((uint32_t*)p_block->p_buffer)[1]-1;
        dp_hdr.timestamp = i_pts;

        memset(&transform_in, 0, sizeof(transform_in_t));

        transform_in.len = dp_hdr.len;
        transform_in.extra = extra;
        transform_in.chunks = dp_hdr.chunks;
        transform_in.timestamp = dp_hdr.timestamp;

        memset (p_sys->plane, 0, p_dec->fmt_in.video.i_width * p_dec->fmt_in.video.i_height *3/2 );

        #ifdef WIN32
        if (dll_type == 1)
            result=(*wrvyuv_transform)(dp_data, p_sys->plane, &transform_in, transform_out, p_sys->handle);
        else
        #endif
            result=(*rvyuv_transform)(dp_data, p_sys->plane, &transform_in, transform_out, p_sys->handle);

        /* msg_Warn(p_dec, "Real Size %d X %d", transform_out[3],
                 transform_out[4]); */
        /* some bug rm file will print the messages :
                [00000551] realvideo decoder warning: Real Size 320 X 240
                [00000551] realvideo decoder warning: Real Size 480 X 272
                [00000551] realvideo decoder warning: Real Size 320 X 240
                [00000551] realvideo decoder warning: Real Size 320 X 240
                ...
            so it needs fixing!
        */
        if ( p_sys->inited == 0 )
        {
            /* fix and get the correct image size! */
            if ( p_dec->fmt_in.video.i_width != transform_out[3]
             || p_dec->fmt_in.video.i_height  != transform_out[4] )
            {
                msg_Warn(p_dec, "Warning, Real's Header give a wrong "
                         "information about media's width and height!\n"
                         "\tRealHeader: \t %d X %d  \t %d X %d",
                         p_dec->fmt_in.video.i_width,
                         p_dec->fmt_in.video.i_height,
                         transform_out[3],transform_out[4]);
                
                if ( p_dec->fmt_in.video.i_width * p_dec->fmt_in.video.i_height >= transform_out[3] * transform_out[4] )
                {
                    p_dec->fmt_out.video.i_width = 
                    p_dec->fmt_out.video.i_visible_width = 
                    p_dec->fmt_in.video.i_width = transform_out[3] ;

                    p_dec->fmt_out.video.i_height= 
                    p_dec->fmt_out.video.i_visible_height = 
                    p_dec->fmt_in.video.i_height= transform_out[4];

                    p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR * p_dec->fmt_in.video.i_width / p_dec->fmt_in.video.i_height;
                }
                else
                {
                    // TODO: realloc plane's size! but [in fact] it maybe not happen! 
                    msg_Err(p_dec,"plane space not enough ,skip");
                }
            }
            p_sys->inited = 1;
        }

        img_size = p_dec->fmt_in.video.i_width * p_dec->fmt_in.video.i_height;
        memcpy( p_pic->p[0].p_pixels, p_sys->plane,  img_size);
        memcpy( p_pic->p[1].p_pixels, p_sys->plane + img_size, img_size/4);
        memcpy( p_pic->p[2].p_pixels, p_sys->plane + img_size * 5/4, img_size/4);
        p_pic->date = i_pts ;

        /*  real video frame is small( frame and frame's time-shift is short), 
            so it will become late picture easier (when render-time changed)and
            droped by video-output.*/
        p_pic->b_force = 1;
    }

    vlc_mutex_unlock( lock );

    block_Release( p_block );
    return p_pic;
}

