/*****************************************************************************
 * video.c: libvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005-2010 VLC authors and VideoLAN
 *
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
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_url.h>

#include "libvlc_internal.h"
#include "media_player_internal.h"
#include <math.h>
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
    input_Release(p_input);
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
            vout_Release(pp_vouts[i]);
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
        vout_Release(pp_vouts[i]);
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
        vout_Release(p_vout);
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
    vout_Release(p_vout);
    return 0;
}

int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned num,
                           unsigned *restrict px, unsigned *restrict py )
{
    if (p_mi->p_md == NULL)
        return -1;

    libvlc_media_track_t **tracks;
    unsigned count = libvlc_media_tracks_get(p_mi->p_md, &tracks);
    int ret = -1;

    for (unsigned i = 0; i < count; i++)
        if (tracks[i]->i_type == libvlc_track_video && num-- == 0) {
            *px = tracks[i]->video->i_width;
            *py = tracks[i]->video->i_height;
            ret = 0;
            break;
        }

    libvlc_media_tracks_release(tracks, count);
    return ret;
}

int libvlc_video_get_cursor( libvlc_media_player_t *mp, unsigned num,
                             int *restrict px, int *restrict py )
{
    vout_thread_t *p_vout = GetVout (mp, num);
    if (p_vout == NULL)
        return -1;

    var_GetCoords (p_vout, "mouse-moved", px, py);
    vout_Release(p_vout);
    return 0;
}

unsigned libvlc_media_player_has_vout( libvlc_media_player_t *p_mi )
{
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
        vout_Release(pp_vouts[i]);
    free (pp_vouts);
    return n;
}

float libvlc_video_get_scale( libvlc_media_player_t *mp )
{
    float f_scale = var_GetFloat (mp, "zoom");
    if (var_GetBool (mp, "autoscale"))
        f_scale = 0.f;
    return f_scale;
}

void libvlc_video_set_scale( libvlc_media_player_t *p_mp, float f_scale )
{
    if (isfinite(f_scale) && f_scale != 0.f)
        var_SetFloat (p_mp, "zoom", f_scale);
    var_SetBool (p_mp, "autoscale", f_scale == 0.f);

    /* Apply to current video outputs (if any) */
    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mp, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        if (isfinite(f_scale) && f_scale != 0.f)
            var_SetFloat (p_vout, "zoom", f_scale);
        var_SetBool (p_vout, "autoscale", f_scale == 0.f);
        vout_Release(p_vout);
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
        vout_Release(p_vout);
    }
    free (pp_vouts);
}

libvlc_video_viewpoint_t *libvlc_video_new_viewpoint(void)
{
    libvlc_video_viewpoint_t *p_vp = malloc(sizeof *p_vp);
    if (unlikely(p_vp == NULL))
        return NULL;
    p_vp->f_yaw = p_vp->f_pitch = p_vp->f_roll = p_vp->f_field_of_view = 0.0f;
    return p_vp;
}

int libvlc_video_update_viewpoint( libvlc_media_player_t *p_mi,
                                   const libvlc_video_viewpoint_t *p_viewpoint,
                                   bool b_absolute )
{
    vlc_viewpoint_t update = {
        .yaw   = p_viewpoint->f_yaw,
        .pitch = p_viewpoint->f_pitch,
        .roll  = p_viewpoint->f_roll,
        .fov   = p_viewpoint->f_field_of_view,
    };

    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    if( p_input_thread != NULL )
    {
        if( input_UpdateViewpoint( p_input_thread, &update,
                                   b_absolute ) != VLC_SUCCESS )
        {
            input_Release(p_input_thread);
            return -1;
        }
        input_Release(p_input_thread);
        return 0;
    }

    /* Save the viewpoint in case the input is not created yet */
    if( !b_absolute )
    {
        p_mi->viewpoint.yaw += update.yaw;
        p_mi->viewpoint.pitch += update.pitch;
        p_mi->viewpoint.roll += update.roll;
        p_mi->viewpoint.fov += update.fov;
    }
    else
        p_mi->viewpoint = update;

    vlc_viewpoint_clip( &p_mi->viewpoint );

    return 0;
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
    input_Release(p_input_thread);
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_spu_count;

    if( !p_input_thread )
        return 0;

    i_spu_count = var_CountChoices( p_input_thread, "spu-es" );
    input_Release(p_input_thread);
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
    vlc_value_t *list;
    size_t count;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change(p_input_thread, "spu-es", VLC_VAR_GETCHOICES,
               &count, &list, (char ***)NULL);
    for (size_t i = 0; i < count; i++)
    {
        if( i_spu == list[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "spu-es", i_spu ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    input_Release(p_input_thread);
    free(list);
    return i_ret;
}

int64_t libvlc_video_get_spu_delay( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int64_t val = 0;

    if( p_input_thread )
    {
        val = US_FROM_VLC_TICK( var_GetInteger( p_input_thread, "spu-delay" ) );
        input_Release(p_input_thread);
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
        var_SetInteger( p_input_thread, "spu-delay", VLC_TICK_FROM_US( i_delay ) );
        input_Release(p_input_thread);
        ret = 0;
    }
    else
    {
        libvlc_printerr( "No active input" );
    }

    return ret;
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

        var_SetString (p_vout, "crop", psz_geometry);
        vout_Release(p_vout);
    }
    free (pp_vouts);
}

int libvlc_video_get_teletext( libvlc_media_player_t *p_mi )
{
    return var_GetInteger (p_mi, "vbi-page");
}

static void teletext_enable( input_thread_t *p_input_thread, bool b_enable )
{
    if( b_enable )
    {
        vlc_value_t *list;
        size_t count;

        if( !var_Change( p_input_thread, "teletext-es", VLC_VAR_GETCHOICES,
                         &count, &list, (char ***)NULL ) )
        {
            if( count > 0 )
                var_SetInteger( p_input_thread, "spu-es", list[0].i_int );

            free(list);
        }
    }
    else
        var_SetInteger( p_input_thread, "spu-es", -1 );
}

void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page )
{
    input_thread_t *p_input_thread;
    vlc_object_t *p_zvbi = NULL;
    int telx;
    bool b_key = false;

    if( i_page >= 0 && i_page < 1000 )
        var_SetInteger( p_mi, "vbi-page", i_page );
    else if( i_page >= 1000 )
    {
        switch (i_page)
        {
            case libvlc_teletext_key_red:
            case libvlc_teletext_key_green:
            case libvlc_teletext_key_yellow:
            case libvlc_teletext_key_blue:
            case libvlc_teletext_key_index:
                b_key = true;
                break;
            default:
                libvlc_printerr("Invalid key action");
                return;
        }
    }
    else
    {
        libvlc_printerr("Invalid page number");
        return;
    }

    p_input_thread = libvlc_get_input_thread( p_mi );
    if( !p_input_thread ) return;

    if( var_CountChoices( p_input_thread, "teletext-es" ) <= 0 )
    {
        input_Release(p_input_thread);
        return;
    }

    if( i_page == 0 )
    {
        teletext_enable( p_input_thread, false );
    }
    else
    {
        telx = var_GetInteger( p_input_thread, "teletext-es" );
        if( telx >= 0 )
        {
            if( input_GetEsObjects( p_input_thread, telx, &p_zvbi )
                == VLC_SUCCESS )
            {
                var_SetInteger( p_zvbi, "vbi-page", i_page );
                vlc_object_release( p_zvbi );
            }
        }
        else if (!b_key)
        {
            /* the "vbi-page" will be selected on es creation */
            teletext_enable( p_input_thread, true );
        }
        else
            libvlc_printerr("Key action sent while the teletext is disabled");
    }
    input_Release(p_input_thread);
}

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    int i_track_count;

    if( !p_input_thread )
        return -1;

    i_track_count = var_CountChoices( p_input_thread, "video-es" );

    input_Release(p_input_thread);
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
    input_Release(p_input_thread);
    return id;
}

int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    input_thread_t *p_input_thread = libvlc_get_input_thread( p_mi );
    vlc_value_t *val_list;
    size_t count;
    int i_ret = -1;

    if( !p_input_thread )
        return -1;

    var_Change( p_input_thread, "video-es", VLC_VAR_GETCHOICES,
                &count, &val_list, (char ***)NULL );
    for( size_t i = 0; i < count; i++ )
    {
        if( i_track == val_list[i].i_int )
        {
            if( var_SetInteger( p_input_thread, "video-es", i_track ) < 0 )
                break;
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    free(val_list);
    input_Release(p_input_thread);
    return i_ret;
}

/******************************************************************************
 * libvlc_video_set_deinterlace : enable/disable/auto deinterlace and filter
 *****************************************************************************/
void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi, int deinterlace,
                                   const char *psz_mode )
{
    if (deinterlace != 0 && deinterlace != 1)
        deinterlace = -1;

    if (psz_mode
     && strcmp (psz_mode, "blend")    && strcmp (psz_mode, "bob")
     && strcmp (psz_mode, "discard")  && strcmp (psz_mode, "linear")
     && strcmp (psz_mode, "mean")     && strcmp (psz_mode, "x")
     && strcmp (psz_mode, "yadif")    && strcmp (psz_mode, "yadif2x")
     && strcmp (psz_mode, "phosphor") && strcmp (psz_mode, "ivtc")
     && strcmp (psz_mode, "auto"))
        return;

    if (psz_mode && deinterlace != 0)
        var_SetString (p_mi, "deinterlace-mode", psz_mode);

    var_SetInteger (p_mi, "deinterlace", deinterlace);

    size_t n;
    vout_thread_t **pp_vouts = GetVouts (p_mi, &n);
    for (size_t i = 0; i < n; i++)
    {
        vout_thread_t *p_vout = pp_vouts[i];

        if (psz_mode && deinterlace != 0)
            var_SetString (p_vout, "deinterlace-mode", psz_mode);

        var_SetInteger (p_vout, "deinterlace", deinterlace);
        vout_Release(p_vout);
    }
    free (pp_vouts);
}

/* ************** */
/* module helpers */
/* ************** */

static int get_filter_str( vlc_object_t *p_parent, const char *psz_name,
                           bool b_add, const char **ppsz_filter_type,
                           char **ppsz_filter_value)
{
    char *psz_parser;
    char *psz_string;
    const char *psz_filter_type;

    module_t *p_obj = module_find( psz_name );
    if( !p_obj )
    {
        msg_Err( p_parent, "Unable to find filter module \"%s\".", psz_name );
        return VLC_EGENERIC;
    }

    if( module_provides( p_obj, "video filter" ) )
    {
        psz_filter_type = "video-filter";
    }
    else if( module_provides( p_obj, "sub source" ) )
    {
        psz_filter_type = "sub-source";
    }
    else if( module_provides( p_obj, "sub filter" ) )
    {
        psz_filter_type = "sub-filter";
    }
    else
    {
        msg_Err( p_parent, "Unknown video filter type." );
        return VLC_EGENERIC;
    }

    psz_string = var_GetString( p_parent, psz_filter_type );

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
                return VLC_EGENERIC;
            }
            free( psz_parser );
        }
        else
        {
            free( psz_string );
            return VLC_EGENERIC;
        }
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
                *(psz_string+strlen(psz_string ) -1 ) = '\0';
        }
        else
        {
            free( psz_string );
            return VLC_EGENERIC;
        }
    }

    *ppsz_filter_type = psz_filter_type;
    *ppsz_filter_value = psz_string;
    return VLC_SUCCESS;
}

static bool find_sub_source_by_name( libvlc_media_player_t *p_mi, const char *restrict name )
{
    vout_thread_t *vout = GetVout( p_mi, 0 );
    if (!vout)
        return false;

    char *psz_sources = var_GetString( vout, "sub-source" );
    if( !psz_sources )
    {
        libvlc_printerr( "%s not enabled", name );
        vout_Release(vout);
        return false;
    }

    /* Find 'name'  */
    char *p = strstr( psz_sources, name );
    free( psz_sources );
    vout_Release(vout);
    return (p != NULL);
}

typedef const struct {
    const char name[20];
    unsigned type;
} opt_t;

static void
set_value( libvlc_media_player_t *p_mi, const char *restrict name,
           const opt_t *restrict opt, unsigned i_expected_type,
           const vlc_value_t *val, bool b_sub_source )
{
    if( !opt ) return;

    int i_type = opt->type;
    vlc_value_t new_val = *val;
    const char *psz_opt_name = opt->name;
    switch( i_type )
    {
        case 0: /* the enabler */
        {
            int i_ret = get_filter_str( VLC_OBJECT( p_mi ), opt->name, val->i_int,
                                        &psz_opt_name, &new_val.psz_string );
            if( i_ret != VLC_SUCCESS )
                return;
            i_type = VLC_VAR_STRING;
            break;
        }
        case VLC_VAR_INTEGER:
        case VLC_VAR_FLOAT:
        case VLC_VAR_STRING:
            if( i_expected_type != opt->type )
            {
                libvlc_printerr( "Invalid argument to %s", name );
                return;
            }
            break;
        default:
            libvlc_printerr( "Invalid argument to %s", name );
            return;
    }

    /* Set the new value to the media player. Next vouts created from this
     * media player will inherit this new value */
    var_SetChecked( p_mi, psz_opt_name, i_type, new_val );

    /* Set the new value to every loaded vouts */
    size_t i_vout_count;
    vout_thread_t **pp_vouts = GetVouts( p_mi, &i_vout_count );
    for( size_t i = 0; i < i_vout_count; ++i )
    {
        var_SetChecked( pp_vouts[i], psz_opt_name, i_type, new_val );
        if( b_sub_source )
            var_TriggerCallback( pp_vouts[i], "sub-source" );
        vout_Release(pp_vouts[i]);
    }

    if( opt->type == 0 )
        free( new_val.psz_string );
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
            bool b_enabled = find_sub_source_by_name( p_mi, name );
            return b_enabled ? 1 : 0;
        }
    case VLC_VAR_INTEGER:
        return var_GetInteger(p_mi, opt->name);
    case VLC_VAR_FLOAT:
        return lroundf(var_GetFloat(p_mi, opt->name));
    default:
        libvlc_printerr( "Invalid argument to %s in %s", name, "get int" );
        return 0;
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
    set_value( p_mi, "marq", marq_option_bynumber(option), VLC_VAR_INTEGER,
               &(vlc_value_t) { .i_int = value }, true );
}

/*****************************************************************************
 * libvlc_video_set_marquee_string: set a string option
 *****************************************************************************/
void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
                unsigned option, const char * value )
{
    set_value( p_mi, "marq", marq_option_bynumber(option), VLC_VAR_STRING,
               &(vlc_value_t){ .psz_string = (char *)value }, true );
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
    set_value( p_mi,"logo",logo_option_bynumber(option), VLC_VAR_STRING,
               &(vlc_value_t){ .psz_string = (char *)psz_value }, true );
}


void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                unsigned option, int value )
{
    set_value( p_mi, "logo", logo_option_bynumber(option), VLC_VAR_INTEGER,
               &(vlc_value_t) { .i_int = value }, true );
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
        { "adjust",     0 },
        { "contrast",   VLC_VAR_FLOAT },
        { "brightness", VLC_VAR_FLOAT },
        { "hue",        VLC_VAR_FLOAT },
        { "saturation", VLC_VAR_FLOAT },
        { "gamma",      VLC_VAR_FLOAT },
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
    set_value( p_mi, "adjust", adjust_option_bynumber(option), VLC_VAR_INTEGER,
               &(vlc_value_t) { .i_int = value }, false );
}


int libvlc_video_get_adjust_int( libvlc_media_player_t *p_mi,
                                 unsigned option )
{
    return get_int( p_mi, "adjust", adjust_option_bynumber(option) );
}


void libvlc_video_set_adjust_float( libvlc_media_player_t *p_mi,
                                    unsigned option, float value )
{
    set_value( p_mi, "adjust", adjust_option_bynumber(option), VLC_VAR_FLOAT,
               &(vlc_value_t) { .f_float = value }, false );
}


float libvlc_video_get_adjust_float( libvlc_media_player_t *p_mi,
                                     unsigned option )
{
    return get_float( p_mi, "adjust", adjust_option_bynumber(option) );
}
