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
    vlc_player_t *player = p_mi->player;
    return vlc_player_vout_HoldAll(player, n);
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

void libvlc_set_fullscreen(libvlc_media_player_t *p_mi, bool b_fullscreen)
{
    /* This will work even if the video is not currently active */
    var_SetBool(p_mi, "fullscreen", b_fullscreen);

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

bool libvlc_get_fullscreen( libvlc_media_player_t *p_mi )
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

int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned ignored,
                           unsigned *restrict px, unsigned *restrict py )
{
    (void) ignored;
    if (p_mi->p_md == NULL)
        return -1;

    int ret = -1;
    libvlc_media_track_t *track =
        libvlc_media_player_get_selected_track( p_mi, libvlc_track_video );

    if (track)
    {
        *px = track->video->i_width;
        *py = track->video->i_height;
        ret = 0;
        libvlc_media_track_release(track);
    }

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

    enum vlc_player_whence whence = b_absolute ? VLC_PLAYER_WHENCE_ABSOLUTE
                                               : VLC_PLAYER_WHENCE_RELATIVE;

    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    vlc_player_UpdateViewpoint(player, &update, whence);

    vlc_player_Unlock(player);

    /* may not fail anymore, keep int not to break the API */
    return 0;
}

int libvlc_video_get_spu( libvlc_media_player_t *p_mi )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    const struct vlc_player_track *track =
        vlc_player_GetSelectedTrack(player, SPU_ES);
    int i_spu = track ? vlc_es_id_GetInputId(track->es_id) : -1;

    vlc_player_Unlock(player);
    return i_spu;
}

int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    int ret = vlc_player_GetTrackCount(p_mi->player, SPU_ES);

    vlc_player_Unlock(player);
    return ret;
}

libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, SPU_ES );
}

int libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu )
{
    int i_ret = -1;

    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    size_t count = vlc_player_GetSubtitleTrackCount(player);
    for (size_t i = 0; i < count; i++)
    {
        const struct vlc_player_track *track =
            vlc_player_GetSubtitleTrackAt(player, i);
        if (i_spu == vlc_es_id_GetInputId(track->es_id))
        {
            /* found */
            vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    vlc_player_Unlock(player);
    return i_ret;
}

int64_t libvlc_video_get_spu_delay( libvlc_media_player_t *p_mi )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    vlc_tick_t delay = vlc_player_GetSubtitleDelay(player);

    vlc_player_Unlock(player);

    return US_FROM_VLC_TICK(delay);
}

int libvlc_video_set_spu_delay( libvlc_media_player_t *p_mi,
                                int64_t i_delay )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    vlc_player_SetSubtitleDelay(player, VLC_TICK_FROM_US(i_delay),
                                VLC_PLAYER_WHENCE_ABSOLUTE);

    vlc_player_Unlock(player);
    /* may not fail anymore, keep int not to break the API */
    return 0;
}

void libvlc_video_set_spu_text_scale( libvlc_media_player_t *p_mi,
                                      float f_scale )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    vlc_player_SetSubtitleTextScale(player, lroundf(f_scale * 100.f));

    vlc_player_Unlock(player);
}

static void libvlc_video_set_crop(libvlc_media_player_t *mp,
                                  const char *geometry)
{
    var_SetString(mp, "crop", geometry);

    size_t n;
    vout_thread_t **vouts = GetVouts(mp, &n);

    for (size_t i = 0; i < n; i++)
    {
        var_SetString(vouts[i], "crop", geometry);
        vout_Release(vouts[i]);
    }
    free(vouts);
}

void libvlc_video_set_crop_ratio(libvlc_media_player_t *mp,
                                 unsigned num, unsigned den)
{
    char geometry[2 * (3 * sizeof (unsigned) + 1)];

    if (den == 0)
        geometry[0] = '\0';
    else
        sprintf(geometry, "%u:%u", num, den);

    libvlc_video_set_crop(mp, geometry);
}

void libvlc_video_set_crop_window(libvlc_media_player_t *mp,
                                  unsigned x, unsigned y,
                                  unsigned width, unsigned height)
{
    char geometry[4 * (3 * sizeof (unsigned) + 1)];

    assert(width != 0 && height != 0);
    sprintf(geometry, "%ux%u+%u+%u", x, y, width, height);
    libvlc_video_set_crop(mp, geometry);
}

void libvlc_video_set_crop_border(libvlc_media_player_t *mp,
                                  unsigned left, unsigned right,
                                  unsigned top, unsigned bottom)
{
    char geometry[4 * (3 * sizeof (unsigned) + 1)];

    sprintf(geometry, "%u+%u+%u+%u", left, top, right, bottom);
    libvlc_video_set_crop(mp, geometry);
}

int libvlc_video_get_teletext( libvlc_media_player_t *p_mi )
{
    return var_GetInteger (p_mi, "vbi-page");
}

void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    if (i_page == 0)
        vlc_player_SetTeletextEnabled(player, false);
    else if (i_page > 0)
    {
        if (i_page >= 1000)
        {
            bool is_key = i_page == libvlc_teletext_key_red
                       || i_page == libvlc_teletext_key_green
                       || i_page == libvlc_teletext_key_yellow
                       || i_page == libvlc_teletext_key_blue
                       || i_page == libvlc_teletext_key_index;
            if (!is_key)
            {
                libvlc_printerr("Invalid key action");
                return;
            }
        }
        vlc_player_SetTeletextEnabled(player, true);
        vlc_player_SelectTeletextPage(player, i_page);
    }
    else
    {
        libvlc_printerr("Invalid page number");
        return;
    }

    vlc_player_Unlock(player);
}

int libvlc_video_get_track_count( libvlc_media_player_t *p_mi )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    int ret = vlc_player_GetTrackCount(p_mi->player, VIDEO_ES);

    vlc_player_Unlock(player);
    return ret;
}

libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi )
{
    return libvlc_get_track_description( p_mi, VIDEO_ES );
}

int libvlc_video_get_track( libvlc_media_player_t *p_mi )
{
    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    const struct vlc_player_track * track =
        vlc_player_GetSelectedTrack(player, VIDEO_ES);
    int id = track ? vlc_es_id_GetInputId(track->es_id) : -1;

    vlc_player_Unlock(player);
    return id;
}

int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track )
{
    int i_ret = -1;

    vlc_player_t *player = p_mi->player;
    vlc_player_Lock(player);

    size_t count = vlc_player_GetVideoTrackCount(player);
    for( size_t i = 0; i < count; i++ )
    {
        const struct vlc_player_track *track =
            vlc_player_GetVideoTrackAt(player, i);
        if (i_track == vlc_es_id_GetInputId(track->es_id))
        {
            /* found */
            vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
            i_ret = 0;
            goto end;
        }
    }
    libvlc_printerr( "Track identifier not found" );
end:
    vlc_player_Unlock(player);
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
