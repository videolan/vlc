/*****************************************************************************
 * resource.c
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_decoder.h>
#include <vlc_vout.h>
#include <vlc_spu.h>
#include <vlc_aout.h>
#include <vlc_sout.h>
#include "../libvlc.h"
#include "../stream_output/stream_output.h"
#include "../audio_output/aout_internal.h"
#include "../video_output/vout_internal.h"
#include "input_interface.h"
#include "event.h"
#include "resource.h"

struct vout_resource
{
    vout_thread_t *vout;
    enum vlc_vout_order order;
    bool started;

    struct vlc_list node;
};

struct input_resource_t
{
    vlc_atomic_rc_t rc;

    vlc_object_t   *p_parent;

    /* This lock is used to serialize request and protect
     * our variables */
    vlc_mutex_t    lock;

    /* */
    input_thread_t *p_input;

    sout_instance_t *p_sout;
    vout_thread_t   *p_vout_dummy;
    struct vout_resource *vout_rsc_free;

    /* This lock is used to protect vout resources access (for hold)
     * It is a special case because of embed video (possible deadlock
     * between vout window request and vout holds in some(qt) interface)
     */
    vlc_mutex_t    lock_hold;

    /* You need lock+lock_hold to write to the following variables and
     * only lock or lock_hold to read them */
    struct vlc_list vout_rscs;

    bool            b_aout_busy;
    audio_output_t *p_aout;
};

#define resource_GetFirstVoutRsc(resource) \
    vlc_list_first_entry_or_null(&resource->vout_rscs, struct vout_resource, node)

static inline struct vout_resource *
resource_GetVoutRsc(input_resource_t *resource, vout_thread_t *vout)
{
    struct vout_resource *vout_rsc;

    vlc_list_foreach(vout_rsc, &resource->vout_rscs, node)
        if (vout_rsc->vout == vout)
            return vout_rsc;

    vlc_assert_unreachable();
}

static inline struct vout_resource *
vout_resource_Create(vout_thread_t *vout)
{
    struct vout_resource *vout_rsc = malloc(sizeof(*vout_rsc));
    if (unlikely(vout_rsc == NULL))
        return NULL;

    vout_rsc->started = false;
    vout_rsc->vout = vout;
    return vout_rsc;
}

static inline void
vout_resource_Add(struct vout_resource *vout_rsc, input_resource_t *resource)
{
    vlc_list_append(&vout_rsc->node, &resource->vout_rscs);
}

static inline vout_thread_t *
vout_resource_Remove(struct vout_resource *vout_rsc)
{
    vlc_list_remove(&vout_rsc->node);
    vout_thread_t *vout = vout_rsc->vout;
    free(vout_rsc);

    assert(vout);
    return vout;
}

/* */
static void DestroySout( input_resource_t *p_resource )
{
#ifdef ENABLE_SOUT
    if( p_resource->p_sout )
        sout_DeleteInstance( p_resource->p_sout );
#endif
    p_resource->p_sout = NULL;
}

static sout_instance_t *RequestSout( input_resource_t *p_resource,
                                     sout_instance_t *p_sout, const char *psz_sout )
{
#ifdef ENABLE_SOUT
    if( !p_sout && !psz_sout )
    {
        if( p_resource->p_sout )
        {
            msg_Dbg( p_resource->p_sout, "destroying useless sout" );
            DestroySout( p_resource );
        }
        return NULL;
    }

    assert( !p_sout || ( !p_resource->p_sout && !psz_sout ) );

    /* Check the validity of the sout */
    if( p_resource->p_sout &&
        strcmp( p_resource->p_sout->psz_sout, psz_sout ) )
    {
        msg_Dbg( p_resource->p_parent, "destroying unusable sout" );
        DestroySout( p_resource );
    }

    if( psz_sout )
    {
        if( p_resource->p_sout )
        {
            /* Reuse it */
            msg_Dbg( p_resource->p_parent, "reusing sout" );
            msg_Dbg( p_resource->p_parent, "you probably want to use gather stream_out" );
        }
        else
        {
            /* Create a new one */
            p_resource->p_sout = sout_NewInstance( p_resource->p_parent, psz_sout );
        }

        p_sout = p_resource->p_sout;
        p_resource->p_sout = NULL;

        return p_sout;
    }
    else
    {
        p_resource->p_sout = p_sout;
        return NULL;
    }
#else
    VLC_UNUSED (p_resource); VLC_UNUSED (p_sout); VLC_UNUSED (psz_sout);
    return NULL;
#endif
}

/* */
static void DestroyVout( input_resource_t *p_resource )
{
    assert( vlc_list_is_empty( &p_resource->vout_rscs )
         || p_resource->vout_rsc_free == resource_GetFirstVoutRsc( p_resource ) );

    if( p_resource->vout_rsc_free )
    {
        vlc_mutex_lock(&p_resource->lock_hold);
        vout_thread_t *vout = vout_resource_Remove( p_resource->vout_rsc_free );
        vlc_mutex_unlock(&p_resource->lock_hold);

        vout_Close( vout );
        p_resource->vout_rsc_free = NULL;
    }
}

static void DisplayVoutTitle( input_resource_t *p_resource,
                              vout_thread_t *p_vout )
{
    if( p_resource->p_input == NULL )
        return;

    /* TODO display the title only one time for the same input ? */

    input_item_t *p_item = input_GetItem( p_resource->p_input );

    char *psz_nowplaying = input_item_GetNowPlayingFb( p_item );
    if( psz_nowplaying && *psz_nowplaying )
    {
        vout_DisplayTitle( p_vout, psz_nowplaying );
    }
    else
    {
        char *psz_artist = input_item_GetArtist( p_item );
        char *psz_name = input_item_GetTitle( p_item );

        if( !psz_name || *psz_name == '\0' )
        {
            free( psz_name );
            psz_name = input_item_GetName( p_item );
        }
        if( psz_artist && *psz_artist )
        {
            char *psz_string;
            if( asprintf( &psz_string, "%s - %s", psz_name, psz_artist ) != -1 )
            {
                vout_DisplayTitle( p_vout, psz_string );
                free( psz_string );
            }
        }
        else if( psz_name )
        {
            vout_DisplayTitle( p_vout, psz_name );
        }
        free( psz_name );
        free( psz_artist );
    }
    free( psz_nowplaying );
}

/* Audio output */
audio_output_t *input_resource_GetAout( input_resource_t *p_resource )
{
    audio_output_t *p_aout;

    vlc_mutex_lock( &p_resource->lock_hold );
    p_aout = p_resource->p_aout;

    if( p_aout == NULL || p_resource->b_aout_busy )
    {
        msg_Dbg( p_resource->p_parent, "creating audio output" );
        vlc_mutex_unlock( &p_resource->lock_hold );

        p_aout = aout_New( p_resource->p_parent );
        if( p_aout == NULL )
            return NULL; /* failed */

        vlc_mutex_lock( &p_resource->lock_hold );
        if( p_resource->p_aout == NULL )
            p_resource->p_aout = p_aout;
    }
    else
        msg_Dbg( p_resource->p_parent, "reusing audio output" );

    if( p_resource->p_aout == p_aout )
    {
        assert( !p_resource->b_aout_busy );
        p_resource->b_aout_busy = true;
    }
    vlc_mutex_unlock( &p_resource->lock_hold );
    return p_aout;
}

void input_resource_PutAout( input_resource_t *p_resource,
                             audio_output_t *p_aout )
{
    assert( p_aout != NULL );

    vlc_mutex_lock( &p_resource->lock_hold );
    if( p_aout == p_resource->p_aout )
    {
        assert( p_resource->b_aout_busy );
        p_resource->b_aout_busy = false;
        msg_Dbg( p_resource->p_parent, "keeping audio output" );
        p_aout = NULL;
    }
    else
        msg_Dbg( p_resource->p_parent, "destroying extra audio output" );
    vlc_mutex_unlock( &p_resource->lock_hold );

    if( p_aout != NULL )
        aout_Destroy( p_aout );
}

audio_output_t *input_resource_HoldAout( input_resource_t *p_resource )
{
    audio_output_t *p_aout;

    vlc_mutex_lock( &p_resource->lock_hold );
    p_aout = p_resource->p_aout;
    if( p_aout != NULL )
        aout_Hold(p_aout);
    vlc_mutex_unlock( &p_resource->lock_hold );

    return p_aout;
}

void input_resource_ResetAout( input_resource_t *p_resource )
{
    audio_output_t *p_aout = NULL;

    vlc_mutex_lock( &p_resource->lock_hold );
    if( !p_resource->b_aout_busy )
        p_aout = p_resource->p_aout;

    p_resource->p_aout = NULL;
    p_resource->b_aout_busy = false;
    vlc_mutex_unlock( &p_resource->lock_hold );

    if( p_aout != NULL )
        aout_Destroy( p_aout );
}

/* Common */
input_resource_t *input_resource_New( vlc_object_t *p_parent )
{
    input_resource_t *p_resource = calloc( 1, sizeof(*p_resource) );
    if( !p_resource )
        return NULL;

    p_resource->p_vout_dummy = vout_CreateDummy(p_parent);
    if( !p_resource->p_vout_dummy )
    {
        free( p_resource );
        return NULL;
    }

    vlc_list_init( &p_resource->vout_rscs );

    vlc_atomic_rc_init( &p_resource->rc );
    p_resource->p_parent = p_parent;
    vlc_mutex_init( &p_resource->lock );
    vlc_mutex_init( &p_resource->lock_hold );
    return p_resource;
}

void input_resource_Release( input_resource_t *p_resource )
{
    if( !vlc_atomic_rc_dec( &p_resource->rc ) )
        return;

    DestroySout( p_resource );
    DestroyVout( p_resource );
    if( p_resource->p_aout != NULL )
        aout_Destroy( p_resource->p_aout );

    vout_Release( p_resource->p_vout_dummy );
    free( p_resource );
}

input_resource_t *input_resource_Hold( input_resource_t *p_resource )
{
    vlc_atomic_rc_inc( &p_resource->rc );
    return p_resource;
}

void input_resource_SetInput( input_resource_t *p_resource, input_thread_t *p_input )
{
    vlc_mutex_lock( &p_resource->lock );

    if( p_resource->p_input && !p_input )
        assert( vlc_list_is_empty( &p_resource->vout_rscs )
             || p_resource->vout_rsc_free == resource_GetFirstVoutRsc( p_resource ) );

    /* */
    p_resource->p_input = p_input;

    vlc_mutex_unlock( &p_resource->lock );
}

static void input_resource_PutVoutLocked(input_resource_t *p_resource,
                                         vout_thread_t *vout, bool *has_stopped)
{
    assert(vout != NULL);
    struct vout_resource *vout_rsc = resource_GetVoutRsc(p_resource, vout);
    assert(vout_rsc != NULL);

    if (has_stopped != NULL)
        *has_stopped = vout_rsc->started;

    if (vout_rsc->started)
    {
        vout_StopDisplay(vout_rsc->vout);
        vout_rsc->started = false;
    }

    if (vout_rsc == resource_GetFirstVoutRsc(p_resource))
    {
        assert(p_resource->vout_rsc_free == NULL || p_resource->vout_rsc_free == vout_rsc);

        msg_Dbg(p_resource->p_parent, "saving a free vout");
        p_resource->vout_rsc_free = vout_rsc;
    }
    else
    {
        msg_Dbg(p_resource->p_parent, "destroying vout (already one saved or active)");

        vlc_mutex_lock(&p_resource->lock_hold);
        vout_resource_Remove(vout_rsc);
        vlc_mutex_unlock(&p_resource->lock_hold);

        vout_Stop(vout);
        vout_Close(vout);
    }
}

void input_resource_PutVout(input_resource_t *p_resource,
                                   vout_thread_t *vout, bool *stopped)
{
    vlc_mutex_lock( &p_resource->lock );
    input_resource_PutVoutLocked( p_resource, vout, stopped );
    vlc_mutex_unlock( &p_resource->lock );
}

static struct vout_resource *
RequestVoutRsc(input_resource_t *p_resource)
{
    struct vout_resource *vout_rsc = NULL;

    if( p_resource->vout_rsc_free != NULL )
    {
        /* The free vout is always the first one */
        msg_Dbg(p_resource->p_parent, "trying to reuse free vout");

        vout_rsc = p_resource->vout_rsc_free;
        p_resource->vout_rsc_free = NULL;
    }
    else
    {
        /* Use the dummy vout as the parent of the future main vout. This
         * will allow the future vout to inherit all parameters
         * pre-configured on this dummy vout. */
        vlc_object_t *parent = vlc_list_is_empty( &p_resource->vout_rscs ) ?
            VLC_OBJECT(p_resource->p_vout_dummy) : p_resource->p_parent;
        vout_thread_t *vout = vout_Create(parent);
        if (vout == NULL)
            return NULL;

        vout_rsc = vout_resource_Create(vout);
        if (vout_rsc == NULL)
        {
            vout_Close(vout);
            return NULL;
        }

        vout_rsc->order = vlc_list_is_empty( &p_resource->vout_rscs ) ?
            VLC_VOUT_ORDER_PRIMARY : VLC_VOUT_ORDER_SECONDARY;

        vlc_mutex_lock(&p_resource->lock_hold);
        vout_resource_Add(vout_rsc, p_resource);
        vlc_mutex_unlock(&p_resource->lock_hold);
    }

    return vout_rsc;
}

vout_thread_t *input_resource_RequestVout(input_resource_t *p_resource,
                                          vlc_video_context *vctx,
                                          const vout_configuration_t *cfg,
                                          enum vlc_vout_order *order,
                                          bool *has_started)
{
    vlc_mutex_lock( &p_resource->lock );
    struct vout_resource *vout_rsc = NULL;

    if (has_started != NULL)
        *has_started = false;

    vout_configuration_t dcfg = *cfg;
    if (dcfg.vout == NULL)
    {
        vout_rsc = RequestVoutRsc(p_resource);
        if (vout_rsc == NULL)
        {
            vlc_mutex_unlock( &p_resource->lock );
            return NULL;
        }
        dcfg.vout = vout_rsc->vout;
    }
    else
    {
        vout_rsc = resource_GetVoutRsc(p_resource, dcfg.vout);
        assert(vout_rsc != NULL);

        /* the caller is going to reuse the free vout, it's not free anymore */
        if (p_resource->vout_rsc_free == vout_rsc)
            p_resource->vout_rsc_free = NULL;
    }

    if (order != NULL)
        *order = vout_rsc->order;

    if (dcfg.fmt == NULL)
    {
        /* A NULL fmt means that only the vout creation is requested, do not
         * start it with vout_Request(). */
        vlc_mutex_unlock(&p_resource->lock);
        return dcfg.vout;
    }

    if (vout_rsc->started)
    {
        assert(cfg->vout != NULL);
        int ret = vout_ChangeSource(dcfg.vout, dcfg.fmt);
        if (ret == 0)
        {
            vlc_mutex_unlock(&p_resource->lock);
            return dcfg.vout;
        }
    }

    if (vout_Request(&dcfg, vctx, p_resource->p_input)) {
        input_resource_PutVoutLocked(p_resource, dcfg.vout, NULL);
        vlc_mutex_unlock(&p_resource->lock);
        return NULL;
    }

    vout_rsc->started = true;
    if (has_started != NULL)
        *has_started = true;

    DisplayVoutTitle(p_resource, cfg->vout);

    /* Send original viewpoint to the input in order to update other ESes */
    if (p_resource->p_input != NULL)
    {
        input_control_param_t param = { .viewpoint = cfg->fmt->pose };
        input_ControlPush(p_resource->p_input, INPUT_CONTROL_SET_INITIAL_VIEWPOINT,
                          &param);
    }
    vlc_mutex_unlock( &p_resource->lock );

    return dcfg.vout;
}

vout_thread_t *input_resource_HoldVout( input_resource_t *p_resource )
{
    vlc_mutex_lock( &p_resource->lock_hold );

    struct vout_resource *vout_rsc = resource_GetFirstVoutRsc( p_resource );
    vout_thread_t *p_vout = vout_rsc != NULL ? vout_rsc->vout : NULL;
    if( p_vout )
        vout_Hold(p_vout);

    vlc_mutex_unlock( &p_resource->lock_hold );

    return p_vout;
}

vout_thread_t *input_resource_HoldDummyVout( input_resource_t *p_resource )
{
    return vout_Hold(p_resource->p_vout_dummy);
}

void input_resource_HoldVouts( input_resource_t *p_resource, vout_thread_t ***ppp_vout,
                               size_t *pi_vout )
{
    vout_thread_t **pp_vout;

    *pi_vout = 0;
    *ppp_vout = NULL;

    vlc_mutex_lock( &p_resource->lock_hold );

    if( vlc_list_is_empty( &p_resource->vout_rscs ) )
        goto exit;

    size_t count = 0;
    struct vout_resource *vout_rsc;
    vlc_list_foreach( vout_rsc, &p_resource->vout_rscs, node )
        count ++;

    pp_vout = vlc_alloc( count, sizeof(*pp_vout) );
    if( !pp_vout )
        goto exit;

    *ppp_vout = pp_vout;
    *pi_vout = count;;

    count = 0;
    vlc_list_foreach( vout_rsc, &p_resource->vout_rscs, node )
        pp_vout[count++] = vout_Hold( vout_rsc->vout );

exit:
    vlc_mutex_unlock( &p_resource->lock_hold );
}

void input_resource_StopFreeVout(input_resource_t *p_resource)
{
    vlc_mutex_lock(&p_resource->lock);
    if (p_resource->vout_rsc_free != NULL)
    {
        msg_Dbg(p_resource->vout_rsc_free->vout, "stop free vout");
        vout_Stop(p_resource->vout_rsc_free->vout);
    }
    vlc_mutex_unlock(&p_resource->lock);
}

/* */
sout_instance_t *input_resource_RequestSout( input_resource_t *p_resource, sout_instance_t *p_sout, const char *psz_sout )
{
    vlc_mutex_lock( &p_resource->lock );
    sout_instance_t *p_ret = RequestSout( p_resource, p_sout, psz_sout );
    vlc_mutex_unlock( &p_resource->lock );

    return p_ret;
}
void input_resource_TerminateSout( input_resource_t *p_resource )
{
    input_resource_RequestSout( p_resource, NULL, NULL );
}
