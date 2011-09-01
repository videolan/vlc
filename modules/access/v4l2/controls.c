/*****************************************************************************
 * controls.c : Video4Linux2 device controls for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 the VideoLAN team
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "v4l2.h"
/* TODO: make callbacks independent of object type */
/* TODO: remove callbacks at exit */
#include <vlc_access.h>
#include <vlc_demux.h>

#include <ctype.h>
#include <sys/ioctl.h>

static const struct
{
    const char *psz_name;
    unsigned int i_cid;
} controls[] =
{
    { "brightness", V4L2_CID_BRIGHTNESS },
    { "contrast", V4L2_CID_CONTRAST },
    { "saturation", V4L2_CID_SATURATION },
    { "hue", V4L2_CID_HUE },
    { "audio-volume", V4L2_CID_AUDIO_VOLUME },
    { "audio-balance", V4L2_CID_AUDIO_BALANCE },
    { "audio-bass", V4L2_CID_AUDIO_BASS },
    { "audio-treble", V4L2_CID_AUDIO_TREBLE },
    { "audio-mute", V4L2_CID_AUDIO_MUTE },
    { "audio-loudness", V4L2_CID_AUDIO_LOUDNESS },
    { "black-level", V4L2_CID_BLACK_LEVEL },
    { "auto-white-balance", V4L2_CID_AUTO_WHITE_BALANCE },
    { "do-white-balance", V4L2_CID_DO_WHITE_BALANCE },
    { "red-balance", V4L2_CID_RED_BALANCE },
    { "blue-balance", V4L2_CID_BLUE_BALANCE },
    { "gamma", V4L2_CID_GAMMA },
    { "exposure", V4L2_CID_EXPOSURE },
    { "autogain", V4L2_CID_AUTOGAIN },
    { "gain", V4L2_CID_GAIN },
    { "hflip", V4L2_CID_HFLIP },
    { "vflip", V4L2_CID_VFLIP },
    { "hcenter", V4L2_CID_HCENTER },
    { "vcenter", V4L2_CID_VCENTER },
    { NULL, 0 }
};

static void name2var( unsigned char *name )
{
    for( ; *name; name++ )
        *name = (*name == ' ') ? '_' : tolower( *name );
}

/*****************************************************************************
 * Issue user-class v4l2 controls
 *****************************************************************************/
static int Control( vlc_object_t *p_obj, int i_fd,
                    const char *psz_name, int i_cid, int i_value )
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    struct v4l2_ext_control ext_control;
    struct v4l2_ext_controls ext_controls;

    if( i_value == -1 )
        return VLC_SUCCESS;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    queryctrl.id = i_cid;

    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) < 0
        || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
    {
        msg_Dbg( p_obj, "%s (%x) control is not supported.", psz_name, i_cid );
        return VLC_EGENERIC;
    }

    memset( &control, 0, sizeof( control ) );
    memset( &ext_control, 0, sizeof( ext_control ) );
    memset( &ext_controls, 0, sizeof( ext_controls ) );
    control.id = i_cid;
    ext_control.id = i_cid;
    ext_controls.ctrl_class = V4L2_CTRL_ID2CLASS( i_cid );
    ext_controls.count = 1;
    ext_controls.controls = &ext_control;

    int i_ret = -1;

    if( i_value >= queryctrl.minimum && i_value <= queryctrl.maximum )
    {
        ext_control.value = i_value;
        if( v4l2_ioctl( i_fd, VIDIOC_S_EXT_CTRLS, &ext_controls ) < 0 )
        {
            control.value = i_value;
            if( v4l2_ioctl( i_fd, VIDIOC_S_CTRL, &control ) < 0 )
            {
                msg_Err( p_obj, "unable to set %s (%x) to %d (%m)",
                         psz_name, i_cid, i_value );
                return VLC_EGENERIC;
            }
            i_ret = v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control );
        }
        else
        {
            i_ret = v4l2_ioctl( i_fd, VIDIOC_G_EXT_CTRLS, &ext_controls );
            control.value = ext_control.value;
        }
    }

    if( i_ret >= 0 )
    {
        vlc_value_t val;
        msg_Dbg( p_obj, "video %s: %d", psz_name, control.value );
        switch( var_Type( p_obj, psz_name ) & VLC_VAR_TYPE )
        {
            case VLC_VAR_BOOL:
                val.b_bool = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_TriggerCallback( p_obj, "controls-update" );
                break;
            case VLC_VAR_INTEGER:
                val.i_int = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_TriggerCallback( p_obj, "controls-update" );
                break;
        }
    }
    return VLC_SUCCESS;
}

static int DemuxControlCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    (void)oldval;
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_cid = (long int)p_data;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

static int AccessControlCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    (void)oldval;
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;
    int i_cid = (long int)p_data;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

/**
 * Resets all user-class V4L2 controls to their default value
 */
static int ControlReset( vlc_object_t *p_obj, int i_fd )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;
    memset( &queryctrl, 0, sizeof( queryctrl ) );

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
    {
        /* Extended control API supported */
        queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            if( queryctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS
             || V4L2_CTRL_ID2CLASS( queryctrl.id ) == V4L2_CTRL_CLASS_MPEG )
            {
                queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
                continue;
            }
            struct v4l2_control control;
            memset( &control, 0, sizeof( control ) );
            control.id = queryctrl.id;
            if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
             && queryctrl.default_value != control.value )
            {
                int i;
                for( i = 0; controls[i].psz_name != NULL; i++ )
                    if( controls[i].i_cid == queryctrl.id ) break;
                name2var( queryctrl.name );
                Control( p_obj, i_fd,
                         controls[i].psz_name ? controls[i].psz_name
                          : (const char *)queryctrl.name,
                         queryctrl.id, queryctrl.default_value );
            }
            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }
    else
    {

        /* public controls */
        for( i_cid = V4L2_CID_BASE;
             i_cid < V4L2_CID_LASTP1;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                struct v4l2_control control;
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
                 && queryctrl.default_value != control.value )
                {
                    int i;
                    for( i = 0; controls[i].psz_name != NULL; i++ )
                        if( controls[i].i_cid == queryctrl.id ) break;
                    name2var( queryctrl.name );
                    Control( p_obj, i_fd,
                             controls[i].psz_name ? controls[i].psz_name
                              : (const char *)queryctrl.name,
                             queryctrl.id, queryctrl.default_value );
                }
            }
        }

        /* private controls */
        for( i_cid = V4L2_CID_PRIVATE_BASE;
             ;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                struct v4l2_control control;
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
                 && queryctrl.default_value != control.value )
                {
                    name2var( queryctrl.name );
                    Control( p_obj, i_fd, (const char *)queryctrl.name,
                             queryctrl.id, queryctrl.default_value );
                }
            }
            else
                break;
        }
    }
    return VLC_SUCCESS;
}

static int DemuxControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_var;    (void)oldval;    (void)newval;    (void)p_data;
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}

static int AccessControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_var;     (void)oldval;     (void)newval;     (void)p_data;
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Print a user-class v4l2 control's details, create the relevant variable,
 * change the value if needed.
 *****************************************************************************/
static void ControlListPrint( vlc_object_t *p_obj, int i_fd,
                              struct v4l2_queryctrl queryctrl,
                              bool b_reset, bool b_demux )
{
    struct v4l2_querymenu querymenu;
    unsigned int i_mid;

    int i;
    int i_val;

    char *psz_name;
    vlc_value_t val, val2;

    if( queryctrl.flags & V4L2_CTRL_FLAG_GRABBED )
        msg_Dbg( p_obj, "    control is busy" );
    if( queryctrl.flags & V4L2_CTRL_FLAG_READ_ONLY )
        msg_Dbg( p_obj, "    control is read-only" );

    for( i = 0; controls[i].psz_name != NULL; i++ )
        if( controls[i].i_cid == queryctrl.id ) break;

    if( controls[i].psz_name )
    {
        psz_name = strdup( controls[i].psz_name );
        char psz_cfg_name[40];
        sprintf( psz_cfg_name, CFG_PREFIX "%s", psz_name );
        i_val = var_CreateGetInteger( p_obj, psz_cfg_name );
        var_Destroy( p_obj, psz_cfg_name );
    }
    else
    {
        psz_name = strdup( (const char *)queryctrl.name );
        name2var( (unsigned char *)psz_name );
        i_val = -1;
    }

    switch( queryctrl.type )
    {
        case V4L2_CTRL_TYPE_INTEGER:
            msg_Dbg( p_obj, "    integer control" );
            msg_Dbg( p_obj,
                     "    valid values: %d to %d by steps of %d",
                     queryctrl.minimum, queryctrl.maximum,
                     queryctrl.step );

            var_Create( p_obj, psz_name,
                        VLC_VAR_INTEGER | VLC_VAR_HASMIN | VLC_VAR_HASMAX
                      | VLC_VAR_HASSTEP | VLC_VAR_ISCOMMAND );
            val.i_int = queryctrl.minimum;
            var_Change( p_obj, psz_name, VLC_VAR_SETMIN, &val, NULL );
            val.i_int = queryctrl.maximum;
            var_Change( p_obj, psz_name, VLC_VAR_SETMAX, &val, NULL );
            val.i_int = queryctrl.step;
            var_Change( p_obj, psz_name, VLC_VAR_SETSTEP, &val, NULL );
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
            msg_Dbg( p_obj, "    boolean control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_BOOL | VLC_VAR_ISCOMMAND );
            break;
        case V4L2_CTRL_TYPE_MENU:
            msg_Dbg( p_obj, "    menu control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_INTEGER | VLC_VAR_HASCHOICE
                      | VLC_VAR_ISCOMMAND );
            memset( &querymenu, 0, sizeof( querymenu ) );
            for( i_mid = queryctrl.minimum;
                 i_mid <= (unsigned)queryctrl.maximum;
                 i_mid++ )
            {
                querymenu.index = i_mid;
                querymenu.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_QUERYMENU, &querymenu ) >= 0 )
                {
                    msg_Dbg( p_obj, "        %d: %s",
                             querymenu.index, querymenu.name );
                    val.i_int = querymenu.index;
                    val2.psz_string = (char *)querymenu.name;
                    var_Change( p_obj, psz_name,
                                VLC_VAR_ADDCHOICE, &val, &val2 );
                }
            }
            break;
        case V4L2_CTRL_TYPE_BUTTON:
            msg_Dbg( p_obj, "    button control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
            break;
        case V4L2_CTRL_TYPE_CTRL_CLASS:
            msg_Dbg( p_obj, "    control class" );
            var_Create( p_obj, psz_name, VLC_VAR_VOID );
            break;
        default:
            msg_Dbg( p_obj, "    unknown control type (FIXME)" );
            /* FIXME */
            break;
    }

    switch( queryctrl.type )
    {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_MENU:
            {
                struct v4l2_control control;
                msg_Dbg( p_obj, "    default value: %d",
                         queryctrl.default_value );
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0 )
                {
                    msg_Dbg( p_obj, "    current value: %d", control.value );
                }
                if( i_val == -1 )
                {
                    i_val = control.value;
                    if( b_reset && queryctrl.default_value != control.value )
                    {
                        msg_Dbg( p_obj, "    reset value to default" );
                        Control( p_obj, i_fd, psz_name,
                                 queryctrl.id, queryctrl.default_value );
                    }
                }
                else
                {
                    Control( p_obj, i_fd, psz_name, queryctrl.id, i_val );
                }
            }
            break;
        default:
            break;
    }

    val.psz_string = (char *)queryctrl.name;
    var_Change( p_obj, psz_name, VLC_VAR_SETTEXT, &val, NULL );
    val.i_int = queryctrl.id;
    val2.psz_string = (char *)psz_name;
    var_Change( p_obj, "allcontrols", VLC_VAR_ADDCHOICE, &val, &val2 );
    /* bad things happen changing MPEG mid-stream
     * so don't add to Ext Settings GUI */
    if( V4L2_CTRL_ID2CLASS( queryctrl.id ) != V4L2_CTRL_CLASS_MPEG )
        var_Change( p_obj, "controls", VLC_VAR_ADDCHOICE, &val, &val2 );

    switch( var_Type( p_obj, psz_name ) & VLC_VAR_TYPE )
    {
        case VLC_VAR_BOOL:
            var_SetBool( p_obj, psz_name, i_val );
            break;
        case VLC_VAR_INTEGER:
            var_SetInteger( p_obj, psz_name, i_val );
            break;
        case VLC_VAR_VOID:
            break;
        default:
            msg_Warn( p_obj, "FIXME: %s %s %d", __FILE__, __func__,
                      __LINE__ );
            break;
    }

    if( b_demux )
        var_AddCallback( p_obj, psz_name,
                        DemuxControlCallback, (void*)(intptr_t)queryctrl.id );
    else
        var_AddCallback( p_obj, psz_name,
                        AccessControlCallback, (void*)(intptr_t)queryctrl.id );

    free( psz_name );
}

static void SetAvailControlsByString( vlc_object_t *p_obj, int i_fd )
{
    char *ctrls = var_InheritString( p_obj, CFG_PREFIX"set-ctrls" );
    if( ctrls == NULL )
        return;

    vlc_value_t val, text;

    if( var_Change( p_obj, "allcontrols", VLC_VAR_GETCHOICES, &val, &text ) )
    {
        msg_Err( p_obj, "Oops, can't find 'allcontrols' variable." );
        free( ctrls );
        return;
    }

    char *p = ctrls;
    if( *p == '{' )
        p++;

    while( p != NULL && *p && *p != '}' )
    {
        p += strspn( p, ", " );

        const char *name = p;
        char *end = strchr( p, ',' );
        if( end == NULL )
            end = strchr( p, '}' );
        if( end != NULL )
            *(end++) = '\0';

        char *value = strchr( p, '=' );
        if( value == NULL )
        {
            msg_Err( p_obj, "syntax error in \"%s\": missing '='", name );
            p = end;
            continue;
        }
        *(value++) = '\0';

        for( int i = 0; i < val.p_list->i_count; i++ )
        {
            vlc_value_t vartext;
            const char *var = text.p_list->p_values[i].psz_string;

            var_Change( p_obj, var, VLC_VAR_GETTEXT, &vartext, NULL );
            if( !strcasecmp( vartext.psz_string, name ) )
            {
                Control( p_obj, i_fd, name,
                         val.p_list->p_values[i].i_int,
                         strtol( value, NULL, 0 ) );
                free( vartext.psz_string );
                goto found;
            }
            free( vartext.psz_string );
        }
        msg_Err( p_obj, "control %s not available", name );
    found:
        p = end;
    }
    var_FreeList( &val, &text );
    free( ctrls );
}

/**
 * Lists all user-class v4l2 controls, sets them to the user specified
 * value and create the relevant variables to enable run-time changes.
 */
int ControlList( vlc_object_t *p_obj, int i_fd, bool b_demux )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;
    const bool b_reset = var_InheritBool( p_obj, CFG_PREFIX"controls-reset" );

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    /* A list of available controls (aka the variable name) will be
     * stored as choices in the "allcontrols" variable. We'll thus be able
     * to use those to create an appropriate interface
     * A list of available controls that can be changed mid-stream will
     * be stored in the "controls" variable */
    var_Create( p_obj, "controls", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    var_Create( p_obj, "allcontrols", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );

    var_Create( p_obj, "controls-update", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );

    /* Add a control to reset all controls to their default values */
    vlc_value_t val, val2;
    var_Create( p_obj, "controls-reset", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    val.psz_string = _( "Reset controls to default" );
    var_Change( p_obj, "controls-reset", VLC_VAR_SETTEXT, &val, NULL );
    val.i_int = -1;
    val2.psz_string = (char *)"controls-reset";
    var_Change( p_obj, "controls", VLC_VAR_ADDCHOICE, &val, &val2 );
    if (b_demux)
        var_AddCallback( p_obj, "controls-reset", DemuxControlResetCallback, NULL );
    else
        var_AddCallback( p_obj, "controls-reset", AccessControlResetCallback, NULL );

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
    {
        msg_Dbg( p_obj, "Extended control API supported by v4l2 driver" );

        /* List extended controls */
        queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            if( queryctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS )
            {
                msg_Dbg( p_obj, "%s", queryctrl.name );
                queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
                continue;
            }
            switch( V4L2_CTRL_ID2CLASS( queryctrl.id ) )
            {
                case V4L2_CTRL_CLASS_USER:
                    msg_Dbg( p_obj, "Available control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
                case V4L2_CTRL_CLASS_MPEG:
                    name2var( queryctrl.name );
                    msg_Dbg( p_obj, "Available MPEG control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
                default:
                    msg_Dbg( p_obj, "Available private control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
            }
            ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }
    else
    {
        msg_Dbg( p_obj, "Extended control API not supported by v4l2 driver" );

        /* List public controls */
        for( i_cid = V4L2_CID_BASE;
             i_cid < V4L2_CID_LASTP1;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                msg_Dbg( p_obj, "Available control: %s (%x)",
                         queryctrl.name, queryctrl.id );
                ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            }
        }

        /* List private controls */
        for( i_cid = V4L2_CID_PRIVATE_BASE;
             ;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                msg_Dbg( p_obj, "Available private control: %s (%x)",
                         queryctrl.name, queryctrl.id );
                ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            }
            else
                break;
        }
    }

    SetAvailControlsByString( p_obj, i_fd );
    return VLC_SUCCESS;
}
