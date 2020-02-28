/*****************************************************************************
 * input_internal.h: Internal input structures
 *****************************************************************************
 * Copyright (C) 1998-2006 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_INTERNAL_H
#define LIBVLC_INPUT_INTERNAL_H 1

#include <stddef.h>
#include <stdatomic.h>

#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_viewpoint.h>
#include <vlc_atomic.h>
#include <libvlc.h>
#include "input_interface.h"
#include "misc/interrupt.h"

struct input_stats;

/*****************************************************************************
 * input defines/constants.
 *****************************************************************************/

/**
 * Main structure representing an input thread. This structure is mostly
 * private. The only public fields are read-only and constant.
 */
typedef struct input_thread_t
{
    struct vlc_object_t obj;
} input_thread_t;

/*****************************************************************************
 * Input events and variables
 *****************************************************************************/

/**
 * Input state
 *
 * This enum is used by the variable "state"
 */
typedef enum input_state_e
{
    INIT_S = 0,
    OPENING_S,
    PLAYING_S,
    PAUSE_S,
    END_S,
    ERROR_S,
} input_state_e;

/**
 * Input events
 *
 * You can catch input event by adding a callback on the variable "intf-event".
 * This variable is an integer that will hold a input_event_type_e value.
 */
typedef enum input_event_type_e
{
    /* "state" has changed */
    INPUT_EVENT_STATE,
    /* b_dead is true */
    INPUT_EVENT_DEAD,

    /* "rate" has changed */
    INPUT_EVENT_RATE,

    /* "capabilities" has changed */
    INPUT_EVENT_CAPABILITIES,

    /* At least one of "position", "time" "length" has changed */
    INPUT_EVENT_TIMES,

    /* The output PTS changed */
    INPUT_EVENT_OUTPUT_CLOCK,

    /* A title has been added or removed or selected.
     * It implies that the chapter has changed (no chapter event is sent) */
    INPUT_EVENT_TITLE,
    /* A chapter has been added or removed or selected. */
    INPUT_EVENT_CHAPTER,

    /* A program ("program") has been added or removed or selected,
     * or "program-scrambled" has changed.*/
    INPUT_EVENT_PROGRAM,
    /* A ES has been added or removed or selected */
    INPUT_EVENT_ES,

    /* "record" has changed */
    INPUT_EVENT_RECORD,

    /* input_item_t media has changed */
    INPUT_EVENT_ITEM_META,
    /* input_item_t info has changed */
    INPUT_EVENT_ITEM_INFO,
    /* input_item_t epg has changed */
    INPUT_EVENT_ITEM_EPG,

    /* Input statistics have been updated */
    INPUT_EVENT_STATISTICS,
    /* At least one of "signal-quality" or "signal-strength" has changed */
    INPUT_EVENT_SIGNAL,

    /* "bookmark" has changed */
    INPUT_EVENT_BOOKMARK,

    /* cache" has changed */
    INPUT_EVENT_CACHE,

    /* A vout_thread_t object has been created/deleted by *the input* */
    INPUT_EVENT_VOUT,

    /* (pre-)parsing events */
    INPUT_EVENT_SUBITEMS,

    /* vbi_page has changed */
    INPUT_EVENT_VBI_PAGE,
    /* vbi_transparent has changed */
    INPUT_EVENT_VBI_TRANSPARENCY,

    /* subs_fps has changed */
    INPUT_EVENT_SUBS_FPS,

    /* Thumbnail generation */
    INPUT_EVENT_THUMBNAIL_READY,
} input_event_type_e;

#define VLC_INPUT_CAPABILITIES_SEEKABLE (1<<0)
#define VLC_INPUT_CAPABILITIES_PAUSEABLE (1<<1)
#define VLC_INPUT_CAPABILITIES_CHANGE_RATE (1<<2)
#define VLC_INPUT_CAPABILITIES_REWINDABLE (1<<3)
#define VLC_INPUT_CAPABILITIES_RECORDABLE (1<<4)

struct vlc_input_event_state
{
    input_state_e value;
    /* Only valid for PAUSE_S and PLAYING_S states */
    vlc_tick_t date;
};

struct vlc_input_event_times
{
    float percentage;
    vlc_tick_t ms;
    vlc_tick_t normal_time;
    vlc_tick_t length;
};

struct vlc_input_event_output_clock
{
    vlc_es_id_t *id;
    bool master;
    vlc_tick_t system_ts;
    vlc_tick_t ts;
    double rate;
    unsigned frame_rate;
    unsigned frame_rate_base;
};

struct vlc_input_event_title
{
    enum {
        VLC_INPUT_TITLE_NEW_LIST,
        VLC_INPUT_TITLE_SELECTED,
    } action;
    union
    {
        struct
        {
            input_title_t *const *array;
            size_t count;
        } list;
        size_t selected_idx;
    };
};

struct vlc_input_event_chapter
{
    int title;
    int seekpoint;
};

struct vlc_input_event_program {
    enum {
        VLC_INPUT_PROGRAM_ADDED,
        VLC_INPUT_PROGRAM_DELETED,
        VLC_INPUT_PROGRAM_UPDATED,
        VLC_INPUT_PROGRAM_SELECTED,
        VLC_INPUT_PROGRAM_SCRAMBLED,
    } action;
    int id;
    union {
        const char *title;
        bool scrambled;
    };
};

struct vlc_input_event_es {
    enum {
        VLC_INPUT_ES_ADDED,
        VLC_INPUT_ES_DELETED,
        VLC_INPUT_ES_UPDATED,
        VLC_INPUT_ES_SELECTED,
        VLC_INPUT_ES_UNSELECTED,
    } action;
    /**
     * ES track id: only valid from the event callback, unless the id is held
     * by the user with vlc_es_Hold(). */
    vlc_es_id_t *id;
    /**
     * Title of ES track, can be updated after the VLC_INPUT_ES_UPDATED event.
     */
    const char *title;
    /**
     * ES track information, can be updated after the VLC_INPUT_ES_UPDATED event.
     */
    const es_format_t *fmt;
    /**
     * Only valid with VLC_INPUT_ES_SELECTED, true if the track was selected by
     * the user.
     */
    bool forced;
};

struct vlc_input_event_signal {
    float quality;
    float strength;
};

struct vlc_input_event_vout
{
    enum {
        VLC_INPUT_EVENT_VOUT_STARTED,
        VLC_INPUT_EVENT_VOUT_STOPPED,
    } action;
    vout_thread_t *vout;
    enum vlc_vout_order order;
    vlc_es_id_t *id;
};

struct vlc_input_event
{
    input_event_type_e type;

    union {
        /* INPUT_EVENT_STATE */
        struct vlc_input_event_state state;
        /* INPUT_EVENT_RATE */
        float rate;
        /* INPUT_EVENT_CAPABILITIES */
        int capabilities; /**< cf. VLC_INPUT_CAPABILITIES_* bitwise flags */
        /* INPUT_EVENT_TIMES */
        struct vlc_input_event_times times;
        /* INPUT_EVENT_OUTPUT_CLOCK */
        struct vlc_input_event_output_clock output_clock;
        /* INPUT_EVENT_TITLE */
        struct vlc_input_event_title title;
        /* INPUT_EVENT_CHAPTER */
        struct vlc_input_event_chapter chapter;
        /* INPUT_EVENT_PROGRAM */
        struct vlc_input_event_program program;
        /* INPUT_EVENT_ES */
        struct vlc_input_event_es es;
        /* INPUT_EVENT_RECORD */
        bool record;
        /* INPUT_EVENT_STATISTICS */
        const struct input_stats_t *stats;
        /* INPUT_EVENT_SIGNAL */
        struct vlc_input_event_signal signal;
        /* INPUT_EVENT_CACHE */
        float cache;
        /* INPUT_EVENT_VOUT */
        struct vlc_input_event_vout vout;
        /* INPUT_EVENT_SUBITEMS */
        input_item_node_t *subitems;
        /* INPUT_EVENT_VBI_PAGE */
        unsigned vbi_page;
        /* INPUT_EVENT_VBI_TRANSPARENCY */
        bool vbi_transparent;
        /* INPUT_EVENT_SUBS_FPS */
        float subs_fps;
        /* INPUT_EVENT_THUMBNAIL_READY */
        picture_t *thumbnail;
    };
};

typedef void (*input_thread_events_cb)( input_thread_t *input,
                                        const struct vlc_input_event *event,
                                        void *userdata);

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
input_thread_t * input_Create( vlc_object_t *p_parent,
                               input_thread_events_cb event_cb, void *events_data,
                               input_item_t *, input_resource_t *,
                               vlc_renderer_item_t* p_renderer ) VLC_USED;
#define input_Create(a,b,c,d,e,f) input_Create(VLC_OBJECT(a),b,c,d,e,f)


/**
 * Creates an item preparser.
 *
 * Creates an input thread to preparse an item. The input needs to be started
 * with input_Start() afterwards.
 *
 * @param obj parent object
 * @param item input item to preparse
 * @return an input thread or NULL on error
 */
input_thread_t *input_CreatePreparser(vlc_object_t *obj,
                                      input_thread_events_cb events_cb,
                                      void *events_data, input_item_t *item)
VLC_USED;

VLC_API
input_thread_t *input_CreateThumbnailer(vlc_object_t *obj,
                                        input_thread_events_cb events_cb,
                                        void *events_data, input_item_t *item)
VLC_USED;

int input_Start( input_thread_t * );

void input_Stop( input_thread_t * );

void input_Close( input_thread_t * );

void input_SetTime( input_thread_t *, vlc_tick_t i_time, bool b_fast );

void input_SetPosition( input_thread_t *, float f_position, bool b_fast );

/**
 * Set the delay of an ES identifier
 */
void input_SetEsIdDelay(input_thread_t *input, vlc_es_id_t *es_id,
                        vlc_tick_t delay);

/**
 * Get the input item for an input thread
 *
 * You have to keep a reference to the input or to the input_item_t until
 * you do not need it anymore.
 */
input_item_t* input_GetItem( input_thread_t * ) VLC_USED;

/*****************************************************************************
 *  Private input fields
 *****************************************************************************/

#define INPUT_CONTROL_FIFO_SIZE    100

/* input_source_t: gathers all information per input source */
struct input_source_t
{
    vlc_atomic_rc_t rc;

    demux_t  *p_demux; /**< Demux object (most downstream) */
    es_out_t *p_slave_es_out; /**< Slave es out */

    char *str_id;
    int auto_id;
    bool autoselect_cats[ES_CATEGORY_COUNT];

    /* Title infos for that input */
    bool         b_title_demux; /* Titles/Seekpoints provided by demux */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    int i_title_start;
    int i_title_end;
    int i_seekpoint_start;
    int i_seekpoint_end;

    /* Properties */
    bool b_can_pause;
    bool b_can_pace_control;
    bool b_can_rate_control;
    bool b_can_stream_record;
    bool b_rescale_ts;
    double f_fps;

    /* sub-fps handling */
    bool b_slave_sub;
    float sub_rate;

    /* */
    vlc_tick_t i_pts_delay;

    bool       b_eof;   /* eof of demuxer */

};

typedef union
{
    vlc_value_t val;
    vlc_viewpoint_t viewpoint;
    vlc_es_id_t *id;
    struct {
        enum es_format_category_e cat;
        vlc_es_id_t **ids;
    } list;
    struct {
        bool b_fast_seek;
        vlc_tick_t i_val;
    } time;
    struct {
        bool b_fast_seek;
        float f_val;
    } pos;
    struct
    {
        enum es_format_category_e cat;
        vlc_tick_t delay;
    } cat_delay;
    struct
    {
        enum es_format_category_e cat;
        char *str_ids;
    } cat_ids;
    struct
    {
        vlc_es_id_t *id;
        vlc_tick_t delay;
    } es_delay;
    struct {
        vlc_es_id_t *id;
        unsigned page;
    } vbi_page;
    struct {
        vlc_es_id_t *id;
        bool enabled;
    } vbi_transparency;
} input_control_param_t;

typedef struct
{
    int         i_type;
    input_control_param_t param;
} input_control_t;

/** Private input fields */
typedef struct input_thread_private_t
{
    struct input_thread_t input;

    input_thread_events_cb events_cb;
    void *events_data;

    /* Global properties */
    bool        b_preparsing;
    bool        b_can_pause;
    bool        b_can_rate_control;
    bool        b_can_pace_control;

    /* Current state */
    int         i_state;
    bool        is_running;
    bool        is_stopped;
    bool        b_recording;
    bool        b_thumbnailing;
    float       rate;
    vlc_tick_t  normal_time;

    /* Playtime configuration and state */
    vlc_tick_t  i_start;    /* :start-time,0 by default */
    vlc_tick_t  i_stop;     /* :stop-time, 0 if none */

    /* Delays */
    bool        b_low_delay;

    /* Output */
    bool            b_out_pace_control; /* XXX Move it ot es_sout ? */
    sout_instance_t *p_sout;            /* Idem ? */
    es_out_t        *p_es_out;
    es_out_t        *p_es_out_display;
    vlc_viewpoint_t viewpoint;
    bool            viewpoint_changed;
    vlc_renderer_item_t *p_renderer;


    int i_title_offset;
    int i_seekpoint_offset;

    /* Input attachment */
    int i_attachment;
    input_attachment_t **attachment;
    const demux_t **attachment_demux;

    /* Main input properties */

    /* Input item */
    input_item_t   *p_item;

    /* Main source */
    input_source_t *master;
    /* Slave sources (subs, and others) */
    int            i_slave;
    input_source_t **slave;
    float          slave_subs_rate;

    /* Resources */
    input_resource_t *p_resource;

    /* Stats counters */
    struct input_stats *stats;

    /* Buffer of pending actions */
    vlc_mutex_t lock_control;
    vlc_cond_t  wait_control;
    size_t i_control;
    input_control_t control[INPUT_CONTROL_FIFO_SIZE];

    vlc_thread_t thread;
    vlc_interrupt_t interrupt;
} input_thread_private_t;

static inline input_thread_private_t *input_priv(input_thread_t *input)
{
    return container_of(input, input_thread_private_t, input);
}

/***************************************************************************
 * Internal control helpers
 ***************************************************************************/
enum input_control_e
{
    INPUT_CONTROL_SET_STATE,

    INPUT_CONTROL_SET_RATE,

    INPUT_CONTROL_SET_POSITION,
    INPUT_CONTROL_JUMP_POSITION,

    INPUT_CONTROL_SET_TIME,
    INPUT_CONTROL_JUMP_TIME,

    INPUT_CONTROL_SET_PROGRAM,

    INPUT_CONTROL_SET_TITLE,
    INPUT_CONTROL_SET_TITLE_NEXT,
    INPUT_CONTROL_SET_TITLE_PREV,

    INPUT_CONTROL_SET_SEEKPOINT,
    INPUT_CONTROL_SET_SEEKPOINT_NEXT,
    INPUT_CONTROL_SET_SEEKPOINT_PREV,

    INPUT_CONTROL_SET_BOOKMARK,

    INPUT_CONTROL_NAV_ACTIVATE, // NOTE: INPUT_CONTROL_NAV_* values must be
    INPUT_CONTROL_NAV_UP,       // contiguous and in the same order as
    INPUT_CONTROL_NAV_DOWN,     // INPUT_NAV_* and DEMUX_NAV_*.
    INPUT_CONTROL_NAV_LEFT,
    INPUT_CONTROL_NAV_RIGHT,
    INPUT_CONTROL_NAV_POPUP,
    INPUT_CONTROL_NAV_MENU,

    INPUT_CONTROL_SET_ES,
    INPUT_CONTROL_SET_ES_LIST,  // select a list of ES atomically
    INPUT_CONTROL_UNSET_ES,
    INPUT_CONTROL_RESTART_ES,
    INPUT_CONTROL_SET_ES_CAT_IDS,

    INPUT_CONTROL_SET_VIEWPOINT,    // new absolute viewpoint
    INPUT_CONTROL_SET_INITIAL_VIEWPOINT, // set initial viewpoint (generally from video)
    INPUT_CONTROL_UPDATE_VIEWPOINT, // update viewpoint relative to current

    INPUT_CONTROL_SET_CATEGORY_DELAY,
    INPUT_CONTROL_SET_ES_DELAY,

    INPUT_CONTROL_ADD_SLAVE,
    INPUT_CONTROL_SET_SUBS_FPS,

    INPUT_CONTROL_SET_RECORD_STATE,

    INPUT_CONTROL_SET_FRAME_NEXT,

    INPUT_CONTROL_SET_RENDERER,

    INPUT_CONTROL_SET_VBI_PAGE,
    INPUT_CONTROL_SET_VBI_TRANSPARENCY,
};

/* Internal helpers */

int input_ControlPush( input_thread_t *, int, const input_control_param_t * );

/* XXX for string value you have to allocate it before calling
 * input_ControlPushHelper
 */
static inline int input_ControlPushHelper( input_thread_t *p_input, int i_type, vlc_value_t *val )
{
    if( val != NULL )
    {
        input_control_param_t param = { .val = *val };
        return input_ControlPush( p_input, i_type, &param );
    }
    else
    {
        return input_ControlPush( p_input, i_type, NULL );
    }
}

static inline int input_ControlPushEsHelper( input_thread_t *p_input, int i_type,
                                             vlc_es_id_t *id )
{
    assert( i_type == INPUT_CONTROL_SET_ES || i_type == INPUT_CONTROL_UNSET_ES ||
            i_type == INPUT_CONTROL_RESTART_ES );
    return input_ControlPush( p_input, i_type, &(input_control_param_t) {
        .id = vlc_es_id_Hold( id ),
    } );
}

/**
 * Set the list of string ids to enable for a category
 *
 * cf. ES_OUT_SET_ES_CAT_IDS
 * This function can be called before start or while started.
 */
void input_SetEsCatIds(input_thread_t *, enum es_format_category_e cat,
                       const char *str_ids);

bool input_Stopped( input_thread_t * );

int input_GetAttachments(input_thread_t *input, input_attachment_t ***attachments);

input_attachment_t *input_GetAttachment(input_thread_t *input, const char *name);

/**
 * Hold the input_source_t
 */
input_source_t *input_source_Hold( input_source_t *in );

/**
 * Release the input_source_t
 */
void input_source_Release( input_source_t *in );

/**
 * Returns the string identifying this input source
 *
 * @return a string id or NULL if the source is the master
 */
const char *input_source_GetStrId( input_source_t *in );

/**
 * Get a new fmt.i_id from the input source
 *
 * This auto id will be relative to this input source. It allows to have stable
 * ids across different playback instances, by not relying on the input source
 * addition order.
 */
int input_source_GetNewAutoId( input_source_t *in );

/**
 * Returns true if a category should be auto-selected for a given source
 */
bool input_source_IsCatAutoselected( input_source_t *in,
                                     enum es_format_category_e cat );

/* Bound pts_delay */
#define INPUT_PTS_DELAY_MAX VLC_TICK_FROM_SEC(60)

/**********************************************************************
 * Item metadata
 **********************************************************************/
/* input_ExtractAttachmentAndCacheArt:
 *  Be careful: p_item lock will be taken! */
void input_ExtractAttachmentAndCacheArt( input_thread_t *, const char *name );

/***************************************************************************
 * Internal prototypes
 ***************************************************************************/

/* var.c */

void input_ConfigVarInit ( input_thread_t * );

/* Subtitles */
int subtitles_Detect( input_thread_t *, char *, const char *, input_item_slave_t ***, int * );
int subtitles_Filter( const char *);

/* meta.c */
void vlc_audio_replay_gain_MergeFromMeta( audio_replay_gain_t *p_dst,
                                          const vlc_meta_t *p_meta );

/* stats.c */
typedef struct input_rate_t
{
    vlc_mutex_t lock;
    uintmax_t updates;
    uintmax_t value;
    struct
    {
        uintmax_t  value;
        vlc_tick_t date;
    } samples[2];
} input_rate_t;

struct input_stats {
    input_rate_t input_bitrate;
    input_rate_t demux_bitrate;
    atomic_uintmax_t demux_corrupted;
    atomic_uintmax_t demux_discontinuity;
    atomic_uintmax_t decoded_audio;
    atomic_uintmax_t decoded_video;
    atomic_uintmax_t played_abuffers;
    atomic_uintmax_t lost_abuffers;
    atomic_uintmax_t displayed_pictures;
    atomic_uintmax_t lost_pictures;
};

struct input_stats *input_stats_Create(void);
void input_stats_Destroy(struct input_stats *);
void input_rate_Add(input_rate_t *, uintmax_t);
void input_stats_Compute(struct input_stats *, input_stats_t*);

#endif
