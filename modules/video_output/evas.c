/*****************************************************************************
 * evas.c: EFL Evas video output
 *****************************************************************************
 * Copyright (C) 2015 VLC authors, VideoLAN, and VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_filter.h>

#include <Evas.h>
#include <Ecore.h>

/* Deactivate TBM surface for now: it's impossible to specify a crop (visible
 * lines/pitch) and to avoid green borders. */
#undef HAVE_TIZEN_SDK

#ifdef HAVE_TIZEN_SDK
# include <tbm_surface.h>
#endif

#if defined(EVAS_VERSION_MAJOR) && defined(EVAS_VERSION_MINOR)
# if EVAS_VERSION_MAJOR > 1 || ( EVAS_VERSION_MAJOR == 1 && EVAS_VERSION_MINOR >= 10 )
#  define HAVE_EVAS_CALLBACK_KEY_UP
# endif
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CHROMA_TEXT "Chroma used"
#define CHROMA_LONGTEXT "Force use of a specific chroma for evas image"

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_shortname( "evas" )
    set_description( "evas video output" )
    set_capability( "vout display", 220 )
    add_shortcut( "evas" )
    add_string( "evas-image-chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int EvasImageSetup( vout_display_t * );
#ifdef HAVE_TIZEN_SDK
static bool EvasIsOpenGLSupported( vout_display_t * );
static int TbmSurfaceSetup( vout_display_t * );
#endif

/* Buffer and Event Fifo */

struct fifo_item
{
    struct fifo_item *p_next;
};

struct fifo
{
    vlc_mutex_t lock;
    struct fifo_item *p_first;
    struct fifo_item *p_last;
};

struct buffer
{
    struct fifo_item fifo_item;
    uint8_t *p[PICTURE_PLANE_MAX];
    bool b_locked;
#ifdef HAVE_TIZEN_SDK
    tbm_surface_h p_tbm_surface;
#endif
};

struct event
{
    struct fifo_item fifo_item;
    int i_type;
    union {
        int i_key;
        int i_button;
        struct {
            int i_x, i_y;
        } point;
    } u;
};

struct vout_display_sys_t
{
    picture_pool_t  *p_pool;

    int              i_width, i_height;

    /* Planes */
    plane_t          p_planes[PICTURE_PLANE_MAX];
    int              i_planes_order[PICTURE_PLANE_MAX];
    unsigned int     i_nb_planes;

    /* Array of video buffers */
    struct buffer   *p_buffers;
    unsigned int     i_nb_buffers;

    /* FIFO of unlocked video buffers */
    struct fifo      buffer_fifo;

    /* New buffer to display */
    struct buffer   *p_new_buffer;
    /* Buffer being displayed */
    struct buffer   *p_current_buffer;

    /* Evas */
    Evas_Object     *p_evas;
    Ecore_Animator  *p_anim;

    /* If true: this module doesn't own the Evas_Object anymore */
    bool             b_evas_changed;
    /* If true: apply rotation to vd->fmt */
    bool             b_apply_rotation;

    /* FIFO of events */
    struct fifo      event_fifo;

    /* lock and cond used by EcoreMainLoopCallSync */
    vlc_mutex_t      cb_lock;
    vlc_cond_t       cb_wait;

    union {
        struct evas
        {
            Evas_Colorspace     i_colorspace;
            bool                b_yuv;
        } evas;
#ifdef HAVE_TIZEN_SDK
        struct {
            tbm_format          i_format;
            int                 i_angle;
        } tbm;
#endif
    } u;

    /* Specific callbacks for EvasImage or TBMSurface */
    int     (*pf_set_data)( vout_display_t * );
    int     (*pf_buffers_alloc)( vout_display_t *, video_format_t * );
    void    (*pf_buffers_free)( vout_display_t * );
};

struct picture_sys_t
{
    vout_display_sys_t *p_vd_sys;
    struct buffer *p_buffer;
};

static void
fifo_push( struct fifo *p_fifo, struct fifo_item *p_new )
{
    struct fifo_item *p_last;

    vlc_mutex_lock( &p_fifo->lock );

    p_new->p_next = NULL;
    p_last = p_fifo->p_last;

    if( p_last )
        p_last->p_next = p_new;
    else
        p_fifo->p_first = p_new;
    p_fifo->p_last = p_new;

    vlc_mutex_unlock( &p_fifo->lock );
}

static struct fifo_item *
fifo_pop( struct fifo *p_fifo )
{
    struct fifo_item *p_new;

    vlc_mutex_lock( &p_fifo->lock );

    p_new = p_fifo->p_first;

    if( p_new )
    {
        if( p_fifo->p_last == p_fifo->p_first )
            p_fifo->p_first = p_fifo->p_last = NULL;
        else
            p_fifo->p_first = p_new->p_next;
    }

    vlc_mutex_unlock( &p_fifo->lock );
    return p_new;
}

static void
fifo_init( struct fifo *p_fifo )
{
    vlc_mutex_init( &p_fifo->lock );
    p_fifo->p_first = p_fifo->p_last = NULL;
}

static void
fifo_deinit( struct fifo *p_fifo )
{
    vlc_mutex_destroy( &p_fifo->lock );
}

#define BUFFER_FIFO_PUSH( buffer ) fifo_push( &sys->buffer_fifo, (struct fifo_item *)(buffer) )
#define BUFFER_FIFO_POP() (struct buffer *) fifo_pop( &sys->buffer_fifo )
#define EVENT_FIFO_PUSH( event ) fifo_push( &sys->event_fifo, (struct fifo_item *)(event) )
#define EVENT_FIFO_POP() (struct event *) fifo_pop( &sys->event_fifo )

typedef int (*mainloop_cb)( vout_display_t *vd );

struct mainloop_cb_args
{
    vout_display_t *vd;
    mainloop_cb p_cb;
    int i_ret;
    bool b_signal;
};

static void
EcoreMainLoopCb( void *p_opaque )
{
    struct mainloop_cb_args *p_args = p_opaque;
    vout_display_sys_t *sys = p_args->vd->sys;

    p_args->i_ret = p_args->p_cb( p_args->vd );

    vlc_mutex_lock( &sys->cb_lock );
    p_args->b_signal = true;
    vlc_cond_signal( &sys->cb_wait );
    vlc_mutex_unlock( &sys->cb_lock );
}

static int
EcoreMainLoopCallSync( vout_display_t *vd, mainloop_cb p_cb )
{
    vout_display_sys_t *sys = vd->sys;
    struct mainloop_cb_args args = { .vd = vd, .p_cb = p_cb, .b_signal = false };
    ecore_main_loop_thread_safe_call_async( EcoreMainLoopCb, &args );

    vlc_mutex_lock( &sys->cb_lock );
    while( !args.b_signal )
        vlc_cond_wait( &sys->cb_wait, &sys->cb_lock );
    vlc_mutex_unlock( &sys->cb_lock );

    return args.i_ret;
}

#ifdef HAVE_EVAS_CALLBACK_KEY_UP
static void
EventSendKey( vout_display_t *vd, int i_key )
{
    vout_display_sys_t *sys = vd->sys;
    struct event *p_event = malloc( sizeof(struct event) );

    if( !p_event )
        return;
    p_event->i_type = VOUT_DISPLAY_EVENT_KEY;
    p_event->u.i_key = i_key;
    EVENT_FIFO_PUSH( p_event );
}

static void
EvasKeyUpCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj;
    Evas_Event_Key_Up *p_key_up = (Evas_Event_Key_Up *) event;

    EventSendKey( data, p_key_up->keycode );
}
#endif

static void
EventSendMouseMoved( vout_display_t *vd, int i_x, int i_y )
{
    vout_display_sys_t *sys = vd->sys;
    Evas_Coord i_ox, i_oy, i_ow, i_oh;
    struct event *p_event = malloc( sizeof(struct event) );

    if( !p_event )
        return;
    evas_object_geometry_get( sys->p_evas, &i_ox, &i_oy, &i_ow, &i_oh );
    p_event->i_type = VOUT_DISPLAY_EVENT_MOUSE_MOVED;
    p_event->u.point.i_x = ( ( i_x - i_ox ) * sys->i_width ) / i_ow;
    p_event->u.point.i_y = ( ( i_y - i_oy ) * sys->i_height ) / i_oh;
    EVENT_FIFO_PUSH( p_event );
}

static void
EventSendMouseButton( vout_display_t *vd, int i_type, int i_button )
{
    vout_display_sys_t *sys = vd->sys;
    struct event *p_event = malloc( sizeof(struct event) );

    if( !p_event )
        return;
    p_event->i_type = i_type;
    p_event->u.i_button = i_button;
    EVENT_FIFO_PUSH( p_event );
}

static void
EventMouseDownCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj;
    Evas_Event_Mouse_Down *p_mouse_down = (Evas_Event_Mouse_Down *) event;

    EventSendMouseButton( data, VOUT_DISPLAY_EVENT_MOUSE_PRESSED,
                             p_mouse_down->button - 1 );
}

static void
EvasMouseUpCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj;
    Evas_Event_Mouse_Up *p_mouse_up = (Evas_Event_Mouse_Up *) event;

    EventSendMouseButton( data, VOUT_DISPLAY_EVENT_MOUSE_RELEASED,
                             p_mouse_up->button - 1 );
}

static void
EvasMouseMoveCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj;
    Evas_Event_Mouse_Move *p_mouse_move = (Evas_Event_Mouse_Move *) event;
    EventSendMouseMoved( data, p_mouse_move->cur.canvas.x,
                            p_mouse_move->cur.canvas.y );
}

static void
EvasMultiDownCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj; (void) event;
    EventSendMouseButton( data, VOUT_DISPLAY_EVENT_MOUSE_PRESSED, 1 );
}

static void
EvasMultiUpCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj; (void) event;
    EventSendMouseButton( data, VOUT_DISPLAY_EVENT_MOUSE_RELEASED, 1 );
}

static void
EvasMultiMoveCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj;
    Evas_Event_Multi_Move *p_multi_move = (Evas_Event_Multi_Move *) event;

    EventSendMouseMoved( data, p_multi_move->cur.canvas.x,
                            p_multi_move->cur.canvas.y );
}

static void
FmtUpdate( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_place_t place;
    video_format_t src;

    vout_display_PlacePicture( &place, &vd->source, vd->cfg, false );

    if( sys->b_apply_rotation )
    {
        video_format_ApplyRotation( &src, &vd->source );
        vd->fmt.orientation = 0;
    }
    else
        src = vd->source;

    vd->fmt.i_width  = src.i_width  * place.width / src.i_visible_width;
    vd->fmt.i_height = src.i_height * place.height / src.i_visible_height;

    vd->fmt.i_visible_width  = place.width;
    vd->fmt.i_visible_height = place.height;
    vd->fmt.i_x_offset = src.i_x_offset * place.width / src.i_visible_width;
    vd->fmt.i_y_offset = src.i_y_offset * place.height / src.i_visible_height;

    sys->i_width  = vd->fmt.i_visible_width;
    sys->i_height = vd->fmt.i_visible_height;
}

static Eina_Bool
mainloop_evas_anim_cb( void *p_opaque )
{
    vout_display_t *vd = p_opaque;
    vout_display_sys_t *sys = vd->sys;
    evas_object_image_pixels_dirty_set( sys->p_evas, 1 );

    sys->p_anim = NULL;
    return false;
}

static void
EvasResizeCb( void *data, Evas *e, Evas_Object *obj, void *event )
{
    (void) e; (void) obj; (void) event;
    vout_display_t *vd = data;
    vout_display_sys_t *sys = vd->sys;

    sys->b_evas_changed = true;
}

static int
EvasDisplayMainloopCb( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    if( sys->b_evas_changed || sys->pf_set_data( vd ) )
        return -1;

    evas_object_image_data_update_add( sys->p_evas, 0, 0,
                                       sys->i_width, sys->i_height );
    evas_object_image_pixels_dirty_set( sys->p_evas, 0 );

    if( !sys->p_anim )
        sys->p_anim = ecore_animator_add( mainloop_evas_anim_cb, vd );

    if( sys->p_current_buffer )
        BUFFER_FIFO_PUSH( sys->p_current_buffer );

    sys->p_current_buffer = sys->p_new_buffer;
    sys->p_new_buffer = NULL;
    return 0;
}

static int
EvasInitMainloopCb( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

#ifdef HAVE_TIZEN_SDK
    if( !EvasIsOpenGLSupported( vd ) || TbmSurfaceSetup( vd ) )
#endif
    if( EvasImageSetup( vd ) )
        return -1;

    evas_object_image_alpha_set( sys->p_evas, 0 );
    evas_object_image_size_set( sys->p_evas, sys->i_width, sys->i_height );

    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MOUSE_DOWN,
                                    EventMouseDownCb, vd );
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MOUSE_UP,
                                    EvasMouseUpCb, vd );
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MOUSE_MOVE,
                                    EvasMouseMoveCb, vd );
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MULTI_DOWN,
                                    EvasMultiDownCb, vd );
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MULTI_UP,
                                    EvasMultiUpCb, vd );
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_MULTI_MOVE,
                                    EvasMultiMoveCb, vd );
#ifdef HAVE_EVAS_CALLBACK_KEY_UP
    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_KEY_UP,
                                    EvasKeyUpCb, sys );
#endif

    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_IMAGE_RESIZE,
                                    EvasResizeCb, vd );

    return 0;
}

static int
EvasDeinitMainloopCb( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    if( sys->p_anim )
    {
        ecore_animator_del( sys->p_anim );
        sys->p_anim = NULL;
    }

    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_IMAGE_RESIZE,
                                         EvasResizeCb, vd );

    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MOUSE_DOWN,
                                         EventMouseDownCb, vd );
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MOUSE_UP,
                                         EvasMouseUpCb, vd );
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MOUSE_MOVE,
                                         EvasMouseMoveCb, vd );
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MULTI_DOWN,
                                         EvasMultiDownCb, vd );
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MULTI_UP,
                                         EvasMultiUpCb, vd );
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_MULTI_MOVE,
                                         EvasMultiMoveCb, vd );
#ifdef HAVE_EVAS_CALLBACK_KEY_UP
    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_KEY_UP,
                                         EvasKeyUpCb, vd );
#endif

    if( !sys->b_evas_changed )
    {
        evas_object_image_data_set( sys->p_evas, NULL );
        evas_object_image_pixels_dirty_set( sys->p_evas, 0 );
    }

    return 0;
}

static int
EvasResetMainloopCb( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    if( sys->b_evas_changed )
        return -1;

    if( sys->p_anim )
    {
        ecore_animator_del( sys->p_anim );
        sys->p_anim = NULL;
    }

    evas_object_event_callback_del_full( sys->p_evas, EVAS_CALLBACK_IMAGE_RESIZE,
                                         EvasResizeCb, vd );

    FmtUpdate( vd );

    evas_object_image_data_set( sys->p_evas, NULL );
    evas_object_image_size_set( sys->p_evas, sys->i_width, sys->i_height );

    evas_object_event_callback_add( sys->p_evas, EVAS_CALLBACK_IMAGE_RESIZE,
                                    EvasResizeCb, vd );
    return 0;
}

static int
BuffersSetup( vout_display_t *vd, video_format_t *p_fmt,
              unsigned int *p_requested_count )
{
    vout_display_sys_t *sys = vd->sys;

    sys->i_nb_buffers = *p_requested_count;
    if( sys->pf_buffers_alloc( vd, p_fmt ) )
    {
        sys->i_nb_planes = 0;
        return VLC_EGENERIC;
    }
    *p_requested_count = sys->i_nb_buffers;

    for( unsigned int i = 0; i < sys->i_nb_buffers; ++i )
        BUFFER_FIFO_PUSH( &sys->p_buffers[i] );
    return VLC_SUCCESS;
}

static void
BuffersClean( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    if( sys->p_buffers )
        sys->pf_buffers_free( vd );
    sys->buffer_fifo.p_first = sys->buffer_fifo.p_last = NULL;
}

static picture_t *
PictureAlloc( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    picture_resource_t rsc;
    picture_t *p_pic = NULL;
    picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));

    if( !p_picsys )
        return NULL;

    p_picsys->p_vd_sys = vd->sys;
    memset(&rsc, 0, sizeof(picture_resource_t));
    rsc.p_sys = p_picsys;
    for( unsigned int i = 0; i < sys->i_nb_planes; ++i )
    {
        rsc.p[i].i_lines = sys->p_planes[i].i_lines;
        rsc.p[i].i_pitch = sys->p_planes[i].i_pitch;
    }

    p_pic = picture_NewFromResource( &vd->fmt, &rsc );
    if( !p_pic )
    {
        free( p_picsys );
        return NULL;
    }

    return p_pic;
}

static int
PoolLockPicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    vout_display_sys_t *sys = p_picsys->p_vd_sys;

    struct buffer *p_buffer = BUFFER_FIFO_POP();
    if( !p_buffer || !p_buffer->p[0] )
        return -1;

    p_picsys->p_buffer = p_buffer;
    for( unsigned int i = 0; i < sys->i_nb_planes; ++i )
        p_pic->p[i].p_pixels = p_buffer->p[i];
    return 0;
}

static void
PoolUnlockPicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    vout_display_sys_t *sys = p_picsys->p_vd_sys;

    if( p_picsys->p_buffer )
    {
        BUFFER_FIFO_PUSH( p_picsys->p_buffer );
        p_picsys->p_buffer = NULL;
    }
}

static picture_pool_t *
PoolAlloc( vout_display_t *vd, unsigned i_requested_count )
{
    picture_t **pp_pics = NULL;
    picture_pool_t *p_pool;
    picture_pool_configuration_t pool_cfg;

    msg_Dbg(vd, "PoolAlloc, requested_count: %d", i_requested_count);

    i_requested_count++; /* picture owned by evas */

    if( BuffersSetup( vd, &vd->fmt, &i_requested_count) )
    {
        msg_Err( vd, "BuffersSetup failed" );
        return NULL;
    }
    if( i_requested_count <= 1 )
    {
        msg_Err( vd, "not enough buffers allocated" );
        goto error;
    }
    i_requested_count--;

    msg_Dbg( vd, "PoolAlloc, got: %d", i_requested_count );

    if( !( pp_pics = calloc( i_requested_count, sizeof(picture_t) ) ) )
        goto error;

    for( unsigned int i = 0; i < i_requested_count; ++i )
    {
        if( !( pp_pics[i] = PictureAlloc( vd ) ) )
        {
            i_requested_count = i;
            msg_Err( vd, "PictureAlloc failed" );
            goto error;
        }
    }

    memset( &pool_cfg, 0, sizeof(pool_cfg) );
    pool_cfg.picture_count = i_requested_count;
    pool_cfg.picture       = pp_pics;
    pool_cfg.lock          = PoolLockPicture;
    pool_cfg.unlock        = PoolUnlockPicture;

    p_pool = picture_pool_NewExtended( &pool_cfg );
    if( p_pool )
        return p_pool;

error:
    if( pp_pics )
    {
        for( unsigned int i = 0; i < i_requested_count; ++i )
            picture_Release( pp_pics[i] );
        free( pp_pics );
    }
    BuffersClean( vd );
    return NULL;
}

static picture_pool_t *
Pool( vout_display_t *vd, unsigned i_requested_count )
{
    vout_display_sys_t *sys = vd->sys;

    if( sys->p_pool == NULL )
        sys->p_pool = PoolAlloc( vd, i_requested_count );
    return sys->p_pool;
}

static void
Display( vout_display_t *vd, picture_t *p_pic, subpicture_t *p_subpic )
{
    (void) p_subpic;
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *p_picsys = p_pic->p_sys;

    if( p_picsys->p_buffer )
    {
        sys->p_new_buffer = p_picsys->p_buffer;
        p_picsys->p_buffer = NULL;

        EcoreMainLoopCallSync( vd, EvasDisplayMainloopCb );
    }
    picture_Release( p_pic );
}

static int
Control( vout_display_t *vd, int i_query, va_list ap )
{
    vout_display_sys_t *sys = vd->sys;

    switch( i_query )
    {
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    {
        vout_display_place_t place;
        video_format_t fmt;

        msg_Dbg( vd, "VOUT_DISPLAY_CHANGE_SOURCE_ASPECT" );

        video_format_ApplyRotation( &fmt, &vd->source );
        vout_display_PlacePicture( &place, &fmt, vd->cfg, false );

        if( place.width != (unsigned) sys->i_width
         && place.height != (unsigned) sys->i_height )
        {
            if( vd->info.has_pictures_invalid )
            {
                msg_Warn( vd, "ratio changed: invalidate pictures" );
                vout_display_SendEventPicturesInvalid( vd );
            }
            else
                return VLC_EGENERIC;
        }
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_RESET_PICTURES:
        msg_Dbg( vd, "VOUT_DISPLAY_RESET_PICTURES" );

        EcoreMainLoopCallSync( vd, EvasResetMainloopCb );

        BuffersClean( vd );

        if( sys->p_pool )
        {
            picture_pool_Release( sys->p_pool );
            sys->p_pool = NULL;
        }
        return VLC_SUCCESS;
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        return VLC_EGENERIC;
    default:
        msg_Warn( vd, "Unknown request in evas_output" );
        return VLC_EGENERIC;
    }
}

static void
Manage( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    struct event *p_event;

    while( ( p_event = EVENT_FIFO_POP() ) )
    {
        switch( p_event->i_type )
        {
            case VOUT_DISPLAY_EVENT_MOUSE_MOVED:
                vout_display_SendEventMouseMoved( vd, p_event->u.point.i_x,
                                                  p_event->u.point.i_y );
                break;
            case VOUT_DISPLAY_EVENT_MOUSE_PRESSED:
            case VOUT_DISPLAY_EVENT_MOUSE_RELEASED:
                vout_display_SendEvent( vd, p_event->i_type,
                                        p_event->u.i_button );
                break;
            case VOUT_DISPLAY_EVENT_KEY:
                vout_display_SendEventKey( vd, p_event->u.i_key );
                break;
        }
        free( p_event );
    }
}

static void
Close( vlc_object_t *p_this )
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;
    struct event *p_event;

    if (!sys)
        return;

    EcoreMainLoopCallSync( vd, EvasDeinitMainloopCb );

    BuffersClean( vd );
    fifo_deinit( &sys->buffer_fifo );

    if( sys->p_pool )
        picture_pool_Release(sys->p_pool);

    while( ( p_event = EVENT_FIFO_POP() ) )
        free( p_event );

    vlc_mutex_destroy( &sys->cb_lock );
    vlc_cond_destroy( &sys->cb_wait );

    free( sys );
}

static int
Open( vlc_object_t *p_this )
{
    vout_display_t *vd = (vout_display_t*)p_this;
    vout_display_sys_t *sys;
    Evas_Object *p_evas;

    if( vout_display_IsWindowed( vd ) )
        return VLC_EGENERIC;

    p_evas = var_InheritAddress( p_this, "drawable-evasobject" );
    if( !p_evas )
        return VLC_EGENERIC;

    vd->sys = sys = (struct vout_display_sys_t *) calloc( 1, sizeof(*sys) );
    if( !sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &sys->cb_lock );
    vlc_cond_init( &sys->cb_wait );
    fifo_init( &sys->buffer_fifo );
    fifo_init( &sys->event_fifo );

    sys->p_evas = p_evas;

    msg_Dbg( vd, "request video format: %4.4s",
             (const char *)&vd->fmt.i_chroma );

    /* Evas Initialisation must be done from the Mainloop */
    if( EcoreMainLoopCallSync( vd, EvasInitMainloopCb ) )
    {
        msg_Err( vd, "EvasInitMainloopCb failed" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    for( unsigned int i = 0; i < PICTURE_PLANE_MAX; ++i )
        sys->i_planes_order[i] = i;

    switch( vd->fmt.i_chroma )
    {
        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGB16:
            video_format_FixRgb(&vd->fmt);
            break;
        case VLC_CODEC_YV12:
            sys->i_planes_order[1] = 2;
            sys->i_planes_order[2] = 1;
        default:
            break;
    }

    msg_Dbg( vd, "got video format: %4.4s, size: %dx%d",
             (const char *)&vd->fmt.i_chroma,
             sys->i_width, sys->i_height );

    /* Setup vout_display */
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    return VLC_SUCCESS;
}

static int
EvasImageSetData( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    struct buffer *p_buffer = sys->p_new_buffer;

    if( sys->u.evas.b_yuv )
    {
        void *p_data = evas_object_image_data_get( sys->p_evas, 1 );
        const uint8_t **pp_rows = (const uint8_t **) p_data;

        if( !p_data )
            return -1;

        for( unsigned int i = 0; i < sys->i_nb_planes; ++i )
        {
            plane_t *p_plane = &sys->p_planes[sys->i_planes_order[i]];

            for( int j = 0; j < p_plane->i_visible_lines; ++j )
                *(pp_rows++) = &p_buffer->p[i][j * p_plane->i_pitch];
        }

        evas_object_image_data_set( sys->p_evas, p_data );
    }
    else
        evas_object_image_data_set( sys->p_evas, p_buffer->p[0] );

    return 0;
}

static void
EvasImageBuffersFree( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    for( unsigned int i = 0; i < sys->i_nb_buffers; i++ )
        aligned_free( sys->p_buffers[i].p[0] );
    free( sys->p_buffers );
    sys->p_buffers = NULL;
    sys->i_nb_buffers = 0;
    sys->i_nb_planes = 0;
}

static int
EvasImageBuffersAlloc( vout_display_t *vd, video_format_t *p_fmt )
{
    vout_display_sys_t *sys = vd->sys;
    picture_t *p_pic = NULL;
    picture_resource_t rsc;
    size_t i_bytes = 0;

    memset(&rsc, 0, sizeof(picture_resource_t));
    if( !( p_pic = picture_NewFromResource( p_fmt, &rsc ) ) )
        return -1;

    if( picture_Setup( p_pic, p_fmt ) )
    {
        picture_Release( p_pic );
        return -1;
    }

    for( int i = 0; i < p_pic->i_planes; ++i )
        memcpy( &sys->p_planes[i], &p_pic->p[i], sizeof(plane_t));
    sys->i_nb_planes = p_pic->i_planes;
    picture_Release( p_pic );

    if( !( sys->p_buffers = calloc( sys->i_nb_buffers, sizeof(struct buffer) ) ) )
        goto error;

    /* Calculate how big the new image should be */
    for( unsigned int i = 0; i < sys->i_nb_planes; i++ )
    {
        const plane_t *p = &sys->p_planes[i];

        if( p->i_pitch < 0 || p->i_lines <= 0 ||
            (size_t)p->i_pitch > (SIZE_MAX - i_bytes)/p->i_lines )
            goto error;
        i_bytes += p->i_pitch * p->i_lines;
    }

    if( !i_bytes )
        goto error;

    for( unsigned int i = 0; i < sys->i_nb_buffers; ++i )
    {
        struct buffer *p_buffer = &sys->p_buffers[i];

        p_buffer->p[0] = aligned_alloc( 16, i_bytes );

        if( !p_buffer->p[0] )
        {
            sys->i_nb_buffers = i;
            break;
        }

        for( unsigned int j = 1; j < sys->i_nb_planes; j++ )
            p_buffer->p[j] = &p_buffer->p[j-1][ sys->p_planes[j-1].i_lines *
                                                sys->p_planes[j-1].i_pitch ];
    }

    return 0;

error:
    if( sys->p_buffers )
        EvasImageBuffersFree( vd );
    return -1;
}

static int
EvasImageSetup( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    char *psz_fcc = var_InheritString( vd, "evas-image-chroma" );

    if( psz_fcc )
    {
        vd->fmt.i_chroma = vlc_fourcc_GetCodecFromString( VIDEO_ES, psz_fcc );
        free( psz_fcc );
    }

    switch( vd->fmt.i_chroma )
    {
        case VLC_CODEC_RGB32:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_ARGB8888;
            break;
        /* Not implemented yet */
#if 0
        case VLC_CODEC_RGB16:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_RGB565_A5P;
            break;
#endif
        case VLC_CODEC_YUYV:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_YCBCR422601_PL;
            sys->u.evas.b_yuv = true;
            break;
        /* FIXME: SIGSEGV in evas_gl_common_texture_nv12_update */
#if 0
        case VLC_CODEC_NV12:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_YCBCR420NV12601_PL;
            sys->u.evas.b_yuv = true;
            break;
#endif
        case VLC_CODEC_YV12:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_YCBCR422P601_PL;
            sys->u.evas.b_yuv = true;
            break;
        default:
        case VLC_CODEC_I420:
            sys->u.evas.i_colorspace = EVAS_COLORSPACE_YCBCR422P601_PL;
            vd->fmt.i_chroma = VLC_CODEC_I420;
            sys->u.evas.b_yuv = true;
            break;
    }

    evas_object_image_colorspace_set( sys->p_evas, sys->u.evas.i_colorspace );
    evas_object_image_data_set( sys->p_evas, NULL );

    /* No rotation support with EvasImage */
    sys->b_apply_rotation = true;
    FmtUpdate( vd );

    /* No aspect ratio support with EvasImage */
    vd->info.has_pictures_invalid = true;

    sys->pf_set_data = EvasImageSetData;
    sys->pf_buffers_alloc = EvasImageBuffersAlloc;
    sys->pf_buffers_free = EvasImageBuffersFree;

    msg_Dbg( vd, "using evas_image" );
    return 0;
}

#ifdef HAVE_TIZEN_SDK

struct tbm_format_to_vlc
{
   tbm_format  i_tbm_format;
   vlc_fourcc_t i_vlc_chroma;
};

struct tbm_format_to_vlc tbm_format_to_vlc_list[] = {
   { TBM_FORMAT_NV12, VLC_CODEC_NV12 },
   { TBM_FORMAT_YUV420, VLC_CODEC_I420 },
   { TBM_FORMAT_BGRA8888, VLC_CODEC_RGB32 },
};
#define TBM_FORMAT_TO_VLC_LIST_COUNT \
  ( sizeof(tbm_format_to_vlc_list) / sizeof(struct tbm_format_to_vlc) )

static bool
EvasIsOpenGLSupported( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    Evas *p_canvas = evas_object_evas_get(sys->p_evas);
    Eina_List *p_engine_list, *p_l;
    int i_render_id;
    char *psz_render_name;
    bool b_is_gl = false;

    if( !p_canvas )
        return false;
    i_render_id = evas_output_method_get( p_canvas );

    p_engine_list = evas_render_method_list();
    if( !p_engine_list )
        return false;

    EINA_LIST_FOREACH( p_engine_list, p_l, psz_render_name )
    {
        if( evas_render_method_lookup( psz_render_name ) == i_render_id )
        {
            b_is_gl = strncmp( psz_render_name, "gl", 2 ) == 0;
            break;
        }
    }

    evas_render_method_list_free( p_engine_list );
    return b_is_gl;
}

static int
TbmSurfaceBufferLock( struct buffer *p_buffer )
{
    tbm_surface_info_s tbm_surface_info;
    if( tbm_surface_map( p_buffer->p_tbm_surface, TBM_SURF_OPTION_WRITE,
                         &tbm_surface_info ) )
        return -1;

    for( unsigned i = 0; i < tbm_surface_info.num_planes; ++i )
        p_buffer->p[i] = tbm_surface_info.planes[i].ptr;
    return 0;
}

static int
TbmSurfaceBufferUnlock( struct buffer *p_buffer )
{
    tbm_surface_unmap( p_buffer->p_tbm_surface );
    p_buffer->p[0] = NULL;
    return 0;
}

static int
TbmSurfaceSetData( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    Evas_Native_Surface surf;

    TbmSurfaceBufferUnlock( sys->p_new_buffer );

    surf.version = EVAS_NATIVE_SURFACE_VERSION;
    surf.type = EVAS_NATIVE_SURFACE_TBM;
    surf.data.tizen.buffer = sys->p_new_buffer->p_tbm_surface;
    surf.data.tizen.rot = sys->u.tbm.i_angle;
    surf.data.tizen.ratio = 0;
    surf.data.tizen.flip = 0;
    evas_object_image_native_surface_set( sys->p_evas, &surf );

    if( sys->p_current_buffer )
        TbmSurfaceBufferLock( sys->p_current_buffer );
    return 0;
}

static void
TbmSurfaceBuffersFree( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;

    for( unsigned int i = 0; i < sys->i_nb_buffers; i++ )
    {
        if( sys->p_buffers[i].p[0] )
            tbm_surface_unmap( sys->p_buffers[i].p_tbm_surface );
        tbm_surface_destroy( sys->p_buffers[i].p_tbm_surface );
    }
    free( sys->p_buffers );
    sys->p_buffers = NULL;
    sys->i_nb_buffers = 0;
    sys->i_nb_planes = 0;
}

static int
TbmSurfaceBuffersAllocMainloopCb( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    tbm_surface_info_s tbm_surface_info;

    sys->i_nb_buffers = 2;

    if( !( sys->p_buffers = calloc( sys->i_nb_buffers, sizeof(struct buffer) ) ) )
        return -1;

    for( unsigned i = 0; i < sys->i_nb_buffers; ++i )
    {
        struct buffer *p_buffer = &sys->p_buffers[i];
        tbm_surface_h p_tbm_surface = tbm_surface_create( sys->i_width,
                                                          sys->i_height,
                                                          sys->u.tbm.i_format );
        if( !p_tbm_surface
         || tbm_surface_get_info( p_tbm_surface, &tbm_surface_info ) )
        {
            tbm_surface_destroy( p_tbm_surface );
            p_tbm_surface = NULL;
        }

        if( !p_tbm_surface )
        {
            sys->i_nb_buffers = i;
            break;
        }
        p_buffer->p_tbm_surface = p_tbm_surface;
        TbmSurfaceBufferLock( p_buffer );
    }

    sys->i_nb_planes = tbm_surface_info.num_planes;
    for( unsigned i = 0; i < tbm_surface_info.num_planes; ++i )
    {
        sys->p_planes[i].i_lines = tbm_surface_info.planes[i].size
                                 / tbm_surface_info.planes[i].stride;
        sys->p_planes[i].i_visible_lines = sys->p_planes[i].i_lines;
        sys->p_planes[i].i_pitch = tbm_surface_info.planes[i].stride;
        sys->p_planes[i].i_visible_pitch = sys->p_planes[i].i_pitch;
    }

    return 0;
}

static int
TbmSurfaceBuffersAlloc( vout_display_t *vd, video_format_t *p_fmt )
{
    (void) p_fmt;
    return EcoreMainLoopCallSync( vd, TbmSurfaceBuffersAllocMainloopCb );
}

static int
TbmSurfaceSetup( vout_display_t *vd )
{
    vout_display_sys_t *sys = vd->sys;
    tbm_format i_tbm_format = 0;
    bool b_found = false;
    uint32_t *p_formats;
    uint32_t i_format_num;

    for( unsigned int i = 0; i < TBM_FORMAT_TO_VLC_LIST_COUNT; ++i )
    {
        if( tbm_format_to_vlc_list[i].i_vlc_chroma == vd->fmt.i_chroma )
        {
            i_tbm_format = tbm_format_to_vlc_list[i].i_tbm_format;
            break;
        }
     }
     if( !i_tbm_format )
     {
        msg_Err( vd, "no tbm format found" );
        return -1;
     }

    if( tbm_surface_query_formats( &p_formats, &i_format_num ) )
    {
        msg_Warn( vd, "tbm_surface_query_formats failed" );
        return -1;
    }

    for( unsigned int i = 0; i < i_format_num; i++ )
    {
        if( p_formats[i] == i_tbm_format )
        {
            b_found = true;
            break;
        }
    }
    if( !b_found )
    {
        if( i_tbm_format != TBM_FORMAT_YUV420 )
        {
            msg_Warn( vd, "vlc format not matching any tbm format: trying with I420");
            i_tbm_format = TBM_FORMAT_YUV420;
            for( uint32_t i = 0; i < i_format_num; i++ )
            {
                if( p_formats[i] == i_tbm_format )
                {
                    vd->fmt.i_chroma = VLC_CODEC_I420;
                    b_found = true;
                    break;
                }
            }
        }
    }
    free( p_formats );

    if( !b_found )
    {
        msg_Warn( vd, "can't find any compatible tbm format" );
        return -1;
    }
    sys->u.tbm.i_format = i_tbm_format;

    switch( vd->fmt.orientation )
    {
        case ORIENT_ROTATED_90:
            sys->u.tbm.i_angle = 270;
            break;
        case ORIENT_ROTATED_180:
            sys->u.tbm.i_angle = 180;
            break;
        case ORIENT_ROTATED_270:
            sys->u.tbm.i_angle = 90;
            break;
        default:
            sys->u.tbm.i_angle = 0;
    }

    sys->b_apply_rotation = false;

    FmtUpdate( vd );

    vd->info.has_pictures_invalid = true;

    sys->pf_set_data = TbmSurfaceSetData;
    sys->pf_buffers_alloc = TbmSurfaceBuffersAlloc;
    sys->pf_buffers_free = TbmSurfaceBuffersFree;

    msg_Dbg( vd, "using tbm_surface" );

    return 0;
}
#endif
