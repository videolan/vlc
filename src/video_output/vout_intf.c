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
#include <dirent.h>                                             /* opendir() */
#include <assert.h>
#include <time.h>                                           /* strftime */

#include <vlc_interface.h>
#include <vlc_block.h>
#include <vlc_playlist.h>

#include <vlc_vout.h>
#include <vlc_image.h>
#include <vlc_vout_osd.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include "../libvlc.h"
#include "vout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* Object variables callbacks */
static int ZoomCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int CropCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int CropBorderCallback( vlc_object_t *, char const *,
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
static int VideoFilterCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int VideoSplitterCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );
static int SubFilterCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int SubMarginCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int TitleShowCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int TitleTimeoutCallback( vlc_object_t *, char const *,
                                 vlc_value_t, vlc_value_t, void * );
static int TitlePositionCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );

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
    { "235:100", "2.35:1" },
    { "239:100", "2.39:1" },
    { "5:4", "5:4" },
    { NULL, NULL } };

static void AddCustomRatios( vout_thread_t *p_vout, const char *psz_var,
                             char *psz_list )
{
    assert( psz_list );

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
    var_Create( p_vout, "align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_vout, "video-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "video-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_vout, "mouse-hide-timeout",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    p_vout->p->title.show = var_CreateGetBool( p_vout, "video-title-show" );
    p_vout->p->title.timeout = var_CreateGetInteger( p_vout,
                                                     "video-title-timeout" );
    p_vout->p->title.position = var_CreateGetInteger( p_vout,
                                                      "video-title-position" );
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

    var_AddCallback( p_vout, "crop-left", CropBorderCallback, NULL );
    var_AddCallback( p_vout, "crop-top", CropBorderCallback, NULL );
    var_AddCallback( p_vout, "crop-right", CropBorderCallback, NULL );
    var_AddCallback( p_vout, "crop-bottom", CropBorderCallback, NULL );

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

    /* Add custom crop ratios */
    psz_buf = var_CreateGetNonEmptyString( p_vout, "custom-crop-ratios" );
    if( psz_buf )
    {
        AddCustomRatios( p_vout, "crop", psz_buf );
        free( psz_buf );
    }

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
            i_aspect = us_atof( val.psz_string );
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
    psz_buf = var_CreateGetNonEmptyString( p_vout, "custom-aspect-ratios" );
    if( psz_buf )
    {
        AddCustomRatios( p_vout, "aspect-ratio", psz_buf );
        free( psz_buf );
    }

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

    var_Create( p_vout, "scale", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Scale factor");
    var_Change( p_vout, "scale", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "scale", ScalingCallback, NULL );

    /* Add a variable to indicate if the window should be on top of others */
    var_Create( p_vout, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Always on top");
    var_Change( p_vout, "video-on-top", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-on-top", OnTopCallback, NULL );
    var_TriggerCallback( p_vout, "video-on-top" );

    /* Add a variable to indicate whether we want window decoration or not */
    var_Create( p_vout, "video-deco", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Add a fullscreen variable */
    var_Create( p_vout, "fullscreen",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Fullscreen");
    var_Change( p_vout, "fullscreen", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "fullscreen", FullscreenCallback, NULL );

    /* Add a snapshot variable */
    var_Create( p_vout, "video-snapshot", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Snapshot");
    var_Change( p_vout, "video-snapshot", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-snapshot", SnapshotCallback, NULL );

    /* Add a video-filter variable */
    var_Create( p_vout, "video-filter",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "video-filter", VideoFilterCallback, NULL );
    var_TriggerCallback( p_vout, "video-filter" );

    /* Add a video-splitter variable
     * TODO rename vout-filter into vout-splitter */
    var_Create( p_vout, "vout-filter",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Video splitter");
    var_Change( p_vout, "vout-filter", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "vout-filter", VideoSplitterCallback, NULL );

    /* Add a sub-filter variable */
    var_Create( p_vout, "sub-filter",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "sub-filter", SubFilterCallback, NULL );
    var_TriggerCallback( p_vout, "sub-filter" );

    /* Add sub-margin variable */
    var_Create( p_vout, "sub-margin",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "sub-margin", SubMarginCallback, NULL );
    var_TriggerCallback( p_vout, "sub-margin" );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_COORDS );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_COORDS );
    var_Create( p_vout, "mouse-object", VLC_VAR_BOOL );
}

/*****************************************************************************
 * vout_Snapshot: generates a snapshot.
 *****************************************************************************/
/**
 * This function will inject a subpicture into the vout with the provided
 * picture
 */
static int VoutSnapshotPip( vout_thread_t *p_vout, picture_t *p_pic )
{
    subpicture_t *p_subpic = subpicture_NewFromPicture( VLC_OBJECT(p_vout),
                                                        p_pic, VLC_CODEC_YUVA );
    if( !p_subpic )
        return VLC_EGENERIC;

    /* FIXME SPU_DEFAULT_CHANNEL is not good (used by the text) but
     * hardcoded 0 doesn't seem right */
    p_subpic->i_channel = 0;
    p_subpic->i_start = mdate();
    p_subpic->i_stop  = p_subpic->i_start + 4000000;
    p_subpic->b_ephemer = true;
    p_subpic->b_fade = true;

    /* Reduce the picture to 1/4^2 of the screen */
    p_subpic->i_original_picture_width  *= 4;
    p_subpic->i_original_picture_height *= 4;

    vout_PutSubpicture( p_vout, p_subpic );
    return VLC_SUCCESS;
}

/**
 * This function will display the name and a PIP of the provided snapshot
 */
static void VoutOsdSnapshot( vout_thread_t *p_vout, picture_t *p_pic, const char *psz_filename )
{
    msg_Dbg( p_vout, "snapshot taken (%s)", psz_filename );
    vout_OSDMessage( p_vout, SPU_DEFAULT_CHANNEL, "%s", psz_filename );

    if( var_GetBool( p_vout, "snapshot-preview" ) )
    {
        if( VoutSnapshotPip( p_vout, p_pic ) )
            msg_Warn( p_vout, "Failed to display snapshot" );
    }
}

/* */
int vout_GetSnapshot( vout_thread_t *p_vout,
                      block_t **pp_image, picture_t **pp_picture,
                      video_format_t *p_fmt,
                      const char *psz_format, mtime_t i_timeout )
{
    picture_t *p_picture = vout_snapshot_Get( &p_vout->p->snapshot, i_timeout );
    if( !p_picture )
    {
        msg_Err( p_vout, "Failed to grab a snapshot" );
        return VLC_EGENERIC;
    }

    if( pp_image )
    {
        vlc_fourcc_t i_format = VLC_CODEC_PNG;
        if( psz_format && image_Type2Fourcc( psz_format ) )
            i_format = image_Type2Fourcc( psz_format );

        const int i_override_width  = var_GetInteger( p_vout, "snapshot-width" );
        const int i_override_height = var_GetInteger( p_vout, "snapshot-height" );

        if( picture_Export( VLC_OBJECT(p_vout), pp_image, p_fmt,
                            p_picture, i_format, i_override_width, i_override_height ) )
        {
            msg_Err( p_vout, "Failed to convert image for snapshot" );
            picture_Release( p_picture );
            return VLC_EGENERIC;
        }
    }
    if( pp_picture )
        *pp_picture = p_picture;
    else
        picture_Release( p_picture );
    return VLC_SUCCESS;
}

/**
 * This function will handle a snapshot request
 */
static void VoutSaveSnapshot( vout_thread_t *p_vout )
{
    char *psz_path = var_GetNonEmptyString( p_vout, "snapshot-path" );
    char *psz_format = var_GetNonEmptyString( p_vout, "snapshot-format" );
    char *psz_prefix = var_GetNonEmptyString( p_vout, "snapshot-prefix" );

    /* */
    picture_t *p_picture;
    block_t *p_image;
    video_format_t fmt;

    /* 500ms timeout
     * XXX it will cause trouble with low fps video (< 2fps) */
    if( vout_GetSnapshot( p_vout, &p_image, &p_picture, &fmt, psz_format, 500*1000 ) )
    {
        p_picture = NULL;
        p_image = NULL;
        goto exit;
    }

    if( !psz_path )
    {
        psz_path = vout_snapshot_GetDirectory();
        if( !psz_path )
        {
            msg_Err( p_vout, "no path specified for snapshots" );
            goto exit;
        }
    }

    vout_snapshot_save_cfg_t cfg;
    memset( &cfg, 0, sizeof(cfg) );
    cfg.is_sequential = var_GetBool( p_vout, "snapshot-sequential" );
    cfg.sequence = var_GetInteger( p_vout, "snapshot-num" );
    cfg.path = psz_path;
    cfg.format = psz_format;
    cfg.prefix_fmt = psz_prefix;

    char *psz_filename;
    int  i_sequence;
    if (vout_snapshot_SaveImage( &psz_filename, &i_sequence,
                                 p_image, VLC_OBJECT(p_vout), &cfg ) )
        goto exit;
    if( cfg.is_sequential )
        var_SetInteger( p_vout, "snapshot-num", i_sequence + 1 );

    VoutOsdSnapshot( p_vout, p_picture, psz_filename );

    /* signal creation of a new snapshot file */
    var_SetString( p_vout->p_libvlc, "snapshot-file", psz_filename );

    free( psz_filename );

exit:
    if( p_image )
        block_Release( p_image );
    if( p_picture )
        picture_Release( p_picture );
    free( psz_prefix );
    free( psz_format );
    free( psz_path );
}

/*****************************************************************************
 * Handle filters
 *****************************************************************************/

void vout_EnableFilter( vout_thread_t *p_vout, const char *psz_name,
                        bool b_add, bool b_setconfig )
{
    char *psz_parser;
    char *psz_string;
    const char *psz_filter_type;

    /* FIXME temporary hack */
    const char *psz_module_name = psz_name;
    if( !strcmp( psz_name, "magnify" ) ||
        !strcmp( psz_name, "puzzle" ) ||
        !strcmp( psz_name, "logo" ) ||
        !strcmp( psz_name, "wall" ) ||
        !strcmp( psz_name, "clone" ) )
        psz_module_name = "video_filter_wrapper";

    module_t *p_obj = module_find( psz_module_name );
    if( !p_obj )
    {
        msg_Err( p_vout, "Unable to find filter module \"%s\".", psz_name );
        return;
    }

    if( module_provides( p_obj, "video filter" ) )
    {
        psz_filter_type = "vout-filter";
    }
    else if( module_provides( p_obj, "video filter2" ) )
    {
        psz_filter_type = "video-filter";
    }
    else if( module_provides( p_obj, "sub filter" ) )
    {
        psz_filter_type = "sub-filter";
    }
    else
    {
        module_release( p_obj );
        msg_Err( p_vout, "Unknown video filter type." );
        return;
    }
    module_release( p_obj );

    psz_string = var_GetString( p_vout, psz_filter_type );

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
    {
        config_PutPsz( p_vout, psz_filter_type, psz_string );
    }

    var_SetString( p_vout, psz_filter_type, psz_string );

    free( psz_string );
}

/*****************************************************************************
 * Object variables callbacks
 *****************************************************************************/
static int ZoomCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_cmd; (void)oldval; (void)p_data;

    return var_SetFloat( p_this, "scale", newval.f_float );
}

static int CropCallback( vlc_object_t *object, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data )
{
    vout_thread_t *vout = (vout_thread_t *)object;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);
    unsigned num, den;
    unsigned y, x;
    unsigned width, height;
    unsigned left, top, right, bottom;

    if (sscanf(newval.psz_string, "%u:%u", &num, &den) == 2) {
        vout_ControlChangeCropRatio(vout, num, den);
    } else if (sscanf(newval.psz_string, "%ux%u+%u+%u",
                      &width, &height, &x, &y) == 4) {
        vout_ControlChangeCropWindow(vout, x, y, width, height);
    } else if (sscanf(newval.psz_string, "%u+%u+%u+%u",
                    &left, &top, &right, &bottom) == 4) {
        vout_ControlChangeCropBorder(vout, left, top, right, bottom);
    } else if (*newval.psz_string == '\0') {
        vout_ControlChangeCropRatio(vout, 0, 0);
    } else {
        msg_Err(object, "Unknown crop format (%s)", newval.psz_string);
    }
    return VLC_SUCCESS;
}

static int CropBorderCallback(vlc_object_t *object, char const *cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_thread_t *vout = (vout_thread_t *)object;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data); VLC_UNUSED(newval);

    vout_ControlChangeCropBorder(vout,
                                 var_GetInteger(object, "crop-left"),
                                 var_GetInteger(object, "crop-top"),
                                 var_GetInteger(object, "crop-right"),
                                 var_GetInteger(object, "crop-bottom"));
    return VLC_SUCCESS;
}

static int AspectCallback( vlc_object_t *object, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data )
{
    vout_thread_t *vout = (vout_thread_t *)object;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);
    unsigned num, den;

    if (sscanf(newval.psz_string, "%u:%u", &num, &den) == 2 &&
        (num > 0) == (den > 0))
        vout_ControlChangeSampleAspectRatio(vout, num, den);
    else if (*newval.psz_string == '\0')
        vout_ControlChangeSampleAspectRatio(vout, 0, 0);
    return VLC_SUCCESS;
}

static int ScalingCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)oldval; (void)newval; (void)p_data;

    if( !strcmp( psz_cmd, "autoscale" ) )
        vout_ControlChangeDisplayFilled( p_vout, newval.b_bool );
    else if( !strcmp( psz_cmd, "scale" ) )
        vout_ControlChangeZoom( p_vout, 1000 * newval.f_float, 1000 );

    return VLC_SUCCESS;
}

static int OnTopCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)psz_cmd; (void)oldval; (void)p_data;

    vout_ControlChangeOnTop( p_vout, newval.b_bool );
    return VLC_SUCCESS;
}

static int FullscreenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    (void)psz_cmd; (void)p_data;

    if( oldval.b_bool != newval.b_bool )
        vout_ControlChangeFullscreen( p_vout, newval.b_bool );
    return VLC_SUCCESS;
}

static int SnapshotCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(newval); VLC_UNUSED(p_data);

    VoutSaveSnapshot( p_vout );
    return VLC_SUCCESS;
}

static int VideoFilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                                vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    vout_ControlChangeFilters( p_vout, newval.psz_string );
    return VLC_SUCCESS;
}

static int VideoSplitterCallback( vlc_object_t *p_this, char const *psz_cmd,
                                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    input_thread_t *p_input;
    (void)psz_cmd; (void)oldval; (void)p_data;

    p_input = (input_thread_t *)vlc_object_find( p_this, VLC_OBJECT_INPUT,
                                                 FIND_PARENT );
    if (!p_input)
    {
        msg_Err( p_vout, "Input not found" );
        return VLC_EGENERIC;
    }

    /* Modify input as well because the vout might have to be restarted */
    var_Create( p_input, "vout-filter", VLC_VAR_STRING );
    var_SetString( p_input, "vout-filter", newval.psz_string );

    /* Now restart current video stream */
    input_Control( p_input, INPUT_RESTART_ES, -VIDEO_ES );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

static int SubFilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    vout_ControlChangeSubFilters( p_vout, newval.psz_string );
    return VLC_SUCCESS;
}

static int SubMarginCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    vout_ControlChangeSubMargin( p_vout, newval.i_int );
    return VLC_SUCCESS;
}

static int TitleShowCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->title.show = newval.b_bool;
    return VLC_SUCCESS;
}

static int TitleTimeoutCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->title.timeout = (mtime_t) newval.i_int;
    return VLC_SUCCESS;
}

static int TitlePositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(p_data);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    p_vout->p->title.position = newval.i_int;
    return VLC_SUCCESS;
}
