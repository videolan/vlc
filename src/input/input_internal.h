/*****************************************************************************
 * input_internal.h: Internal input structures
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _INPUT_INTERNAL_H
#define _INPUT_INTERNAL_H 1

#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>

/*****************************************************************************
 *  Private input fields
 *****************************************************************************/
/* input_source_t: gathers all information per input source */
typedef struct
{
    /* Input item description */
    input_item_t *p_item;

    /* Access/Stream/Demux plugins */
    access_t *p_access;
    stream_t *p_stream;
    demux_t  *p_demux;

    /* Title infos for that input */
    bool   b_title_demux; /* Titles/Seekpoints provided by demux */
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
    bool b_rescale_ts;

    bool b_eof;   /* eof of demuxer */
    double     f_fps;

    /* Clock average variation */
    int     i_cr_average;

} input_source_t;

/** Private input fields */
struct input_thread_private_t
{
    /* Global properties */
    bool  b_can_pause;
    bool  b_can_rate_control;

    int         i_rate;
    /* */
    int64_t     i_start;    /* :start-time,0 by default */
    int64_t     i_stop;     /* :stop-time, 0 if none */
    int64_t     i_run;      /* :run-time, 0 if none */

    /* Title infos FIXME multi-input (not easy) ? */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    /* User bookmarks FIXME won't be easy with multiples input */
    int         i_bookmark;
    seekpoint_t **bookmark;

    /* Input attachment */
    int i_attachment;
    input_attachment_t **attachment;

    /* Output */
    es_out_t    *p_es_out;
    sout_instance_t *p_sout;            /* XXX Move it to es_out ? */
    bool      b_sout_keep;
    bool      b_out_pace_control; /*     idem ? */
    bool      b_owns_its_sout;

    /* Main input properties */
    input_source_t input;
    /* Slave demuxers (subs, and others) */
    int            i_slave;
    input_source_t **slave;

    /* Stats counters */
    struct {
        counter_t *p_read_packets;
        counter_t *p_read_bytes;
        counter_t *p_input_bitrate;
        counter_t *p_demux_read;
        counter_t *p_demux_bitrate;
        counter_t *p_decoded_audio;
        counter_t *p_decoded_video;
        counter_t *p_decoded_sub;
        counter_t *p_sout_sent_packets;
        counter_t *p_sout_sent_bytes;
        counter_t *p_sout_send_bitrate;
        counter_t *p_played_abuffers;
        counter_t *p_lost_abuffers;
        counter_t *p_displayed_pictures;
        counter_t *p_lost_pictures;
        vlc_mutex_t counters_lock;
    } counters;

    /* Buffer of pending actions */
    vlc_mutex_t lock_control;
    int i_control;
    struct
    {
        /* XXX for string value you have to allocate it before calling
         * input_ControlPush */
        int         i_type;
        vlc_value_t val;
    } control[INPUT_CONTROL_FIFO_SIZE];
};

/***************************************************************************
 * Internal control helpers
 ***************************************************************************/
enum input_control_e
{
    INPUT_CONTROL_SET_DIE,

    INPUT_CONTROL_SET_STATE,

    INPUT_CONTROL_SET_RATE,
    INPUT_CONTROL_SET_RATE_SLOWER,
    INPUT_CONTROL_SET_RATE_FASTER,

    INPUT_CONTROL_SET_POSITION,
    INPUT_CONTROL_SET_POSITION_OFFSET,

    INPUT_CONTROL_SET_TIME,
    INPUT_CONTROL_SET_TIME_OFFSET,

    INPUT_CONTROL_SET_PROGRAM,

    INPUT_CONTROL_SET_TITLE,
    INPUT_CONTROL_SET_TITLE_NEXT,
    INPUT_CONTROL_SET_TITLE_PREV,

    INPUT_CONTROL_SET_SEEKPOINT,
    INPUT_CONTROL_SET_SEEKPOINT_NEXT,
    INPUT_CONTROL_SET_SEEKPOINT_PREV,

    INPUT_CONTROL_SET_BOOKMARK,

    INPUT_CONTROL_SET_ES,

    INPUT_CONTROL_SET_AUDIO_DELAY,
    INPUT_CONTROL_SET_SPU_DELAY,

    INPUT_CONTROL_ADD_SLAVE,
};

/* Internal helpers */
static inline void input_ControlPush( input_thread_t *p_input,
                                      int i_type, vlc_value_t *p_val )
{
    vlc_mutex_lock( &p_input->p->lock_control );
    if( i_type == INPUT_CONTROL_SET_DIE )
    {
        /* Special case, empty the control */
        p_input->p->i_control = 1;
        p_input->p->control[0].i_type = i_type;
        memset( &p_input->p->control[0].val, 0, sizeof( vlc_value_t ) );
    }
    else
    {
        if( p_input->p->i_control >= INPUT_CONTROL_FIFO_SIZE )
        {
            msg_Err( p_input, "input control fifo overflow, trashing type=%d",
                     i_type );
            vlc_mutex_unlock( &p_input->p->lock_control );
            return;
        }
        p_input->p->control[p_input->p->i_control].i_type = i_type;
        if( p_val )
            p_input->p->control[p_input->p->i_control].val = *p_val;
        else
            memset( &p_input->p->control[p_input->p->i_control].val, 0,
                    sizeof( vlc_value_t ) );

        p_input->p->i_control++;
    }
    vlc_mutex_unlock( &p_input->p->lock_control );
}

/** Stuff moved out of vlc_input.h -- FIXME: should probably not be inline
 * anyway. */

static inline void input_item_SetPreparsed( input_item_t *p_i, bool preparsed )
{
    bool send_event = false;

    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    vlc_mutex_lock( &p_i->lock );
    int new_status;
    if( preparsed )
        new_status = p_i->p_meta->i_status | ITEM_PREPARSED;
    else
        new_status = p_i->p_meta->i_status & ~ITEM_PREPARSED;
    if ( p_i->p_meta->i_status != new_status )
    {
        p_i->p_meta->i_status = new_status;
        send_event = true;
    }

    vlc_mutex_unlock( &p_i->lock );

    if ( send_event == true )
    {
        vlc_event_t event;
        event.type = vlc_InputItemPreparsedChanged;
        event.u.input_item_preparsed_changed.new_status = new_status;
        vlc_event_send( &p_i->event_manager, &event );
    }
}

static inline void input_item_SetMetaFetched( input_item_t *p_i, bool metafetched )
{
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    if( metafetched )
        p_i->p_meta->i_status |= ITEM_META_FETCHED;
    else
        p_i->p_meta->i_status &= ~ITEM_META_FETCHED;
}

static inline void input_item_SetArtNotFound( input_item_t *p_i, bool notfound )
{
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    if( notfound )
        p_i->p_meta->i_status |= ITEM_ART_NOTFOUND;
    else
        p_i->p_meta->i_status &= ~ITEM_ART_NOTFOUND;
}

static inline void input_item_SetArtFetched( input_item_t *p_i, bool artfetched )
{
    if( !p_i->p_meta )
        p_i->p_meta = vlc_meta_New();

    if( artfetched )
        p_i->p_meta->i_status |= ITEM_ART_FETCHED;
    else
        p_i->p_meta->i_status &= ~ITEM_ART_FETCHED;
}


/**********************************************************************
 * Item metadata
 **********************************************************************/
typedef struct playlist_album_t
{
    char *psz_artist;
    char *psz_album;
    char *psz_arturl;
    bool b_found;
} playlist_album_t;

int         input_MetaFetch     ( playlist_t *, input_item_t * );
int         input_ArtFind       ( playlist_t *, input_item_t * );
bool  input_MetaSatisfied ( playlist_t*, input_item_t*,
                                  uint32_t*, uint32_t* );
int         input_DownloadAndCacheArt ( playlist_t *, input_item_t * );

/* Becarefull; p_item lock HAS to be taken */
void input_ExtractAttachmentAndCacheArt( input_thread_t *p_input );

/***************************************************************************
 * Internal prototypes
 ***************************************************************************/

/* misc/stats.c */
input_stats_t *stats_NewInputStats( input_thread_t *p_input );

/* input.c */
#define input_CreateThreadExtended(a,b,c,d) __input_CreateThreadExtended(VLC_OBJECT(a),b,c,d)
input_thread_t *__input_CreateThreadExtended ( vlc_object_t *, input_item_t *, const char *, sout_instance_t * );

sout_instance_t * input_DetachSout( input_thread_t *p_input );

/* var.c */
void input_ControlVarInit ( input_thread_t * );
void input_ControlVarClean( input_thread_t * );
void input_ControlVarNavigation( input_thread_t * );
void input_ControlVarTitle( input_thread_t *, int i_title );

void input_ConfigVarInit ( input_thread_t * );

/* stream.c */
stream_t *stream_AccessNew( access_t *p_access, bool );
void stream_AccessDelete( stream_t *s );
void stream_AccessReset( stream_t *s );
void stream_AccessUpdate( stream_t *s );

/* decoder.c */
void       input_DecoderDiscontinuity( decoder_t * p_dec, bool b_flush );
bool input_DecoderEmpty( decoder_t * p_dec );
int        input_DecoderSetCcState( decoder_t *, bool b_decode, int i_channel );
int        input_DecoderGetCcState( decoder_t *, bool *pb_decode, int i_channel );
void       input_DecoderIsCcPresent( decoder_t *, bool pb_present[4] );

/* es_out.c */
es_out_t  *input_EsOutNew( input_thread_t *, int i_rate );
void       input_EsOutDelete( es_out_t * );
es_out_id_t *input_EsOutGetFromID( es_out_t *, int i_id );
void       input_EsOutSetDelay( es_out_t *, int i_cat, int64_t );
void       input_EsOutChangeRate( es_out_t *, int );
void       input_EsOutChangeState( es_out_t * );
void       input_EsOutChangePosition( es_out_t * );
bool input_EsOutDecodersEmpty( es_out_t * );

/* clock.c */
enum /* Synchro states */
{
    SYNCHRO_OK     = 0,
    SYNCHRO_START  = 1,
    SYNCHRO_REINIT = 2,
};

typedef struct
{
    /* Synchronization information */
    mtime_t                 delta_cr;
    mtime_t                 cr_ref, sysdate_ref;
    mtime_t                 last_sysdate;
    mtime_t                 last_cr; /* reference to detect unexpected stream
                                      * discontinuities                      */
    mtime_t                 last_pts;
    mtime_t                 last_update;
    int                     i_synchro_state;

    bool              b_master;

    int                     i_rate;

    /* Config */
    int                     i_cr_average;
    int                     i_delta_cr_residue;
} input_clock_t;

void    input_ClockInit( input_clock_t *, bool b_master, int i_cr_average, int i_rate );
void    input_ClockSetPCR( input_thread_t *, input_clock_t *, mtime_t );
void    input_ClockResetPCR( input_clock_t * );
mtime_t input_ClockGetTS( input_thread_t *, input_clock_t *, mtime_t );
void    input_ClockSetRate( input_clock_t *cl, int i_rate );

/* Subtitles */
char **subtitles_Detect( input_thread_t *, char* path, const char *fname );
int subtitles_Filter( const char *);

void MRLSplit( char *, const char **, const char **, char ** );

static inline void input_ChangeState( input_thread_t *p_input, int state )
{
    var_SetInteger( p_input, "state", p_input->i_state = state );
}

/* Access */

#define access_New( a, b, c, d ) __access_New(VLC_OBJECT(a), b, c, d )
access_t * __access_New( vlc_object_t *p_obj, const char *psz_access,
                          const char *psz_demux, const char *psz_path );
access_t * access_FilterNew( access_t *p_source,
                              const char *psz_access_filter );
void access_Delete( access_t * );

/* Demuxer */
#include <vlc_demux.h>

/* stream_t *s could be null and then it mean a access+demux in one */
#define demux_New( a, b, c, d, e, f,g ) __demux_New(VLC_OBJECT(a),b,c,d,e,f,g)
demux_t *__demux_New(vlc_object_t *p_obj, const char *psz_access, const char *psz_demux, const char *psz_path, stream_t *s, es_out_t *out, bool );

void demux_Delete(demux_t *);

static inline int demux_Demux( demux_t *p_demux )
{
    return p_demux->pf_demux( p_demux );
}
static inline int demux_vaControl( demux_t *p_demux, int i_query, va_list args )
{
    return p_demux->pf_control( p_demux, i_query, args );
}
static inline int demux_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}

/* Stream */
/**
 * stream_t definition
 */
struct stream_t
{
    VLC_COMMON_MEMBERS

    /*block_t *(*pf_block)  ( stream_t *, int i_size );*/
    int      (*pf_read)   ( stream_t *, void *p_read, int i_read );
    int      (*pf_peek)   ( stream_t *, const uint8_t **pp_peek, int i_peek );
    int      (*pf_control)( stream_t *, int i_query, va_list );
    void     (*pf_destroy)( stream_t *);

    stream_sys_t *p_sys;

    /* UTF-16 and UTF-32 file reading */
    vlc_iconv_t     conv;
    int             i_char_width;
    bool      b_little_endian;
};

#include <libvlc.h>

static inline stream_t *vlc_stream_create( vlc_object_t *obj )
{
    return (stream_t *)vlc_custom_create( obj, sizeof(stream_t),
                                          VLC_OBJECT_STREAM, "stream" );
}

#endif
