/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdio.h>
#include <stdlib.h>                                                /* free() */
#include <sys/types.h>                                          /* opendir() */
#include <sys/stat.h>
#include <dirent.h>                                             /* opendir() */
#include <assert.h>
#include <time.h>                                           /* strftime */

#include <vlc_interface.h>
#include <vlc_block.h>
#include <vlc_playlist.h>

#include <vlc_vout.h>
#include <vlc_window.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include <vlc_strings.h>
#include <vlc_charset.h>
#include "../libvlc.h"
#include "vout_internal.h"

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
static int ScalingCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );
static int OnTopCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int FullscreenCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int SnapshotCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

static int TitleShowCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int TitleTimeoutCallback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );
static int TitlePositionCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );

/**
 * Creates a video output window.
 * On Unix systems, this is an X11 drawable (handle).
 * On Windows, this is a Win32 window (handle).
 * Video output plugins are supposed to called this function and display the
 * video within the resulting window, while in windowed mode.
 *
 * @param p_vout video output thread to create a window for
 * @param psz_cap VLC module capability (window system type)
 * @param pi_x_hint pointer to store the recommended horizontal position [OUT]
 * @param pi_y_hint pointer to store the recommended vertical position [OUT]
 * @param pi_width_hint pointer to store the recommended width [OUT]
 * @param pi_height_hint pointer to store the recommended height [OUT]
 *
 * @return a vout_window_t object, or NULL in case of failure.
 * The window is released with vout_ReleaseWindow().
 */
vout_window_t *vout_RequestWindow( vout_thread_t *p_vout, const char *psz_cap,
                          int *pi_x_hint, int *pi_y_hint,
                          unsigned int *pi_width_hint,
                          unsigned int *pi_height_hint )
{
    /* Get requested coordinates */
    *pi_x_hint = var_GetInteger( p_vout, "video-x" );
    *pi_y_hint = var_GetInteger( p_vout, "video-y" );

    *pi_width_hint = p_vout->i_window_width;
    *pi_height_hint = p_vout->i_window_height;

    vout_window_t *wnd = vlc_custom_create (VLC_OBJECT(p_vout), sizeof (*wnd),
                                            VLC_OBJECT_GENERIC, "window");
    if (wnd == NULL)
        return NULL;

    wnd->vout = p_vout;
    wnd->width = *pi_width_hint;
    wnd->height = *pi_height_hint;
    wnd->pos_x = *pi_x_hint;
    wnd->pos_y = *pi_y_hint;
    vlc_object_attach (wnd, p_vout);

    wnd->module = module_need (wnd, psz_cap, NULL, false);
    if (wnd->module == NULL)
    {
        msg_Dbg (wnd, "no \"%s\" window provider available", psz_cap);
        vlc_object_release (wnd);
        return NULL;
    }
    *pi_width_hint = wnd->width;
    *pi_height_hint = wnd->height;
    *pi_x_hint = wnd->pos_x;
    *pi_y_hint = wnd->pos_y;
    return wnd;
}

/**
 * Releases a window handle obtained with vout_RequestWindow().
 * @param p_vout video output thread that allocated the window
 *               (if this is NULL; this fnction is a no-op).
 */
void vout_ReleaseWindow( vout_window_t *wnd )
{
    if (wnd == NULL)
        return;

    assert (wnd->module);
    module_unneed (wnd, wnd->module);

    vlc_object_release (wnd);
}

int vout_ControlWindow( vout_window_t *wnd, int i_query, va_list args )
{
    if (wnd == NULL)
        return VLC_EGENERIC;

    assert (wnd->control);
    return wnd->control (wnd, i_query, args);
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
    { "16:10", "16:10" },
    { "16:9", "16:9" },
    { "185:100", "1.85:1" },
    { "221:100", "2.21:1" },
    { "235:100", "2.35:1" },
    { "239:100", "2.39:1" },
    { "5:3", "5:3" },
    { "4:3", "4:3" },
    { "5:4", "5:4" },
    { "1:1", "1:1" },
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
    bool b_force_par = false;
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

    var_Create( p_vout, "mouse-hide-timeout",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    p_vout->p->b_title_show = var_CreateGetBool( p_vout, "video-title-show" );
    p_vout->p->i_title_timeout =
        (mtime_t)var_CreateGetInteger( p_vout, "video-title-timeout" );
    p_vout->p->i_title_position =
        var_CreateGetInteger( p_vout, "video-title-position" );
    p_vout->p->psz_title =  NULL;

    var_AddCallback( p_vout, "video-title-show", TitleShowCallback, NULL );
    var_AddCallback( p_vout, "video-title-timeout", TitleTimeoutCallback, NULL );
    var_AddCallback( p_vout, "video-title-position", TitlePositionCallback, NULL );

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
    var_Create( p_vout, "crop-left", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_Create( p_vout, "crop-top", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_Create( p_vout, "crop-right", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_Create( p_vout, "crop-bottom", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );

    var_AddCallback( p_vout, "crop-left", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-top", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-right", CropCallback, NULL );
    var_AddCallback( p_vout, "crop-bottom", CropCallback, NULL );

    /* Crop object var */
    var_Create( p_vout, "crop", VLC_VAR_STRING | VLC_VAR_ISCOMMAND |
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

    /* update triggered every time the vout's crop parameters are changed */
    var_Create( p_vout, "crop-update", VLC_VAR_VOID );

    /* Add custom crop ratios */
    psz_buf = config_GetPsz( p_vout, "custom-crop-ratios" );
    AddCustomRatios( p_vout, "crop", psz_buf );
    free( psz_buf );

    var_AddCallback( p_vout, "crop", CropCallback, NULL );
    var_Get( p_vout, "crop", &old_val );
    if( old_val.psz_string && *old_val.psz_string )
        var_TriggerCallback( p_vout, "crop" );
    free( old_val.psz_string );

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

        p_vout->p->i_par_num = i_aspect_num;
        p_vout->p->i_par_den = i_aspect_den;

        vlc_ureduce( &p_vout->p->i_par_num, &p_vout->p->i_par_den,
                     p_vout->p->i_par_num, p_vout->p->i_par_den, 0 );

        msg_Dbg( p_vout, "overriding monitor pixel aspect-ratio: %i:%i",
                 p_vout->p->i_par_num, p_vout->p->i_par_den );
        b_force_par = true;
    }
    free( val.psz_string );

    /* Aspect-ratio object var */
    var_Create( p_vout, "aspect-ratio", VLC_VAR_STRING | VLC_VAR_ISCOMMAND |
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
        var_TriggerCallback( p_vout, "aspect-ratio" );
    free( old_val.psz_string );

    /* Add variables to manage scaling video */
    var_Create( p_vout, "autoscale", VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Autoscale video");
    var_Change( p_vout, "autoscale", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "autoscale", ScalingCallback, NULL );
    p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );

    var_Create( p_vout, "scale", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Scale factor");
    var_Change( p_vout, "scale", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "scale", ScalingCallback, NULL );
    p_vout->i_zoom = (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );

    /* Initialize the dimensions of the video window */
    InitWindowSize( p_vout, &p_vout->i_window_width,
                    &p_vout->i_window_height );

    /* Add a variable to indicate if the window should be on top of others */
    var_Create( p_vout, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Always on top");
    var_Change( p_vout, "video-on-top", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-on-top", OnTopCallback, NULL );

    /* Add a variable to indicate whether we want window decoration or not */
    var_Create( p_vout, "video-deco", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Add a fullscreen variable */
    if( var_CreateGetBoolCommand( p_vout, "fullscreen" ) )
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
    var_Create( p_vout, "mouse-clicked", VLC_VAR_BOOL );

    var_Create( p_vout, "intf-change", VLC_VAR_BOOL );
    var_SetBool( p_vout, "intf-change", true );
}

/*****************************************************************************
 * vout_Snapshot: generates a snapshot.
 *****************************************************************************/
/**
 * This function will inject a subpicture into the vout with the provided
 * picture
 */
static int VoutSnapshotPip( vout_thread_t *p_vout, image_handler_t *p_image, picture_t *p_pic, const video_format_t *p_fmt_in )
{
    video_format_t fmt_in = *p_fmt_in;
    video_format_t fmt_out;
    picture_t *p_pip;
    subpicture_t *p_subpic;

    /* */
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out = fmt_in;
    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');

    /* */
    p_pip = image_Convert( p_image, p_pic, &fmt_in, &fmt_out );
    if( !p_pip )
        return VLC_EGENERIC;

    p_subpic = subpicture_New();
    if( p_subpic == NULL )
    {
         picture_Release( p_pip );
         return VLC_EGENERIC;
    }

    p_subpic->i_channel = 0;
    p_subpic->i_start = mdate();
    p_subpic->i_stop = mdate() + 4000000;
    p_subpic->b_ephemer = true;
    p_subpic->b_fade = true;
    p_subpic->i_original_picture_width = fmt_out.i_width * 4;
    p_subpic->i_original_picture_height = fmt_out.i_height * 4;
    fmt_out.i_aspect = 0;
    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 0;

    p_subpic->p_region = subpicture_region_New( &fmt_out );
    if( p_subpic->p_region )
    {
        picture_Release( p_subpic->p_region->p_picture );
        p_subpic->p_region->p_picture = p_pip;
    }
    else
    {
        picture_Release( p_pip );
    }

    spu_DisplaySubpicture( p_vout->p_spu, p_subpic );
    return VLC_SUCCESS;
}
/**
 * This function will return the default directory used for snapshots
 */
static char *VoutSnapshotGetDefaultDirectory( void )
{
    char *psz_path = NULL;
#if defined(__APPLE__) || defined(SYS_BEOS)

    if( asprintf( &psz_path, "%s/Desktop",
                  config_GetHomeDir() ) == -1 )
        psz_path = NULL;

#elif defined(WIN32) && !defined(UNDER_CE)

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
        psz_path = strdup( config_GetHomeDir() );
    else
        psz_path = p_mypicturesdir;

#else

    /* XXX: This saves in the data directory. Shouldn't we try saving
     *      to psz_homedir/Desktop or something nicer ? */
    char *psz_datadir = config_GetUserDataDir();
    if( psz_datadir )
    {
        if( asprintf( &psz_path, "%s", psz_datadir ) == -1 )
            psz_path = NULL;
        free( psz_datadir );
    }

#endif

    return psz_path;
}

int vout_Snapshot( vout_thread_t *p_vout, picture_t *p_pic )
{
    image_handler_t *p_image = image_HandlerCreate( p_vout );
    video_format_t fmt_in, fmt_out;
    char *psz_filename = NULL;
    vlc_value_t val, format;
    DIR *path;
    int i_ret;
    bool b_embedded_snapshot;
    void *p_obj;

    /* */
    val.psz_string = var_GetNonEmptyString( p_vout, "snapshot-path" );

    /* Embedded snapshot : if snapshot-path == object:object_ptr */
    if( val.psz_string && sscanf( val.psz_string, "object:%p", &p_obj ) > 0 )
        b_embedded_snapshot = true;
    else
        b_embedded_snapshot = false;

    /* */
    memset( &fmt_in, 0, sizeof(video_format_t) );
    fmt_in = p_vout->fmt_out;
    if( fmt_in.i_sar_num <= 0 || fmt_in.i_sar_den <= 0 )
    {
        fmt_in.i_sar_num =
        fmt_in.i_sar_den = 1;
    }

    /* */
    memset( &fmt_out, 0, sizeof(video_format_t) );
    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 1;
    fmt_out.i_chroma = b_embedded_snapshot ? VLC_FOURCC('p','n','g',' ') : 0;
    fmt_out.i_width = var_GetInteger( p_vout, "snapshot-width" );
    fmt_out.i_height = var_GetInteger( p_vout, "snapshot-height" );

    if( b_embedded_snapshot &&
        fmt_out.i_width == 0 && fmt_out.i_height == 0 )
    {
        /* If snapshot-width and/or snapshot height were not specified,
           use a default snapshot width of 320 */
        fmt_out.i_width = 320;
    }

    if( fmt_out.i_height == 0 && fmt_out.i_width > 0 )
    {
        fmt_out.i_height = fmt_in.i_height * fmt_out.i_width / fmt_in.i_width;
        const int i_height = fmt_out.i_height * fmt_in.i_sar_den / fmt_in.i_sar_num;
        if( i_height > 0 )
            fmt_out.i_height = i_height;
    }
    else
    {
        if( fmt_out.i_width == 0 && fmt_out.i_height > 0 )
        {
            fmt_out.i_width = fmt_in.i_width * fmt_out.i_height / fmt_in.i_height;
        }
        else
        {
            fmt_out.i_width = fmt_in.i_width;
            fmt_out.i_height = fmt_in.i_height;
        }
        const int i_width = fmt_out.i_width * fmt_in.i_sar_num / fmt_in.i_sar_den;
        if( i_width > 0 )
            fmt_out.i_width = i_width;
    }

    /* Embedded snapshot
       create a snapshot_t* and store it in
       object_ptr->p_private, then unlock and signal the
       waiting object.
     */
    if( b_embedded_snapshot )
    {
        block_t *p_block;
        snapshot_t *p_snapshot = p_obj;
        size_t i_size;

	vlc_mutex_lock( &p_snapshot->p_mutex );
	p_snapshot->p_data = NULL;

        /* Save the snapshot to a memory zone */
        p_block = image_Write( p_image, p_pic, &fmt_in, &fmt_out );
        if( !p_block )
        {
            msg_Err( p_vout, "Could not get snapshot" );
            image_HandlerDelete( p_image );
	    vlc_cond_signal( &p_snapshot->p_condvar );
	    vlc_mutex_unlock( &p_snapshot->p_mutex );
            return VLC_EGENERIC;
        }

        i_size = p_block->i_buffer;

        p_snapshot->i_width = fmt_out.i_width;
        p_snapshot->i_height = fmt_out.i_height;
        p_snapshot->i_datasize = i_size;
        p_snapshot->date = p_block->i_pts; /* FIXME ?? */
        p_snapshot->p_data = malloc( i_size );
        if( !p_snapshot->p_data )
        {
            block_Release( p_block );
            image_HandlerDelete( p_image );
	    vlc_cond_signal( &p_snapshot->p_condvar );
	    vlc_mutex_unlock( &p_snapshot->p_mutex );
            return VLC_ENOMEM;
        }
        memcpy( p_snapshot->p_data, p_block->p_buffer, p_block->i_buffer );

        block_Release( p_block );

        /* Unlock the object */
	vlc_cond_signal( &p_snapshot->p_condvar );
	vlc_mutex_unlock( &p_snapshot->p_mutex );

        image_HandlerDelete( p_image );
        return VLC_SUCCESS;
    }

    /* Get default directory if none provided */
    if( !val.psz_string )
        val.psz_string = VoutSnapshotGetDefaultDirectory( );
    if( !val.psz_string )
    {
        msg_Err( p_vout, "no path specified for snapshots" );
        image_HandlerDelete( p_image );
        return VLC_EGENERIC;
    }

    /* Get snapshot format, default being "png" */
    format.psz_string = var_GetNonEmptyString( p_vout, "snapshot-format" );
    if( !format.psz_string )
        format.psz_string = strdup( "png" );
    if( !format.psz_string )
    {
        free( val.psz_string );
        image_HandlerDelete( p_image );
        return VLC_ENOMEM;
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
        if( var_GetBool( p_vout, "snapshot-sequential" ) == true )
        {
            int i_num = var_GetInteger( p_vout, "snapshot-num" );
            struct stat st;

            do
            {
                free( psz_filename );
                if( asprintf( &psz_filename, "%s" DIR_SEP "%s%05d.%s",
                              val.psz_string, psz_prefix, i_num++,
                              format.psz_string ) == -1 )
                {
                    msg_Err( p_vout, "could not create snapshot" );
                    image_HandlerDelete( p_image );
                    return VLC_EGENERIC;
                }
            }
            while( utf8_stat( psz_filename, &st ) == 0 );

            var_SetInteger( p_vout, "snapshot-num", i_num );
        }
        else
        {
            struct tm    curtime;
            time_t        lcurtime ;
            lcurtime = time( NULL ) ;
            if ( ( localtime_r( &lcurtime, &curtime ) == NULL ) )
            {
                msg_Warn( p_vout, "failed to get current time. Falling back to legacy snapshot naming" );
                /* failed to get current time. Fallback to old format */
                if( asprintf( &psz_filename, "%s" DIR_SEP "%s%u.%s",
                          val.psz_string, psz_prefix,
                          (unsigned int)(p_pic->date / 100000) & 0xFFFFFF,
                          format.psz_string ) == -1 )
                {
                    msg_Err( p_vout, "could not create snapshot" );
                    image_HandlerDelete( p_image );
                    return VLC_EGENERIC;
                }
            }
            else
            {
                char psz_curtime[15] ;
                if( strftime( psz_curtime, 15, "%y%m%d-%H%M%S", &curtime ) == 0 )
                {
                    msg_Warn( p_vout, "snapshot date string truncated" ) ;
                }
                if( asprintf( &psz_filename, "%s" DIR_SEP "%s%s%1u.%s",
                      val.psz_string, psz_prefix, psz_curtime,
                     /* suffix with the last decimal digit in 10s of seconds resolution */
                     (unsigned int)(p_pic->date / (100*1000)) & 0xFF,
                      format.psz_string ) == -1 )
                {
                    msg_Err( p_vout, "could not create snapshot" );
                    image_HandlerDelete( p_image );
                    return VLC_EGENERIC;
                }
            } //end if time() < 0
        } //end snapshot sequential
        free( psz_prefix );
    }
    else // The user specified a full path name (including file name)
    {
        psz_filename = str_format( p_vout, val.psz_string );
        path_sanitize( psz_filename );
    }

    free( val.psz_string );
    free( format.psz_string );

    /* Save the snapshot */
    i_ret = image_WriteUrl( p_image, p_pic, &fmt_in, &fmt_out, psz_filename );
    if( i_ret != VLC_SUCCESS )
    {
        msg_Err( p_vout, "could not create snapshot %s", psz_filename );
        free( psz_filename );
        image_HandlerDelete( p_image );
        return VLC_EGENERIC;
    }

    /* */
    msg_Dbg( p_vout, "snapshot taken (%s)", psz_filename );
    vout_OSDMessage( VLC_OBJECT( p_vout ), DEFAULT_CHAN,
                     "%s", psz_filename );

    /* Generate a media player event  - Right now just trigger a global libvlc var
        CHECK: Could not find a more local object. The goal is to communicate
        vout_thread with libvlc_media_player or its input_thread*/
    val.psz_string =  psz_filename  ;

    var_Set( p_vout->p_libvlc, "vout-snapshottaken", val );
    /* var_Set duplicates data for transport so we can free*/
    free( psz_filename );

    /* */
    if( var_GetBool( p_vout, "snapshot-preview" ) )
    {
        if( VoutSnapshotPip( p_vout, p_image, p_pic, &fmt_in ) )
            msg_Warn( p_vout, "Failed to display snapshot" );
    }
    image_HandlerDelete( p_image );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Handle filters
 *****************************************************************************/

void vout_EnableFilter( vout_thread_t *p_vout, char *psz_name,
                        bool b_add, bool b_setconfig )
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
            if( asprintf( &psz_string, (*psz_string) ? "%s:%s" : "%s%s",
                          psz_string, psz_name ) == -1 )
            {
                free( psz_parser );
                return;
            }
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
 * InitWindowSize: find the initial dimensions the video window should have.
 *****************************************************************************
 * This function will check the "width", "height" and "zoom" config options and
 * will calculate the size that the video window should have.
 *****************************************************************************/
static void InitWindowSize( vout_thread_t *p_vout, unsigned *pi_width,
                            unsigned *pi_height )
{
#define FP_FACTOR 1000                             /* our fixed point factor */

    int i_width = var_GetInteger( p_vout, "width" );
    int i_height = var_GetInteger( p_vout, "height" );
    float f_zoom = var_GetFloat( p_vout, "zoom" );
    uint64_t ll_zoom = (uint64_t)( FP_FACTOR * f_zoom );

    if( i_width > 0 && i_height > 0)
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
    }
    else if( i_width > 0 )
    {
        *pi_width = (int)( i_width * ll_zoom / FP_FACTOR );
        *pi_height = (int)( p_vout->fmt_in.i_visible_height * ll_zoom *
            p_vout->fmt_in.i_sar_den * i_width / p_vout->fmt_in.i_sar_num /
            FP_FACTOR / p_vout->fmt_in.i_visible_width );
    }
    else if( i_height > 0 )
    {
        *pi_height = (int)( i_height * ll_zoom / FP_FACTOR );
        *pi_width = (int)( p_vout->fmt_in.i_visible_width * ll_zoom *
            p_vout->fmt_in.i_sar_num * i_height / p_vout->fmt_in.i_sar_den /
            FP_FACTOR / p_vout->fmt_in.i_visible_height );
    }
    else if( p_vout->fmt_in.i_sar_num == 0 || p_vout->fmt_in.i_sar_den == 0 )
    {
        msg_Warn( p_vout, "aspect ratio screwed up" );
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

    msg_Dbg( p_vout, "window size: %ux%u", *pi_width, *pi_height );

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

    var_SetVoid( p_vout, "crop-update" );

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
    if( p_vout->p->i_par_num && p_vout->p->i_par_den )
    {
        p_vout->fmt_in.i_sar_num *= p_vout->p->i_par_den;
        p_vout->fmt_in.i_sar_den *= p_vout->p->i_par_num;
        p_vout->fmt_in.i_aspect = p_vout->fmt_in.i_aspect *
            p_vout->p->i_par_den / p_vout->p->i_par_num;
        p_vout->render.i_aspect = p_vout->fmt_in.i_aspect;
    }

    p_vout->i_changes |= VOUT_ASPECT_CHANGE;

    vlc_ureduce( &i_aspect_num, &i_aspect_den,
                 p_vout->fmt_in.i_aspect, VOUT_ASPECT_FACTOR, 0 );
    msg_Dbg( p_vout, "new aspect-ratio %i:%i, sample aspect-ratio %i:%i",
             i_aspect_num, i_aspect_den,
             p_vout->fmt_in.i_sar_num, p_vout->fmt_in.i_sar_den );

    if( var_Get( p_vout, "crop", &val ) )
        return VLC_EGENERIC;

    int i_ret = CropCallback( p_this, "crop", val, val, 0 );
    free( val.psz_string );
    return i_ret;
}

static int ScalingCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)oldval; (void)newval; (void)p_data;

    vlc_mutex_lock( &p_vout->change_lock );

    if( !strcmp( psz_cmd, "autoscale" ) )
    {
        p_vout->i_changes |= VOUT_SCALE_CHANGE;
    }
    else if( !strcmp( psz_cmd, "scale" ) )
    {
        p_vout->i_changes |= VOUT_ZOOM_CHANGE;
    }

    vlc_mutex_unlock( &p_vout->change_lock );

    return VLC_SUCCESS;
}

static int OnTopCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    vlc_mutex_lock( &p_vout->change_lock );
    p_vout->i_changes |= VOUT_ON_TOP_CHANGE;
    p_vout->b_on_top = newval.b_bool;
    vlc_mutex_unlock( &p_vout->change_lock );

    /* Modify libvlc as well because the vout might have to be restarted */
    var_Create( p_vout->p_libvlc, "video-on-top", VLC_VAR_BOOL );
    var_Set( p_vout->p_libvlc, "video-on-top", newval );

    (void)psz_cmd; (void)oldval; (void)p_data;
    return VLC_SUCCESS;
}

static int FullscreenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vlc_value_t val;
    (void)psz_cmd; (void)oldval; (void)p_data;

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    /* Modify libvlc as well because the vout might have to be restarted */
    var_Create( p_vout->p_libvlc, "fullscreen", VLC_VAR_BOOL );
    var_Set( p_vout->p_libvlc, "fullscreen", newval );

    val.b_bool = true;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}

static int SnapshotCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* FIXME: this is definitely not thread-safe */
    p_vout->p->b_snapshot = true;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(newval); VLC_UNUSED(p_data);
    return VLC_SUCCESS;
}

static int TitleShowCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->b_title_show = newval.b_bool;
    return VLC_SUCCESS;
}

static int TitleTimeoutCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->i_title_timeout = (mtime_t) newval.i_int;
    return VLC_SUCCESS;
}

static int TitlePositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->i_title_position = newval.i_int;
    return VLC_SUCCESS;
}
