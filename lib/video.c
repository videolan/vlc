/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005-2010 VLC authors and VideoLAN
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Filippo Carone <littlejohn@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
 *          Damien Fouilleul <damienf a_t videolan dot org>
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
#include <assert.h>

/*
 * Remember to release the returned vout_thread_t.
 */
static vout_thread_t **GetVouts( libvlc_media_player_t *p_mi, size_t *n )
{
    input_thread_t *p_input = libvlc_get_input_thread( p_mi );
    if( !p_input )
    {
        *n = 0;
        return NULL;
    }

    vout_thread_t **pp_vouts;
    if (input_Control( p_input, INPUT_GET_VOUTS, &pp_vouts, n))
    {
        *n = 0;
        pp_vouts = NULL;
    }
    vlc_object_release (p_input);
    return pp_vouts;
}

static vout_thread_t *GetVout (libvlc_media_player_t *mp, size_t num)
{
    vout_thread_t *p_vout = NULL;
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (mp, &n);
    if (pp_vouts == NULL)
        goto err;

    if (num < n)
        p_vout = pp_vouts[num];

    for (size_t i = 0; i < n; i++)
        if (i != num)
            vlc_object_release (pp_vouts[i]);
    free (pp_vouts);

    if (p_vout == NULL)
err:
        libvlc_printerr ("Video output not active");
    return p_vout;
}

/**********************************************************************
 * Exported functions
 **********************************************************************/

void libvlc_set_fullscreen( libvlc_media_player_t *p_mi, int b_fullscreen )
{
    /* This will work even if the video is not currently active */
    var_SetBool (p_mi, "fullscreen", !!b_fullscreen);

    /* Apply to current video outputs (if any) */
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
    {
        var_SetBool (pp_vouts[i], "fullscreen", b_fullscreen);
        vlc_object_release (pp_vouts[i]);
    }
    free (pp_vouts);
}

int libvlc_get_fullscreen( libvlc_media_player_t *p_mi )
{
    return var_GetBool (p_mi, "fullscreen");
}

void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi )
{
    bool b_fullscreen = var_ToggleBool (p_mi, "fullscreen");

    /* Apply to current video outputs (if any) */
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        var_SetBool (p_vout, "fullscreen", b_fullscreen);
        vlc_object_release (p_vout);
    }
    free (pp_vouts);
}

void libvlc_video_set_key_input( libvlc_media_player_t *p_mi, unsigned on )
{
    var_SetBool (p_mi, "keyboard-events", !!on);
}

void libvlc_video_set_mouse_input( libvlc_media_player_t *p_mi, unsigned on )
{
    var_SetBool (p_mi, "mouse-events", !!on);
}

int
libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, unsigned num,
                            const char *psz_filepath,
                            unsigned int i_width, unsigned int i_height )
{
    assert( psz_filepath );

    vout_thread_t *p_vout = GetVout (p_mi, num);
    if (p_vout == NULL)
        return -1;

    /* FIXME: This is not atomic. All parameters should be passed at once
     * (obviously _not_ with var_*()). Also, the libvlc object should not be
     * used for the callbacks: that breaks badly if there are concurrent
     * media players in the instance. */
    var_Create( p_vout, "snapshot-width", VLC_VAR_INTEGER );
    var_SetInteger( p_vout, "snapshot-width", i_width);
    var_Create( p_vout, "snapshot-height", VLC_VAR_INTEGER );
    var_SetInteger( p_vout, "snapshot-height", i_height );
    var_Create( p_vout, "snapshot-path", VLC_VAR_STRING );
    var_SetString( p_vout, "snapshot-path", psz_filepath );
    var_Create( p_vout, "snapshot-format", VLC_VAR_STRING );
    var_SetString( p_vout, "snapshot-format", "png" );
    var_TriggerCallback( p_vout, "video-snapshot" );
    vlc_object_release( p_vout );
    return 0;
}

int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned num,
                           unsigned *restrict px, unsigned *restrict py )
{
    libvlc_media_track_info_t *info;
    int ret = -1;
    if (!p_mi->p_md)
        return ret;
    int infos = libvlc_media_get_tracks_info(p_mi->p_md, &info);
    if (infos <= 0)
        return ret;

    for (int i = 0; i < infos; i++)
        if (info[i].i_type == libvlc_track_video && num-- == 0) {
            *px = info[i].u.video.i_width;
            *py = info[i].u.video.i_height;
            ret = 0;
            break;
        }

    free(info);
    return ret;
}

int libvlc_video_get_height( libvlc_media_player_t *p_mi )
{
    unsigned width, height;

    if (libvlc_video_get_size (p_mi, 0, &width, &height))
        return 0;
    return height;
}

int libvlc_video_get_width( libvlc_media_player_t *p_mi )
{
    unsigned width, height;

    if (libvlc_video_get_size (p_mi, 0, &width, &height))
        return 0;
    return width;
}

int libvlc_video_get_cursor( libvlc_media_player_t *mp, unsigned num,
                             int *restrict px, int *restrict py )
{
    vout_thread_t *p_vout = GetVout (mp, num);
    if (p_vout == NULL)
        return -1;

    var_GetCoords (p_vout, "mouse-moved", px, py);
    vlc_object_release (p_vout);
    return 0;
}

unsigned libvlc_media_player_has_vout( libvlc_media_player_t *p_mi )
{
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
        vlc_object_release (pp_vouts[i]);
    free (pp_vouts);
    return n;
}

float libvlc_video_get_scale( libvlc_media_player_t *mp )
{
    float f_scale = var_GetFloat (mp, "scale");
    if (var_GetBool (mp, "autoscale"))
        f_scale = 0.;
    return f_scale;
}

void libvlc_video_set_scale( libvlc_media_player_t *p_mp, float f_scale )
{
    if (f_scale != 0.)
        var_SetFloat (p_mp, "scale", f_scale);
    var_SetBool (p_mp, "autoscale", f_scale == 0.);

    /* Apply to current video outputs (if any) */
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mp, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        if (f_scale != 0.)
            var_SetFloat (p_vout, "scale", f_scale);
        var_SetBool (p_vout, "autoscale", f_scale == 0.);
        vlc_object_release (p_vout);
    }
    free (pp_vouts);
}

char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi )
{
    return var_GetNonEmptyString (p_mi, "aspect-ratio");
}

void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi,
                                    const char *psz_aspect )
{
    if (psz_aspect == NULL)
        psz_aspect = "";
    var_SetString (p_mi, "aspect-ratio", psz_aspect);

    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        var_SetString (p_vout, "aspect-ratio", psz_aspect);
        vlc_object_release (p_vout);
    }
    free (pp_vouts);
}

int libvlc_video_get_spu( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );

    if( !p_input_thread )
    {
        libvlc_printerr( "No active input" );
        return -1;
    }

    int i_spu = var_GetInteger( p_input_thread, "spu-es" );
    vlc_object_release( p_input_thread );
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_spu_count;

    if( !p_input_thread )
        return 0;

    i_spu_count = var_CountChoices( p_input_thread, "spu-es" );
    vlc_object_release( p_input_thread );
    return i_spu_count;
}

libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, "spu-es" );
}

int libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t list;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change (p_input_thread, "spu-es", VLC_VAR_GETCHOICES, &list, NULL);
    for (int i = 0; i < list.p_list->i_count; i++)
    {
        if( i_spu == list.p_list->p_values[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "spu-es", i_spu ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    vlc_object_release (p_input_thread);
    var_FreeList (&list, NULL);
    return i_ret;
}

int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi,
                                    const char *psz_subtitle )
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

int64_t libvlc_video_get_spu_delay( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int64_t val = 0;

    if( p_input_thread )
    {
        val = var_GetTime( p_input_thread, "spu-delay" );
        vlc_object_release( p_input_thread );
    }
    else
    {
        libvlc_printerr( "No active input" );
    }

    return val;
}

int libvlc_video_set_spu_delay( libvlc_media_player_t *p_mi,
                                int64_t i_delay )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int ret = -1;

    if( p_input_thread )
    {
        var_SetTime( p_input_thread, "spu-delay", i_delay );
        vlc_object_release( p_input_thread );
        ret = 0;
    }
    else
    {
        libvlc_printerr( "No active input" );
    }

    return ret;
}

libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, "title" );
}

libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi,
                                              int i_title )
{
    char psz_title[12];
    sprintf( psz_title,  "title %2i", i_title );
    return libvlc_get_track_description( p_mi, psz_title );
}

char *libvlc_video_get_crop_geometry (libvlc_media_player_t *p_mi)
{
    return var_GetNonEmptyString (p_mi, "crop");
}

void libvlc_video_set_crop_geometry( libvlc_media_player_t *p_mi,
                                     const char *psz_geometry )
{
    if (psz_geometry == NULL)
        psz_geometry = "";

    var_SetString (p_mi, "crop", psz_geometry);

    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);

    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];
        vlc_value_t val;

        /* Make sure the geometry is in the choice list */
        /* Earlier choices are removed to not grow a long list over time. */
        /* FIXME: not atomic - lock? */
        val.psz_string = (char *)psz_geometry;
        var_Change (p_vout, "crop", VLC_VAR_CLEARCHOICES, NULL, NULL);
        var_Change (p_vout, "crop", VLC_VAR_ADDCHOICE, &val, &val);
        var_SetString (p_vout, "crop", psz_geometry);
        vlc_object_release (p_vout);
    }
    free (pp_vouts);
}

int libvlc_video_get_teletext( libvlc_media_player_t *p_mi )
{
    return var_GetInteger (p_mi, "vbi-page");
}

void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page )
{
    input_thread_t *p_input_thread;
    vlc_object_t *p_zvbi = NULL;
    int telx;

    var_SetInteger (p_mi, "vbi-page", i_page);

    p_input_thread = libvlc_get_input_thread( p_mi );
    if( !p_input_thread ) return;

    if( var_CountChoices( p_input_thread, "teletext-es" ) <= 0 )
    {
        vlc_object_release( p_input_thread );
        return;
    }

    telx = var_GetInteger( p_input_thread, "teletext-es" );
    if( input_GetEsObjects( p_input_thread, telx, &p_zvbi, NULL, NULL )
        == VLC_SUCCESS )
    {
        var_SetInteger( p_zvbi, "vbi-page", i_page );
        vlc_object_release( p_zvbi );
    }
    vlc_object_release( p_input_thread );
}

void libvlc_toggle_teletext( libvlc_media_player_t *p_mi )
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

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi )
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
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, "video-es" );
}

int libvlc_video_get_track( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );

    if( !p_input_thread )
        return -1;

    int id = var_GetInteger( p_input_thread, "video-es" );
    vlc_object_release( p_input_thread );
    return id;
}

int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t val_list;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES, &val_list, NULL );
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        if( i_track == val_list.p_list->p_values[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "video-es", i_track ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    var_FreeList( &val_list, NULL );
    vlc_object_release( p_input_thread );
    return i_ret;
}

/******************************************************************************
 * libvlc_video_set_deinterlace : enable deinterlace
 *****************************************************************************/
void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi,
                                   const char *psz_mode )
{
    if (psz_mode == NULL)
        psz_mode = "";
    if (*psz_mode
     && strcmp (psz_mode, "blend")    && strcmp (psz_mode, "bob")
     && strcmp (psz_mode, "discard")  && strcmp (psz_mode, "linear")
     && strcmp (psz_mode, "mean")     && strcmp (psz_mode, "x")
     && strcmp (psz_mode, "yadif")    && strcmp (psz_mode, "yadif2x")
     && strcmp (psz_mode, "phosphor") && strcmp (psz_mode, "ivtc"))
        return;

    if (*psz_mode)
    {
        var_SetString (p_mi, "deinterlace-mode", psz_mode);
        var_SetInteger (p_mi, "deinterlace", 1);
    }
    else
        var_SetInteger (p_mi, "deinterlace", 0);

    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        if (*psz_mode)
        {
            var_SetString (p_vout, "deinterlace-mode", psz_mode);
            var_SetInteger (p_vout, "deinterlace", 1);
        }
        else
            var_SetInteger (p_vout, "deinterlace", 0);
        vlc_object_release (p_vout);
    }
    free (pp_vouts);
}

/* ************** */
/* module helpers */
/* ************** */


static vlc_object_t *get_object( libvlc_media_player_t * p_mi,
                                 const char *name )
{
    vlc_object_t *object;
    vout_thread_t *vout = GetVout( p_mi, 0 );

    if( vout )
    {
        object = vlc_object_find_name( vout, name );
        vlc_object_release(vout);
    }
    else
        object = NULL;

    if( !object )
        libvlc_printerr( "%s not enabled", name );
    return object;
}


typedef const struct {
    const char name[20];
    unsigned type;
} opt_t;


static void
set_int( libvlc_media_player_t *p_mi, const char *restrict name,
         const opt_t *restrict opt, int value )
{
    if( !opt ) return;

    if( !opt->type ) /* the enabler */
    {
        vout_thread_t *vout = GetVout( p_mi, 0 );
        if (vout)
        {
            vout_EnableFilter( vout, opt->name, value, false );
            vlc_object_release( vout );
        }
        return;
    }

    if( opt->type != VLC_VAR_INTEGER )
    {
        libvlc_printerr( "Invalid argument to %s in %s", name, "set int" );
        return;
    }

    var_SetInteger(p_mi, opt->name, value);
    vlc_object_t *object = get_object( p_mi, name );
    if( object )
    {
        var_SetInteger(object, opt->name, value);
        vlc_object_release( object );
    }
}


static int
get_int( libvlc_media_player_t *p_mi, const char *restrict name,
         const opt_t *restrict opt )
{
    if( !opt ) return 0;

    switch( opt->type )
    {
        case 0: /* the enabler */
        {
            vlc_object_t *object = get_object( p_mi, name );
            vlc_object_release( object );
            return object != NULL;
        }
    case VLC_VAR_INTEGER:
        return var_GetInteger(p_mi, opt->name);
    default:
        libvlc_printerr( "Invalid argument to %s in %s", name, "get int" );
        return 0;
    }
}


static void
set_float( libvlc_media_player_t *p_mi, const char *restrict name,
            const opt_t *restrict opt, float value )
{
    if( !opt ) return;

    if( opt->type != VLC_VAR_FLOAT )
    {
        libvlc_printerr( "Invalid argument to %s in %s", name, "set float" );
        return;
    }

    var_SetFloat( p_mi, opt->name, value );

    vlc_object_t *object = get_object( p_mi, name );
    if( object )
    {
        var_SetFloat(object, opt->name, value );
        vlc_object_release( object );
    }
}


static float
get_float( libvlc_media_player_t *p_mi, const char *restrict name,
            const opt_t *restrict opt )
{
    if( !opt ) return 0.0;


    if( opt->type != VLC_VAR_FLOAT )
    {
        libvlc_printerr( "Invalid argument to %s in %s", name, "get float" );
        return 0.0;
    }

    return var_GetFloat( p_mi, opt->name );
}


static void
set_string( libvlc_media_player_t *p_mi, const char *restrict name,
            const opt_t *restrict opt, const char *restrict psz_value )
{
    if( !opt ) return;

    if( opt->type != VLC_VAR_STRING )
    {
        libvlc_printerr( "Invalid argument to %s in %s", name, "set string" );
        return;
    }

    var_SetString( p_mi, opt->name, psz_value );

    vlc_object_t *object = get_object( p_mi, name );
    if( object )
    {
        var_SetString(object, opt->name, psz_value );
        vlc_object_release( object );
    }
}


static char *
get_string( libvlc_media_player_t *p_mi, const char *restrict name,
            const opt_t *restrict opt )
{
    if( !opt ) return NULL;

    if( opt->type != VLC_VAR_STRING )
    {
        libvlc_printerr( "Invalid argument to %s in %s", name, "get string" );
        return NULL;
    }

    return var_GetString( p_mi, opt->name );
}


static const opt_t *
marq_option_bynumber(unsigned option)
{
    static const opt_t optlist[] =
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

    const opt_t *r = option < num_opts ? optlist+option : NULL;
    if( !r )
        libvlc_printerr( "Unknown marquee option" );
    return r;
}

static vlc_object_t *get_object( libvlc_media_player_t *, const char *);

/*****************************************************************************
 * libvlc_video_get_marquee_int : get a marq option value
 *****************************************************************************/
int libvlc_video_get_marquee_int( libvlc_media_player_t *p_mi,
                                  unsigned option )
{
    return get_int( p_mi, "marq", marq_option_bynumber(option) );
}

/*****************************************************************************
 * libvlc_video_get_marquee_string : get a marq option value
 *****************************************************************************/
char * libvlc_video_get_marquee_string( libvlc_media_player_t *p_mi,
                                        unsigned option )
{
    return get_string( p_mi, "marq", marq_option_bynumber(option) );
}

/*****************************************************************************
 * libvlc_video_set_marquee_int: enable, disable or set an int option
 *****************************************************************************/
void libvlc_video_set_marquee_int( libvlc_media_player_t *p_mi,
                         unsigned option, int value )
{
    set_int( p_mi, "marq", marq_option_bynumber(option), value );
}

/*****************************************************************************
 * libvlc_video_set_marquee_string: set a string option
 *****************************************************************************/
void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
                unsigned option, const char * value )
{
    set_string( p_mi, "marq", marq_option_bynumber(option), value );
}


/* logo module support */


static const opt_t *
logo_option_bynumber( unsigned option )
{
    static const opt_t vlogo_optlist[] =
    /* depends on libvlc_video_logo_option_t */
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

    const opt_t *r = option < num_vlogo_opts ? vlogo_optlist+option : NULL;
    if( !r )
        libvlc_printerr( "Unknown logo option" );
    return r;
}


void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
                                   unsigned option, const char *psz_value )
{
    set_string( p_mi,"logo",logo_option_bynumber(option),psz_value );
}


void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                unsigned option, int value )
{
    set_int( p_mi, "logo", logo_option_bynumber(option), value );
}


int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
                               unsigned option )
{
    return get_int( p_mi, "logo", logo_option_bynumber(option) );
}


/* adjust module support */


static const opt_t *
adjust_option_bynumber( unsigned option )
{
    static const opt_t optlist[] =
    {
        { "adjust",               0 },
        { "contrast",             VLC_VAR_FLOAT },
        { "brightness",           VLC_VAR_FLOAT },
        { "hue",                  VLC_VAR_INTEGER },
        { "saturation",           VLC_VAR_FLOAT },
        { "gamma",                VLC_VAR_FLOAT },
    };
    enum { num_opts = sizeof(optlist) / sizeof(*optlist) };

    const opt_t *r = option < num_opts ? optlist+option : NULL;
    if( !r )
        libvlc_printerr( "Unknown adjust option" );
    return r;
}


void libvlc_video_set_adjust_int( libvlc_media_player_t *p_mi,
                                  unsigned option, int value )
{
    set_int( p_mi, "adjust", adjust_option_bynumber(option), value );
}


int libvlc_video_get_adjust_int( libvlc_media_player_t *p_mi,
                                 unsigned option )
{
    return get_int( p_mi, "adjust", adjust_option_bynumber(option) );
}


void libvlc_video_set_adjust_float( libvlc_media_player_t *p_mi,
                                    unsigned option, float value )
{
    set_float( p_mi, "adjust", adjust_option_bynumber(option), value );
}


float libvlc_video_get_adjust_float( libvlc_media_player_t *p_mi,
                                     unsigned option )
{
    return get_float( p_mi, "adjust", adjust_option_bynumber(option) );
}
