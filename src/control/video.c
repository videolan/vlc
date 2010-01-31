/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005-2010 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Filippo Carone <littlejohn@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
 *          Damien Fouilleul <damienf a_t videolan dot org>
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

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_vout.h>

#include "media_player_internal.h"
#include <vlc_osd.h>
#include <assert.h>

/*
 * Remember to release the returned vout_thread_t.
 */
static vout_thread_t *GetVout( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi );
    vout_thread_t *p_vout = NULL;

    if( p_input )
    {
        p_vout = input_GetVout( p_input );
        if( !p_vout )
        {
            libvlc_exception_raise( p_exception );
            libvlc_printerr( "No active video output" );
        }
        vlc_object_release( p_input );
    }
    else
    {
        libvlc_exception_raise( p_exception );
        libvlc_printerr( "No active input" );
    }
    return p_vout;
}

/**********************************************************************
 * Exported functions
 **********************************************************************/

void libvlc_set_fullscreen( libvlc_media_player_t *p_mi, int b_fullscreen,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    var_SetBool( p_vout, "fullscreen", b_fullscreen );

    vlc_object_release( p_vout );
}

int libvlc_get_fullscreen( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout ) return 0;

    i_ret = var_GetBool( p_vout, "fullscreen" );

    vlc_object_release( p_vout );

    return i_ret;
}

void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi,
                               libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    /* GetVout will raise the exception for us */
    if( !p_vout ) return;

    var_ToggleBool( p_vout, "fullscreen" );

    vlc_object_release( p_vout );
}

void libvlc_video_set_key_input( libvlc_media_player_t *p_mi, unsigned on )
{
    var_SetBool (p_mi, "keyboard-events", !!on);
}

void libvlc_video_set_mouse_input( libvlc_media_player_t *p_mi, unsigned on )
{
    var_SetBool (p_mi, "mouse-events", !!on);
}

void
libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, const char *psz_filepath,
        unsigned int i_width, unsigned int i_height, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout;

    assert( psz_filepath );

    /* We must have an input */
    if( !p_mi->p_input_thread )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Input does not exist" );
        return;
    }

    /* GetVout will raise the exception for us */
    p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return;

    var_SetInteger( p_vout, "snapshot-width", i_width );
    var_SetInteger( p_vout, "snapshot-height", i_height );

    var_SetString( p_vout, "snapshot-path", psz_filepath );
    var_SetString( p_vout, "snapshot-format", "png" );

    var_TriggerCallback( p_vout, "video-snapshot" );
    vlc_object_release( p_vout );
}

int libvlc_video_get_height( libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    int height;

    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return 0;

    height = p_vout->i_window_height;

    vlc_object_release( p_vout );

    return height;
}

int libvlc_video_get_width( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    int width;

    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    if( !p_vout ) return 0;

    width = p_vout->i_window_width;

    vlc_object_release( p_vout );

    return width;
}

int libvlc_media_player_has_vout( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread(p_mi);
    bool has_vout = false;

    if( p_input_thread )
    {
        vout_thread_t *p_vout;

        p_vout = input_GetVout( p_input_thread );
        if( p_vout )
        {
            has_vout = true;
            vlc_object_release( p_vout );
        }
        vlc_object_release( p_input_thread );
    }
    return has_vout;
}

float libvlc_video_get_scale( libvlc_media_player_t *p_mp,
                              libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mp, p_e );
    if( !p_vout )
        return 0.;

    float f_scale = var_GetFloat( p_vout, "scale" );
    if( var_GetBool( p_vout, "autoscale" ) )
        f_scale = 0.;
    vlc_object_release( p_vout );
    return f_scale;
}

void libvlc_video_set_scale( libvlc_media_player_t *p_mp, float f_scale,
                             libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mp, p_e );
    if( !p_vout )
        return;

    if( f_scale != 0. )
        var_SetFloat( p_vout, "scale", f_scale );
    var_SetBool( p_vout, "autoscale", f_scale != 0. );
    vlc_object_release( p_vout );
}

char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi,
                                     libvlc_exception_t *p_e )
{
    char *psz_aspect = NULL;
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout ) return NULL;

    psz_aspect = var_GetNonEmptyString( p_vout, "aspect-ratio" );
    vlc_object_release( p_vout );
    return psz_aspect ? psz_aspect : strdup("");
}

void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi,
                                    const char *psz_aspect, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "aspect-ratio", psz_aspect );
    vlc_object_release( p_vout );
    if( i_ret )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Bad or unsupported aspect ratio" );
    }
}

int libvlc_video_get_spu( libvlc_media_player_t *p_mi,
                          libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_spu = -1;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No active input" );
        return -1;
    }

    i_ret = var_Get( p_input_thread, "spu-es", &val );
    if( i_ret < 0 )
    {
        vlc_object_release( p_input_thread );
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle informations not found" );
        return i_ret;
    }

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( val.i_int == val_list.p_list->p_values[i].i_int )
        {
            i_spu = i;
            break;
        }
    }
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi,
                                libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_spu_count;

    if( !p_input_thread )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No active input" );
        return -1;
    }

    i_spu_count = var_CountChoices( p_input_thread, "spu-es" );

    vlc_object_release( p_input_thread );
    return i_spu_count;
}

libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi,
                                          libvlc_exception_t *p_e )
{
    return libvlc_get_track_description( p_mi, "spu-es" );
}

void libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu,
                           libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    vlc_value_t newval;
    int i_ret = -1;

    if( !p_input_thread )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "No active input" );
        return;
    }

    var_Change( p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &val_list, NULL );

    if( ( val_list.p_list->i_count == 0 )
     || (i_spu < 0) || (i_spu > val_list.p_list->i_count) )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle number out of range" );
        goto end;
    }

    newval = val_list.p_list->p_values[i_spu];
    i_ret = var_Set( p_input_thread, "spu-es", newval );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Subtitle selection error" );
    }

end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
}

int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi,
                                    const char *psz_subtitle,
                                    libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread ( p_mi );
    bool b_ret = false;

    if( p_input_thread )
    {
        if( !input_AddSubtitle( p_input_thread, psz_subtitle, true ) )
            b_ret = true;
        vlc_object_release( p_input_thread );
    }
    return b_ret;
}

libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi,
                                            libvlc_exception_t * p_e )
{
    return libvlc_get_track_description( p_mi, "title" );
}

libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi,
                                              int i_title,
                                              libvlc_exception_t *p_e )
{
    char psz_title[12];
    sprintf( psz_title,  "title %2i", i_title );
    return libvlc_get_track_description( p_mi, psz_title );
}

char *libvlc_video_get_crop_geometry( libvlc_media_player_t *p_mi,
                                   libvlc_exception_t *p_e )
{
    char *psz_geometry = 0;
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout ) return 0;

    psz_geometry = var_GetNonEmptyString( p_vout, "crop" );
    vlc_object_release( p_vout );
    return psz_geometry ? psz_geometry : strdup("");
}

void libvlc_video_set_crop_geometry( libvlc_media_player_t *p_mi,
                                     const char *psz_geometry, libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );
    int i_ret = -1;

    if( !p_vout ) return;

    i_ret = var_SetString( p_vout, "crop", psz_geometry );
    vlc_object_release( p_vout );

    if( i_ret )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Bad or unsupported cropping geometry" );
    }
}

void libvlc_toggle_teletext( libvlc_media_player_t *p_mi,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread;

    p_input_thread = libvlc_get_input_thread(p_mi);
    if( !p_input_thread ) return;

    if( var_CountChoices( p_input_thread, "teletext-es" ) <= 0 )
    {
        vlc_object_release( p_input_thread );
        return;
    }
    const bool b_selected = var_GetInteger( p_input_thread, "teletext-es" ) >= 0;
    if( b_selected )
    {
        var_SetInteger( p_input_thread, "spu-es", -1 );
    }
    else
    {
        vlc_value_t list;
        if( !var_Change( p_input_thread, "teletext-es", VLC_VAR_GETLIST, &list, NULL ) )
        {
            if( list.p_list->i_count > 0 )
                var_SetInteger( p_input_thread, "spu-es", list.p_list->p_values[0].i_int );

            var_FreeList( &list, NULL );
        }
    }
    vlc_object_release( p_input_thread );
}

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi,
                                  libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_track_count;

    if( !p_input_thread )
        return -1;

    i_track_count = var_CountChoices( p_input_thread, "video-es" );

    vlc_object_release( p_input_thread );
    return i_track_count;
}

libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi,
                                            libvlc_exception_t *p_e )
{
    return libvlc_get_track_description( p_mi, "video-es" );
}

int libvlc_video_get_track( libvlc_media_player_t *p_mi,
                            libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    vlc_value_t val;
    int i_track = -1;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return -1;

    i_ret = var_Get( p_input_thread, "video-es", &val );
    if( i_ret < 0 )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Video track information not found" );
        vlc_object_release( p_input_thread );
        return -1;
    }

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( val_list.p_list->p_values[i].i_int == val.i_int )
        {
            i_track = i;
            break;
        }
    }
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_track;
}

void libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track,
                             libvlc_exception_t *p_e )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    int i_ret = -1;
    int i;

    if( !p_input_thread )
        return;

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( i_track == val_list.p_list->p_values[i].i_int )
        {
            i_ret = var_SetInteger( p_input_thread, "video-es", i_track );
            if( i_ret < 0 )
                break;
            goto end;
        }
    }
    libvlc_exception_raise( p_e );
    libvlc_printerr( "Video track number out of range" );
end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
}

/******************************************************************************
 * libvlc_video_set_deinterlace : enable deinterlace
 *****************************************************************************/
void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi, int b_enable,
                                   const char *psz_mode,
                                   libvlc_exception_t *p_e )
{
    vout_thread_t *p_vout = GetVout( p_mi, p_e );

    if( !p_vout )
        return;

    if( b_enable )
    {
        /* be sure that the filter name given is supported */
        if( !strcmp(psz_mode, "blend")   || !strcmp(psz_mode, "bob")
         || !strcmp(psz_mode, "discard") || !strcmp(psz_mode, "linear")
         || !strcmp(psz_mode, "mean")    || !strcmp(psz_mode, "x")
         || !strcmp(psz_mode, "yadif")   || !strcmp(psz_mode, "yadif2x") )
        {
            /* set deinterlace filter chosen */
            var_SetString( p_vout, "deinterlace-mode", psz_mode );
            var_SetInteger( p_vout, "deinterlace", 1 );
        }
        else
        {
            libvlc_exception_raise( p_e );
            libvlc_printerr( "Bad or unsupported deinterlacing mode" );
        }
    }
    else
    {
        /* disable deinterlace filter */
        var_SetInteger( p_vout, "deinterlace", 0 );
    }

    vlc_object_release( p_vout );
}


/* ************** */
/* module helpers */
/* ************** */


static vlc_object_t *get_object( libvlc_media_player_t * p_mi,
                                 const char *name, libvlc_exception_t *p_e )
{
    vlc_object_t *object = NULL;
    vout_thread_t  *vout = GetVout( p_mi, p_e );
    libvlc_exception_clear( p_e );
    if( vout )
    {
        object = vlc_object_find_name( vout, name, FIND_CHILD );
        vlc_object_release(vout);
    }
    if( !object )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "%s not enabled", name );
    }
    return object;
}


typedef const struct {
    const char name[20]; /* probably will become a const char * sometime */
    unsigned type; 
} opt_t;


static void
set_int( libvlc_media_player_t *p_mi, const char *name,
         const opt_t *opt, int value, libvlc_exception_t *p_e )
{
    if( !opt ) return;

    if( !opt->type ) /* the enabler */
    {
        vout_thread_t *vout = GetVout( p_mi, p_e );
        libvlc_exception_clear( p_e );
        if (vout)
        {
            vout_EnableFilter( vout, opt->name, value, false );
            vlc_object_release( vout );
        }
        return;
    }

    vlc_object_t *object = get_object( p_mi, name, p_e );
    if( !object ) return;

    switch( opt->type )
    {
    case VLC_VAR_INTEGER:
        var_SetInteger(object, opt->name, value);
        break;
    default:
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Invalid argument for %s in %s", name, "set int" );
        break;
    }
    vlc_object_release( object );
}


static int
get_int( libvlc_media_player_t *p_mi, const char *name,
        const opt_t *opt, libvlc_exception_t *p_e )
{
    if( !opt ) return 0;

    vlc_object_t *object = get_object( p_mi, name, p_e );
    if( !object ) return 0;

    int ret;
    switch( opt->type )
    {
    case 0: /* the enabler */
        ret = NULL != object;
        break;
    case VLC_VAR_INTEGER:
        ret = var_GetInteger(object, opt->name);
        break;
    default:
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Invalid argument for %s in %s", name, "get int" );
        ret = 0;
        break;
    }
    vlc_object_release( object );
    return ret;
}


static void
set_string( libvlc_media_player_t *p_mi, const char *name, const opt_t *opt,
            const char *psz_value, libvlc_exception_t *p_e )
{
    if( !opt ) return;
    vlc_object_t *object = get_object( p_mi, name, p_e );
    if( !object ) return;

    switch( opt->type )
    {
    case VLC_VAR_STRING:
        var_SetString( object, opt->name, psz_value );
        break;
    default:
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Invalid argument for %s in %s", name, "set string" );
        break;
    }
    vlc_object_release( object );
}


static char *
get_string( libvlc_media_player_t *p_mi, const char *name,
            const opt_t *opt, libvlc_exception_t *p_e )
{
    if( !opt ) return NULL;
    vlc_object_t *object = get_object( p_mi, name, p_e );
    if( !object ) return NULL;

    char *ret;
    switch( opt->type )
    {
    case VLC_VAR_STRING:
        ret = var_GetString( object, opt->name );
        break;
    default:
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Invalid argument for %s in %s", name, "get string" );
        ret = NULL;
        break;
    }
    vlc_object_release( object );
    return ret;
}


/*****************************************************************************
 * Marquee: FIXME: That implementation has no persistent state and requires
 * a vout
 *****************************************************************************/

static const opt_t *
marq_option_bynumber(unsigned option, libvlc_exception_t *p_e)
{
    opt_t optlist[] =
    {
        { "marq",          0 },
        { "marq-marquee",  VLC_VAR_STRING },
        { "marq-color",    VLC_VAR_INTEGER },
        { "marq-opacity",  VLC_VAR_INTEGER },
        { "marq-position", VLC_VAR_INTEGER },
        { "marq-refresh",  VLC_VAR_INTEGER },
        { "marq-size",     VLC_VAR_INTEGER },
        { "marq-timeout",  VLC_VAR_INTEGER },
        { "marq-x",        VLC_VAR_INTEGER },
        { "marq-y",        VLC_VAR_INTEGER },
    };
    enum { num_opts = sizeof(optlist) / sizeof(*optlist) };

    opt_t *r = option < num_opts ? optlist+option : NULL;
    if( !r )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown marquee option" );
    }
    return r;
}

static vlc_object_t *get_object( libvlc_media_player_t *,
                                 const char *, libvlc_exception_t *);

/*****************************************************************************
 * libvlc_video_get_marquee_int : get a marq option value
 *****************************************************************************/
int libvlc_video_get_marquee_int( libvlc_media_player_t *p_mi,
                                  unsigned option, libvlc_exception_t *p_e )
{
    return get_int( p_mi, "marq", marq_option_bynumber(option,p_e), p_e );
}

/*****************************************************************************
 * libvlc_video_get_marquee_string : get a marq option value
 *****************************************************************************/
char * libvlc_video_get_marquee_string( libvlc_media_player_t *p_mi,
                                    unsigned option, libvlc_exception_t *p_e )
{
    return get_string( p_mi, "marq", marq_option_bynumber(option,p_e), p_e );
}

/*****************************************************************************
 * libvlc_video_set_marquee_int: enable, disable or set an int option
 *****************************************************************************/
void libvlc_video_set_marquee_int( libvlc_media_player_t *p_mi,
                         unsigned option, int value, libvlc_exception_t *p_e )
{
    set_int( p_mi, "marq", marq_option_bynumber(option,p_e), value, p_e );
}

/*****************************************************************************
 * libvlc_video_set_marquee_string: set a string option
 *****************************************************************************/
void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
                unsigned option, const char * value, libvlc_exception_t *p_e )
{
    set_string( p_mi, "marq", marq_option_bynumber(option,p_e), value, p_e );
}


/* logo module support */


static opt_t *
logo_option_bynumber( unsigned option, libvlc_exception_t *p_e )
{
    opt_t vlogo_optlist[] = /* depends on libvlc_video_logo_option_t */
    {
        { "logo",          0 },
        { "logo-file",     VLC_VAR_STRING },
        { "logo-x",        VLC_VAR_INTEGER },
        { "logo-y",        VLC_VAR_INTEGER },
        { "logo-delay",    VLC_VAR_INTEGER },
        { "logo-repeat",   VLC_VAR_INTEGER },
        { "logo-opacity",  VLC_VAR_INTEGER },
        { "logo-position", VLC_VAR_INTEGER },
    };
    enum { num_vlogo_opts = sizeof(vlogo_optlist) / sizeof(*vlogo_optlist) };

    opt_t *r = option < num_vlogo_opts ? vlogo_optlist+option : NULL;
    if( !r )
    {
        libvlc_exception_raise( p_e );
        libvlc_printerr( "Unknown logo option" );
    }
    return r;
}


void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
                                   unsigned option, const char *psz_value,
                                   libvlc_exception_t *p_e )
{
    set_string( p_mi,"logo",logo_option_bynumber(option,p_e),psz_value,p_e );
}


void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                unsigned option, int value,
                                libvlc_exception_t *p_e )
{
    set_int( p_mi, "logo", logo_option_bynumber(option, p_e), value, p_e );
}


int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
                               unsigned option, libvlc_exception_t *p_e )
{
    return get_int( p_mi, "logo", logo_option_bynumber(option,p_e), p_e );
}


