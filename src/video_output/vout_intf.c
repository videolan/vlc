/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "vlc_video.h"
#include "video_output.h"
#include "vlc_image.h"
#include "vlc_spu.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void InitWindowSize( vout_thread_t *, unsigned *, unsigned * );

/* Object variables callbacks */
static int ZoomCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int CropCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int AspectCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int OnTopCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int FullscreenCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int SnapshotCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * vout_RequestWindow: Create/Get a video window if possible.
 *****************************************************************************
 * This function looks for the main interface and tries to request
 * a new video window. If it fails then the vout will still need to create the
 * window by itself.
 *****************************************************************************/
void *vout_RequestWindow( vout_thread_t *p_vout,
                          int *pi_x_hint, int *pi_y_hint,
                          unsigned int *pi_width_hint,
                          unsigned int *pi_height_hint )
{
    intf_thread_t *p_intf = NULL;
    vlc_list_t *p_list;
    void *p_window;
    vlc_value_t val;
    int i;

    /* Small kludge */
    if( !var_Type( p_vout, "aspect-ratio" ) ) vout_IntfInit( p_vout );

    /* Get requested coordinates */
    var_Get( p_vout, "video-x", &val );
    *pi_x_hint = val.i_int ;
    var_Get( p_vout, "video-y", &val );
    *pi_y_hint = val.i_int;

    *pi_width_hint = p_vout->i_window_width;
    *pi_height_hint = p_vout->i_window_height;

    /* Check whether someone provided us with a window ID */
    var_Get( p_vout->p_vlc, "drawable", &val );
    if( val.i_int ) return (void *)val.i_int;

    /* Find if the main interface supports embedding */
    p_list = vlc_list_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list ) return NULL;

    for( i = 0; i < p_list->i_count; i++ )
    {
        p_intf = (intf_thread_t *)p_list->p_values[i].p_object;
        if( p_intf->b_block && p_intf->pf_request_window ) break;
        p_intf = NULL;
    }

    if( !p_intf )
    {
        vlc_list_release( p_list );
        return NULL;
    }

    vlc_object_yield( p_intf );
    vlc_list_release( p_list );

    p_window = p_intf->pf_request_window( p_intf, p_vout, pi_x_hint, pi_y_hint,
                                          pi_width_hint, pi_height_hint );

    if( !p_window ) vlc_object_release( p_intf );
    else p_vout->p_parent_intf = p_intf;

    return p_window;
}

void vout_ReleaseWindow( vout_thread_t *p_vout, void *p_window )
{
    intf_thread_t *p_intf = p_vout->p_parent_intf;

    if( !p_intf ) return;

    vlc_mutex_lock( &p_intf->object_lock );
    if( p_intf->b_dead )
    {
        vlc_mutex_unlock( &p_intf->object_lock );
        return;
    }

    if( !p_intf->pf_release_window )
    {
        msg_Err( p_vout, "no pf_release_window");
        vlc_mutex_unlock( &p_intf->object_lock );
        vlc_object_release( p_intf );
        return;
    }

    p_intf->pf_release_window( p_intf, p_window );

    p_vout->p_parent_intf = NULL;
    vlc_mutex_unlock( &p_intf->object_lock );
    vlc_object_release( p_intf );
}

int vout_ControlWindow( vout_thread_t *p_vout, void *p_window,
                        int i_query, va_list args )
{
    intf_thread_t *p_intf = p_vout->p_parent_intf;
    int i_ret;

    if( !p_intf ) return VLC_EGENERIC;

    vlc_mutex_lock( &p_intf->object_lock );
    if( p_intf->b_dead )
    {
        vlc_mutex_unlock( &p_intf->object_lock );
        return VLC_EGENERIC;
    }

    if( !p_intf->pf_control_window )
    {
        msg_Err( p_vout, "no pf_control_window");
        vlc_mutex_unlock( &p_intf->object_lock );
        return VLC_EGENERIC;
    }

    i_ret = p_intf->pf_control_window( p_intf, p_window, i_query, args );
    vlc_mutex_unlock( &p_intf->object_lock );
    return i_ret;
}

/*****************************************************************************
 * vout_IntfInit: called during the vout creation to initialise misc things.
 *****************************************************************************/
void vout_IntfInit( vout_thread_t *p_vout )
{
    vlc_value_t val, text, old_val;
    vlc_bool_t b_force_par = VLC_FALSE;

    /* Create a few object variables we'll need later on */
    var_Create( p_vout, "snapshot-path", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-format", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_vout, "align", &val );
    p_vout->i_alignment = val.i_int;

    var_Create( p_vout, "video-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "video-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Zoom object var */
    var_Create( p_vout, "zoom", VLC_VAR_FLOAT | VLC_VAR_ISCOMMAND |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Zoom");
    var_Change( p_vout, "zoom", VLC_VAR_SETTEXT, &text, NULL );

    var_Get( p_vout, "zoom", &old_val );
    if( old_val.f_float == 0.25 ||
        old_val.f_float == 0.5 ||
        old_val.f_float == 1 ||
        old_val.f_float == 2 )
    {
        var_Change( p_vout, "zoom", VLC_VAR_DELCHOICE, &old_val, NULL );
    }

    val.f_float = 0.25; text.psz_string = _("1:4 Quarter");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 0.5; text.psz_string = _("1:2 Half");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 1; text.psz_string = _("1:1 Original");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 2; text.psz_string = _("2:1 Double");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );

    var_Set( p_vout, "zoom", old_val );

    var_AddCallback( p_vout, "zoom", ZoomCallback, NULL );

    /* Crop object var */
    var_Create( p_vout, "crop", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Crop");
    var_Change( p_vout, "crop", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = "";
    var_Change( p_vout, "crop", VLC_VAR_DELCHOICE, &val, 0 );
    val.psz_string = ""; text.psz_string = _("Default");
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "001:1"; text.psz_string = "1:1";
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "004:3"; text.psz_string = "4:3";
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "16:10"; text.psz_string = "16:10";
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "16:9"; text.psz_string = "16:9";
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "221:100"; text.psz_string = "221:100";
    var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_vout, "crop", CropCallback, NULL );
    var_Get( p_vout, "crop", &old_val );
    if( old_val.psz_string && *old_val.psz_string )
        var_Change( p_vout, "crop", VLC_VAR_TRIGGER_CALLBACKS, 0, 0 );
    if( old_val.psz_string ) free( old_val.psz_string );

    /* Monitor pixel aspect-ratio */
    var_Create( p_vout, "monitor-par", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_vout, "monitor-par", &val );
    if( val.psz_string && *val.psz_string )
    {
        char *psz_parser = strchr( val.psz_string, ':' );
        unsigned int i_aspect_num = 0, i_aspect_den = 0;
        float i_aspect = 0;
        if( psz_parser )
        {
            i_aspect_num = strtol( val.psz_string, 0, 0 );
            i_aspect_den = strtol( ++psz_parser, 0, 0 );
        }
        else
        {
            i_aspect = atof( val.psz_string );
            vlc_ureduce( &i_aspect_num, &i_aspect_den,
                         i_aspect *VOUT_ASPECT_FACTOR, VOUT_ASPECT_FACTOR, 0 );
        }
        if( !i_aspect_num || !i_aspect_den ) i_aspect_num = i_aspect_den = 1;

        p_vout->i_par_num = i_aspect_num;
        p_vout->i_par_den = i_aspect_den;

        vlc_ureduce( &p_vout->i_par_num, &p_vout->i_par_den,
                     p_vout->i_par_num, p_vout->i_par_den, 0 );

        msg_Dbg( p_vout, "monitor pixel aspect-ratio overriding: %i:%i",
                 p_vout->i_par_num, p_vout->i_par_den );
        b_force_par = VLC_TRUE;
    }
    if( val.psz_string ) free( val.psz_string );

    /* Aspect-ratio object var */
    var_Create( p_vout, "aspect-ratio", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Aspect-ratio");
    var_Change( p_vout, "aspect-ratio", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = "";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_DELCHOICE, &val, 0 );
    val.psz_string = ""; text.psz_string = _("Default");
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "001:1"; text.psz_string = "1:1";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "004:3"; text.psz_string = "4:3";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "16:10"; text.psz_string = "16:10";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "16:9"; text.psz_string = "16:9";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    val.psz_string = "221:100"; text.psz_string = "221:100";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );

    var_AddCallback( p_vout, "aspect-ratio", AspectCallback, NULL );
    var_Get( p_vout, "aspect-ratio", &old_val );
    if( (old_val.psz_string && *old_val.psz_string) || b_force_par )
        var_Change( p_vout, "aspect-ratio", VLC_VAR_TRIGGER_CALLBACKS, 0, 0 );
    if( old_val.psz_string ) free( old_val.psz_string );

    /* Initialize the dimensions of the video window */
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );

    /* Add a variable to indicate if the window should be on top of others */
    var_Create( p_vout, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    text.psz_string = _("Always on top");
    var_Change( p_vout, "video-on-top", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-on-top", OnTopCallback, NULL );

    /* Add a variable to indicate whether we want window decoration or not */
    var_Create( p_vout, "video-deco", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Add a fullscreen variable */
    var_Create( p_vout, "fullscreen", VLC_VAR_BOOL );
    text.psz_string = _("Fullscreen");
    var_Change( p_vout, "fullscreen", VLC_VAR_SETTEXT, &text, NULL );
    var_Change( p_vout, "fullscreen", VLC_VAR_INHERITVALUE, &val, NULL );
    if( val.b_bool )
    {
        /* user requested fullscreen */
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    }
    var_AddCallback( p_vout, "fullscreen", FullscreenCallback, NULL );

    /* Add a snapshot variable */
    var_Create( p_vout, "video-snapshot", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Snapshot");
    var_Change( p_vout, "video-snapshot", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-snapshot", SnapshotCallback, NULL );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_INTEGER );

    var_Create( p_vout, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
}

/*****************************************************************************
 * vout_Snapshot: generates a snapshot.
 *****************************************************************************/
int vout_Snapshot( vout_thread_t *p_vout, picture_t *p_pic )
{
    image_handler_t *p_image = image_HandlerCreate( p_vout );
    video_format_t fmt_in = {0}, fmt_out = {0};
    char *psz_filename;
    subpicture_t *p_subpic;
    picture_t *p_pif;
    vlc_value_t val, format;
    int i_ret;

    var_Get( p_vout, "snapshot-path", &val );
    if( val.psz_string && !*val.psz_string )
    {
        free( val.psz_string );
        val.psz_string = 0;
    }

#if defined(SYS_DARWIN) || defined(SYS_BEOS)
    if( !val.psz_string && p_vout->p_vlc->psz_homedir )
    {
        asprintf( &val.psz_string, "%s/Desktop",
                  p_vout->p_vlc->psz_homedir );
    }

#elif defined(WIN32) && !defined(UNDER_CE)
    if( !val.psz_string && p_vout->p_vlc->psz_homedir )
    {
        /* Get the My Pictures folder path */

        char *p_mypicturesdir = NULL;
        typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                                   LPSTR );
        #ifndef CSIDL_FLAG_CREATE
        #   define CSIDL_FLAG_CREATE 0x8000
        #endif
        #ifndef CSIDL_MYPICTURES
        #   define CSIDL_MYPICTURES 0x27
        #endif
        #ifndef SHGFP_TYPE_CURRENT
        #   define SHGFP_TYPE_CURRENT 0
        #endif

        HINSTANCE shfolder_dll;
        SHGETFOLDERPATH SHGetFolderPath ;

        /* load the shfolder dll to retrieve SHGetFolderPath */
        if( ( shfolder_dll = LoadLibrary( _T("SHFolder.dll") ) ) != NULL )
        {
            SHGetFolderPath = (void *)GetProcAddress( shfolder_dll,
                                                      _T("SHGetFolderPathA") );
            if( SHGetFolderPath != NULL )
            {
                p_mypicturesdir = (char *)malloc( MAX_PATH );
                if( p_mypicturesdir ) 
                {

                    if( S_OK != SHGetFolderPath( NULL,
                                        CSIDL_MYPICTURES | CSIDL_FLAG_CREATE,
                                        NULL, SHGFP_TYPE_CURRENT,
                                        p_mypicturesdir ) )
                    {
                        free( p_mypicturesdir );
                        p_mypicturesdir = NULL;
                    }
                }
            }
            FreeLibrary( shfolder_dll );
        }

        if( p_mypicturesdir == NULL )
        {
            asprintf( &val.psz_string, "%s/" CONFIG_DIR,
                      p_vout->p_vlc->psz_homedir );
        }
        else
        {
            asprintf( &val.psz_string, p_mypicturesdir );
            free( p_mypicturesdir );
        }
    }

#else
    if( !val.psz_string && p_vout->p_vlc->psz_homedir )
    {
        asprintf( &val.psz_string, "%s/" CONFIG_DIR,
                  p_vout->p_vlc->psz_homedir );
    }
#endif

    if( !val.psz_string )
    {
        msg_Err( p_vout, "no directory specified for snapshots" );
        return VLC_EGENERIC;
    }
    var_Get( p_vout, "snapshot-format", &format );
    if( !format.psz_string || !*format.psz_string )
    {
        if( format.psz_string ) free( format.psz_string );
        format.psz_string = strdup( "png" );
    }

    asprintf( &psz_filename, "%s/vlcsnap-%u.%s", val.psz_string,
              (unsigned int)(p_pic->date / 100000) & 0xFFFFFF,
              format.psz_string );
    free( val.psz_string );
    free( format.psz_string );

    /* Save the snapshot */
    fmt_in = p_vout->fmt_in;
    fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
    i_ret = image_WriteUrl( p_image, p_pic, &fmt_in, &fmt_out, psz_filename );
    if( i_ret != VLC_SUCCESS )
    {
        msg_Err( p_vout, "could not create snapshot %s", psz_filename );
        free( psz_filename );
        image_HandlerDelete( p_image );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "snapshot taken (%s)", psz_filename );
    free( psz_filename );

    /* Inject a subpicture with the snapshot */
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_pif = image_Convert( p_image, p_pic, &fmt_in, &fmt_out );
    image_HandlerDelete( p_image );
    if( !p_pif ) return VLC_EGENERIC;

    p_subpic = spu_CreateSubpicture( p_vout->p_spu );
    if( p_subpic == NULL )
    {
         p_pif->pf_release( p_pif );
         return VLC_EGENERIC;
    }

    p_subpic->i_channel = 0;
    p_subpic->i_start = mdate();
    p_subpic->i_stop = mdate() + 4000000;
    p_subpic->b_ephemer = VLC_TRUE;
    p_subpic->b_fade = VLC_TRUE;
    p_subpic->i_original_picture_width = p_vout->render.i_width * 4;
    p_subpic->i_original_picture_height = p_vout->render.i_height * 4;

    p_subpic->p_region = spu_CreateRegion( p_vout->p_spu, &fmt_out );
    vout_CopyPicture( p_image->p_parent, &p_subpic->p_region->picture, p_pif );
    p_pif->pf_release( p_pif );

    spu_DisplaySubpicture( p_vout->p_spu, p_subpic );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vout_ControlDefault: default methods for video output control.
 *****************************************************************************/
int vout_vaControlDefault( vout_thread_t *p_vout, int i_query, va_list args )
{
    switch( i_query )
    {
    case VOUT_REPARENT:
    case VOUT_CLOSE:
        if( p_vout->p_parent_intf )
        {
            vlc_object_release( p_vout->p_parent_intf );
            p_vout->p_parent_intf = NULL;
        }
        return VLC_SUCCESS;
        break;

    case VOUT_SNAPSHOT:
        p_vout->b_snapshot = VLC_TRUE;
        return VLC_SUCCESS;
        break;

    default:
        msg_Dbg( p_vout, "control query not supported" );
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * InitWindowSize: find the initial dimensions the video window should have.
 *****************************************************************************
 * This function will check the "width", "height" and "zoom" config options and
 * will calculate the size that the video window should have.
 *****************************************************************************/
static void InitWindowSize( vout_thread_t *p_vout, unsigned *pi_width,
                            unsigned *pi_height )
{
    vlc_value_t val;
    int i_width, i_height;
    uint64_t ll_zoom;

#define FP_FACTOR 1000                             /* our fixed point factor */

    var_Get( p_vout, "width", &val );
    i_width = val.i_int;
    var_Get( p_vout, "height", &val );
    i_height = val.i_int;
    var_Get( p_vout, "zoom", &val );
    ll_zoom = (uint64_t)( FP_FACTOR * val.f_float );

    if( i_width > 0 && i_height > 0)
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
        goto initwsize_end;
    }
    else if( i_width > 0 )
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( p_vout->fmt_in.i_visible_height * ll_zoom *
            p_vout->fmt_in.i_sar_den * i_width / p_vout->fmt_in.i_sar_num /
            FP_FACTOR / p_vout->fmt_in.i_visible_width );
        goto initwsize_end;
    }
    else if( i_height > 0 )
    {
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
        *pi_width = (int)( p_vout->fmt_in.i_visible_width * ll_zoom *
            p_vout->fmt_in.i_sar_num * i_height / p_vout->fmt_in.i_sar_den /
            FP_FACTOR / p_vout->fmt_in.i_visible_height );
        goto initwsize_end;
    }

    if( p_vout->fmt_in.i_sar_num >= p_vout->fmt_in.i_sar_den )
    {
        *pi_width = (int)( p_vout->fmt_in.i_visible_width * ll_zoom *
            p_vout->fmt_in.i_sar_num / p_vout->fmt_in.i_sar_den / FP_FACTOR );
        *pi_height = (int)( p_vout->fmt_in.i_visible_height * ll_zoom 
            / FP_FACTOR );
    }
    else
    {
        *pi_width = (int)( p_vout->fmt_in.i_visible_width * ll_zoom 
            / FP_FACTOR );
        *pi_height = (int)( p_vout->fmt_in.i_visible_height * ll_zoom *
            p_vout->fmt_in.i_sar_den / p_vout->fmt_in.i_sar_num / FP_FACTOR );
    }

initwsize_end:
    msg_Dbg( p_vout, "window size: %dx%d", p_vout->i_window_width, 
             p_vout->i_window_height );

#undef FP_FACTOR
}

/*****************************************************************************
 * Object variables callbacks
 *****************************************************************************/
static int ZoomCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );
    vout_Control( p_vout, VOUT_SET_SIZE, 0, 0 );
    return VLC_SUCCESS;
}

static int CropCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int64_t i_aspect_num, i_aspect_den;
    unsigned int i_width, i_height;

    char *psz_end, *psz_parser = strchr( newval.psz_string, ':' );

    /* Restore defaults */
    p_vout->fmt_in.i_x_offset = p_vout->fmt_render.i_x_offset;
    p_vout->fmt_in.i_visible_width = p_vout->fmt_render.i_visible_width;
    p_vout->fmt_in.i_y_offset = p_vout->fmt_render.i_y_offset;
    p_vout->fmt_in.i_visible_height = p_vout->fmt_render.i_visible_height;

    if( !psz_parser ) goto crop_end;

    i_aspect_num = strtol( newval.psz_string, &psz_end, 0 );
    if( psz_end == newval.psz_string || !i_aspect_num ) goto crop_end;

    i_aspect_den = strtol( ++psz_parser, &psz_end, 0 );
    if( psz_end == psz_parser || !i_aspect_den ) goto crop_end;

    i_width = p_vout->fmt_in.i_sar_den * p_vout->fmt_render.i_visible_height *
        i_aspect_num / i_aspect_den / p_vout->fmt_in.i_sar_num;
    i_height = p_vout->fmt_render.i_visible_width * p_vout->fmt_in.i_sar_num *
        i_aspect_den / i_aspect_num / p_vout->fmt_in.i_sar_den;

    if( i_width < p_vout->fmt_render.i_visible_width )
    {
        p_vout->fmt_in.i_x_offset = p_vout->fmt_render.i_x_offset +
            (p_vout->fmt_render.i_visible_width - i_width) / 2;
        p_vout->fmt_in.i_visible_width = i_width;
    }
    else
    {
        p_vout->fmt_in.i_y_offset = p_vout->fmt_render.i_y_offset +
            (p_vout->fmt_render.i_visible_height - i_height) / 2;
        p_vout->fmt_in.i_visible_height = i_height;
    }

 crop_end:
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );

    p_vout->i_changes |= VOUT_CROP_CHANGE;

    msg_Dbg( p_vout, "cropping picture %ix%i to %i,%i,%ix%i",
             p_vout->fmt_in.i_width, p_vout->fmt_in.i_height,
             p_vout->fmt_in.i_x_offset, p_vout->fmt_in.i_y_offset,
             p_vout->fmt_in.i_visible_width,
             p_vout->fmt_in.i_visible_height );

    return VLC_SUCCESS;
}

static int AspectCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    unsigned int i_aspect_num, i_aspect_den, i_sar_num, i_sar_den;
    vlc_value_t val;

    char *psz_end, *psz_parser = strchr( newval.psz_string, ':' );

    /* Restore defaults */
    p_vout->fmt_in.i_sar_num = p_vout->fmt_render.i_sar_num;
    p_vout->fmt_in.i_sar_den = p_vout->fmt_render.i_sar_den;
    p_vout->fmt_in.i_aspect = p_vout->fmt_render.i_aspect;
    p_vout->render.i_aspect = p_vout->fmt_render.i_aspect;

    if( !psz_parser ) goto aspect_end;

    i_aspect_num = strtol( newval.psz_string, &psz_end, 0 );
    if( psz_end == newval.psz_string || !i_aspect_num ) goto aspect_end;

    i_aspect_den = strtol( ++psz_parser, &psz_end, 0 );
    if( psz_end == psz_parser || !i_aspect_den ) goto aspect_end;

    i_sar_num = i_aspect_num * p_vout->fmt_render.i_visible_height;
    i_sar_den = i_aspect_den * p_vout->fmt_render.i_visible_width;
    vlc_ureduce( &i_sar_num, &i_sar_den, i_sar_num, i_sar_den, 0 );
    p_vout->fmt_in.i_sar_num = i_sar_num;
    p_vout->fmt_in.i_sar_den = i_sar_den;
    p_vout->fmt_in.i_aspect = i_aspect_num * VOUT_ASPECT_FACTOR / i_aspect_den;
    p_vout->render.i_aspect = p_vout->fmt_in.i_aspect;

 aspect_end:
    if( p_vout->i_par_num && p_vout->i_par_den )
    {
        p_vout->fmt_in.i_sar_num *= p_vout->i_par_den;
        p_vout->fmt_in.i_sar_den *= p_vout->i_par_num;
        p_vout->fmt_in.i_aspect = p_vout->fmt_in.i_aspect *
            p_vout->i_par_den / p_vout->i_par_num;
        p_vout->render.i_aspect = p_vout->fmt_in.i_aspect;
    }

    p_vout->i_changes |= VOUT_ASPECT_CHANGE;

    vlc_ureduce( &i_aspect_num, &i_aspect_den,
                 p_vout->fmt_in.i_aspect, VOUT_ASPECT_FACTOR, 0 );
    msg_Dbg( p_vout, "new aspect-ratio %i:%i, sample aspect-ratio %i:%i",
             i_aspect_num, i_aspect_den,
             p_vout->fmt_in.i_sar_num, p_vout->fmt_in.i_sar_den );

    var_Get( p_vout, "crop", &val );
    return CropCallback( p_this, 0, val, val, 0 );

    return VLC_SUCCESS;
}

static int OnTopCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    playlist_t *p_playlist;
    vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, newval.b_bool );

    p_playlist = (playlist_t *)vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( p_playlist )
    {
        /* Modify playlist as well because the vout might have to be restarted */
        var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL );
        var_Set( p_playlist, "video-on-top", newval );

        vlc_object_release( p_playlist );
    }
    return VLC_SUCCESS;
}

static int FullscreenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    playlist_t *p_playlist;
    vlc_value_t val;

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    p_playlist = (playlist_t *)vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( p_playlist )
    {
        /* Modify playlist as well because the vout might have to be restarted */
        var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL );
        var_Set( p_playlist, "fullscreen", newval );

        vlc_object_release( p_playlist );
    }

    /* Disable "always on top" in fullscreen mode */
    var_Get( p_vout, "video-on-top", &val );
    if( newval.b_bool && val.b_bool )
    {
        val.b_bool = VLC_FALSE;
        vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, val.b_bool );
    }
    else if( !newval.b_bool && val.b_bool )
    {
        vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, val.b_bool );
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}

static int SnapshotCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_Control( p_vout, VOUT_SNAPSHOT );
    return VLC_SUCCESS;
}
