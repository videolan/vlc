/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdio.h>
#include <stdlib.h>                                                /* free() */
#include <sys/types.h>                                          /* opendir() */
#include <dirent.h>                                             /* opendir() */

#include <vlc_interface.h>
#include <vlc_block.h>
#include <vlc_playlist.h>

#include <vlc_vout.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include <vlc_strings.h>
#include <vlc_charset.h>
#include "../libvlc.h"

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
static int TitleCallback( vlc_object_t *, char const *,
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
    var_Get( p_vout->p_libvlc, "drawable", &val );
    if( val.i_int ) return (void *)(intptr_t)val.i_int;

    /* Find if the main interface supports embedding */
    p_list = vlc_list_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list ) return NULL;

    for( i = 0; i < p_list->i_count; i++ )
    {
        p_intf = (intf_thread_t *)p_list->p_values[i].p_object;
        if( p_intf->pf_request_window ) break;
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
static const struct
{
    double f_value;
    const char *psz_label;
} p_zoom_values[] = {
    { 0.25, N_("1:4 Quarter") },
    { 0.5, N_("1:2 Half") },
    { 1, N_("1:1 Original") },
    { 2, N_("2:1 Double") },
    { 0, NULL } };

static const struct
{
    const char *psz_value;
    const char *psz_label;
} p_crop_values[] = {
    { "", N_("Default") },
    { "1:1", "1:1" },
    { "4:3", "4:3" },
    { "16:9", "16:9" },
    { "16:10", "16:10" },
    { "5:4", "5:4" },
    { "5:3", "5:3" },
    { "1.85:1", "1.85:1" },
    { "221:100", "2.21:1" },
    { "235:100", "2.35:1" },
    { "239:100", "2.39:1" },
    { NULL, NULL } };

static const struct
{
    const char *psz_value;
    const char *psz_label;
} p_aspect_ratio_values[] = {
    { "", N_("Default") },
    { "1:1", "1:1" },
    { "4:3", "4:3" },
    { "16:9", "16:9" },
    { "16:10", "16:10" },
    { "221:100", "2.21:1" },
    { "5:4", "5:4" },
    { NULL, NULL } };

static void AddCustomRatios( vout_thread_t *p_vout, const char *psz_var,
                             char *psz_list )
{
    if( psz_list && *psz_list )
    {
        char *psz_cur = psz_list;
        char *psz_next;
        while( psz_cur && *psz_cur )
        {
            vlc_value_t val, text;
            psz_next = strchr( psz_cur, ',' );
            if( psz_next )
            {
                *psz_next = '\0';
                psz_next++;
            }
            val.psz_string = psz_cur;
            text.psz_string = psz_cur;
            var_Change( p_vout, psz_var, VLC_VAR_ADDCHOICE, &val, &text);
            psz_cur = psz_next;
        }
    }
}

void vout_IntfInit( vout_thread_t *p_vout )
{
    vlc_value_t val, text, old_val;
    vlc_bool_t b_force_par = VLC_FALSE;
    char *psz_buf;
    int i;

    /* Create a few object variables we'll need later on */
    var_Create( p_vout, "snapshot-path", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-prefix", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-format", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-preview", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-sequential",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-num", VLC_VAR_INTEGER );
    var_SetInteger( p_vout, "snapshot-num", 1 );
    var_Create( p_vout, "snapshot-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "snapshot-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_vout, "width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_vout->i_alignment = var_CreateGetInteger( p_vout, "align" );

    var_Create( p_vout, "video-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "video-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    p_vout->b_title_show = var_CreateGetBool( p_vout, "video-title-show" );
    p_vout->i_title_timeout =
        (mtime_t)var_CreateGetInteger( p_vout, "video-title-timeout" );
    p_vout->i_title_position =
        var_CreateGetInteger( p_vout, "video-title-position" );

    var_AddCallback( p_vout, "video-title-show", TitleCallback, NULL );
    var_AddCallback( p_vout, "video-title-timeout", TitleCallback, NULL );
    var_AddCallback( p_vout, "video-title-position", TitleCallback, NULL );

    /* Zoom object var */
    var_Create( p_vout, "zoom", VLC_VAR_FLOAT | VLC_VAR_ISCOMMAND |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Zoom");
    var_Change( p_vout, "zoom", VLC_VAR_SETTEXT, &text, NULL );

    var_Get( p_vout, "zoom", &old_val );

    for( i = 0; p_zoom_values[i].f_value; i++ )
    {
        if( old_val.f_float == p_zoom_values[i].f_value )
            var_Change( p_vout, "zoom", VLC_VAR_DELCHOICE, &old_val, NULL );
        val.f_float = p_zoom_values[i].f_value;
        text.psz_string = _( p_zoom_values[i].psz_label );
        var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    }

    var_Set( p_vout, "zoom", old_val ); /* Is this really needed? */

    var_AddCallback( p_vout, "zoom", ZoomCallback, NULL );

    /* Crop offset vars */
    var_Create( p_vout, "crop-left", VLC_VAR_INTEGER );
    var_Create( p_vout, "crop-top", VLC_VAR_INTEGER );
    var_Create( p_vout, "crop-right", VLC_VAR_INTEGER );
    var_Create( p_vout, "crop-bottom", VLC_VAR_INTEGER );

    var_AddCallback( p_vout, "crop-left", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-top", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-right", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-bottom", CropCallback, NULL );

    /* Crop object var */
    var_Create( p_vout, "crop", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Crop");
    var_Change( p_vout, "crop", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = (char*)"";
    var_Change( p_vout, "crop", VLC_VAR_DELCHOICE, &val, 0 );

    for( i = 0; p_crop_values[i].psz_value; i++ )
    {
        val.psz_string = (char*)p_crop_values[i].psz_value;
        text.psz_string = _( p_crop_values[i].psz_label );
        var_Change( p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &text );
    }

    /* Add custom crop ratios */
    psz_buf = config_GetPsz( p_vout, "custom-crop-ratios" );
    AddCustomRatios( p_vout, "crop", psz_buf );
    free( psz_buf );

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
            i_aspect_num = strtol( val.psz_string, 0, 10 );
            i_aspect_den = strtol( ++psz_parser, 0, 10 );
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

        msg_Dbg( p_vout, "overriding monitor pixel aspect-ratio: %i:%i",
                 p_vout->i_par_num, p_vout->i_par_den );
        b_force_par = VLC_TRUE;
    }
    if( val.psz_string ) free( val.psz_string );

    /* Aspect-ratio object var */
    var_Create( p_vout, "aspect-ratio", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Aspect-ratio");
    var_Change( p_vout, "aspect-ratio", VLC_VAR_SETTEXT, &text, NULL );

    val.psz_string = (char*)"";
    var_Change( p_vout, "aspect-ratio", VLC_VAR_DELCHOICE, &val, 0 );

    for( i = 0; p_aspect_ratio_values[i].psz_value; i++ )
    {
        val.psz_string = (char*)p_aspect_ratio_values[i].psz_value;
        text.psz_string = _( p_aspect_ratio_values[i].psz_label );
        var_Change( p_vout, "aspect-ratio", VLC_VAR_ADDCHOICE, &val, &text );
    }

    /* Add custom aspect ratios */
    psz_buf = config_GetPsz( p_vout, "custom-aspect-ratios" );
    AddCustomRatios( p_vout, "aspect-ratio", psz_buf );
    free( psz_buf );

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
    if( var_CreateGetBool( p_vout, "fullscreen" ) )
    {
        /* user requested fullscreen */
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    }
    text.psz_string = _("Fullscreen");
    var_Change( p_vout, "fullscreen", VLC_VAR_SETTEXT, &text, NULL );
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
    var_SetBool( p_vout, "intf-change", VLC_TRUE );
}

/*****************************************************************************
 * vout_Snapshot: generates a snapshot.
 *****************************************************************************/
int vout_Snapshot( vout_thread_t *p_vout, picture_t *p_pic )
{
    image_handler_t *p_image = image_HandlerCreate( p_vout );
    video_format_t fmt_in, fmt_out;
    char *psz_filename = NULL;
    subpicture_t *p_subpic;
    picture_t *p_pif;
    vlc_value_t val, format;
    DIR *path;
    int i_ret;

    memset( &fmt_in, 0, sizeof(video_format_t));
    memset( &fmt_out, 0, sizeof(video_format_t));

    var_Get( p_vout, "snapshot-path", &val );
    if( val.psz_string && !*val.psz_string )
    {
        free( val.psz_string );
        val.psz_string = 0;
    }

    /* Embedded snapshot : if snapshot-path == object:object-id, then
       create a snapshot_t* and store it in
       object(object-id)->p_private, then unlock and signal the
       waiting object.
     */
    if( val.psz_string && !strncmp( val.psz_string, "object:", 7 ) )
    {
        int i_id;
        vlc_object_t* p_dest;
        block_t *p_block;
        snapshot_t *p_snapshot;
        int i_size;

        /* Destination object-id is following object: */
        i_id = atoi( &val.psz_string[7] );
        p_dest = ( vlc_object_t* )vlc_current_object( i_id );
        if( !p_dest )
        {
            msg_Err( p_vout, "Cannot find calling object" );
            image_HandlerDelete( p_image );
            return VLC_EGENERIC;
        }
        /* Object must be locked. We will unlock it once we get the
           snapshot and written it to p_private */
        p_dest->p_private = NULL;

        /* Save the snapshot to a memory zone */
        fmt_in = p_vout->fmt_in;
        fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
        fmt_out.i_width = var_GetInteger( p_vout, "snapshot-width" );
        fmt_out.i_height = var_GetInteger( p_vout, "snapshot-height" );
        fmt_out.i_chroma = VLC_FOURCC( 'p','n','g',' ' );

        p_block = ( block_t* ) image_Write( p_image, p_pic, &fmt_in, &fmt_out );
        if( !p_block )
        {
            msg_Err( p_vout, "Could not get snapshot" );
            image_HandlerDelete( p_image );
            vlc_cond_signal( &p_dest->object_wait );
            vlc_object_release( p_dest );
            return VLC_EGENERIC;
        }

        /* Copy the p_block data to a snapshot structure */
        /* FIXME: get the timestamp */
        p_snapshot = ( snapshot_t* ) malloc( sizeof( snapshot_t ) );
        if( !p_snapshot )
        {
            block_Release( p_block );
            image_HandlerDelete( p_image );
            vlc_cond_signal( &p_dest->object_wait );
            vlc_object_release( p_dest );
            return VLC_ENOMEM;
        }

        i_size = p_block->i_buffer;

        p_snapshot->i_width = fmt_out.i_width;
        p_snapshot->i_height = fmt_out.i_height;
        p_snapshot->i_datasize = i_size;
        p_snapshot->date = p_block->i_pts; /* FIXME ?? */
        p_snapshot->p_data = ( char* ) malloc( i_size );
        if( !p_snapshot->p_data )
        {
            block_Release( p_block );
            free( p_snapshot );
            image_HandlerDelete( p_image );
            vlc_cond_signal( &p_dest->object_wait );
            vlc_object_release( p_dest );
            return VLC_ENOMEM;
        }
        memcpy( p_snapshot->p_data, p_block->p_buffer, p_block->i_buffer );

        p_dest->p_private = p_snapshot;

        block_Release( p_block );

        /* Unlock the object */
        vlc_cond_signal( &p_dest->object_wait );
        vlc_object_release( p_dest );

        image_HandlerDelete( p_image );
        return VLC_SUCCESS;
    }

#if defined(__APPLE__) || defined(SYS_BEOS)
    if( !val.psz_string && p_vout->p_libvlc->psz_homedir )
    {
        if( asprintf( &val.psz_string, "%s/Desktop",
                      p_vout->p_libvlc->psz_homedir ) == -1 )
            val.psz_string = NULL;
    }

#elif defined(WIN32) && !defined(UNDER_CE)
    if( !val.psz_string && p_vout->p_libvlc->psz_homedir )
    {
        /* Get the My Pictures folder path */

        char *p_mypicturesdir = NULL;
        typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                                   LPWSTR );
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
           wchar_t wdir[PATH_MAX];
           SHGetFolderPath = (void *)GetProcAddress( shfolder_dll,
                                                      _T("SHGetFolderPathW") );
            if ((SHGetFolderPath != NULL )
             && SUCCEEDED (SHGetFolderPath (NULL,
                                           CSIDL_MYPICTURES | CSIDL_FLAG_CREATE,
                                           NULL, SHGFP_TYPE_CURRENT,
                                           wdir)))
                p_mypicturesdir = FromWide (wdir);

            FreeLibrary( shfolder_dll );
        }

        if( p_mypicturesdir == NULL )
        {
            if( asprintf( &val.psz_string, "%s",
                          p_vout->p_libvlc->psz_homedir ) == -1 )
                val.psz_string = NULL;
        }
        else
        {
            if( asprintf( &val.psz_string, p_mypicturesdir ) == -1 )
                val.psz_string = NULL;
            free( p_mypicturesdir );
        }
    }

#else
    /* XXX: This saves in the data directory. Shouldn't we try saving
     *      to psz_homedir/Desktop or something nicer ? */
    if( !val.psz_string && p_vout->p_libvlc->psz_datadir )
    {
        if( asprintf( &val.psz_string, "%s",
                      p_vout->p_libvlc->psz_datadir ) == -1 )
            val.psz_string = NULL;
    }
#endif

    if( !val.psz_string )
    {
        msg_Err( p_vout, "no path specified for snapshots" );
        return VLC_EGENERIC;
    }
    var_Get( p_vout, "snapshot-format", &format );
    if( !format.psz_string || !*format.psz_string )
    {
        if( format.psz_string ) free( format.psz_string );
        format.psz_string = strdup( "png" );
    }

    /*
     * Did the user specify a directory? If not, path = NULL.
     */
    path = utf8_opendir ( (const char *)val.psz_string  );
    if( path != NULL )
    {
        char *psz_prefix = var_GetNonEmptyString( p_vout, "snapshot-prefix" );
        if( psz_prefix == NULL )
            psz_prefix = strdup( "vlcsnap-" );
        else
        {
            char *psz_tmp = str_format( p_vout, psz_prefix );
            filename_sanitize( psz_tmp );
            free( psz_prefix );
            psz_prefix = psz_tmp;
        }

        closedir( path );
        if( var_GetBool( p_vout, "snapshot-sequential" ) == VLC_TRUE )
        {
            int i_num = var_GetInteger( p_vout, "snapshot-num" );
            FILE *p_file;
            do
            {
                asprintf( &psz_filename, "%s" DIR_SEP "%s%05d.%s", val.psz_string,
                          psz_prefix, i_num++, format.psz_string );
            }
            while( ( p_file = utf8_fopen( psz_filename, "r" ) ) && !fclose( p_file ) );
            var_SetInteger( p_vout, "snapshot-num", i_num );
        }
        else
        {
            asprintf( &psz_filename, "%s" DIR_SEP "%s%u.%s", val.psz_string,
                      psz_prefix,
                      (unsigned int)(p_pic->date / 100000) & 0xFFFFFF,
                      format.psz_string );
        }

        free( psz_prefix );
    }
    else // The user specified a full path name (including file name)
    {
        psz_filename = str_format( p_vout, val.psz_string );
        path_sanitize( psz_filename );
    }

    free( val.psz_string );
    free( format.psz_string );

    fmt_out.i_width = var_GetInteger( p_vout, "snapshot-width" );
    fmt_out.i_height = var_GetInteger( p_vout, "snapshot-height" );

    if( fmt_out.i_width == 0 && fmt_out.i_height > 0 )
    {
        float f = (float)p_vout->fmt_in.i_height / fmt_out.i_height;
        fmt_out.i_width = p_vout->fmt_in.i_width / f;
    }
    else if( fmt_out.i_height == 0 && fmt_out.i_width > 0 )
    {
        float f = (float)p_vout->fmt_in.i_width / fmt_out.i_width;
        fmt_out.i_height = p_vout->fmt_in.i_height / f;
    }

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
    vout_OSDMessage( VLC_OBJECT( p_vout ), DEFAULT_CHAN,
                     "%s", psz_filename );
    free( psz_filename );

    if( var_GetBool( p_vout, "snapshot-preview" ) )
    {
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
        vout_CopyPicture( p_image->p_parent, &p_subpic->p_region->picture,
                          p_pif );
        p_pif->pf_release( p_pif );

        spu_DisplaySubpicture( p_vout->p_spu, p_subpic );
    }
    else
    {
        image_HandlerDelete( p_image );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Handle filters
 *****************************************************************************/

void vout_EnableFilter( vout_thread_t *p_vout, char *psz_name,
                        vlc_bool_t b_add, vlc_bool_t b_setconfig )
{
    char *psz_parser;
    char *psz_string = config_GetPsz( p_vout, "vout-filter" );

    /* Todo : Use some generic chain manipulation functions */
    if( !psz_string ) psz_string = strdup("");

    psz_parser = strstr( psz_string, psz_name );
    if( b_add )
    {
        if( !psz_parser )
        {
            psz_parser = psz_string;
            asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                            psz_string, psz_name );
            free( psz_parser );
        }
        else
            return;
    }
    else
    {
        if( psz_parser )
        {
            memmove( psz_parser, psz_parser + strlen(psz_name) +
                            (*(psz_parser + strlen(psz_name)) == ':' ? 1 : 0 ),
                            strlen(psz_parser + strlen(psz_name)) + 1 );

            /* Remove trailing : : */
            if( *(psz_string+strlen(psz_string ) -1 ) == ':' )
            {
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
            }
         }
         else
         {
             free( psz_string );
             return;
         }
    }
    if( b_setconfig )
        config_PutPsz( p_vout, "vout-filter", psz_string );

    var_SetString( p_vout, "vout-filter", psz_string );
    free( psz_string );
}

/*****************************************************************************
 * vout_ControlDefault: default methods for video output control.
 *****************************************************************************/
int vout_vaControlDefault( vout_thread_t *p_vout, int i_query, va_list args )
{
    (void)args;
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

    if( p_vout->fmt_in.i_sar_num == 0 || p_vout->fmt_in.i_sar_den == 0 ) {
        msg_Warn( p_vout, "fucked up aspect" );
        *pi_width = (int)( p_vout->fmt_in.i_visible_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( p_vout->fmt_in.i_visible_height * ll_zoom /FP_FACTOR);
    }
    else if( p_vout->fmt_in.i_sar_num >= p_vout->fmt_in.i_sar_den )
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
    (void)psz_cmd; (void)oldval; (void)newval; (void)p_data;
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );
    vout_Control( p_vout, VOUT_SET_SIZE, p_vout->i_window_width,
                  p_vout->i_window_height );
    return VLC_SUCCESS;
}

static int CropCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int64_t i_aspect_num, i_aspect_den;
    unsigned int i_width, i_height;

    (void)oldval; (void)p_data;

    /* Restore defaults */
    p_vout->fmt_in.i_x_offset = p_vout->fmt_render.i_x_offset;
    p_vout->fmt_in.i_visible_width = p_vout->fmt_render.i_visible_width;
    p_vout->fmt_in.i_y_offset = p_vout->fmt_render.i_y_offset;
    p_vout->fmt_in.i_visible_height = p_vout->fmt_render.i_visible_height;

    if( !strcmp( psz_cmd, "crop" ) )
    {
        char *psz_end = NULL, *psz_parser = strchr( newval.psz_string, ':' );
        if( psz_parser )
        {
            /* We're using the 3:4 syntax */
            i_aspect_num = strtol( newval.psz_string, &psz_end, 10 );
            if( psz_end == newval.psz_string || !i_aspect_num ) goto crop_end;

            i_aspect_den = strtol( ++psz_parser, &psz_end, 10 );
            if( psz_end == psz_parser || !i_aspect_den ) goto crop_end;

            i_width = p_vout->fmt_in.i_sar_den*p_vout->fmt_render.i_visible_height *
                i_aspect_num / i_aspect_den / p_vout->fmt_in.i_sar_num;
            i_height = p_vout->fmt_render.i_visible_width*p_vout->fmt_in.i_sar_num *
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
        }
        else
        {
            psz_parser = strchr( newval.psz_string, 'x' );
            if( psz_parser )
            {
                /* Maybe we're using the <width>x<height>+<left>+<top> syntax */
                unsigned int i_crop_width, i_crop_height, i_crop_top, i_crop_left;

                i_crop_width = strtol( newval.psz_string, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_parser = strchr( ++psz_end, '+' );
                i_crop_height = strtol( psz_end, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_parser = strchr( ++psz_end, '+' );
                i_crop_left = strtol( psz_end, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_end++;
                i_crop_top = strtol( psz_end, &psz_end, 10 );
                if( *psz_end != '\0' ) goto crop_end;

                i_width = i_crop_width;
                p_vout->fmt_in.i_visible_width = i_width;

                i_height = i_crop_height;
                p_vout->fmt_in.i_visible_height = i_height;

                p_vout->fmt_in.i_x_offset = i_crop_left;
                p_vout->fmt_in.i_y_offset = i_crop_top;
            }
            else
            {
                /* Maybe we're using the <left>+<top>+<right>+<bottom> syntax */
                unsigned int i_crop_top, i_crop_left, i_crop_bottom, i_crop_right;

                psz_parser = strchr( newval.psz_string, '+' );
                i_crop_left = strtol( newval.psz_string, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_parser = strchr( ++psz_end, '+' );
                i_crop_top = strtol( psz_end, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_parser = strchr( ++psz_end, '+' );
                i_crop_right = strtol( psz_end, &psz_end, 10 );
                if( psz_end != psz_parser ) goto crop_end;

                psz_end++;
                i_crop_bottom = strtol( psz_end, &psz_end, 10 );
                if( *psz_end != '\0' ) goto crop_end;

                i_width = p_vout->fmt_render.i_visible_width
                          - i_crop_left - i_crop_right;
                p_vout->fmt_in.i_visible_width = i_width;

                i_height = p_vout->fmt_render.i_visible_height
                           - i_crop_top - i_crop_bottom;
                p_vout->fmt_in.i_visible_height = i_height;

                p_vout->fmt_in.i_x_offset = i_crop_left;
                p_vout->fmt_in.i_y_offset = i_crop_top;
            }
        }
    }
    else if( !strcmp( psz_cmd, "crop-top" )
          || !strcmp( psz_cmd, "crop-left" )
          || !strcmp( psz_cmd, "crop-bottom" )
          || !strcmp( psz_cmd, "crop-right" ) )
    {
        unsigned int i_crop_top, i_crop_left, i_crop_bottom, i_crop_right;

        i_crop_top = var_GetInteger( p_vout, "crop-top" );
        i_crop_left = var_GetInteger( p_vout, "crop-left" );
        i_crop_right = var_GetInteger( p_vout, "crop-right" );
        i_crop_bottom = var_GetInteger( p_vout, "crop-bottom" );

        i_width = p_vout->fmt_render.i_visible_width
                  - i_crop_left - i_crop_right;
        p_vout->fmt_in.i_visible_width = i_width;

        i_height = p_vout->fmt_render.i_visible_height
                   - i_crop_top - i_crop_bottom;
        p_vout->fmt_in.i_visible_height = i_height;

        p_vout->fmt_in.i_x_offset = i_crop_left;
        p_vout->fmt_in.i_y_offset = i_crop_top;
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
    (void)psz_cmd; (void)oldval; (void)p_data;

    /* Restore defaults */
    p_vout->fmt_in.i_sar_num = p_vout->fmt_render.i_sar_num;
    p_vout->fmt_in.i_sar_den = p_vout->fmt_render.i_sar_den;
    p_vout->fmt_in.i_aspect = p_vout->fmt_render.i_aspect;
    p_vout->render.i_aspect = p_vout->fmt_render.i_aspect;

    if( !psz_parser ) goto aspect_end;

    i_aspect_num = strtol( newval.psz_string, &psz_end, 10 );
    if( psz_end == newval.psz_string || !i_aspect_num ) goto aspect_end;

    i_aspect_den = strtol( ++psz_parser, &psz_end, 10 );
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
    return CropCallback( p_this, "crop", val, val, 0 );
}

static int OnTopCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    playlist_t *p_playlist = pl_Yield( p_this );
    vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, newval.b_bool );
    (void)psz_cmd; (void)oldval; (void)p_data;

    /* Modify playlist as well because the vout might have to be restarted */
    var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL );
    var_Set( p_playlist, "video-on-top", newval );

    pl_Release( p_this );
    return VLC_SUCCESS;
}

static int FullscreenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;
    playlist_t *p_playlist = pl_Yield( p_this );
    (void)psz_cmd; (void)oldval; (void)p_data;

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    /* Modify playlist as well because the vout might have to be restarted */
    var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL );
    var_Set( p_playlist, "fullscreen", newval );
    pl_Release( p_playlist );

    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}

static int SnapshotCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_Control( p_vout, VOUT_SNAPSHOT );
    (void)psz_cmd; (void)oldval; (void)newval; (void)p_data;
    return VLC_SUCCESS;
}

static int TitleCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    if( !strncmp( psz_cmd, "video-title-show", 16 ) )
        p_vout->b_title_show = newval.b_bool;
    else if( !strncmp( psz_cmd, "video-title-timeout", 19 ) )
        p_vout->i_title_timeout = (mtime_t) newval.i_int;
    else if( !strncmp( psz_cmd, "video-title-position", 20 ) )
        p_vout->i_title_position = newval.i_int;
    return VLC_SUCCESS;
}
