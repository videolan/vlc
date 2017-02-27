/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2007 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <stdio.h>
#include <stdlib.h>                                                /* free() */
#include <assert.h>

#include <vlc_block.h>
#include <vlc_modules.h>

#include <vlc_vout.h>
#include <vlc_vout_osd.h>
#include <vlc_strings.h>
#include <vlc_charset.h>
#include "vout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* Object variables callbacks */
static int CropCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int CropBorderCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int AspectCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int AutoScaleCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int ZoomCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int AboveCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int WallPaperCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int FullscreenCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );
static int SnapshotCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );
static int VideoFilterCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int SubSourceCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int SubFilterCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int SubMarginCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int ViewpointCallback( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * vout_IntfInit: called during the vout creation to initialise misc things.
 *****************************************************************************/
static const struct
{
    double f_value;
    char psz_label[13];
} p_zoom_values[] = {
    { 0.25, N_("1:4 Quarter") },
    { 0.5, N_("1:2 Half") },
    { 1, N_("1:1 Original") },
    { 2, N_("2:1 Double") },
};

static const struct
{
    char psz_value[8];
    char psz_label[8];
} p_crop_values[] = {
    { "", N_("Default") },
    { "16:10", "16:10" },
    { "16:9", "16:9" },
    { "4:3", "4:3" },
    { "185:100", "1.85:1" },
    { "221:100", "2.21:1" },
    { "235:100", "2.35:1" },
    { "239:100", "2.39:1" },
    { "5:3", "5:3" },
    { "5:4", "5:4" },
    { "1:1", "1:1" },
};

static const struct
{
    char psz_value[8];
    char psz_label[8];
} p_aspect_ratio_values[] = {
    { "", N_("Default") },
    { "16:9", "16:9" },
    { "4:3", "4:3" },
    { "1:1", "1:1" },
    { "16:10", "16:10" },
    { "221:100", "2.21:1" },
    { "235:100", "2.35:1" },
    { "239:100", "2.39:1" },
    { "5:4", "5:4" },
};

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
    vlc_value_t val, text;
    char *psz_buf;

    /* Create a few object variables we'll need later on */
    var_Create( p_vout, "snapshot-num", VLC_VAR_INTEGER );
    var_SetInteger( p_vout, "snapshot-num", 1 );

    var_Create( p_vout, "width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_vout, "mouse-hide-timeout",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Add variables to manage scaling video */
    var_Create( p_vout, "autoscale", VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Autoscale video");
    var_Change( p_vout, "autoscale", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "autoscale", AutoScaleCallback, NULL );

    var_Create( p_vout, "zoom", VLC_VAR_FLOAT | VLC_VAR_ISCOMMAND |
                VLC_VAR_DOINHERIT );

    text.psz_string = _("Zoom");
    var_Change( p_vout, "zoom", VLC_VAR_SETTEXT, &text, NULL );

    for( size_t i = 0; i < ARRAY_SIZE(p_zoom_values); i++ )
    {
        val.f_float = p_zoom_values[i].f_value;
        text.psz_string = vlc_gettext( p_zoom_values[i].psz_label );
        var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    }

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
                VLC_VAR_DOINHERIT );

    text.psz_string = _("Crop");
    var_Change( p_vout, "crop", VLC_VAR_SETTEXT, &text, NULL );

    for( size_t i = 0; i < ARRAY_SIZE(p_crop_values); i++ )
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

    /* Monitor pixel aspect-ratio */
    var_Create( p_vout, "monitor-par", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* Aspect-ratio object var */
    var_Create( p_vout, "aspect-ratio", VLC_VAR_STRING | VLC_VAR_ISCOMMAND |
                VLC_VAR_DOINHERIT );

    text.psz_string = _("Aspect ratio");
    var_Change( p_vout, "aspect-ratio", VLC_VAR_SETTEXT, &text, NULL );

    for( size_t i = 0; i < ARRAY_SIZE(p_aspect_ratio_values); i++ )
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

    /* Add a variable to indicate if the window should be on top of others */
    var_Create( p_vout, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Always on top");
    var_Change( p_vout, "video-on-top", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-on-top", AboveCallback, NULL );

    /* Add a variable to indicate if the window should be below all others */
    var_Create( p_vout, "video-wallpaper", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_AddCallback( p_vout, "video-wallpaper", WallPaperCallback,
                     (void *)(uintptr_t)VOUT_WINDOW_STATE_BELOW );

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

    /* Add a sub-source variable */
    var_Create( p_vout, "sub-source",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "sub-source", SubSourceCallback, NULL );

    /* Add a sub-filter variable */
    var_Create( p_vout, "sub-filter",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "sub-filter", SubFilterCallback, NULL );

    /* Add sub-margin variable */
    var_Create( p_vout, "sub-margin",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_vout, "sub-margin", SubMarginCallback, NULL );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_COORDS );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_COORDS );

    /* Device orientation */
    var_Create( p_vout, "viewpoint-moved", VLC_VAR_ADDRESS );

    /* Viewpoint */
    var_Create( p_vout, "viewpoint", VLC_VAR_ADDRESS  );
    var_AddCallback( p_vout, "viewpoint", ViewpointCallback, NULL );
    var_Create( p_vout, "viewpoint-changeable", VLC_VAR_BOOL );

    vout_IntfReinit( p_vout );
}

void vout_IntfReinit( vout_thread_t *p_vout )
{
    var_TriggerCallback( p_vout, "zoom" );
    var_TriggerCallback( p_vout, "crop" );
    var_TriggerCallback( p_vout, "aspect-ratio" );

    var_TriggerCallback( p_vout, "video-on-top" );
    var_TriggerCallback( p_vout, "video-wallpaper" );

    var_TriggerCallback( p_vout, "video-filter" );
    var_TriggerCallback( p_vout, "sub-source" );
    var_TriggerCallback( p_vout, "sub-filter" );
    var_TriggerCallback( p_vout, "sub-margin" );
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
    vout_OSDMessage( p_vout, VOUT_SPU_CHANNEL_OSD, "%s", psz_filename );

    if( var_InheritBool( p_vout, "snapshot-preview" ) )
    {
        if( VoutSnapshotPip( p_vout, p_pic ) )
            msg_Warn( p_vout, "Failed to display snapshot" );
    }
}

/**
 * This function will handle a snapshot request
 */
static void VoutSaveSnapshot( vout_thread_t *p_vout )
{
    char *psz_path = var_InheritString( p_vout, "snapshot-path" );
    char *psz_format = var_InheritString( p_vout, "snapshot-format" );
    char *psz_prefix = var_InheritString( p_vout, "snapshot-prefix" );

    /* */
    picture_t *p_picture;
    block_t *p_image;

    /* 500ms timeout
     * XXX it will cause trouble with low fps video (< 2fps) */
    if( vout_GetSnapshot( p_vout, &p_image, &p_picture, NULL, psz_format, 500*1000 ) )
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
    cfg.is_sequential = var_InheritBool( p_vout, "snapshot-sequential" );
    cfg.sequence = var_GetInteger( p_vout, "snapshot-num" );
    cfg.path = psz_path;
    cfg.format = psz_format;
    cfg.prefix_fmt = psz_prefix;

    char *psz_filename;
    int  i_sequence;
    if (vout_snapshot_SaveImage( &psz_filename, &i_sequence,
                                 p_image, p_vout, &cfg ) )
        goto exit;
    if( cfg.is_sequential )
        var_SetInteger( p_vout, "snapshot-num", i_sequence + 1 );

    VoutOsdSnapshot( p_vout, p_picture, psz_filename );

    /* signal creation of a new snapshot file */
    var_SetString( p_vout->obj.libvlc, "snapshot-file", psz_filename );

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
 * Object variables callbacks
 *****************************************************************************/
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
    char buf[4 * 21];

    snprintf(buf, sizeof (buf), "%"PRIu64"+%"PRIu64"+%"PRIu64"+%"PRIu64,
             var_GetInteger(object, "crop-left"),
             var_GetInteger(object, "crop-top"),
             var_GetInteger(object, "crop-right"),
             var_GetInteger(object, "crop-bottom"));
    var_SetString(object, "crop", buf);

    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data); VLC_UNUSED(newval);
    return VLC_SUCCESS;
}

static int AspectCallback( vlc_object_t *object, char const *cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *data )
{
    vout_thread_t *vout = (vout_thread_t *)object;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);
    unsigned num, den;

    if (sscanf(newval.psz_string, "%u:%u", &num, &den) == 2 &&
        (num != 0) == (den != 0))
        vout_ControlChangeSampleAspectRatio(vout, num, den);
    else if (*newval.psz_string == '\0')
        vout_ControlChangeSampleAspectRatio(vout, 0, 0);
    return VLC_SUCCESS;
}

static int AutoScaleCallback( vlc_object_t *obj, char const *name,
                              vlc_value_t prev, vlc_value_t cur, void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *)obj;

    (void) name; (void) prev; (void) data;
    vout_ControlChangeDisplayFilled( p_vout, cur.b_bool );
    return VLC_SUCCESS;
}

static int ZoomCallback( vlc_object_t *obj, char const *name,
                         vlc_value_t prev, vlc_value_t cur, void *data )
{
    vout_thread_t *p_vout = (vout_thread_t *)obj;

    (void) name; (void) prev; (void) data;
    vout_ControlChangeZoom( p_vout, 1000 * cur.f_float, 1000 );
    return VLC_SUCCESS;
}

static int AboveCallback( vlc_object_t *obj, char const *name,
                          vlc_value_t prev, vlc_value_t cur, void *data )
{
    vout_ControlChangeWindowState( (vout_thread_t *)obj,
        cur.b_bool ? VOUT_WINDOW_STATE_ABOVE : VOUT_WINDOW_STATE_NORMAL );
    (void) name; (void) prev; (void) data;
    return VLC_SUCCESS;
}

static int WallPaperCallback( vlc_object_t *obj, char const *name,
                              vlc_value_t prev, vlc_value_t cur, void *data )
{
    vout_thread_t *vout = (vout_thread_t *)obj;

    if( cur.b_bool )
    {
        vout_ControlChangeWindowState( vout, VOUT_WINDOW_STATE_BELOW );
        vout_ControlChangeFullscreen( vout, true );
    }
    else
    {
        var_TriggerCallback( obj, "fullscreen" );
        var_TriggerCallback( obj, "video-on-top" );
    }
    (void) name; (void) prev; (void) data;
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

static int SubSourceCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    vout_ControlChangeSubSources( p_vout, newval.psz_string );
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

static int ViewpointCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( newval.p_address != NULL )
        vout_ControlChangeViewpoint( p_vout, newval.p_address );
    return VLC_SUCCESS;
}
