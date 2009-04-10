/*****************************************************************************
 * xvmc.c : XVMC plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id$
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_keys.h>

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_SYS_SHM_H
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#endif

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef HAVE_SYS_SHM_H
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/vldXvMC.h>

#include "../../codec/xvmc/accel_xvmc.h"
#include "xcommon.h"
#include "../../codec/spudec/spudec.h"
#include <unistd.h>

/* picture structure */
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
#define FRAME_PICTURE 3

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  Activate   ( vlc_object_t * );
extern void Deactivate ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADAPTOR_TEXT N_("XVMC adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "If you graphics card provides several adaptors, this option allows you " \
    "to choose which one will be used (you shouldn't have to change this).")

#define ALT_FS_TEXT N_("Alternate fullscreen method")
#define ALT_FS_LONGTEXT N_( \
    "There are two ways to make a fullscreen window, unfortunately each one " \
    "has its drawbacks.\n" \
    "1) Let the window manager handle your fullscreen window (default), but " \
    "things like taskbars will likely show on top of the video.\n" \
    "2) Completely bypass the window manager, but then nothing will be able " \
    "to show on top of the video.")

#define DISPLAY_TEXT N_("X11 display name")
#define DISPLAY_LONGTEXT N_( \
    "Specify the X11 hardware display you want to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define CHROMA_TEXT N_("XVimage chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the XVideo renderer to use a specific chroma format instead of " \
    "trying to improve performances by using the most efficient one.")

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

#define SCREEN_TEXT N_("Screen to be used for fullscreen mode.")
#define SCREEN_LONGTEXT N_( \
    "Choose the screen you want to use in fullscreen mode. For instance " \
    "set it to 0 for first screen, 1 for the second.")

#define MODE_TEXT N_("Deinterlace mode")
#define MODE_LONGTEXT N_("You can choose the default deinterlace mode")

#define CROP_TEXT N_("Crop")
#define CROP_LONGTEXT N_("You can choose the crop style to apply.")

vlc_module_begin ()
    set_shortname( "XVMC" )
    add_string( "xvmc-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true )
    add_integer( "xvmc-adaptor", -1, NULL, ADAPTOR_TEXT, ADAPTOR_LONGTEXT, true )
    add_bool( "xvmc-altfullscreen", 0, NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT, true )
    add_string( "xvmc-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
#ifdef HAVE_SYS_SHM_H
    add_bool( "xvmc-shm", 1, NULL, SHM_TEXT, SHM_LONGTEXT, true )
#endif
#ifdef HAVE_XINERAMA
    add_integer ( "xvmc-xineramascreen", -1, NULL, SCREEN_TEXT, SCREEN_LONGTEXT, true )
#endif
    add_string( "xvmc-deinterlace-mode", "bob", NULL, MODE_TEXT, MODE_LONGTEXT, false )
    add_string( "xvmc-crop-style", "eq", NULL, CROP_TEXT, CROP_LONGTEXT, false )

    set_description( N_("XVMC extension video output") )
    set_capability( "video output", 10 )
    set_callbacks( Activate, Deactivate )
vlc_module_end ()

/* following functions are local */

static const unsigned accel_priority[] = {
    VLC_XVMC_ACCEL_VLD,
};

#define NUM_ACCEL_PRIORITY (sizeof(accel_priority)/sizeof(accel_priority[0]))

/*
 * Additional thread safety, since the plugin may decide to destroy a context
 * while it's surfaces are still active in the video-out loop.
 * When / If XvMC libs are reasonably thread-safe, the locks can be made
 * more efficient by allowing multiple threads in that do not destroy
 * the context or surfaces that may be active in other threads.
 */

static void init_context_lock( context_lock_t *c )
{
    pthread_cond_init(&c->cond,NULL);
    pthread_mutex_init(&c->mutex,NULL);
    c->num_readers = 0;
}

void free_context_lock( context_lock_t *c )
{
    pthread_mutex_destroy(&c->mutex);
    pthread_cond_destroy(&c->cond);
}

void xvmc_context_reader_lock( context_lock_t *c )
{
    pthread_mutex_lock(&c->mutex);
    c->num_readers++;
    pthread_mutex_unlock(&c->mutex);
}

void xvmc_context_reader_unlock( context_lock_t *c )
{
    pthread_mutex_lock(&c->mutex);
    if (c->num_readers > 0) {
        if (--(c->num_readers) == 0) {
        pthread_cond_broadcast(&c->cond);
        }
    }
    pthread_mutex_unlock(&c->mutex);
}

void xvmc_context_writer_lock( context_lock_t *c )
{
    pthread_mutex_lock(&c->mutex);
    while(c->num_readers) {
        pthread_cond_wait( &c->cond, &c->mutex );
    }
}

void xvmc_context_writer_unlock( context_lock_t *c )
{
    pthread_mutex_unlock( &c->mutex );
}

void clear_xx44_palette( xx44_palette_t *p )
{
    int i;
    uint32_t *cluts = p->cluts;
    int *ids = p->lookup_cache;

    i= p->size;
    while(i--)
        *cluts++ = 0;
    i = 2*OVL_PALETTE_SIZE;
    while(i--)
        *ids++ = -1;
    p->max_used=1;
}

static void init_xx44_palette( xx44_palette_t *p, unsigned num_entries )
{
    p->size = (num_entries > XX44_PALETTE_SIZE) ?
                    XX44_PALETTE_SIZE : num_entries;
}

static void dispose_xx44_palette(xx44_palette_t *p)
{
    /* Nothing to do */
}

static void colorToPalette( const uint32_t *icolor, unsigned char *palette_p,
                            unsigned num_xvmc_components, char *xvmc_components )
{
    const clut_t *color = (const clut_t *) icolor;
    unsigned int i;

    for (i=0; i<num_xvmc_components; ++i)
    {
        switch(xvmc_components[i])
        {
            case 'V': *palette_p = color->cr; break;
            case 'U': *palette_p = color->cb; break;
            case 'Y':
            default:  *palette_p = color->y; break;
        }
        *palette_p++;
    }
}


void xx44_to_xvmc_palette( const xx44_palette_t *p,unsigned char *xvmc_palette,
                           unsigned first_xx44_entry, unsigned num_xx44_entries,
                           unsigned num_xvmc_components, char *xvmc_components )
{
    unsigned int i;
    const uint32_t *cluts = p->cluts + first_xx44_entry;

    for( i=0; i<num_xx44_entries; ++i )
    {
        if( (cluts - p->cluts) < p->size )
        {
            colorToPalette( cluts++, xvmc_palette,
                            num_xvmc_components, xvmc_components );
            xvmc_palette += num_xvmc_components;
        }
    }
}

static int xx44_paletteIndex( xx44_palette_t *p, int color, uint32_t clut )
{
    unsigned int i;
    uint32_t *cluts = p->cluts;
    int tmp;

    if( (tmp = p->lookup_cache[color]) >= 0 )
    {
        if (cluts[tmp] == clut)
            return tmp;
    }
    for (i=0; i<p->max_used; ++i)
    {
        if (*cluts++ == clut) {
            p->lookup_cache[color] = i;
            return p->lookup_cache[color];
        }
    }

    if( p->max_used == (p->size -1) )
    {
        //printf("video_out: Warning! Out of xx44 palette colors!\n");
        return 1;
    }
    p->cluts[p->max_used] = clut;
    p->lookup_cache[color] = p->max_used++;
    return p->lookup_cache[color];
}

static void memblend_xx44( uint8_t *mem, uint8_t val,
                           size_t size, uint8_t mask )
{
    uint8_t masked_val = val & mask;

/* size_t is unsigned, therefore always positive
   if (size < 0)
        return;*/

    while(size--)
    {
        if( (*mem & mask) <= masked_val )
            *mem = val;
        mem++;
    }
}

void blend_xx44( uint8_t *dst_img, subpicture_t *sub_img,
                 int dst_width, int dst_height, int dst_pitch,
                 xx44_palette_t *palette, int ia44 )
{
    int src_width;
    int src_height;
    int mask;
    int x_off;
    int y_off;
    int x, y;
    uint8_t norm_pixel,clip_pixel;
    uint8_t *dst_y;
    uint8_t *dst;
    uint8_t alphamask;
    int clip_right;
    int i_len, i_color;
    uint16_t *p_source = NULL;
#if 0
    if (!sub_img)
        return;

    src_width  = sub_img->i_width;
    src_height = sub_img->i_height;
    x_off = sub_img->i_x;
    y_off = sub_img->i_y;
    alphamask = (ia44) ? 0x0F : 0xF0;
    p_source = (uint16_t *)sub_img->p_sys->p_data;

    dst_y = dst_img + dst_pitch*y_off + x_off;

    if( (x_off + sub_img->i_width) <= dst_width )
        clip_right = sub_img->i_width;
    else
        clip_right = dst_width - x_off;

    if ((src_height + y_off) > dst_height)
        src_height = dst_height - y_off;

    for (y = 0; y < src_height; y++)
    {
        mask = !( (y < sub_img->p_sys->i_y_start) ||
                  (y >= sub_img->p_sys->i_y_end) );
        dst = dst_y;

        for (x = 0; x < src_width;)
        {
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;

            if( (i_len > 0) && ((x+i_len) <= src_width) )
            {
                /* Get the RLE part, then draw the line */
                uint32_t color = (sub_img->p_sys->pi_yuv[i_color][0] << 16) |
                                 (sub_img->p_sys->pi_yuv[i_color][1] << 0) |
                                 (sub_img->p_sys->pi_yuv[i_color][2] << 8);

                norm_pixel = (uint8_t)(
                            (xx44_paletteIndex( palette,i_color, color ) << 4) |
                            (sub_img->p_sys->pi_alpha[i_color] & 0x0F) );
                clip_pixel = (uint8_t)(
                            (xx44_paletteIndex( palette,i_color + OVL_PALETTE_SIZE,
                                                sub_img->p_sys->pi_yuv[i_color][0] ) << 4) |
                            (sub_img->p_sys->pi_alpha[i_color] & 0x0F));

                if( !ia44 )
                {
                    norm_pixel = ((norm_pixel & 0x0F) << 4) | ((norm_pixel & 0xF0) >> 4);
                    clip_pixel = ((clip_pixel & 0x0F) << 4) | ((clip_pixel & 0xF0) >> 4);
                }
                if( mask )
                {
                    if( x < sub_img->p_sys->i_x_start )
                    {
                        if( (x + i_len) <= sub_img->p_sys->i_x_start )
                        {
                            memblend_xx44( dst, norm_pixel, i_len, alphamask );
                            dst += i_len;
                        }
                        else
                        {
                            memblend_xx44( dst, norm_pixel,
                                           sub_img->p_sys->i_x_start - x,
                                           alphamask );
                            dst += sub_img->p_sys->i_x_start - x;
                            i_len -= sub_img->p_sys->i_x_start - x;
                            if( i_len <= (sub_img->p_sys->i_x_end -
                                          sub_img->p_sys->i_x_start) )
                            {
                                memblend_xx44( dst, clip_pixel,
                                               i_len, alphamask);
                                dst += i_len;
                            }
                            else
                            {
                                memblend_xx44( dst, clip_pixel,
                                               sub_img->p_sys->i_x_end -
                                                    sub_img->p_sys->i_x_start,
                                               alphamask );
                                dst += (sub_img->p_sys->i_x_end -
                                        sub_img->p_sys->i_x_start);
                                i_len -= (sub_img->p_sys->i_x_end -
                                          sub_img->p_sys->i_x_start);
                                memblend_xx44( dst, norm_pixel,
                                               i_len, alphamask );
                                dst += i_len;
                            }
                        }
                    }
                    else if( x < sub_img->p_sys->i_x_end )
                    {
                        if( i_len <= (sub_img->p_sys->i_x_end - x) )
                        {
                            memblend_xx44( dst, clip_pixel, i_len, alphamask);
                            dst += i_len;
                        }
                        else
                        {
                            memblend_xx44( dst, clip_pixel,
                                           sub_img->p_sys->i_x_end - x,
                                           alphamask);
                            dst += (sub_img->p_sys->i_x_end - x);
                            i_len -= (sub_img->p_sys->i_x_end - x);
                            memblend_xx44( dst, norm_pixel, i_len, alphamask);
                            dst += i_len;
                        }
                    }
                    else
                    {
                        memblend_xx44( dst, norm_pixel, i_len, alphamask );
                        dst += i_len;
                    }
                }
                else
                {
                    memblend_xx44( dst, norm_pixel, i_len, alphamask );
                    dst += i_len;
                }
            }
            else
            {
                return;
            }
            x += i_len;
        }
        dst_y += dst_pitch;
    }
#endif
}

int xxmc_xvmc_surface_valid( vout_thread_t *p_vout, XvMCSurface *surf )
{
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;
    unsigned long index = surf - handler->surfaces;
    int ret;

    if( index >= XVMC_MAX_SURFACES )
        return 0;
    pthread_mutex_lock(&handler->mutex);
    ret = handler->surfValid[index];
    pthread_mutex_unlock(&handler->mutex);
    return ret;
}

static void xxmc_xvmc_dump_subpictures( vout_thread_t *p_vout )
{
    int i;
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;

    for( i=0; i < XVMC_MAX_SUBPICTURES; ++i )
    {
        msg_Dbg( p_vout, "handler in use %d, valid %d",
                         handler->subInUse[i],
                         handler->subValid[i]);
    }
}

XvMCSubpicture *xxmc_xvmc_alloc_subpicture( vout_thread_t *p_vout,
                    XvMCContext *context, unsigned short width,
                    unsigned short height, int xvimage_id )
{
    int i;
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;
    int status;

    pthread_mutex_lock(&handler->mutex);
    /* xxmc_xvmc_dump_subpictures(p_vout); */
    for( i=0; i<XVMC_MAX_SUBPICTURES; ++i )
    {
        if( handler->subValid[i] && !handler->subInUse[i] )
        {
            XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
            if( XvMCGetSubpictureStatus( p_vout->p_sys->p_display,
                                         handler->subpictures + i,
                                         &status ) )
            {
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                continue;
            }
            XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
            if( status & XVMC_DISPLAYING )
                continue;
            handler->subInUse[i] = 1;
            /* xxmc_xvmc_dump_subpictures(p_vout); */
            pthread_mutex_unlock(&handler->mutex);
            return (handler->subpictures + i);
        }
    }
    for (i=0; i<XVMC_MAX_SUBPICTURES; ++i)
    {
        if( !handler->subInUse[i] )
        {
            XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
            if( Success != XvMCCreateSubpicture( p_vout->p_sys->p_display,
                                                 context,
                                                 handler->subpictures + i,
                                                 width, height, xvimage_id ) )
            {
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                pthread_mutex_unlock( &handler->mutex );
                return NULL;
            }
            XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
            msg_Dbg( p_vout, "video_out_xxmc: created subpicture %d", i );
            handler->subInUse[i] = 1;
            handler->subValid[i] = 1;
            pthread_mutex_unlock( &handler->mutex );
            return (handler->subpictures + i);
        }
    }
    pthread_mutex_unlock( &handler->mutex );
    return NULL;
}

void xxmc_xvmc_free_subpicture( vout_thread_t *p_vout, XvMCSubpicture *sub )
{
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;
    unsigned int index = sub - handler->subpictures;

    if( index >= XVMC_MAX_SUBPICTURES )
        return;

    pthread_mutex_lock( &handler->mutex );
    handler->subInUse[index] = 0;
    /* xxmc_xvmc_dump_subpictures(p_vout); */
    pthread_mutex_unlock( &handler->mutex );
}

static void xxmc_xvmc_surface_handler_construct( vout_thread_t *p_vout )
{
    int i;
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;
 
    pthread_mutex_init( &handler->mutex, NULL );
    for( i=0; i<XVMC_MAX_SURFACES; ++i )
    {
        handler->surfInUse[i] = 0;
        handler->surfValid[i] = 0;
    }
    for( i=0; i<XVMC_MAX_SUBPICTURES; ++i )
    {
        handler->subInUse[i] = 0;
        handler->subValid[i] = 0;
    }
}

static void xxmc_xvmc_dump_surfaces( vout_thread_t *p_vout )
{
    int i;
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;

    for (i=0; i<XVMC_MAX_SURFACES; ++i)
    {
        msg_Dbg(p_vout, "surfaces in use %d, valid %d;",
                        handler->surfInUse[i],
                        handler->surfValid[i]);
    }
}

void xxmc_xvmc_free_surface( vout_thread_t *p_vout, XvMCSurface *surf )
{
    xvmc_surface_handler_t *handler = &p_vout->p_sys->xvmc_surf_handler;
    unsigned int index = 0;

    index = (surf - handler->surfaces);

    if (index < XVMC_MAX_SURFACES)
    {
        pthread_mutex_lock(&handler->mutex);
        msg_Dbg( p_vout,"free surface %d",index );
        handler->surfInUse[index]--;
        xxmc_xvmc_dump_surfaces(p_vout);
        pthread_mutex_unlock(&handler->mutex);
    }
}

int checkXvMCCap( vout_thread_t *p_vout )
{
    int i_xvport = 0;
    int numSurf = 0;
    int numSub = 0;
    int i,j;
    XvMCSurfaceInfo     *surfaceInfo =NULL;
    XvMCSurfaceInfo     *curInfo = NULL;
    XvMCContext         c;
    xvmc_capabilities_t *curCap = NULL;
    XvImageFormatValues *formatValues = NULL;

    i_xvport = p_vout->p_sys->i_xvport;
    p_vout->p_sys->xvmc_cap = 0;

    init_context_lock( &p_vout->p_sys->xvmc_lock );
    xvmc_context_writer_lock( &p_vout->p_sys->xvmc_lock );

    p_vout->p_sys->old_subpic = NULL;
    p_vout->p_sys->new_subpic = NULL;
    p_vout->p_sys->contextActive = 0;
    p_vout->p_sys->subImage = NULL;
    p_vout->p_sys->hwSubpictures = 0;
    p_vout->p_sys->xvmc_palette = NULL;

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );

    if( !XvMCQueryExtension( p_vout->p_sys->p_display,
                             &p_vout->p_sys->xvmc_eventbase,
                             &p_vout->p_sys->xvmc_errbase ) )
    {
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_vout,"XvMC extension found" );

    surfaceInfo = XvMCListSurfaceTypes(p_vout->p_sys->p_display, i_xvport, &numSurf);
    if( !surfaceInfo )
    {
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->xvmc_cap =
            (xvmc_capabilities_t *) malloc( numSurf *
                                            sizeof(xvmc_capabilities_t) );
    if( !p_vout->p_sys->xvmc_cap )
    {
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->xvmc_num_cap = numSurf;
    curInfo = surfaceInfo;
    curCap = p_vout->p_sys->xvmc_cap;

    msg_Dbg( p_vout,"found %d XvMC surface types", numSurf );

    for( i=0; i< numSurf; ++i )
    {
        curCap->mpeg_flags = 0;
        curCap->accel_flags = 0;
        if( curInfo->chroma_format == XVMC_CHROMA_FORMAT_420 )
        {
            curCap->mpeg_flags |= ((curInfo->mc_type & XVMC_MPEG_1) ?
                                                VLC_XVMC_MPEG_1 : 0);
            curCap->mpeg_flags |= ((curInfo->mc_type & XVMC_MPEG_2) ?
                                                VLC_XVMC_MPEG_2 : 0);
            curCap->mpeg_flags |= ((curInfo->mc_type & XVMC_MPEG_4) ?
                                                VLC_XVMC_MPEG_4 : 0);
            curCap->accel_flags |= ((curInfo->mc_type & XVMC_VLD) ?
                                              VLC_XVMC_ACCEL_VLD : 0);
            curCap->accel_flags |= ((curInfo->mc_type & XVMC_IDCT) ?
                                             VLC_XVMC_ACCEL_IDCT : 0);
            curCap->accel_flags |= ((curInfo->mc_type & (XVMC_VLD | XVMC_IDCT)) ?
                                            0 : VLC_XVMC_ACCEL_MOCOMP);
            curCap->max_width = curInfo->max_width;
            curCap->max_height = curInfo->max_height;
            curCap->sub_max_width = curInfo->subpicture_max_width;
            curCap->sub_max_height = curInfo->subpicture_max_height;
            curCap->flags = curInfo->flags;

            msg_Dbg (p_vout, "surface type %d: Max size: %d %d.",
                            i, curCap->max_width, curCap->max_height);
            msg_Dbg (p_vout, "surface subtype %d: Max subpic size: %d %d.",
                            i, curCap->sub_max_width, curCap->sub_max_height);

            curCap->type_id = curInfo->surface_type_id;
            formatValues = XvMCListSubpictureTypes( p_vout->p_sys->p_display,
                                                    i_xvport,
                                                    curCap->type_id,
                                                    &numSub );
            curCap->subPicType.id = 0;
            if( formatValues )
            {
                msg_Dbg( p_vout, "surface type %d: found %d XvMC subpicture types",
                                i, numSub);
                for( j = 0; j<numSub; ++j )
                {
                    if( formatValues[j].id == FOURCC_IA44 )
                    {
                        curCap->subPicType = formatValues[j];
                        msg_Dbg( p_vout,
                                    "surface type %d: detected and using "
                                    "IA44 subpicture type.", i );
                        /* Prefer IA44 */
                        break;
                    }
                    else if( formatValues[j].id == FOURCC_AI44 )
                    {
                        curCap->subPicType = formatValues[j];
                        msg_Dbg( p_vout,
                                 "surface type %d: detected AI44 "
                                 "subpicture type.", i );
                    }
                }
            }
            XFree(formatValues);
            curInfo++;
            curCap++;
        }
    }
    XFree(surfaceInfo);

    /*
     * Try to create a direct rendering context. This will fail if we are not
     * on the displaying computer or an indirect context is not available.
     */
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    curCap = p_vout->p_sys->xvmc_cap;
    if( Success == XvMCCreateContext( p_vout->p_sys->p_display, i_xvport,
                                      curCap->type_id,
                                      curCap->max_width,
                                      curCap->max_height,
                                      XVMC_DIRECT, &c ) )
    {
        msg_Dbg( p_vout, "using direct XVMC rendering context" );
        p_vout->p_sys->context_flags = XVMC_DIRECT;
    }
    else if( Success == XvMCCreateContext( p_vout->p_sys->p_display, i_xvport,
                                           curCap->type_id,
                                           curCap->max_width,
                                           curCap->max_height,
                                           0, &c ) )
    {
        msg_Dbg( p_vout, "using default XVMC rendering context" );
        p_vout->p_sys->context_flags = 0;
    }
    else
    {
        if( p_vout->p_sys->xvmc_cap )
            free( p_vout->p_sys->xvmc_cap );
        p_vout->p_sys->xvmc_cap = NULL;
        msg_Err( p_vout, "use of direct XvMC context on a remote display failed"
                         " falling back to XV." );
        xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
        return VLC_SUCCESS;
    }
    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    XvMCDestroyContext( p_vout->p_sys->p_display, &c );
    xxmc_xvmc_surface_handler_construct( p_vout );
    /*  p_vout->p_sys->capabilities |= VO_CAP_XXMC; */
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    init_xx44_palette( &p_vout->p_sys->palette , 0 );
    p_vout->p_sys->last_accel_request = 0xFFFFFFFF;
    xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock );
    return VLC_SUCCESS;
}

static int xxmc_setup_subpictures( vout_thread_t *p_vout,
        unsigned int width, unsigned int height )
{
    xvmc_capabilities_t *curCap = NULL;
    XvMCSubpicture *sp = NULL;

    if( p_vout->p_sys->contextActive )
    {
        curCap = p_vout->p_sys->xvmc_cap + p_vout->p_sys->xvmc_cur_cap;

        if( (width > curCap->sub_max_width) ||
            (height > curCap->sub_max_height) )
            return VLC_EGENERIC;

        if( (p_vout->p_sys->xvmc_backend_subpic =
                (curCap->flags & XVMC_BACKEND_SUBPICTURE)) )
            msg_Dbg( p_vout, "using backend subpictures." );

        if (!p_vout->p_sys->subImage)
        {
            XLockDisplay( p_vout->p_sys->p_display );
            msg_Dbg(p_vout, "xxmc_setup_subpictures");
#ifdef HAVE_SYS_SHM_H
            if( p_vout->p_sys->i_shm_opcode )
            {
                /* Create image using XShm extension */
                p_vout->p_sys->subImage = CreateShmImage( p_vout,
                                            p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_xvport,
                                            curCap->subPicType.id,
                                            /* VLC2X11_FOURCC( p_vout->output. i_chroma ), */
                                            &p_vout->p_sys->subShmInfo,
                                            p_vout->output.i_width,
                                            p_vout->output.i_height );
            }
#endif /* HAVE_SYS_SHM_H */
            XUnlockDisplay( p_vout->p_sys->p_display );
            if( !p_vout->p_sys->subImage )
            {
                msg_Dbg(p_vout, "failed allocating XvImage for supbictures" );
                return VLC_EGENERIC;
            }
        }

        sp = xxmc_xvmc_alloc_subpicture( p_vout, &p_vout->p_sys->context,
                                         width, height,
                                         curCap->subPicType.id );
        if( sp )
        {
            init_xx44_palette( &p_vout->p_sys->palette, sp->num_palette_entries );
            p_vout->p_sys->xvmc_palette = (char *) malloc( sp->num_palette_entries
                    * sp->entry_bytes );
            xxmc_xvmc_free_subpicture( p_vout, sp);
            if( !p_vout->p_sys->xvmc_palette )
                return VLC_EGENERIC;
            p_vout->p_sys->hwSubpictures = 1;
        }
    }
    return VLC_SUCCESS;
}

static void xvmc_check_colorkey_properties( vout_thread_t *p_vout )
{
    int num,i;
    XvAttribute *xvmc_attributes = NULL;
    Atom ap;

    /*
    * Determine if the context is of "Overlay" type. If so,
    * check whether we can autopaint.
    */
    p_vout->p_sys->have_xvmc_autopaint = 0;
    if( p_vout->p_sys->context_flags & XVMC_OVERLAID_SURFACE )
    {
        msg_Dbg( p_vout, "check colorkey properties" );
        XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
        xvmc_attributes = XvMCQueryAttributes( p_vout->p_sys->p_display,
                                               &p_vout->p_sys->context,
                                               &num );
        if( xvmc_attributes )
        {
            for( i = 0; i < num; ++i )
            {
                if( strncmp( "XV_AUTOPAINT_COLORKEY",
                             xvmc_attributes[i].name,
                             21) == 0)
                {
                    ap = XInternAtom( p_vout->p_sys->p_display,
                                      "XV_AUTOPAINT_COLORKEY",
                                       False );
                    XvMCSetAttribute( p_vout->p_sys->p_display,
                                      &p_vout->p_sys->context,
                                      ap,
                                      1 ); /* p_vout->p_sys->props[VO_PROP_AUTOPAINT_COLORKEY].value */
                    p_vout->p_sys->have_xvmc_autopaint = 1;
                    msg_Dbg( p_vout, "has xvmc autopaint" );
                }
            }
        }
        XFree( xvmc_attributes );
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        /* p_vout->p_sys->xvmc_xoverlay_type = X11OSD_COLORKEY; */
    }
#if 0
    else
    {
        p_vout->p_sys->xvmc_xoverlay_type = X11OSD_SHAPED;
    }
#endif
}

static void xxmc_xvmc_destroy_surfaces( vout_thread_t *p_vout )
{
    int i;
    xvmc_surface_handler_t *handler = NULL;

    handler = &p_vout->p_sys->xvmc_surf_handler;

    pthread_mutex_lock( &handler->mutex );
    for( i = 0; i < XVMC_MAX_SURFACES; ++i )
    {
        XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
        if( handler->surfValid[i] )
        {
            XvMCFlushSurface( p_vout->p_sys->p_display , handler->surfaces+i);
            XvMCSyncSurface( p_vout->p_sys->p_display, handler->surfaces+i );
            XvMCHideSurface( p_vout->p_sys->p_display, handler->surfaces+i );
            XvMCDestroySurface( p_vout->p_sys->p_display, handler->surfaces+i );
        }
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        handler->surfValid[i] = 0;
    }
    pthread_mutex_unlock( &handler->mutex );
}

static void xxmc_xvmc_destroy_subpictures( vout_thread_t *p_vout )
{
    int i;
    xvmc_surface_handler_t *handler = NULL;

    handler = &p_vout->p_sys->xvmc_surf_handler;

    pthread_mutex_lock( &handler->mutex );
    for( i = 0; i < XVMC_MAX_SUBPICTURES; ++i )
    {
        XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
        if( handler->subValid[i] )
        {
            XvMCFlushSubpicture( p_vout->p_sys->p_display , handler->subpictures+i);
            XvMCSyncSubpicture( p_vout->p_sys->p_display, handler->subpictures+i );
            XvMCDestroySubpicture( p_vout->p_sys->p_display, handler->subpictures+i );
        }
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        handler->subValid[i] = 0;
    }
    pthread_mutex_unlock( &handler->mutex );
}

static XvMCSurface *xxmc_xvmc_alloc_surface( vout_thread_t *p_vout,
        XvMCContext *context )
{
    xvmc_surface_handler_t *handler = NULL;
    int i;

    handler = &p_vout->p_sys->xvmc_surf_handler;

    pthread_mutex_lock( &handler->mutex );
    xxmc_xvmc_dump_surfaces( p_vout );
    for( i = 0; i < XVMC_MAX_SURFACES; ++i )
    {
        if( handler->surfValid[i] && !handler->surfInUse[i] )
        {
            handler->surfInUse[i] = 1;
            msg_Dbg( p_vout, "reusing surface %d", i );
            xxmc_xvmc_dump_surfaces( p_vout );
            pthread_mutex_unlock( &handler->mutex );
            return (handler->surfaces + i);
        }
    }
    for( i = 0; i < XVMC_MAX_SURFACES; ++i )
    {
        if( !handler->surfInUse[i] )
        {
            XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
            if( Success != XvMCCreateSurface( p_vout->p_sys->p_display,
                                              context,
                                              handler->surfaces + i) )
            {
                XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
                pthread_mutex_unlock( &handler->mutex );
                return NULL;
            }
            XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );

            msg_Dbg( p_vout, "created surface %d", i );
            handler->surfInUse[i] = 1;
            handler->surfValid[i] = 1;
            pthread_mutex_unlock( &handler->mutex );
            return (handler->surfaces + i);
        }
    }
    pthread_mutex_unlock( &handler->mutex );
    return NULL;
}

void xxmc_dispose_context( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->contextActive )
    {
        if( p_vout->p_sys->xvmc_accel &
            (VLC_XVMC_ACCEL_MOCOMP | VLC_XVMC_ACCEL_IDCT) )
        {
            xvmc_macroblocks_t *macroblocks = NULL;

            macroblocks = &p_vout->p_sys->macroblocks;
            XvMCDestroyMacroBlocks( p_vout->p_sys->p_display,
                                    &macroblocks->macro_blocks );
            XvMCDestroyBlocks( p_vout->p_sys->p_display,
                               &macroblocks->blocks );
        }

        msg_Dbg( p_vout, "freeing up XvMC surfaces and subpictures" );
        if( p_vout->p_sys->xvmc_palette )
            free( p_vout->p_sys->xvmc_palette );
        dispose_xx44_palette( &p_vout->p_sys->palette );
        xxmc_xvmc_destroy_subpictures( p_vout );
        xxmc_xvmc_destroy_surfaces( p_vout );

        msg_Dbg(p_vout, "freeing up XvMC Context.");
        XLockDisplay( p_vout->p_sys->p_display );
        if( p_vout->p_sys->subImage )
        {
            XFree( p_vout->p_sys->subImage );
            p_vout->p_sys->subImage = NULL;
        }
        p_vout->p_sys->subImage = NULL;
        XUnlockDisplay( p_vout->p_sys->p_display );
        XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
        XvMCDestroyContext( p_vout->p_sys->p_display,
                            &p_vout->p_sys->context );
        XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
        p_vout->p_sys->contextActive = 0;
        p_vout->p_sys->hwSubpictures = 0;
        p_vout->p_sys->xvmc_accel = 0;
    }
}

static int xxmc_find_context( vout_thread_t *p_vout, vlc_xxmc_t *xxmc,
        unsigned int width, unsigned int height )
{
    unsigned int i, k;
    bool found = false;
    xvmc_capabilities_t *curCap = NULL;
    unsigned int request_mpeg_flags, request_accel_flags;

    request_mpeg_flags = xxmc->mpeg;
    for( k = 0; k < NUM_ACCEL_PRIORITY; ++k )
    {
        request_accel_flags = xxmc->acceleration & accel_priority[k];
        if( !request_accel_flags )
            continue;

        curCap = p_vout->p_sys->xvmc_cap;
        for( i =0; i < p_vout->p_sys->xvmc_num_cap; ++i )
        {
            msg_Dbg( p_vout, "surface type %d, capabilities 0x%8x 0x%8x",
                             i,
                             curCap->mpeg_flags,
                             curCap->accel_flags );
            msg_Dbg( p_vout, "fequests: 0x%8x 0x%8x",
                             request_mpeg_flags,
                             request_accel_flags );
            if( ( (curCap->mpeg_flags & request_mpeg_flags) == request_mpeg_flags) &&
                  (curCap->accel_flags & request_accel_flags) &&
                  (width <= curCap->max_width) &&
                  (height <= curCap->max_height) )
            {
                found = true;
                break;
            }
            curCap++;
        }
        if( found )
        {
            p_vout->p_sys->xvmc_cur_cap = i;
            break;
        }
    }
    if( found )
    {
        p_vout->p_sys->xvmc_accel = request_accel_flags;
        p_vout->p_sys->unsigned_intra = (curCap->flags & XVMC_INTRA_UNSIGNED);
        return 1;
    }
    p_vout->p_sys->xvmc_accel = 0;
    return 0;
}

static int xxmc_create_context( vout_thread_t *p_vout,
        unsigned int width, unsigned int height )
{
    xvmc_capabilities_t *curCap = NULL;

    curCap = p_vout->p_sys->xvmc_cap + p_vout->p_sys->xvmc_cur_cap;

    msg_Dbg( p_vout, "creating new XvMC context %d", curCap->type_id );

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    if( Success == XvMCCreateContext( p_vout->p_sys->p_display,
                                      p_vout->p_sys->i_xvport,
                                      curCap->type_id,
                                      width,
                                      height,
                                      p_vout->p_sys->context_flags,
                                      &p_vout->p_sys->context ) )
    {
        p_vout->p_sys->xvmc_mpeg = curCap->mpeg_flags;
        p_vout->p_sys->xvmc_width = width;
        p_vout->p_sys->xvmc_height = height;
        p_vout->p_sys->contextActive = 1;
    }
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    return p_vout->p_sys->contextActive;
}

static void xvmc_flushsync(picture_t *picture)
{
    vout_thread_t *p_vout = picture->p_sys->p_vout;

    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    if( !xxmc_xvmc_surface_valid( p_vout, picture->p_sys->xvmc_surf ) )
    {
        msg_Dbg(p_vout, "xvmc_flushsync 1 : %d", picture->p_sys->xxmc_data.result );
        picture->p_sys->xxmc_data.result = 128;
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        return;
    }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    picture->p_sys->xxmc_data.result =
            XvMCFlushSurface( p_vout->p_sys->p_display,
                              picture->p_sys->xvmc_surf );
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
}

static void xvmc_flush(picture_t *picture)
{
    vout_thread_t *p_vout = picture->p_sys->p_vout;

    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    if ( !xxmc_xvmc_surface_valid( p_vout, picture->p_sys->xvmc_surf ) )
    {
        msg_Dbg(p_vout, "xvmc flush 1 : %d", picture->p_sys->xxmc_data.result );
        picture->p_sys->xxmc_data.result = 128;
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        return;
    }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    picture->p_sys->xxmc_data.result =
            XvMCFlushSurface( p_vout->p_sys->p_display,
                              picture->p_sys->xvmc_surf );
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
}

static int xxmc_frame_updates( vout_thread_t *p_vout, picture_t *picture )
{
    vlc_xxmc_t *xxmc = &picture->p_sys->xxmc_data;

    /*
     * If we have changed context since the surface was updated, xvmc_surf
     * is either NULL or invalid. If it is invalid. Set it to NULL.
     * Also if there are other users of this surface, deregister our use of
     * it and later try to allocate a new, fresh one.
     */

    if( picture->p_sys->xvmc_surf )
    {
        if( !xxmc_xvmc_surface_valid( p_vout, picture->p_sys->xvmc_surf ) )
        {
            xxmc_xvmc_free_surface( p_vout , picture->p_sys->xvmc_surf );
            picture->p_sys->xvmc_surf = NULL;
        }
    }
#if 0
    if( picture->p_sys->p_image )
    {
        memset( picture->p_sys->p_image->data, 0,
                picture->p_sys->p_image->width
                    * picture->p_sys->p_image->height );
    }
#endif
    /*
     * If it is NULL create a new surface.
     */
    if( !picture->p_sys->xvmc_surf )
    {
        picture->p_sys->xvmc_surf = xxmc_xvmc_alloc_surface( p_vout,
                                                    &p_vout->p_sys->context );
        if( !picture->p_sys->xvmc_surf )
        {
            msg_Err( p_vout, "accelerated surface allocation failed.\n"
                            " You are probably out of framebuffer memory.\n"
                            " Falling back to software decoding." );
            p_vout->p_sys->xvmc_accel = 0;
            xxmc_dispose_context( p_vout );
            return VLC_EGENERIC;
        }
    }
    xxmc->acceleration = p_vout->p_sys->xvmc_accel;

    xxmc->proc_xxmc_flush = xvmc_flush;
    xxmc->proc_xxmc_flushsync = xvmc_flushsync;
    xxmc->xvmc.proc_macro_block = NULL;
#if 0
    frame->vo_frame.proc_duplicate_frame_data = xxmc_duplicate_frame_data;
#endif
    xxmc->proc_xxmc_begin = xvmc_vld_frame;
    xxmc->proc_xxmc_slice = xvmc_vld_slice;
    return VLC_SUCCESS;
}

static int xxmc_xvmc_update_context( vout_thread_t *p_vout,
    picture_t *picture, uint32_t width, uint32_t height )
{
    vlc_xxmc_t *xxmc = &picture->p_sys->xxmc_data;

    /*
     * Are we at all capable of doing XvMC ?
     */
    if( p_vout->p_sys->xvmc_cap == 0 )
        return VLC_EGENERIC;

    msg_Dbg( p_vout, "new format: need to change XvMC context. "
                     "width: %d height: %d mpeg: %d acceleration: %d",
                     width, height,
                     xxmc->mpeg, xxmc->acceleration );

    if( picture->p_sys->xvmc_surf )
        xxmc_xvmc_free_surface( p_vout , picture->p_sys->xvmc_surf );
    picture->p_sys->xvmc_surf = NULL;

    xxmc_dispose_context( p_vout );

    if( xxmc_find_context( p_vout, xxmc, width, height ) )
    {
        xxmc_create_context( p_vout, width, height);
        xvmc_check_colorkey_properties( p_vout );
        xxmc_setup_subpictures(p_vout, width, height);
    }

    if( !p_vout->p_sys->contextActive )
    {
        msg_Dbg( p_vout, "using software decoding for this stream" );
        p_vout->p_sys->xvmc_accel = 0;
    }
    else
    {
        msg_Dbg(p_vout, "using hardware decoding for this stream." );
    }

    p_vout->p_sys->xvmc_mpeg = xxmc->mpeg;
    p_vout->p_sys->xvmc_width = width;
    p_vout->p_sys->xvmc_height = height;
    return p_vout->p_sys->contextActive;
}


void xxmc_do_update_frame( picture_t *picture, uint32_t width, uint32_t height,
        double ratio, int format, int flags)
{
    vout_thread_t *p_vout = picture->p_sys->p_vout;
    int indextime = 0;
    int status = 0;

    picture->p_sys->xxmc_data.decoded = 0;
    picture->p_sys->nb_display = 0;
    picture->b_force = 0;
    vlc_xxmc_t *xxmc = &picture->p_sys->xxmc_data;

    xvmc_context_writer_lock( &p_vout->p_sys->xvmc_lock);
    if( (p_vout->p_sys->last_accel_request != xxmc->acceleration) ||
        (p_vout->p_sys->xvmc_mpeg != xxmc->mpeg) ||
        (p_vout->p_sys->xvmc_width != width) ||
        (p_vout->p_sys->xvmc_height != height))
    {
        p_vout->p_sys->last_accel_request = xxmc->acceleration;
        xxmc_xvmc_update_context( p_vout, picture, width, height );
    }

    if( p_vout->p_sys->contextActive )
        xxmc_frame_updates( p_vout, picture );

    if( !p_vout->p_sys->contextActive )
    {
        xxmc->acceleration = 0;
        xxmc->xvmc.macroblocks = 0;
    }
    else
    {
        picture->format.i_chroma = format;
    }
    xvmc_context_writer_unlock( &p_vout->p_sys->xvmc_lock);

    XvMCGetSurfaceStatus( p_vout->p_sys->p_display,
                          picture->p_sys->xvmc_surf,
                          &status );
    /* Wait a little till frame is being displayed */
    while( status & XVMC_DISPLAYING )
    {
        /* msleep(1); */

        XvMCGetSurfaceStatus( p_vout->p_sys->p_display,
                              picture->p_sys->xvmc_surf,
                              &status );

        indextime++;
        if( indextime > 4 )
            break;
    }
}

#if 0
/* called xlocked */
static void dispose_ximage( vout_thread_t *p_vout, XShmSegmentInfo *shminfo,
                XvImage *myimage )
{
# ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->i_shm_opcode )
    {
        XShmDetach( p_vout->p_sys->p_display, shminfo );
        XFree( myimage );
        shmdt( shminfo->shmaddr );
        if( shminfo->shmid >= 0 )
        {
            shmctl( shminfo->shmid, IPC_RMID, 0 );
            shminfo->shmid = -1;
        }
    }
    else
#endif
    {
        if( myimage->data )
            free(myimage->data);
        XFree (myimage);
    }
}
#endif

void xvmc_vld_frame( picture_t *picture )
{
    picture_sys_t *p_sys  = picture->p_sys;
    vout_thread_t *p_vout = p_sys->p_vout;
    vlc_vld_frame_t *vft  = &(p_sys->xxmc_data.vld_frame);
    picture_t *ff         = (picture_t *) vft->forward_reference_picture;
    picture_t *bf         = (picture_t *) vft->backward_reference_picture;
    XvMCMpegControl ctl;
    XvMCSurface *fs=0, *bs=0;
    XvMCQMatrix qmx;

    ctl.BHMV_range = vft->mv_ranges[0][0];
    ctl.BVMV_range = vft->mv_ranges[0][1];
    ctl.FHMV_range = vft->mv_ranges[1][0];
    ctl.FVMV_range = vft->mv_ranges[1][1];
    ctl.picture_structure = vft->picture_structure;
    ctl.intra_dc_precision = vft->intra_dc_precision;
    ctl.picture_coding_type = vft->picture_coding_type;
    ctl.mpeg_coding = (vft->mpeg_coding == 0) ? XVMC_MPEG_1 : XVMC_MPEG_2;
    ctl.flags = 0;
    ctl.flags |= (vft->progressive_sequence) ? XVMC_PROGRESSIVE_SEQUENCE : 0;
    ctl.flags |= (vft->scan) ? XVMC_ALTERNATE_SCAN : XVMC_ZIG_ZAG_SCAN;
    ctl.flags |= (vft->pred_dct_frame) ?
                    XVMC_PRED_DCT_FRAME : XVMC_PRED_DCT_FIELD;
    ctl.flags |= (picture->b_top_field_first) ?
                    XVMC_TOP_FIELD_FIRST : XVMC_BOTTOM_FIELD_FIRST;
    ctl.flags |= (vft->concealment_motion_vectors) ?
                    XVMC_CONCEALMENT_MOTION_VECTORS : 0;
    ctl.flags |= (vft->q_scale_type) ? XVMC_Q_SCALE_TYPE : 0;
    ctl.flags |= (vft->intra_vlc_format) ? XVMC_INTRA_VLC_FORMAT : 0;
    ctl.flags |= (vft->second_field) ? XVMC_SECOND_FIELD : 0;

    if( ff )
        fs = ff->p_sys->xvmc_surf;
    if( bf )
        bs = bf->p_sys->xvmc_surf;

    /*
     * Below is for interlaced streams and second_field.
     */
    if( ctl.picture_coding_type == P_TYPE ) /* XVMC_P_PICTURE) */
        bs = picture->p_sys->xvmc_surf;

    if( (qmx.load_intra_quantiser_matrix = vft->load_intra_quantizer_matrix) )
    {
        memcpy( qmx.intra_quantiser_matrix, vft->intra_quantizer_matrix,
                sizeof(qmx.intra_quantiser_matrix) );
    }
    if( (qmx.load_non_intra_quantiser_matrix =
                vft->load_non_intra_quantizer_matrix) )
    {
        memcpy( qmx.non_intra_quantiser_matrix, vft->non_intra_quantizer_matrix,
               sizeof(qmx.non_intra_quantiser_matrix) );
    }
    qmx.load_chroma_intra_quantiser_matrix = 0;
    qmx.load_chroma_non_intra_quantiser_matrix = 0;
    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );

    if( !xxmc_xvmc_surface_valid( p_vout, picture->p_sys->xvmc_surf ) )
    {
        picture->p_sys->xxmc_data.result = 128;
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        return;
    }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    XvMCLoadQMatrix( p_vout->p_sys->p_display, &p_vout->p_sys->context, &qmx );
    do {
        picture->p_sys->xxmc_data.result =
                XvMCBeginSurface( p_vout->p_sys->p_display,
                                  &p_vout->p_sys->context,
                                  picture->p_sys->xvmc_surf,
                                  fs, bs, &ctl );
    } while( !picture->p_sys->xxmc_data.result );
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
}

void xvmc_vld_slice( picture_t *picture )
{
    picture_sys_t *p_sys  = picture->p_sys;
    vout_thread_t *p_vout = p_sys->p_vout;

    xvmc_context_reader_lock( &p_vout->p_sys->xvmc_lock );
    if( !xxmc_xvmc_surface_valid( p_vout, picture->p_sys->xvmc_surf ) )
    {
        picture->p_sys->xxmc_data.result = 128;
        xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
        msg_Err(p_vout, "vld slice error" );
        return;
    }

    XVMCLOCKDISPLAY( p_vout->p_sys->p_display );
    picture->p_sys->xxmc_data.result =
            XvMCPutSlice2( p_vout->p_sys->p_display,
                           &p_vout->p_sys->context,
                            (char *)picture->p_sys->xxmc_data.slice_data,
                            picture->p_sys->xxmc_data.slice_data_size,
                            picture->p_sys->xxmc_data.slice_code );

    if( picture->p_sys->xxmc_data.result != 0 )
        msg_Err( p_vout, "vlc slice error %d",
                 picture->p_sys->xxmc_data.result );
    /*
     * If CPU-saving mode is enabled, sleep after every xxmc->sleep slice. This will free
     * up the cpu while the decoder is working on the slice. The value of xxmc->sleep is calculated
     * so that the decoder thread sleeps at most 50% of the frame delay,
     * assuming a 2.6 kernel clock of 1000 Hz.
     */
    XVMCUNLOCKDISPLAY( p_vout->p_sys->p_display );
    xvmc_context_reader_unlock( &p_vout->p_sys->xvmc_lock );
#if 0
    if( p_vout->p_sys->cpu_save_enabled )
    {
        p_vout->p_sys->cpu_saver += 1.;
        if( p_vout->p_sys->cpu_saver >= picture->p_sys->xxmc_data.sleep )
        {
            usleep(1);
            p_vout->p_sys->cpu_saver -= picture->p_sys->xxmc_data.sleep;
        }
    }
#endif
}
