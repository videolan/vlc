/*****************************************************************************
 * video_output.c : video output thread
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppened video output thread.
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"
#include "video.h"
#include "video_output.h"
#include "video_text.h"
#include "video_spu.h"
#include "video_yuv.h"

#include "intf_msg.h"
#include "main.h"

#include "vdec_idct.h"
#include "video_decoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      BinaryLog         ( u32 i );
static void     MaskToShift       ( int *pi_left, int *pi_right, u32 i_mask );
static int      InitThread        ( vout_thread_t *p_vout );
static void     RunThread         ( vout_thread_t *p_vout );
static void     ErrorThread       ( vout_thread_t *p_vout );
static void     EndThread         ( vout_thread_t *p_vout );
static void     DestroyThread     ( vout_thread_t *p_vout, int i_status );
static void     Print             ( vout_thread_t *p_vout, int i_x, int i_y,
                                    int i_h_align, int i_v_align,
                                    unsigned char *psz_text );
static void     SetBufferArea     ( vout_thread_t *p_vout, int i_x, int i_y,
                                    int i_w, int i_h );
static void     SetBufferPicture  ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderPicture     ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderPictureInfo ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderSubPicture  ( vout_thread_t *p_vout,
                                    subpicture_t *p_subpic );
static void     RenderInterface   ( vout_thread_t *p_vout );
static int      RenderIdle        ( vout_thread_t *p_vout );
static int      RenderSplash      ( vout_thread_t *p_vout );
static void     RenderInfo        ( vout_thread_t *p_vout );
static int      Manage            ( vout_thread_t *p_vout );
static int      Align             ( vout_thread_t *p_vout, int *pi_x,
                                    int *pi_y, int i_width, int i_height,
                                    int i_h_align, int i_v_align );
static void     SetPalette        ( p_vout_thread_t p_vout, u16 *red,
                                    u16 *green, u16 *blue, u16 *transp );

/*****************************************************************************
 * vout_CreateThread: creates a new video output thread
 *****************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
vout_thread_t * vout_CreateThread   ( char *psz_display, int i_root_window,
                          int i_width, int i_height, int *pi_status,
                          int i_method, void *p_data )
{
    vout_thread_t * p_vout;                             /* thread descriptor */
    typedef void    ( vout_getplugin_t ) ( vout_thread_t * p_vout );
    int             i_status;                               /* thread status */
    int             i_index;               /* index for array initialization */
    int             i_best_index = 0, i_best_score = 0;

    /* Allocate descriptor */
    intf_DbgMsg("\n");
    p_vout = (vout_thread_t *) malloc( sizeof(vout_thread_t) );
    if( p_vout == NULL )
    {
        intf_ErrMsg( "error: %s\n", strerror(ENOMEM) );
        return( NULL );
    }

    p_vout->p_set_palette       = SetPalette;

    /* Get a suitable video plugin */
    for( i_index = 0 ; i_index < p_main->p_bank->i_plugin_count ; i_index++ )
    {
        /* If there's a plugin in p_info ... */
        if( p_main->p_bank->p_info[ i_index ] != NULL )
        {
            /* ... and if this plugin provides the functions we want ... */
            if( p_main->p_bank->p_info[ i_index ]->vout_GetPlugin != NULL )
            {
                /* ... and if this plugin has a good score ... */
                if( p_main->p_bank->p_info[ i_index ]->i_score > i_best_score )
                {
                    /* ... then take it */
                    i_best_score = p_main->p_bank->p_info[ i_index ]->i_score;
                    i_best_index = i_index;
                }
            }
        }
    }

    if( i_best_score == 0 )
    {
        free( p_vout );
        return( NULL );
    }

    /* Get the plugin functions */
    ( (vout_getplugin_t *)
      p_main->p_bank->p_info[ i_best_index ]->vout_GetPlugin )( p_vout );

    /* Initialize thread properties - thread id and locks will be initialized
     * later */
    p_vout->b_die               = 0;
    p_vout->b_error             = 0;
    p_vout->b_active            = 0;
    p_vout->pi_status           = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status          = THREAD_CREATE;

    /* Initialize some fields used by the system-dependant method - these
     * fields will probably be modified by the method, and are only
     * preferences */
    p_vout->i_changes             = 0;
    p_vout->i_width               = i_width;
    p_vout->i_height              = i_height;
    p_vout->i_bytes_per_line      = i_width * 2;
    p_vout->i_screen_depth        = 15;
    p_vout->i_bytes_per_pixel     = 2;
    p_vout->f_gamma               = VOUT_GAMMA;

    p_vout->b_grayscale           = main_GetIntVariable( VOUT_GRAYSCALE_VAR,
                                                       VOUT_GRAYSCALE_DEFAULT );
    p_vout->vdec_DecodeMacroblock = vdec_DecodeMacroblockC;
    p_vout->b_info                = 0;
    p_vout->b_interface           = 0;
    p_vout->b_scale               = 1;

    intf_DbgMsg( "wished configuration: %dx%d, %d/%d bpp (%d Bpl)\n",
                 p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                 p_vout->i_bytes_per_pixel * 8, p_vout->i_bytes_per_line );

    /* Initialize idle screen */
    p_vout->last_display_date   = 0;
    p_vout->last_idle_date      = 0;
    p_vout->init_display_date   = mdate();
    p_vout->render_time         = 10000;

    /* Initialize statistics fields */
    p_vout->c_fps_samples       = 0;

    /* Initialize buffer index */
    p_vout->i_buffer_index      = 0;

    /* Initialize pictures and subpictures - translation tables and functions
     * will be initialized later in InitThread */
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_type   =   EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_status =   FREE_PICTURE;
    }
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++)
    {
        p_vout->p_subpicture[i_index].i_type  = EMPTY_SUBPICTURE;
        p_vout->p_subpicture[i_index].i_status= FREE_SUBPICTURE;
    }
    p_vout->i_pictures = 0;

    /* Create and initialize system-dependant method - this function issues its
     * own error messages */
    if( p_vout->p_sys_create( p_vout, psz_display, i_root_window, p_data ) )
    {
        free( p_vout );
        return( NULL );
    }
    intf_DbgMsg( "actual configuration: %dx%d, %d/%d bpp (%d Bpl), "
                 "masks: 0x%x/0x%x/0x%x\n",
                 p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                 p_vout->i_bytes_per_pixel * 8, p_vout->i_bytes_per_line,
                 p_vout->i_red_mask, p_vout->i_green_mask,
                 p_vout->i_blue_mask );

    /* Calculate shifts from system-updated masks */
    MaskToShift( &p_vout->i_red_lshift, &p_vout->i_red_rshift,
                 p_vout->i_red_mask );
    MaskToShift( &p_vout->i_green_lshift, &p_vout->i_green_rshift,
                 p_vout->i_green_mask );
    MaskToShift( &p_vout->i_blue_lshift, &p_vout->i_blue_rshift,
                 p_vout->i_blue_mask );

    /* Set some useful colors */
    p_vout->i_white_pixel = RGB2PIXEL( p_vout, 255, 255, 255 );
    p_vout->i_black_pixel = RGB2PIXEL( p_vout, 0, 0, 0 );
    p_vout->i_gray_pixel  = RGB2PIXEL( p_vout, 128, 128, 128 );
    p_vout->i_blue_pixel  = RGB2PIXEL( p_vout, 0, 0, 50 );

    /* Load fonts - fonts must be initialized after the system method since
     * they may be dependant on screen depth and other thread properties */
    p_vout->p_default_font = vout_LoadFont( DATA_PATH "/" VOUT_DEFAULT_FONT );
    if( p_vout->p_default_font == NULL )
    {
        p_vout->p_default_font = vout_LoadFont( "share/" VOUT_DEFAULT_FONT );
    }
    if( p_vout->p_default_font == NULL )
    {
        intf_ErrMsg( "vout error: could not load default font\n" );
        p_vout->p_sys_destroy( p_vout );
        free( p_vout );
        return( NULL );
    }
    p_vout->p_large_font = vout_LoadFont( DATA_PATH "/" VOUT_LARGE_FONT );
    if( p_vout->p_large_font == NULL )
    {
        p_vout->p_large_font = vout_LoadFont( "share/" VOUT_LARGE_FONT );
    }
    if( p_vout->p_large_font == NULL )
    {
        intf_ErrMsg( "vout error: could not load large font\n" );
        vout_UnloadFont( p_vout->p_default_font );
        p_vout->p_sys_destroy( p_vout );
        free( p_vout );
        return( NULL );
    }

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->picture_lock );
    vlc_mutex_init( &p_vout->subpicture_lock );
    vlc_mutex_init( &p_vout->change_lock );
    vlc_mutex_lock( &p_vout->change_lock );
    if( vlc_thread_create( &p_vout->thread_id, "video output",
                           (void *) RunThread, (void *) p_vout) )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        vout_UnloadFont( p_vout->p_default_font );
        vout_UnloadFont( p_vout->p_large_font );
        p_vout->p_sys_destroy( p_vout );
        free( p_vout );
        return( NULL );
    }

    intf_Msg( "Video display initialized (%dx%d, %d/%d bpp)\n", p_vout->i_width,
              p_vout->i_height, p_vout->i_screen_depth,
              p_vout->i_bytes_per_pixel * 8 );

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            return( NULL );
        }
    }
    return( p_vout );
}

/*****************************************************************************
 * vout_DestroyThread: destroys a previously created thread
 *****************************************************************************
 * Destroy a terminated thread.
 * The function will request a destruction of the specified thread. If pi_error
 * is NULL, it will return once the thread is destroyed. Else, it will be
 * update using one of the THREAD_* constants.
 *****************************************************************************/
void vout_DestroyThread( vout_thread_t *p_vout, int *pi_status )
{
    int     i_status;                                       /* thread status */

    /* Set status */
    intf_DbgMsg("\n");
    p_vout->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status = THREAD_DESTROY;

    /* Request thread destruction */
    p_vout->b_die = 1;

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
    }
}

/*****************************************************************************
 * vout_DisplaySubPicture: display a subpicture unit
 *****************************************************************************
 * Remove the reservation flag of an subpicture, which will cause it to be ready
 * for display. The picture does not need to be locked, since it is ignored by
 * the output thread if is reserved.
 *****************************************************************************/
void  vout_DisplaySubPicture( vout_thread_t *p_vout, subpicture_t *p_subpic )
{
#ifdef DEBUG_VIDEO
    char        psz_begin_date[MSTRTIME_MAX_SIZE]; /* buffer for date string */
    char        psz_end_date[MSTRTIME_MAX_SIZE];   /* buffer for date string */
#endif

#ifdef DEBUG
    /* Check if status is valid */
    if( p_subpic->i_status != RESERVED_SUBPICTURE )
    {
        intf_DbgMsg("error: subpicture %p has invalid status %d\n", p_subpic,
                    p_subpic->i_status );
    }
#endif

    /* Remove reservation flag */
    p_subpic->i_status = READY_SUBPICTURE;

#ifdef DEBUG_VIDEO
    /* Send subpicture information */
    intf_DbgMsg("subpicture %p: type=%d, begin date=%s, end date=%s\n",
                p_subpic, p_subpic->i_type,
                mstrtime( psz_begin_date, p_subpic->begin_date ),
                mstrtime( psz_end_date, p_subpic->end_date ) );
#endif
}

/*****************************************************************************
 * vout_CreateSubPicture: allocate a subpicture in the video output heap.
 *****************************************************************************
 * This function create a reserved subpicture in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the spu data fields. It needs locking
 * since several pictures can be created by several producers threads.
 *****************************************************************************/
subpicture_t *vout_CreateSubPicture( vout_thread_t *p_vout, int i_type,
                                     int i_size )
{
    int                 i_subpic;                        /* subpicture index */
    subpicture_t *      p_free_subpic = NULL;       /* first free subpicture */
    subpicture_t *      p_destroyed_subpic = NULL; /* first destroyed subpic */

    /* Get lock */
    vlc_mutex_lock( &p_vout->subpicture_lock );

    /*
     * Look for an empty place
     */
    for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
    {
        if( p_vout->p_subpicture[i_subpic].i_status == DESTROYED_SUBPICTURE )
        {
            /* Subpicture is marked for destruction, but is still allocated */
            if( (p_vout->p_subpicture[i_subpic].i_type  == i_type)   &&
                (p_vout->p_subpicture[i_subpic].i_size  >= i_size) )
            {
                /* Memory size do match or is smaller : memory will not be
                 * reallocated, and function can end immediately - this is
                 * the best possible case, since no memory allocation needs
                 * to be done */
                p_vout->p_subpicture[i_subpic].i_status = RESERVED_SUBPICTURE;
#ifdef DEBUG_VIDEO
                intf_DbgMsg("subpicture %p (in destroyed subpicture slot)\n",
                            &p_vout->p_subpicture[i_subpic] );
#endif
                vlc_mutex_unlock( &p_vout->subpicture_lock );
                return( &p_vout->p_subpicture[i_subpic] );
            }
            else if( p_destroyed_subpic == NULL )
            {
                /* Memory size do not match, but subpicture index will be kept in
                 * case no other place are left */
                p_destroyed_subpic = &p_vout->p_subpicture[i_subpic];
            }
        }
        else if( (p_free_subpic == NULL) &&
                 (p_vout->p_subpicture[i_subpic].i_status == FREE_SUBPICTURE ))
        {
            /* Subpicture is empty and ready for allocation */
            p_free_subpic = &p_vout->p_subpicture[i_subpic];
        }
    }

    /* If no free subpictures are available, use a destroyed subpicture */
    if( (p_free_subpic == NULL) && (p_destroyed_subpic != NULL ) )
    {
        /* No free subpicture or matching destroyed subpictures have been
         * found, but a destroyed subpicture is still avalaible */
        free( p_destroyed_subpic->p_data );
        p_free_subpic = p_destroyed_subpic;
    }

    /*
     * Prepare subpicture
     */
    if( p_free_subpic != NULL )
    {
        /* Allocate memory */
        switch( i_type )
        {
        case TEXT_SUBPICTURE:                             /* text subpicture */
            p_free_subpic->p_data = malloc( i_size + 1 );
            break;
        case DVD_SUBPICTURE:                          /* DVD subpicture unit */
            p_free_subpic->p_data = malloc( i_size );
            break;
#ifdef DEBUG
        default:
            intf_DbgMsg("error: unknown subpicture type %d\n", i_type );
            p_free_subpic->p_data   =  NULL;
            break;
#endif
        }

        if( p_free_subpic->p_data != NULL )
        {
            /* Copy subpicture information, set some default values */
            p_free_subpic->i_type                      = i_type;
            p_free_subpic->i_status                    = RESERVED_SUBPICTURE;
            p_free_subpic->i_size                      = i_size;
            p_free_subpic->i_x                         = 0;
            p_free_subpic->i_y                         = 0;
            p_free_subpic->i_width                     = 0;
            p_free_subpic->i_height                    = 0;
            p_free_subpic->i_horizontal_align          = CENTER_RALIGN;
            p_free_subpic->i_vertical_align            = CENTER_RALIGN;
        }
        else
        {
            /* Memory allocation failed : set subpicture as empty */
            p_free_subpic->i_type   =  EMPTY_SUBPICTURE;
            p_free_subpic->i_status =  FREE_SUBPICTURE;
            p_free_subpic =            NULL;
            intf_ErrMsg("spu warning: %s\n", strerror( ENOMEM ) );
        }

#ifdef DEBUG_VIDEO
        intf_DbgMsg("subpicture %p (in free subpicture slot)\n", p_free_subpic );
#endif
        vlc_mutex_unlock( &p_vout->subpicture_lock );
        return( p_free_subpic );
    }

    /* No free or destroyed subpicture could be found */
    intf_DbgMsg( "warning: subpicture heap is full\n" );
    vlc_mutex_unlock( &p_vout->subpicture_lock );
    return( NULL );
}

/*****************************************************************************
 * vout_DestroySubPicture: remove a subpicture from the heap
 *****************************************************************************
 * This function frees a previously reserved subpicture.
 * It is meant to be used when the construction of a picture aborted.
 * This function does not need locking since reserved subpictures are ignored
 * by the output thread.
 *****************************************************************************/
void vout_DestroySubPicture( vout_thread_t *p_vout, subpicture_t *p_subpic )
{
#ifdef DEBUG
   /* Check if status is valid */
   if( p_subpic->i_status != RESERVED_SUBPICTURE )
   {
       intf_DbgMsg("error: subpicture %p has invalid status %d\n",
                   p_subpic, p_subpic->i_status );
   }
#endif

    p_subpic->i_status = DESTROYED_SUBPICTURE;

#ifdef DEBUG_VIDEO
    intf_DbgMsg("subpicture %p\n", p_subpic);
#endif
}

/*****************************************************************************
 * vout_DisplayPicture: display a picture
 *****************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DatePicture has been
 * called.
 *****************************************************************************/
void  vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DISP_PICTURE;
        break;
    case RESERVED_DATED_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );
        break;
#endif
    }

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);
#endif
    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_DatePicture: date a picture
 *****************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DisplayPicture has been
 * called.
 *****************************************************************************/
void  vout_DatePicture( vout_thread_t *p_vout, picture_t *p_pic, mtime_t date )
{
#ifdef DEBUG_VIDEO
    char        psz_date[MSTRTIME_MAX_SIZE];                         /* date */
#endif

    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->date = date;
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:
        p_pic->i_status = RESERVED_DATED_PICTURE;
        break;
    case RESERVED_DISP_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );
        break;
#endif
    }

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p, display date: %s\n", p_pic, mstrtime( psz_date, p_pic->date) );
#endif
    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_CreatePicture: allocate a picture in the video output heap.
 *****************************************************************************
 * This function create a reserved image in the video output heap.
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields. It needs locking
 * since several pictures can be created by several producers threads.
 *****************************************************************************/
picture_t *vout_CreatePicture( vout_thread_t *p_vout, int i_type,
                               int i_width, int i_height )
{
    int         i_picture;                                  /* picture index */
    int         i_chroma_width = 0;                          /* chroma width */
    picture_t * p_free_picture = NULL;                 /* first free picture */
    picture_t * p_destroyed_picture = NULL;       /* first destroyed picture */

    /* Get lock */
    vlc_mutex_lock( &p_vout->picture_lock );

    /*
     * Look for an empty place
     */
    for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
        if( p_vout->p_picture[i_picture].i_status == DESTROYED_PICTURE )
        {
            /* Picture is marked for destruction, but is still allocated - note
             * that if width and type are the same for two pictures, chroma_width
             * should also be the same */
            if( (p_vout->p_picture[i_picture].i_type           == i_type)   &&
                (p_vout->p_picture[i_picture].i_height         == i_height) &&
                (p_vout->p_picture[i_picture].i_width          == i_width) )
            {
                /* Memory size do match : memory will not be reallocated, and function
                 * can end immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
                p_vout->p_picture[i_picture].i_status = RESERVED_PICTURE;
                p_vout->i_pictures++;
#ifdef DEBUG_VIDEO
                intf_DbgMsg("picture %p (in destroyed picture slot)\n",
                            &p_vout->p_picture[i_picture] );
#endif
                vlc_mutex_unlock( &p_vout->picture_lock );
                return( &p_vout->p_picture[i_picture] );
            }
            else if( p_destroyed_picture == NULL )
            {
                /* Memory size do not match, but picture index will be kept in
                 * case no other place are left */
                p_destroyed_picture = &p_vout->p_picture[i_picture];
            }
        }
        else if( (p_free_picture == NULL) &&
                 (p_vout->p_picture[i_picture].i_status == FREE_PICTURE ))
        {
            /* Picture is empty and ready for allocation */
            p_free_picture = &p_vout->p_picture[i_picture];
        }
    }

    /* If no free picture is available, use a destroyed picture */
    if( (p_free_picture == NULL) && (p_destroyed_picture != NULL ) )
    {
        /* No free picture or matching destroyed picture has been found, but
         * a destroyed picture is still avalaible */
        free( p_destroyed_picture->p_data );
        p_free_picture = p_destroyed_picture;
    }

    /*
     * Prepare picture
     */
    if( p_free_picture != NULL )
    {
        /* Allocate memory */
        switch( i_type )
        {
        case YUV_420_PICTURE:        /* YUV 420: 1,1/4,1/4 samples per pixel */
            i_chroma_width = i_width / 2;
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 3 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*4/2;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*5/2;
            break;
        case YUV_422_PICTURE:        /* YUV 422: 1,1/2,1/2 samples per pixel */
            i_chroma_width = i_width / 2;
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 4 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*2;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*3;
            break;
        case YUV_444_PICTURE:            /* YUV 444: 1,1,1 samples per pixel */
            i_chroma_width = i_width;
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 3 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*2;
            break;
#ifdef DEBUG
        default:
            intf_DbgMsg("error: unknown picture type %d\n", i_type );
            p_free_picture->p_data   =  NULL;
            break;
#endif
        }

        if( p_free_picture->p_data != NULL )
        {
            /* Copy picture information, set some default values */
            p_free_picture->i_type                      = i_type;
            p_free_picture->i_status                    = RESERVED_PICTURE;
            p_free_picture->i_matrix_coefficients       = 1;
            p_free_picture->i_width                     = i_width;
            p_free_picture->i_height                    = i_height;
            p_free_picture->i_chroma_width              = i_chroma_width;
            p_free_picture->i_display_horizontal_offset = 0;
            p_free_picture->i_display_vertical_offset   = 0;
            p_free_picture->i_display_width             = i_width;
            p_free_picture->i_display_height            = i_height;
            p_free_picture->i_aspect_ratio              = AR_SQUARE_PICTURE;
            p_free_picture->i_refcount                  = 0;
            p_vout->i_pictures++;
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_free_picture->i_type   =  EMPTY_PICTURE;
            p_free_picture->i_status =  FREE_PICTURE;
            p_free_picture =            NULL;
            intf_ErrMsg( "vout warning: %s\n", strerror( ENOMEM ) );
        }

#ifdef DEBUG_VIDEO
        intf_DbgMsg("picture %p (in free picture slot)\n", p_free_picture );
#endif
        vlc_mutex_unlock( &p_vout->picture_lock );
        return( p_free_picture );
    }

    /* No free or destroyed picture could be found */
    intf_DbgMsg( "warning: picture heap is full\n" );
    vlc_mutex_unlock( &p_vout->picture_lock );
    return( NULL );
}

/*****************************************************************************
 * vout_DestroyPicture: remove a permanent or reserved picture from the heap
 *****************************************************************************
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 *****************************************************************************/
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
   vlc_mutex_lock( &p_vout->picture_lock );

#ifdef DEBUG
   /* Check if picture status is valid */
   if( (p_pic->i_status != RESERVED_PICTURE) &&
       (p_pic->i_status != RESERVED_DATED_PICTURE) &&
       (p_pic->i_status != RESERVED_DISP_PICTURE) )
   {
       intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );
   }
#endif

   p_pic->i_status = DESTROYED_PICTURE;
   p_vout->i_pictures--;

#ifdef DEBUG_VIDEO
   intf_DbgMsg("picture %p\n", p_pic);
#endif
   vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *****************************************************************************
 * This function increment the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 *****************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount++;

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p refcount=%d\n", p_pic, p_pic->i_refcount );
#endif

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_UnlinkPicture: decrement reference counter of a picture
 *****************************************************************************
 * This function decrement the reference counter of a picture in the video heap.
 *****************************************************************************/
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount--;

#ifdef DEBUG_VIDEO
    if( p_pic->i_refcount < 0 )
    {
        intf_DbgMsg("error: refcount < 0\n");
        p_pic->i_refcount = 0;
    }
#endif

    if( (p_pic->i_refcount == 0) && (p_pic->i_status == DISPLAYED_PICTURE) )
    {
        p_pic->i_status = DESTROYED_PICTURE;
        p_vout->i_pictures--;
    }

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p refcount=%d\n", p_pic, p_pic->i_refcount );
#endif

    vlc_mutex_unlock( &p_vout->picture_lock );
}

/*****************************************************************************
 * vout_SetBuffers: set buffers adresses
 *****************************************************************************
 * This function is called by system drivers to set buffers video memory
 * adresses.
 *****************************************************************************/
void vout_SetBuffers( vout_thread_t *p_vout, void *p_buf1, void *p_buf2 )
{
    /* No picture previously */
    p_vout->p_buffer[0].i_pic_x =         0;
    p_vout->p_buffer[0].i_pic_y =         0;
    p_vout->p_buffer[0].i_pic_width =     0;
    p_vout->p_buffer[0].i_pic_height =    0;
    p_vout->p_buffer[1].i_pic_x =         0;
    p_vout->p_buffer[1].i_pic_y =         0;
    p_vout->p_buffer[1].i_pic_width =     0;
    p_vout->p_buffer[1].i_pic_height =    0;

    /* The first area covers all the screen */
    p_vout->p_buffer[0].i_areas =                 1;
    p_vout->p_buffer[0].pi_area_begin[0] =        0;
    p_vout->p_buffer[0].pi_area_end[0] =          p_vout->i_height - 1;
    p_vout->p_buffer[1].i_areas =                 1;
    p_vout->p_buffer[1].pi_area_begin[0] =        0;
    p_vout->p_buffer[1].pi_area_end[0] =          p_vout->i_height - 1;

    /* Set adresses */
    p_vout->p_buffer[0].p_data = p_buf1;
    p_vout->p_buffer[1].p_data = p_buf2;
}

/*****************************************************************************
 * vout_Pixel2RGB: return red, green and blue from pixel value
 *****************************************************************************
 * Return color values, in 0-255 range, of the decomposition of a pixel. This
 * is a slow routine and should only be used for initialization phase.
 *****************************************************************************/
void vout_Pixel2RGB( vout_thread_t *p_vout, u32 i_pixel, int *pi_red, int *pi_green, int *pi_blue )
{
    *pi_red =   i_pixel & p_vout->i_red_mask;
    *pi_green = i_pixel & p_vout->i_green_mask;
    *pi_blue =  i_pixel & p_vout->i_blue_mask;
}

/* following functions are local */

/*****************************************************************************
 * BinaryLog: computes the base 2 log of a binary value
 *****************************************************************************
 * This functions is used by MaskToShift, to get a bit index from a binary
 * value.
 *****************************************************************************/
static int BinaryLog(u32 i)
{
    int i_log;

    i_log = 0;
    if (i & 0xffff0000)
    {
        i_log = 16;
    }
    if (i & 0xff00ff00)
    {
        i_log += 8;
    }
    if (i & 0xf0f0f0f0)
    {
        i_log += 4;
    }
    if (i & 0xcccccccc)
    {
        i_log += 2;
    }
    if (i & 0xaaaaaaaa)
    {
        i_log++;
    }
    if (i != ((u32)1 << i_log))
    {
        intf_DbgMsg("internal error: binary log overflow\n");
    }

    return( i_log );
}

/*****************************************************************************
 * MaskToShift: transform a color mask into right and left shifts
 *****************************************************************************
 * This function is used for obtaining color shifts from masks.
 *****************************************************************************/
static void MaskToShift( int *pi_left, int *pi_right, u32 i_mask )
{
    u32 i_low, i_high;                 /* lower hand higher bits of the mask */

    /* Get bits */
    i_low =  i_mask & (- i_mask);                   /* lower bit of the mask */
    i_high = i_mask + i_low;                       /* higher bit of the mask */

    /* Transform bits into an index */
    i_low =  BinaryLog (i_low);
    i_high = BinaryLog (i_high);

    /* Update pointers and return */
    *pi_left =   i_low;
    *pi_right = (8 - i_high + i_low);
}

/*****************************************************************************
 * InitThread: initialize video output thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( vout_thread_t *p_vout )
{
    /* Update status */
    intf_DbgMsg("\n");
    *p_vout->pi_status = THREAD_START;

   /* Initialize output method - this function issues its own error messages */
    if( p_vout->p_sys_init( p_vout ) )
    {
        return( 1 );
    }

    /* Initialize convertion tables and functions */
    if( vout_InitYUV( p_vout ) )
    {
        intf_ErrMsg("error: can't allocate YUV translation tables\n");
        return( 1 );
    }

    /* Mark thread as running and return */
    p_vout->b_active =          1;
    *p_vout->pi_status =        THREAD_READY;

    
    intf_DbgMsg("thread ready\n");
    return( 0 );
}

/*****************************************************************************
 * RunThread: video output thread
 *****************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *****************************************************************************/
static void RunThread( vout_thread_t *p_vout)
{
    int             i_index;                                /* index in heap */
    mtime_t         current_date;                            /* current date */
    mtime_t         display_date;                            /* display date */
    boolean_t       b_display;                               /* display flag */
    picture_t *     p_pic;                                /* picture pointer */
    subpicture_t *  p_subpic;                          /* subpicture pointer */

    /*
     * Initialize thread
     */
    p_vout->b_error = InitThread( p_vout );
    if( p_vout->b_error )
    {
        DestroyThread( p_vout, THREAD_ERROR );
        return;
    }
    intf_DbgMsg("\n");

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vout->b_die) && (!p_vout->b_error) )
    {
        /* Initialize loop variables */
        p_pic =         NULL;
        p_subpic =      NULL;
        display_date =  0;
        current_date =  mdate();

        /*
         * Find the picture to display - this operation does not need lock,
         * since only READY_PICTUREs are handled
         */
        for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++ )
        {
            if( (p_vout->p_picture[i_index].i_status == READY_PICTURE) &&
            ( (p_pic == NULL) ||
              (p_vout->p_picture[i_index].date < display_date) ) )
            {
                p_pic = &p_vout->p_picture[i_index];
                display_date = p_pic->date;
            }
        }

        if( p_pic )
        {
            /* Computes FPS rate */
            p_vout->p_fps_sample[ p_vout->c_fps_samples++ % VOUT_FPS_SAMPLES ] = display_date;

            if( display_date < current_date )
            {
                /* Picture is late: it will be destroyed and the thread
                 * will sleep and go to next picture */

                vlc_mutex_lock( &p_vout->picture_lock );
                if( p_pic->i_refcount )
                {
                    p_pic->i_status = DISPLAYED_PICTURE;
                }
                else
                {
                    p_pic->i_status = DESTROYED_PICTURE;
                    p_vout->i_pictures--;
                }
                intf_WarnMsg( 3,
                        "warning: late picture skipped (%p)\n", p_pic );
                vlc_mutex_unlock( &p_vout->picture_lock );

		continue;
            }
            else if( display_date > current_date + VOUT_DISPLAY_DELAY )
            {
                /* A picture is ready to be rendered, but its rendering date
                 * is far from the current one so the thread will perform an
                 * empty loop as if no picture were found. The picture state
                 * is unchanged */
                p_pic =         NULL;
                display_date =  0;
            }
        }
        /*
         * Find the subpictures to display - this operation does not need
         * lock, since only READY_SUBPICTURE are handled. If no picture
         * has been selected, display_date will depend on the subpicture.
         * We get an easily parsable chained list of subpictures which
         * ends with NULL since p_subpic was initialized to NULL.
         */
        for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
        {
            if( p_vout->p_subpicture[i_index].i_status == READY_SUBPICTURE )
            {
                p_vout->p_subpicture[i_index].p_next = p_subpic;
                p_subpic = &p_vout->p_subpicture[i_index];
            }
        }

        /*
         * Perform rendering, sleep and display rendered picture
         */
        if( p_pic )                        /* picture and perhaps subpicture */
        {
            b_display = p_vout->b_active;
            p_vout->last_display_date = display_date;

            if( b_display )
            {
                /* Set picture dimensions and clear buffer */
                SetBufferPicture( p_vout, p_pic );

                /* Render picture and information */
                RenderPicture( p_vout, p_pic );
                if( p_vout->b_info )
                {
                    RenderPictureInfo( p_vout, p_pic );
                    RenderInfo( p_vout );
                }
                if( p_subpic )
                {
                    RenderSubPicture( p_vout, p_subpic );
                }
            }

            /* Remove picture from heap */
            vlc_mutex_lock( &p_vout->picture_lock );
            if( p_pic->i_refcount )
            {
                p_pic->i_status = DISPLAYED_PICTURE;
            }
            else
            {
                p_pic->i_status = DESTROYED_PICTURE;
                p_vout->i_pictures--;
            }
            vlc_mutex_unlock( &p_vout->picture_lock );

            /* Render interface and subpicture */
            if( b_display && p_vout->b_interface )
            {
                RenderInterface( p_vout );
            }

        }
        else if( p_vout->b_active && p_vout->init_display_date == 0)        /* idle or interface screen alone */
        {
            if( p_vout->b_interface && 0 /* && XXX?? intf_change */ )
            {
                /* Interface has changed, so a new rendering is required - force
                 * it by setting last idle date to 0 */
                p_vout->last_idle_date = 0;
            }

            /* Render idle screen and update idle date, then render interface if
             * required */
            b_display = RenderIdle( p_vout );
            if( b_display )
            {
                p_vout->last_idle_date = current_date;
                if( p_vout->b_interface )
                {
                    RenderInterface( p_vout );
                }
            }

        }
        else
        {
            b_display = 0;
        }


        /*
         * chech for the current time and
         * display splash screen if everything is on time
         */
        if( p_vout->init_display_date > 0) 
        {
            if( p_vout->b_active && 
                    mdate()-p_vout->init_display_date < 5000000)
            {
                /* there is something to display ! */
                    b_display = 1;
                    RenderSplash( p_vout );
                
            } else {
                /* no splash screen ! */
                intf_ErrMsgImm("End of splash screen\n");  
                p_vout->init_display_date=0;
            }
        }
            


        
        /*
         * Sleep, wake up and display rendered picture
         */

        if( display_date != 0 )
        {
            /* Store render time */
            p_vout->render_time += mdate() - current_date;
            p_vout->render_time >>= 1;
        }

        /* Give back change lock */
        vlc_mutex_unlock( &p_vout->change_lock );

        /* Sleep a while or until a given date */
        if( display_date != 0 )
        {
            mwait( display_date - VOUT_MWAIT_TOLERANCE );
        }
        else
        {
            msleep( VOUT_IDLE_SLEEP );
        }

        /* On awakening, take back lock and send immediately picture to display,
         * then swap buffers */
        vlc_mutex_lock( &p_vout->change_lock );
#ifdef DEBUG_VIDEO
        intf_DbgMsg( "picture %p, subpicture %p in buffer %d, display=%d\n", p_pic, p_subpic,
                     p_vout->i_buffer_index, b_display /* && !(p_vout->i_changes & VOUT_NODISPLAY_CHANGE) */ );
#endif
        if( b_display /* && !(p_vout->i_changes & VOUT_NODISPLAY_CHANGE) */ )
        {
            p_vout->p_sys_display( p_vout );
#ifndef SYS_BEOS
            p_vout->i_buffer_index = ++p_vout->i_buffer_index & 1;
#endif
        }

        /*
         * Check events and manage thread
         */
        if( p_vout->p_sys_manage( p_vout ) | Manage( p_vout ) )
        {
            /* A fatal error occured, and the thread must terminate immediately,
             * without displaying anything - setting b_error to 1 cause the
             * immediate end of the main while() loop. */
            p_vout->b_error = 1;
        }
    }

    /*
     * Error loop - wait until the thread destruction is requested
     */
    if( p_vout->b_error )
    {
        ErrorThread( p_vout );
    }

    /* End of thread */
    EndThread( p_vout );
    DestroyThread( p_vout, THREAD_OVER );
    intf_DbgMsg( "thread end\n" );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( vout_thread_t *p_vout )
{
    /* Wait until a `die' order */
    intf_DbgMsg("\n");
    while( !p_vout->b_die )
    {
        /* Sleep a while */
        msleep( VOUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization. It frees all ressources allocated by InitThread.
 *****************************************************************************/
static void EndThread( vout_thread_t *p_vout )
{
    int     i_index;                                        /* index in heap */

    /* Store status */
    intf_DbgMsg("\n");
    *p_vout->pi_status = THREAD_END;

    /* Destroy all remaining pictures and subpictures */
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++ )
    {
        if( p_vout->p_picture[i_index].i_status != FREE_PICTURE )
        {
            free( p_vout->p_picture[i_index].p_data );
        }
    }
    for( i_index = 0; i_index < VOUT_MAX_SUBPICTURES; i_index++ )
    {
        if( p_vout->p_subpicture[i_index].i_status != FREE_SUBPICTURE )
        {
            free( p_vout->p_subpicture[i_index].p_data );
        }
    }

    /* Destroy translation tables */
    vout_EndYUV( p_vout );
    p_vout->p_sys_end( p_vout );
}

/*****************************************************************************
 * DestroyThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends. It frees all ressources
 * allocated by CreateThread. Status is available at this stage.
 *****************************************************************************/
static void DestroyThread( vout_thread_t *p_vout, int i_status )
{
    int *pi_status;                                         /* status adress */

    /* Store status adress */
    intf_DbgMsg("\n");
    pi_status = p_vout->pi_status;

    /* Destroy thread structures allocated by Create and InitThread */
    vout_UnloadFont( p_vout->p_default_font );
    vout_UnloadFont( p_vout->p_large_font );
    p_vout->p_sys_destroy( p_vout );

    /* Free structure */
    free( p_vout );
    *pi_status = i_status;
}

/*****************************************************************************
 * Print: print simple text on a picture
 *****************************************************************************
 * This function will print a simple text on the picture. It is designed to
 * print debugging or general information.
 *****************************************************************************/
void Print( vout_thread_t *p_vout, int i_x, int i_y, int i_h_align, int i_v_align, unsigned char *psz_text )
{
    int                 i_text_height;                  /* total text height */
    int                 i_text_width;                    /* total text width */

    /* Update upper left coordinates according to alignment */
    vout_TextSize( p_vout->p_default_font, 0, psz_text, &i_text_width, &i_text_height );
    if( !Align( p_vout, &i_x, &i_y, i_text_width, i_text_height, i_h_align, i_v_align ) )
    {
        /* Set area and print text */
        SetBufferArea( p_vout, i_x, i_y, i_text_width, i_text_height );
        vout_Print( p_vout->p_default_font, p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                    i_y * p_vout->i_bytes_per_line + i_x * p_vout->i_bytes_per_pixel,
                    p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                    p_vout->i_white_pixel, 0, 0,
                    0, psz_text, 100 );
    }
}

/*****************************************************************************
 * SetBufferArea: activate an area in current buffer
 *****************************************************************************
 * This function is called when something is rendered on the current buffer.
 * It set the area as active and prepare it to be cleared on next rendering.
 * Pay attention to the fact that in this functions, i_h is in fact the end y
 * coordinate of the new area.
 *****************************************************************************/
static void SetBufferArea( vout_thread_t *p_vout, int i_x, int i_y, int i_w, int i_h )
{
    vout_buffer_t *     p_buffer;                          /* current buffer */
    int                 i_area_begin, i_area_end; /* area vertical extension */
    int                 i_area, i_area_copy;                   /* area index */
    int                 i_area_shift;            /* shift distance for areas */

    /* Choose buffer and modify h to end of area position */
    p_buffer =  &p_vout->p_buffer[ p_vout->i_buffer_index ];
    i_h +=      i_y - 1;

    /*
     * Remove part of the area which is inside the picture - this is done
     * by calling again SetBufferArea with the correct areas dimensions.
     */
    if( (i_x >= p_buffer->i_pic_x) && (i_x + i_w <= p_buffer->i_pic_x + p_buffer->i_pic_width) )
    {
        i_area_begin =  p_buffer->i_pic_y;
        i_area_end =    i_area_begin + p_buffer->i_pic_height - 1;

        if( ((i_y >= i_area_begin) && (i_y <= i_area_end)) ||
            ((i_h >= i_area_begin) && (i_h <= i_area_end)) ||
            ((i_y <  i_area_begin) && (i_h > i_area_end)) )
        {
            /* Keep the stripe above the picture, if any */
            if( i_y < i_area_begin )
            {
                SetBufferArea( p_vout, i_x, i_y, i_w, i_area_begin - i_y );
            }
            /* Keep the stripe below the picture, if any */
            if( i_h > i_area_end )
            {
                SetBufferArea( p_vout, i_x, i_area_end, i_w, i_h - i_area_end );
            }
            return;
        }
    }

    /* Skip some extensions until interesting areas */
    for( i_area = 0;
         (i_area < p_buffer->i_areas) &&
             (p_buffer->pi_area_end[i_area] + 1 <= i_y);
         i_area++ )
    {
        ;
    }

    if( i_area == p_buffer->i_areas )
    {
        /* New area is below all existing ones: just add it at the end of the
         * array, if possible - otherwise, append it to the last one */
        if( i_area < VOUT_MAX_AREAS )
        {
            p_buffer->pi_area_begin[i_area] = i_y;
            p_buffer->pi_area_end[i_area] = i_h;
            p_buffer->i_areas++;
        }
        else
        {
#ifdef DEBUG_VIDEO
            intf_DbgMsg("area overflow\n");
#endif
            p_buffer->pi_area_end[VOUT_MAX_AREAS - 1] = i_h;
        }
    }
    else
    {
        i_area_begin =  p_buffer->pi_area_begin[i_area];
        i_area_end =    p_buffer->pi_area_end[i_area];

        if( i_y < i_area_begin )
        {
            if( i_h >= i_area_begin - 1 )
            {
                /* Extend area above */
                p_buffer->pi_area_begin[i_area] = i_y;
            }
            else
            {
                /* Create a new area above : merge last area if overflow, then
                 * move all old areas down */
                if( p_buffer->i_areas == VOUT_MAX_AREAS )
                {
#ifdef DEBUG_VIDEO
                    intf_DbgMsg("areas overflow\n");
#endif
                    p_buffer->pi_area_end[VOUT_MAX_AREAS - 2] = p_buffer->pi_area_end[VOUT_MAX_AREAS - 1];
                }
                else
                {
                    p_buffer->i_areas++;
                }
                for( i_area_copy = p_buffer->i_areas - 1; i_area_copy > i_area; i_area_copy-- )
                {
                    p_buffer->pi_area_begin[i_area_copy] = p_buffer->pi_area_begin[i_area_copy - 1];
                    p_buffer->pi_area_end[i_area_copy] =   p_buffer->pi_area_end[i_area_copy - 1];
                }
                p_buffer->pi_area_begin[i_area] = i_y;
                p_buffer->pi_area_end[i_area] = i_h;
                return;
            }
        }
        if( i_h > i_area_end )
        {
            /* Find further areas which can be merged with the new one */
            for( i_area_copy = i_area + 1;
                 (i_area_copy < p_buffer->i_areas) &&
                     (p_buffer->pi_area_begin[i_area] <= i_h);
                 i_area_copy++ )
            {
                ;
            }
            i_area_copy--;

            if( i_area_copy != i_area )
            {
                /* Merge with last possible areas */
                p_buffer->pi_area_end[i_area] = MAX( i_h, p_buffer->pi_area_end[i_area_copy] );

                /* Shift lower areas upward */
                i_area_shift = i_area_copy - i_area;
                p_buffer->i_areas -= i_area_shift;
                for( i_area_copy = i_area + 1; i_area_copy < p_buffer->i_areas; i_area_copy++ )
                {
                    p_buffer->pi_area_begin[i_area_copy] = p_buffer->pi_area_begin[i_area_copy + i_area_shift];
                    p_buffer->pi_area_end[i_area_copy] =   p_buffer->pi_area_end[i_area_copy + i_area_shift];
                }
            }
            else
            {
                /* Extend area below */
                p_buffer->pi_area_end[i_area] = i_h;
            }
        }
    }
}

/*****************************************************************************
 * SetBufferPicture: clear buffer and set picture area
 *****************************************************************************
 * This function is called before any rendering. It clears the current
 * rendering buffer and set the new picture area. If the picture pointer is
 * NULL, then no picture area is defined. Floating operations are avoided since
 * some MMX calculations may follow.
 *****************************************************************************/
static void SetBufferPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_buffer_t *     p_buffer;                          /* current buffer */
    int                 i_pic_x, i_pic_y;                /* picture position */
    int                 i_pic_width, i_pic_height;     /* picture dimensions */
    int                 i_old_pic_y, i_old_pic_height;   /* old picture area */
    int                 i_vout_width, i_vout_height;   /* display dimensions */
    int                 i_area;                                /* area index */
    int                 i_data_index;                     /* area data index */
    int                 i_data_size;   /* area data size, in 256 bytes blocs */
    u64 *               p_data;                   /* area data, for clearing */
    byte_t *            p_data8;           /* area data, for clearing (slow) */

    /* Choose buffer and set display dimensions */
    p_buffer =          &p_vout->p_buffer[ p_vout->i_buffer_index ];
    i_vout_width =      p_vout->i_width;
    i_vout_height =     p_vout->i_height;

    /*
     * Computes new picture size
     */
    if( p_pic != NULL )
    {
        /* Try horizontal scaling first - width must be a mutiple of 16 */
        i_pic_width = (( p_vout->b_scale || (p_pic->i_width > i_vout_width)) ?
                       i_vout_width : p_pic->i_width) & ~0xf;
        switch( p_pic->i_aspect_ratio )
        {
        case AR_3_4_PICTURE:
            i_pic_height = i_pic_width * 3 / 4;
            break;
        case AR_16_9_PICTURE:
            i_pic_height = i_pic_width * 9 / 16;
            break;
        case AR_221_1_PICTURE:
            i_pic_height = i_pic_width * 100 / 221;
            break;
        case AR_SQUARE_PICTURE:
        default:
            i_pic_height = p_pic->i_height * i_pic_width / p_pic->i_width;
            break;
        }

        /* If picture dimensions using horizontal scaling are too large, use
         * vertical scaling. Since width must be a multiple of 16, height is
         * adjusted again after. */
        if( i_pic_height > i_vout_height )
        {
            i_pic_height = ( p_vout->b_scale || (p_pic->i_height > i_vout_height)) ?
                i_vout_height : p_pic->i_height;
            switch( p_pic->i_aspect_ratio )
            {
            case AR_3_4_PICTURE:
                i_pic_width = (i_pic_height * 4 / 3) & ~0xf;
                i_pic_height = i_pic_width * 3 / 4;
                break;
            case AR_16_9_PICTURE:
                i_pic_width = (i_pic_height * 16 / 9) & ~0xf;
                i_pic_height = i_pic_width * 9 / 16;
                break;
            case AR_221_1_PICTURE:
                i_pic_width = (i_pic_height * 221 / 100) & ~0xf;
                i_pic_height = i_pic_width * 100 / 221;
                break;
            case AR_SQUARE_PICTURE:
            default:
                i_pic_width = (p_pic->i_width * i_pic_height / p_pic->i_height) & ~0xf;
                i_pic_height = p_pic->i_height * i_pic_width / p_pic->i_width;
                break;
            }
        }

        /* Set picture position */
        i_pic_x = (p_vout->i_width - i_pic_width) / 2;
        i_pic_y = (p_vout->i_height - i_pic_height) / 2;
    }
    else
    {
        /* No picture: size is 0 */
        i_pic_x =       0;
        i_pic_y =       0;
        i_pic_width =   0;
        i_pic_height =  0;
    }

    /*
     * Set new picture size - if it is smaller than the previous one, clear
     * around it. Since picture are centered, only their size is tested.
     */
    if( (p_buffer->i_pic_width > i_pic_width) || (p_buffer->i_pic_height > i_pic_height) )
    {
        i_old_pic_y =            p_buffer->i_pic_y;
        i_old_pic_height =       p_buffer->i_pic_height;
        p_buffer->i_pic_x =      i_pic_x;
        p_buffer->i_pic_y =      i_pic_y;
        p_buffer->i_pic_width =  i_pic_width;
        p_buffer->i_pic_height = i_pic_height;
        SetBufferArea( p_vout, 0, i_old_pic_y, p_vout->i_width, i_old_pic_height );
    }
    else
    {
        p_buffer->i_pic_x =      i_pic_x;
        p_buffer->i_pic_y =      i_pic_y;
        p_buffer->i_pic_width =  i_pic_width;
        p_buffer->i_pic_height = i_pic_height;
    }

    /*
     * Clear areas
     */
    for( i_area = 0; i_area < p_buffer->i_areas; i_area++ )
    {
#ifdef DEBUG_VIDEO
        intf_DbgMsg("clearing picture %p area in buffer %d: %d-%d\n", p_pic,
                    p_vout->i_buffer_index, p_buffer->pi_area_begin[i_area], p_buffer->pi_area_end[i_area] );
#endif
        i_data_size = (p_buffer->pi_area_end[i_area] - p_buffer->pi_area_begin[i_area] + 1) * p_vout->i_bytes_per_line;
        p_data = (u64*) (p_buffer->p_data + p_vout->i_bytes_per_line * p_buffer->pi_area_begin[i_area]);
        for( i_data_index = i_data_size / 256; i_data_index-- ; )
        {
            /* Clear 256 bytes block */
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
            *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;  *p_data++ = 0;
        }
        for( i_data_index = (i_data_size % 256) / 16; i_data_index--; )
        {
            /* Clear remaining 16 bytes blocks */
            *p_data++ = 0;  *p_data++ = 0;
        }
        p_data8 = (byte_t *)p_data;
        for( i_data_index = i_data_size % 16; i_data_index--; )
        {
            /* Clear remaining bytes */
            *p_data8++ = 0;
        }
    }

    /*
     * Clear areas array
     */
    p_buffer->i_areas = 0;
}

/*****************************************************************************
 * RenderPicture: render a picture
 *****************************************************************************
 * This function converts a picture from a video heap to a pixel-encoded image
 * and copies it to the current rendering buffer. No lock is required, since
 * the * rendered picture has been determined as existant, and will only be
 * destroyed by the vout thread later.
 *****************************************************************************/
static void RenderPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG_VIDEO
    char                psz_date[MSTRTIME_MAX_SIZE];         /* picture date */
    mtime_t             render_time;               /* picture rendering time */
#endif
    vout_buffer_t *     p_buffer;                        /* rendering buffer */
    byte_t *            p_pic_data;                /* convertion destination */

    /* Get and set rendering information */
    p_buffer =          &p_vout->p_buffer[ p_vout->i_buffer_index ];
    p_pic_data =        p_buffer->p_data +
                        p_buffer->i_pic_x * p_vout->i_bytes_per_pixel +
                        p_buffer->i_pic_y * p_vout->i_bytes_per_line;
#ifdef DEBUG_VIDEO
    render_time = mdate();
#endif

    /*
     * Choose appropriate rendering function and render picture
     */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:
        p_vout->yuv.p_Convert420( p_vout, p_pic_data,
                                  p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                  p_pic->i_width, p_pic->i_height,
                                  p_buffer->i_pic_width, p_buffer->i_pic_height,
                                  p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel,
                                  p_pic->i_matrix_coefficients );
        break;
    case YUV_422_PICTURE:
        p_vout->yuv.p_Convert422( p_vout, p_pic_data,
                                  p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                  p_pic->i_width, p_pic->i_height,
                                  p_buffer->i_pic_width, p_buffer->i_pic_height,
                                  p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel,
                                  p_pic->i_matrix_coefficients );
        break;
    case YUV_444_PICTURE:
        p_vout->yuv.p_Convert444( p_vout, p_pic_data,
                                  p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                  p_pic->i_width, p_pic->i_height,
                                  p_buffer->i_pic_width, p_buffer->i_pic_height,
                                  p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel,
                                  p_pic->i_matrix_coefficients );
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("error: unknown picture type %d\n", p_pic->i_type );
        break;
#endif
    }

#ifdef DEBUG_VIDEO
    /* Print picture date and rendering time */
    intf_DbgMsg("picture %p rendered in buffer %d (%ld us), display date: %s\n", p_pic,
                p_vout->i_buffer_index, (long) (mdate() - render_time),
                mstrtime( psz_date, p_pic->date ));
#endif
}

/*****************************************************************************
 * RenderPictureInfo: print additionnal information on a picture
 *****************************************************************************
 * This function will print information such as fps and other picture
 * dependant information.
 *****************************************************************************/
static void RenderPictureInfo( vout_thread_t *p_vout, picture_t *p_pic )
{
    char        psz_buffer[256];                            /* string buffer */

    /*
     * Print FPS rate in upper right corner
     */
    if( p_vout->c_fps_samples > VOUT_FPS_SAMPLES )
    {
        long i_fps = VOUT_FPS_SAMPLES * 1000000 * 10 /
                           ( p_vout->p_fps_sample[ (p_vout->c_fps_samples - 1)
                                                   % VOUT_FPS_SAMPLES ] -
                             p_vout->p_fps_sample[ p_vout->c_fps_samples
                                                   % VOUT_FPS_SAMPLES ] );
        sprintf( psz_buffer, "%li.%i fps", i_fps / 10, (int)i_fps % 10 );
        Print( p_vout, 0, 0, RIGHT_RALIGN, TOP_RALIGN, psz_buffer );
    }

    /*
     * Print frames count and loop time in upper left corner
     */
    sprintf( psz_buffer, "%ld frames, render: %ldus",
             (long) p_vout->c_fps_samples, (long) p_vout->render_time );
    Print( p_vout, 0, 0, LEFT_RALIGN, TOP_RALIGN, psz_buffer );

#ifdef DEBUG
    /*
     * Print picture information in lower right corner
     */
    sprintf( psz_buffer, "%s picture %dx%d (%dx%d%+d%+d %s) -> %dx%d+%d+%d",
             (p_pic->i_type == YUV_420_PICTURE) ? "4:2:0" :
             ((p_pic->i_type == YUV_422_PICTURE) ? "4:2:2" :
              ((p_pic->i_type == YUV_444_PICTURE) ? "4:4:4" : "ukn-type")),
             p_pic->i_width, p_pic->i_height,
             p_pic->i_display_width, p_pic->i_display_height,
             p_pic->i_display_horizontal_offset, p_pic->i_display_vertical_offset,
             (p_pic->i_aspect_ratio == AR_SQUARE_PICTURE) ? "sq" :
             ((p_pic->i_aspect_ratio == AR_3_4_PICTURE) ? "4:3" :
              ((p_pic->i_aspect_ratio == AR_16_9_PICTURE) ? "16:9" :
               ((p_pic->i_aspect_ratio == AR_221_1_PICTURE) ? "2.21:1" : "ukn-ar" ))),
             p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_width,
             p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_height,
             p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_x,
             p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_y );
    Print( p_vout, 0, 0, RIGHT_RALIGN, BOTTOM_RALIGN, psz_buffer );
#endif
}

/*****************************************************************************
 * RenderSplash: render splash picture
 *****************************************************************************
 * This function will print something on the screen. It will return 0 if
 * nothing has been rendered, or 1 if something has been changed on the screen.
 * Note that if you absolutely want something to be printed, you will have
 * to force it by setting the last idle date to 0.
 * Unlike other rendering functions, this one calls the SetBufferPicture
 * function when needed.
 *****************************************************************************/
int RenderSplash( vout_thread_t *p_vout )
{
    int         i_x = 0, i_y = 0;                           /* text position */
    int         i_width, i_height;                              /* text size */
    char *psz_text =    "VideoLAN Client (" VERSION ")";  /* text to display */

    memset( p_vout->p_buffer[ p_vout->i_buffer_index ].p_data,
                  p_vout->i_bytes_per_line * p_vout->i_height, 12);

 //   SetBufferPicture( p_vout, NULL );
    vout_TextSize( p_vout->p_large_font, WIDE_TEXT | OUTLINED_TEXT, psz_text,
                   &i_width, &i_height );
    if( !Align( p_vout, &i_x, &i_y, i_width, i_height, CENTER_RALIGN, CENTER_RALIGN ) )
    {
        vout_Print( p_vout->p_large_font,
                    p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                    i_x * p_vout->i_bytes_per_pixel + (i_y - 16 ) * p_vout->i_bytes_per_line,
                    p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                    p_vout->i_white_pixel, p_vout->i_gray_pixel, 0,
                    WIDE_TEXT | OUTLINED_TEXT, psz_text, 100);
        SetBufferArea( p_vout, i_x, i_y, i_width, i_height);
    }
    return( 1 );
}


/*****************************************************************************
 * RenderIdle: render idle picture
 *****************************************************************************
 * This function will print something on the screen. It will return 0 if
 * nothing has been rendered, or 1 if something has been changed on the screen.
 * Note that if you absolutely want something to be printed, you will have
 * to force it by setting the last idle date to 0.
 * Unlike other rendering functions, this one calls the SetBufferPicture
 * function when needed.
 *****************************************************************************/
int RenderIdle( vout_thread_t *p_vout )
{
    int         i_x = 0, i_y = 0;                           /* text position */
    int         i_width, i_height;                              /* text size */
    mtime_t     current_date;                                /* current date */
    int         i_amount = 0;                             /*  amount to draw */
    char *psz_text =    "Waiting for stream";            /* text to display */
    char *psz_wtext =   "[................]";


    memset( p_vout->p_buffer[ p_vout->i_buffer_index ].p_data,
                    p_vout->i_bytes_per_line * p_vout->i_height, 12);


    current_date = mdate();
    if( (current_date - p_vout->last_display_date) > VOUT_IDLE_DELAY 
//            && (current_date - p_vout->last_idle_date) > VOUT_IDLE_DELAY 
    )
    {
        SetBufferPicture( p_vout, NULL );
        vout_TextSize( p_vout->p_large_font, WIDE_TEXT | OUTLINED_TEXT, psz_text,
                       &i_width, &i_height );
        if( !Align( p_vout, &i_x, &i_y, i_width, i_height * 2, CENTER_RALIGN, CENTER_RALIGN ) )
        {
            i_amount = (int) ((current_date - p_vout->last_display_date ) / 5000LL);            
            vout_Print( p_vout->p_large_font,
                        p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                        i_x * p_vout->i_bytes_per_pixel + i_y * p_vout->i_bytes_per_line,
                        p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                        p_vout->i_white_pixel, p_vout->i_gray_pixel, 0,
                        WIDE_TEXT | OUTLINED_TEXT, psz_text,  (i_amount / 3 ) %110);

            vout_Print( p_vout->p_large_font,
                    p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                    i_x * p_vout->i_bytes_per_pixel + (i_y + 16) * p_vout->i_bytes_per_line,
                    p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                    p_vout->i_white_pixel, p_vout->i_gray_pixel, 0,
                    WIDE_TEXT | OUTLINED_TEXT, psz_wtext,  (i_amount/5)%110 );
            

            SetBufferArea( p_vout, i_x, i_y, i_width, i_height * 2 );
        }
        return( 1 );
    }
    return( 0 );
}

/*****************************************************************************
 * RenderInfo: render additionnal information
 *****************************************************************************
 * This function renders information which do not depend on the current
 * picture rendered.
 *****************************************************************************/
static void RenderInfo( vout_thread_t *p_vout )
{
#ifdef DEBUG
    char        psz_buffer[256];                            /* string buffer */
    int         i_ready_pic = 0;                           /* ready pictures */
    int         i_reserved_pic = 0;                     /* reserved pictures */
    int         i_picture;                                  /* picture index */
#endif

#ifdef DEBUG
    /*
     * Print thread state in lower left corner
     */
    for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
        switch( p_vout->p_picture[i_picture].i_status )
        {
        case RESERVED_PICTURE:
        case RESERVED_DATED_PICTURE:
        case RESERVED_DISP_PICTURE:
            i_reserved_pic++;
            break;
        case READY_PICTURE:
            i_ready_pic++;
            break;
        }
    }
    sprintf( psz_buffer, "pic: %d (%d/%d)/%d",
             p_vout->i_pictures, i_reserved_pic, i_ready_pic, VOUT_MAX_PICTURES );
    Print( p_vout, 0, 0, LEFT_RALIGN, BOTTOM_RALIGN, psz_buffer );
#endif
}

/*****************************************************************************
 * RenderSubPicture: render a subpicture
 *****************************************************************************
 * This function renders a sub picture unit.
 *****************************************************************************/
static void RenderSubPicture( vout_thread_t *p_vout, subpicture_t *p_subpic )
{
    p_vout_font_t       p_font;                                 /* text font */
    int                 i_width, i_height;          /* subpicture dimensions */

    while( p_subpic != NULL )
    {
        switch( p_subpic->i_type )
        {
        case DVD_SUBPICTURE:                          /* DVD subpicture unit */
            /* test if the picture really has to be displayed */
            if( mdate() < p_subpic->begin_date )
            {
                /* not yet, see you later */
                break;
            }
            if( mdate() > p_subpic->end_date )
            {
                /* too late, destroying the subpic */
                vout_DestroySubPicture( p_vout, p_subpic );
                break;
            }
            vout_RenderSPU( &p_vout->p_buffer[ p_vout->i_buffer_index ],
                            p_subpic, p_vout->i_bytes_per_pixel,
                            p_vout->i_bytes_per_line );
            break;
        case TEXT_SUBPICTURE:                            /* single line text */
            /* Select default font if not specified */
            p_font = p_subpic->type.text.p_font;
            if( p_font == NULL )
            {
                p_font = p_vout->p_default_font;
            }

            /* Compute text size (width and height fields are ignored)
             * and print it */
            vout_TextSize( p_font, p_subpic->type.text.i_style,
                           p_subpic->p_data, &i_width, &i_height );
            if( !Align( p_vout, &p_subpic->i_x, &p_subpic->i_y,
                        i_width, i_height, p_subpic->i_horizontal_align,
                        p_subpic->i_vertical_align ) )
            {
                vout_Print( p_font,
                            p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                            p_subpic->i_x * p_vout->i_bytes_per_pixel +
                            p_subpic->i_y * p_vout->i_bytes_per_line,
                            p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                            p_subpic->type.text.i_char_color,
                            p_subpic->type.text.i_border_color,
                            p_subpic->type.text.i_bg_color,
                            p_subpic->type.text.i_style, p_subpic->p_data, 100 );
                SetBufferArea( p_vout, p_subpic->i_x, p_subpic->i_y,
                               i_width, i_height );
            }
            break;

#ifdef DEBUG
        default:
            intf_DbgMsg( "error: unknown subpicture %p type %d\n",
                         p_subpic, p_subpic->i_type );
#endif
        }

        p_subpic = p_subpic->p_next;
    }
}

/*****************************************************************************
 * RenderInterface: render the interface
 *****************************************************************************
 * This function renders the interface, if any.
 *****************************************************************************/
static void RenderInterface( vout_thread_t *p_vout )
{
    int         i_height, i_text_height;            /* total and text height */
    int         i_width_1, i_width_2;                          /* text width */
    int         i_byte;                                        /* byte index */
    const char *psz_text_1 = "[1-9] Channel   [i]nfo   [c]olor     [g/G]amma";
    const char *psz_text_2 = "[+/-] Volume    [m]ute   [s]caling   [Q]uit";

    /* Get text size */
    vout_TextSize( p_vout->p_large_font, OUTLINED_TEXT, psz_text_1, &i_width_1, &i_height );
    vout_TextSize( p_vout->p_large_font, OUTLINED_TEXT, psz_text_2, &i_width_2, &i_text_height );
    i_height += i_text_height;

    /* Render background */
    for( i_byte = (p_vout->i_height - i_height) * p_vout->i_bytes_per_line;
         i_byte < p_vout->i_height * p_vout->i_bytes_per_line;
         i_byte++ )
    {
        /* XXX?? noooo ! */
        p_vout->p_buffer[ p_vout->i_buffer_index ].p_data[ i_byte ] = p_vout->i_blue_pixel;
    }

    /* Render text, if not larger than screen */
    if( i_width_1 < p_vout->i_width )
    {
        vout_Print( p_vout->p_large_font, p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                    (p_vout->i_height - i_height) * p_vout->i_bytes_per_line,
                    p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                    p_vout->i_white_pixel, p_vout->i_black_pixel, 0,
                    OUTLINED_TEXT, psz_text_1, 100 );
    }
    if( i_width_2 < p_vout->i_width )
    {
        vout_Print( p_vout->p_large_font, p_vout->p_buffer[ p_vout->i_buffer_index ].p_data +
                    (p_vout->i_height - i_height + i_text_height) * p_vout->i_bytes_per_line,
                    p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                    p_vout->i_white_pixel, p_vout->i_black_pixel, 0,
                    OUTLINED_TEXT, psz_text_2, 100 );
    }

    /* Activate modified area */
    SetBufferArea( p_vout, 0, p_vout->i_height - i_height, p_vout->i_width, i_height );
}

/*****************************************************************************
 * Manage: manage thread
 *****************************************************************************
 * This function will handle changes in thread configuration.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
#ifdef DEBUG_VIDEO
    if( p_vout->i_changes )
    {
        intf_DbgMsg("changes: 0x%x (no display: 0x%x)\n", p_vout->i_changes,
                    0 /* p_vout->i_changes & VOUT_NODISPLAY_CHANGE */ );
    }
#endif

    /* On gamma or grayscale change, rebuild tables */
    if( p_vout->i_changes & (VOUT_GAMMA_CHANGE | VOUT_GRAYSCALE_CHANGE |
                             VOUT_YUV_CHANGE) )
    {
        /* Change vdec_DecodeMacroblock when switching between BW and C */
        if( !p_vout->b_grayscale ) 
        {
            p_vout->vdec_DecodeMacroblock = vdec_DecodeMacroblockC;
        }
        else if( p_vout->b_grayscale)
        {
            p_vout->vdec_DecodeMacroblock = vdec_DecodeMacroblockBW;
        }  
        if( vout_ResetYUV( p_vout ) )
        {
            intf_ErrMsg("error: can't rebuild convertion tables\n");
            return( 1 );
        }
    }

    /* Clear changes flags which does not need management or have been
     * handled */
    p_vout->i_changes &= ~(VOUT_GAMMA_CHANGE | VOUT_GRAYSCALE_CHANGE |
                           VOUT_YUV_CHANGE   | VOUT_INFO_CHANGE |
                           VOUT_INTF_CHANGE  | VOUT_SCALE_CHANGE );

    /* Detect unauthorized changes */
    if( p_vout->i_changes )
    {
        /* Some changes were not acknowledged by p_vout->p_sys_manage or this
         * function, it means they should not be authorized */
        intf_ErrMsg( "error: unauthorized changes in the video output thread\n" );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * Align: align a subpicture in the screen
 *****************************************************************************
 * This function is used for rendering text or subpictures. It returns non 0
 * it the final aera is not fully included in display area. Return coordinates
 * are absolute.
 *****************************************************************************/
static int Align( vout_thread_t *p_vout, int *pi_x, int *pi_y,
                   int i_width, int i_height, int i_h_align, int i_v_align )
{
    /* Align horizontally */
    switch( i_h_align )
    {
    case CENTER_ALIGN:
        *pi_x -= i_width / 2;
        break;
    case CENTER_RALIGN:
        *pi_x += (p_vout->i_width - i_width) / 2;
        break;
    case RIGHT_ALIGN:
        *pi_x -= i_width;
        break;
    case RIGHT_RALIGN:
        *pi_x += p_vout->i_width - i_width;
        break;
    }

    /* Align vertically */
    switch( i_v_align )
    {
    case CENTER_ALIGN:
        *pi_y -= i_height / 2;
        break;
    case CENTER_RALIGN:
        *pi_y += (p_vout->i_height - i_height) / 2;
        break;
    case BOTTOM_ALIGN:
        *pi_y -= i_height;
        break;
    case BOTTOM_RALIGN:
        *pi_y += p_vout->i_height - i_height;
        break;
    case SUBTITLE_RALIGN:
        *pi_y += (p_vout->i_height + p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_y +
                  p_vout->p_buffer[ p_vout->i_buffer_index ].i_pic_height - i_height) / 2;
        break;
    }

    /* Return non 0 if clipping failed */
    return( (*pi_x < 0) || (*pi_y < 0) ||
            (*pi_x + i_width > p_vout->i_width) || (*pi_y + i_height > p_vout->i_height) );
}

/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function is just a prototype that does nothing. Architectures that
 * support palette allocation should override it.
 *****************************************************************************/
static void     SetPalette        ( p_vout_thread_t p_vout, u16 *red,
                                    u16 *green, u16 *blue, u16 *transp )
{
    intf_ErrMsg( "SetPalette: method does not support palette changing\n" );
}

