/*****************************************************************************
 * es_out_timeshift.c: Es Out timeshift.
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org>
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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#if defined (_WIN32)
#  include <direct.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>
#ifdef _WIN32
#  include <vlc_charset.h>
#endif
#include <vlc_input.h>
#include <vlc_es_out.h>
#include <vlc_block.h>
#include "input_internal.h"
#include "es_out.h"
#include "es_out_timeshift.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* XXX attribute_packed is (and MUST be) used ONLY to reduce memory usage */
#ifdef HAVE_ATTRIBUTE_PACKED
#   define attribute_packed __attribute__((__packed__))
#else
#   define attribute_packed
#endif

enum
{
    C_ADD,
    C_SEND,
    C_DEL,
    C_CONTROL,
};

typedef struct attribute_packed
{
    es_out_id_t *p_es;
    es_format_t *p_fmt;
} ts_cmd_add_t;

typedef struct attribute_packed
{
    es_out_id_t *p_es;
} ts_cmd_del_t;

typedef struct attribute_packed
{
    es_out_id_t *p_es;
    block_t *p_block;
    int     i_offset;  /* We do not use file > INT_MAX */
} ts_cmd_send_t;

typedef struct attribute_packed
{
    int  i_query;

    union
    {
        bool b_bool;
        int  i_int;
        int64_t i_i64;
        es_out_id_t *p_es;
        struct
        {
            int     i_int;
            int64_t i_i64;
        } int_i64;
        struct
        {
            int        i_int;
            vlc_meta_t *p_meta;
        } int_meta;
        struct
        {
            int       i_int;
            vlc_epg_t *p_epg;
        } int_epg;
        struct
        {
            es_out_id_t *p_es;
            bool        b_bool;
        } es_bool;
        struct
        {
            es_out_id_t *p_es;
            es_format_t *p_fmt;
        } es_fmt;
        struct
        {
            /* FIXME Really too big (double make the whole thing too big) */
            double  f_position;
            mtime_t i_time;
            mtime_t i_length;
        } times;
        struct
        {
            mtime_t i_pts_delay;
            mtime_t i_pts_jitter;
            int     i_cr_average;
        } jitter;
    } u;
} ts_cmd_control_t;

typedef struct attribute_packed
{
    int8_t  i_type;
    mtime_t i_date;
    union
    {
        ts_cmd_add_t     add;
        ts_cmd_del_t     del;
        ts_cmd_send_t    send;
        ts_cmd_control_t control;
    } u;
} ts_cmd_t;

typedef struct ts_storage_t ts_storage_t;
struct ts_storage_t
{
    ts_storage_t *p_next;

    /* */
    char    *psz_file;  /* Filename */
    size_t  i_file_max; /* Max size in bytes */
    int64_t i_file_size;/* Current size in bytes */
    FILE    *p_filew;   /* FILE handle for data writing */
    FILE    *p_filer;   /* FILE handle for data reading */

    /* */
    int      i_cmd_r;
    int      i_cmd_w;
    int      i_cmd_max;
    ts_cmd_t *p_cmd;
};

typedef struct
{
    vlc_thread_t   thread;
    input_thread_t *p_input;
    es_out_t       *p_out;
    int64_t        i_tmp_size_max;
    const char     *psz_tmp_path;

    /* Lock for all following fields */
    vlc_mutex_t    lock;
    vlc_cond_t     wait;

    /* */
    bool           b_paused;
    mtime_t        i_pause_date;

    /* */
    int            i_rate;
    int            i_rate_source;
    mtime_t        i_rate_date;
    mtime_t        i_rate_delay;

    /* */
    mtime_t        i_buffering_delay;

    /* */
    ts_storage_t   *p_storage_r;
    ts_storage_t   *p_storage_w;

    mtime_t        i_cmd_delay;

} ts_thread_t;

struct es_out_id_t
{
    es_out_id_t *p_es;
};

struct es_out_sys_t
{
    input_thread_t *p_input;
	es_out_t       *p_out;

    /* Configuration */
    int64_t        i_tmp_size_max;    /* Maximal temporary file size in byte */
    char           *psz_tmp_path;     /* Path for temporary files */

    /* Lock for all following fields */
    vlc_mutex_t    lock;

    /* */
    bool           b_delayed;
    ts_thread_t   *p_ts;

    /* */
    bool           b_input_paused;
    bool           b_input_paused_source;
    int            i_input_rate;
    int            i_input_rate_source;

    /* */
    int            i_es;
    es_out_id_t    **pp_es;
};

static es_out_id_t *Add    ( es_out_t *, const es_format_t * );
static int          Send   ( es_out_t *, es_out_id_t *, block_t * );
static void         Del    ( es_out_t *, es_out_id_t * );
static int          Control( es_out_t *, int i_query, va_list );
static void         Destroy( es_out_t * );

static int          TsStart( es_out_t * );
static void         TsAutoStop( es_out_t * );

static void         TsStop( ts_thread_t * );
static void         TsPushCmd( ts_thread_t *, ts_cmd_t * );
static int          TsPopCmdLocked( ts_thread_t *, ts_cmd_t *, bool b_flush );
static bool         TsHasCmd( ts_thread_t * );
static bool         TsIsUnused( ts_thread_t * );
static int          TsChangePause( ts_thread_t *, bool b_source_paused, bool b_paused, mtime_t i_date );
static int          TsChangeRate( ts_thread_t *, int i_src_rate, int i_rate );

static void         *TsRun( void * );

static ts_storage_t *TsStorageNew( const char *psz_path, int64_t i_tmp_size_max );
static void         TsStorageDelete( ts_storage_t * );
static void         TsStoragePack( ts_storage_t *p_storage );
static bool         TsStorageIsFull( ts_storage_t *, const ts_cmd_t *p_cmd );
static bool         TsStorageIsEmpty( ts_storage_t * );
static void         TsStoragePushCmd( ts_storage_t *, const ts_cmd_t *p_cmd, bool b_flush );
static void         TsStoragePopCmd( ts_storage_t *p_storage, ts_cmd_t *p_cmd, bool b_flush );

static void CmdClean( ts_cmd_t * );
static void cmd_cleanup_routine( void *p ) { CmdClean( p ); }

static int  CmdInitAdd    ( ts_cmd_t *, es_out_id_t *, const es_format_t *, bool b_copy );
static void CmdInitSend   ( ts_cmd_t *, es_out_id_t *, block_t * );
static int  CmdInitDel    ( ts_cmd_t *, es_out_id_t * );
static int  CmdInitControl( ts_cmd_t *, int i_query, va_list, bool b_copy );

/* */
static void CmdCleanAdd    ( ts_cmd_t * );
static void CmdCleanSend   ( ts_cmd_t * );
static void CmdCleanControl( ts_cmd_t *p_cmd );

/* XXX these functions will take the destination es_out_t */
static void CmdExecuteAdd    ( es_out_t *, ts_cmd_t * );
static int  CmdExecuteSend   ( es_out_t *, ts_cmd_t * );
static void CmdExecuteDel    ( es_out_t *, ts_cmd_t * );
static int  CmdExecuteControl( es_out_t *, ts_cmd_t * );

/* File helpers */
static char *GetTmpPath( char *psz_path );
static FILE *GetTmpFile( char **ppsz_file, const char *psz_path );

/*****************************************************************************
 * input_EsOutTimeshiftNew:
 *****************************************************************************/
es_out_t *input_EsOutTimeshiftNew( input_thread_t *p_input, es_out_t *p_next_out, int i_rate )
{
    es_out_t *p_out = malloc( sizeof(*p_out) );
    if( !p_out )
        return NULL;

    es_out_sys_t *p_sys = malloc( sizeof(*p_sys) );
    if( !p_sys )
    {
        free( p_out );
        return NULL;
    }

    /* */
    p_out->pf_add     = Add;
    p_out->pf_send    = Send;
    p_out->pf_del     = Del;
    p_out->pf_control = Control;
    p_out->pf_destroy = Destroy;
    p_out->p_sys      = p_sys;

    /* */
    p_sys->b_input_paused = false;
    p_sys->b_input_paused_source = false;
    p_sys->p_input = p_input;
    p_sys->i_input_rate = i_rate;
    p_sys->i_input_rate_source = i_rate;

    p_sys->p_out = p_next_out;
    vlc_mutex_init_recursive( &p_sys->lock );

    p_sys->b_delayed = false;
    p_sys->p_ts = NULL;

    TAB_INIT( p_sys->i_es, p_sys->pp_es );

    /* */
    const int i_tmp_size_max = var_CreateGetInteger( p_input, "input-timeshift-granularity" );
    if( i_tmp_size_max < 0 )
        p_sys->i_tmp_size_max = 50*1024*1024;
    else
        p_sys->i_tmp_size_max = __MAX( i_tmp_size_max, 1*1024*1024 );

    char *psz_tmp_path = var_CreateGetNonEmptyString( p_input, "input-timeshift-path" );
    p_sys->psz_tmp_path = GetTmpPath( psz_tmp_path );

    msg_Dbg( p_input, "using timeshift granularity of %d MiB, in path '%s'",
             (int)p_sys->i_tmp_size_max/(1024*1024), p_sys->psz_tmp_path );

#if 0
#define S(t) msg_Err( p_input, "SIZEOF("#t")=%d", sizeof(t) )
    S(ts_cmd_t);
    S(ts_cmd_control_t);
    S(ts_cmd_send_t);
    S(ts_cmd_del_t);
    S(ts_cmd_add_t);
#undef S
#endif

    return p_out;
}

/*****************************************************************************
 * Internal functions
 *****************************************************************************/
static void Destroy( es_out_t *p_out )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( p_sys->b_delayed )
    {
        TsStop( p_sys->p_ts );
        p_sys->b_delayed = false;
    }

    while( p_sys->i_es > 0 )
        Del( p_out, p_sys->pp_es[0] );
    TAB_CLEAN( p_sys->i_es, p_sys->pp_es  );

    free( p_sys->psz_tmp_path );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
    free( p_out );
}

static es_out_id_t *Add( es_out_t *p_out, const es_format_t *p_fmt )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    ts_cmd_t cmd;

    es_out_id_t *p_es = malloc( sizeof( *p_es ) );
    if( !p_es )
        return NULL;

    vlc_mutex_lock( &p_sys->lock );

    TsAutoStop( p_out );

    if( CmdInitAdd( &cmd, p_es, p_fmt, p_sys->b_delayed ) )
    {
        vlc_mutex_unlock( &p_sys->lock );
        free( p_es );
        return NULL;
    }

    TAB_APPEND( p_sys->i_es, p_sys->pp_es, p_es );

    if( p_sys->b_delayed )
        TsPushCmd( p_sys->p_ts, &cmd );
    else
        CmdExecuteAdd( p_sys->p_out, &cmd );

    vlc_mutex_unlock( &p_sys->lock );

    return p_es;
}
static int Send( es_out_t *p_out, es_out_id_t *p_es, block_t *p_block )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    ts_cmd_t cmd;
    int i_ret = VLC_SUCCESS;

    vlc_mutex_lock( &p_sys->lock );

    TsAutoStop( p_out );

    CmdInitSend( &cmd, p_es, p_block );
    if( p_sys->b_delayed )
        TsPushCmd( p_sys->p_ts, &cmd );
    else
        i_ret = CmdExecuteSend( p_sys->p_out, &cmd) ;

    vlc_mutex_unlock( &p_sys->lock );

    return i_ret;
}
static void Del( es_out_t *p_out, es_out_id_t *p_es )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    ts_cmd_t cmd;

    vlc_mutex_lock( &p_sys->lock );

    TsAutoStop( p_out );

    CmdInitDel( &cmd, p_es );
    if( p_sys->b_delayed )
        TsPushCmd( p_sys->p_ts, &cmd );
    else
        CmdExecuteDel( p_sys->p_out, &cmd );

    TAB_REMOVE( p_sys->i_es, p_sys->pp_es, p_es );

    vlc_mutex_unlock( &p_sys->lock );
}

static int ControlLockedGetEmpty( es_out_t *p_out, bool *pb_empty )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( p_sys->b_delayed && TsHasCmd( p_sys->p_ts ) )
        *pb_empty = false;
    else
        *pb_empty = es_out_GetEmpty( p_sys->p_out );

    return VLC_SUCCESS;
}
static int ControlLockedGetWakeup( es_out_t *p_out, mtime_t *pi_wakeup )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( p_sys->b_delayed )
    {
        assert( !p_sys->p_input->p->b_can_pace_control );
        *pi_wakeup = 0;
    }
    else
    {
        *pi_wakeup = es_out_GetWakeup( p_sys->p_out );
    }

    return VLC_SUCCESS;
}
static int ControlLockedGetBuffering( es_out_t *p_out, bool *pb_buffering )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( p_sys->b_delayed )
        *pb_buffering = true;
    else
        *pb_buffering = es_out_GetBuffering( p_sys->p_out );

    return VLC_SUCCESS;
}
static int ControlLockedSetPauseState( es_out_t *p_out, bool b_source_paused, bool b_paused, mtime_t i_date )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    int i_ret;

    if( !p_sys->b_delayed && !b_source_paused == !b_paused )
    {
        i_ret = es_out_SetPauseState( p_sys->p_out, b_source_paused, b_paused, i_date );
    }
    else
    {
        i_ret = VLC_EGENERIC;
        if( !p_sys->p_input->p->b_can_pace_control )
        {
            if( !p_sys->b_delayed )
                TsStart( p_out );
            if( p_sys->b_delayed )
                i_ret = TsChangePause( p_sys->p_ts, b_source_paused, b_paused, i_date );
        }
        else
        {
            /* XXX we may do it BUT it would be better to finish the clock clean up+improvments
             * and so be able to advertize correctly pace control property in access
             * module */
            msg_Err( p_sys->p_input, "EsOutTimeshift does not work with streams that have pace control" );
        }
    }

    if( !i_ret )
    {
        p_sys->b_input_paused_source = b_source_paused;
        p_sys->b_input_paused = b_paused;
    }
    return i_ret;
}
static int ControlLockedSetRate( es_out_t *p_out, int i_src_rate, int i_rate )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    int i_ret;

    if( !p_sys->b_delayed && i_src_rate == i_rate )
    {
        i_ret = es_out_SetRate( p_sys->p_out, i_src_rate, i_rate );
    }
    else
    {
        i_ret = VLC_EGENERIC;
        if( !p_sys->p_input->p->b_can_pace_control )
        {
            if( !p_sys->b_delayed )
                TsStart( p_out );
            if( p_sys->b_delayed )
                i_ret = TsChangeRate( p_sys->p_ts, i_src_rate, i_rate );
        }
        else
        {
            /* XXX we may do it BUT it would be better to finish the clock clean up+improvments
             * and so be able to advertize correctly pace control property in access
             * module */
            msg_Err( p_sys->p_input, "EsOutTimeshift does not work with streams that have pace control" );
        }

    }

    if( !i_ret )
    {
        p_sys->i_input_rate_source = i_src_rate;
        p_sys->i_input_rate = i_rate;
    }
    return i_ret;
}
static int ControlLockedSetTime( es_out_t *p_out, mtime_t i_date )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( !p_sys->b_delayed )
        return es_out_SetTime( p_sys->p_out, i_date );

    /* TODO */
    msg_Err( p_sys->p_input, "EsOutTimeshift does not yet support time change" );
    return VLC_EGENERIC;
}
static int ControlLockedSetFrameNext( es_out_t *p_out )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    return es_out_SetFrameNext( p_sys->p_out );
}

static int ControlLocked( es_out_t *p_out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    switch( i_query )
    {
    /* Invalid query for this es_out level */
    case ES_OUT_SET_ES_BY_ID:
    case ES_OUT_RESTART_ES_BY_ID:
    case ES_OUT_SET_ES_DEFAULT_BY_ID:
    case ES_OUT_GET_ES_OBJECTS_BY_ID:
    case ES_OUT_SET_DELAY:
    case ES_OUT_SET_RECORD_STATE:
        assert(0);
        return VLC_EGENERIC;

    /* Pass-through control */
    case ES_OUT_SET_MODE:
    case ES_OUT_SET_GROUP:
    case ES_OUT_SET_PCR:
    case ES_OUT_SET_GROUP_PCR:
    case ES_OUT_RESET_PCR:
    case ES_OUT_SET_NEXT_DISPLAY_TIME:
    case ES_OUT_SET_GROUP_META:
    case ES_OUT_SET_GROUP_EPG:
    case ES_OUT_SET_ES_SCRAMBLED_STATE:
    case ES_OUT_DEL_GROUP:
    case ES_OUT_SET_META:
    case ES_OUT_SET_ES:
    case ES_OUT_RESTART_ES:
    case ES_OUT_SET_ES_DEFAULT:
    case ES_OUT_SET_ES_STATE:
    case ES_OUT_SET_ES_FMT:
    case ES_OUT_SET_TIMES:
    case ES_OUT_SET_JITTER:
    case ES_OUT_SET_EOS:
    {
        ts_cmd_t cmd;
        if( CmdInitControl( &cmd, i_query, args, p_sys->b_delayed ) )
            return VLC_EGENERIC;
        if( p_sys->b_delayed )
        {
            TsPushCmd( p_sys->p_ts, &cmd );
            return VLC_SUCCESS;
        }
        return CmdExecuteControl( p_sys->p_out, &cmd );
    }

    /* Special control when delayed */
    case ES_OUT_GET_ES_STATE:
    {
        es_out_id_t *p_es = (es_out_id_t*)va_arg( args, es_out_id_t * );
        bool *pb_enabled = (bool*)va_arg( args, bool* );

        if( p_sys->b_delayed )
        {
            *pb_enabled = true;
            return VLC_SUCCESS;
        }
        return es_out_Control( p_sys->p_out, ES_OUT_GET_ES_STATE, p_es->p_es, pb_enabled );
    }
    /* Special internal input control */
    case ES_OUT_GET_EMPTY:
    {
        bool *pb_empty = (bool*)va_arg( args, bool* );
        return ControlLockedGetEmpty( p_out, pb_empty );
    }
    case ES_OUT_GET_WAKE_UP: /* TODO ? */
    {
        mtime_t *pi_wakeup = (mtime_t*)va_arg( args, mtime_t* );
        return ControlLockedGetWakeup( p_out, pi_wakeup );
    }
    case ES_OUT_GET_BUFFERING:
    {
        bool *pb_buffering = (bool *)va_arg( args, bool* );
        return ControlLockedGetBuffering( p_out, pb_buffering );
    }
    case ES_OUT_SET_PAUSE_STATE:
    {
        const bool b_source_paused = (bool)va_arg( args, int );
        const bool b_paused = (bool)va_arg( args, int );
        const mtime_t i_date = (mtime_t) va_arg( args, mtime_t );

        return ControlLockedSetPauseState( p_out, b_source_paused, b_paused, i_date );
    }
    case ES_OUT_SET_RATE:
    {
        const int i_src_rate = (int)va_arg( args, int );
        const int i_rate = (int)va_arg( args, int );

        return ControlLockedSetRate( p_out, i_src_rate, i_rate );
    }
    case ES_OUT_SET_TIME:
    {
        const mtime_t i_date = (mtime_t)va_arg( args, mtime_t );

        return ControlLockedSetTime( p_out, i_date );
    }
    case ES_OUT_SET_FRAME_NEXT:
    {
        return ControlLockedSetFrameNext( p_out );
    }
    case ES_OUT_GET_PCR_SYSTEM:
    {
        if( p_sys->b_delayed )
            return VLC_EGENERIC;

        mtime_t *pi_system = (mtime_t*)va_arg( args, mtime_t * );
        mtime_t *pi_delay  = (mtime_t*)va_arg( args, mtime_t * );
        return es_out_ControlGetPcrSystem( p_sys->p_out, pi_system, pi_delay );
    }
    case ES_OUT_MODIFY_PCR_SYSTEM:
    {
        const bool    b_absolute = va_arg( args, int );
        const mtime_t i_system   = va_arg( args, mtime_t );

        if( b_absolute && p_sys->b_delayed )
            return VLC_EGENERIC;

        return es_out_ControlModifyPcrSystem( p_sys->p_out, b_absolute, i_system );
    }
    case ES_OUT_GET_GROUP_FORCED:
    {
        int *pi_group = va_arg( args, int * );
        return es_out_Control( p_sys->p_out, ES_OUT_GET_GROUP_FORCED, pi_group );
    }


    default:
        msg_Err( p_sys->p_input, "Unknown es_out_Control query !" );
        assert(0);
        return VLC_EGENERIC;
    }
}
static int Control( es_out_t *p_out, int i_query, va_list args )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    int i_ret;

    vlc_mutex_lock( &p_sys->lock );

    TsAutoStop( p_out );

    i_ret = ControlLocked( p_out, i_query, args );

    vlc_mutex_unlock( &p_sys->lock );

    return i_ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void TsDestroy( ts_thread_t *p_ts )
{
    vlc_cond_destroy( &p_ts->wait );
    vlc_mutex_destroy( &p_ts->lock );
    free( p_ts );
}
static int TsStart( es_out_t *p_out )
{
    es_out_sys_t *p_sys = p_out->p_sys;
    ts_thread_t *p_ts;

    assert( !p_sys->b_delayed );

    p_sys->p_ts = p_ts = calloc(1, sizeof(*p_ts));
    if( !p_ts )
        return VLC_EGENERIC;

    p_ts->i_tmp_size_max = p_sys->i_tmp_size_max;
    p_ts->psz_tmp_path = p_sys->psz_tmp_path;
    p_ts->p_input = p_sys->p_input;
    p_ts->p_out = p_sys->p_out;
    vlc_mutex_init( &p_ts->lock );
    vlc_cond_init( &p_ts->wait );
    p_ts->b_paused = p_sys->b_input_paused && !p_sys->b_input_paused_source;
    p_ts->i_pause_date = p_ts->b_paused ? mdate() : -1;
    p_ts->i_rate_source = p_sys->i_input_rate_source;
    p_ts->i_rate        = p_sys->i_input_rate;
    p_ts->i_rate_date = -1;
    p_ts->i_rate_delay = 0;
    p_ts->i_buffering_delay = 0;
    p_ts->i_cmd_delay = 0;
    p_ts->p_storage_r = NULL;
    p_ts->p_storage_w = NULL;

    p_sys->b_delayed = true;
    if( vlc_clone( &p_ts->thread, TsRun, p_ts, VLC_THREAD_PRIORITY_INPUT ) )
    {
        msg_Err( p_sys->p_input, "cannot create timeshift thread" );

        TsDestroy( p_ts );

        p_sys->b_delayed = false;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
static void TsAutoStop( es_out_t *p_out )
{
    es_out_sys_t *p_sys = p_out->p_sys;

    if( !p_sys->b_delayed || !TsIsUnused( p_sys->p_ts ) )
        return;

    msg_Warn( p_sys->p_input, "es out timeshift: auto stop" );
    TsStop( p_sys->p_ts );

    p_sys->b_delayed = false;
}
static void TsStop( ts_thread_t *p_ts )
{
    vlc_cancel( p_ts->thread );
    vlc_join( p_ts->thread, NULL );

    vlc_mutex_lock( &p_ts->lock );
    for( ;; )
    {
        ts_cmd_t cmd;

        if( TsPopCmdLocked( p_ts, &cmd, true ) )
            break;

        CmdClean( &cmd );
    }
    assert( !p_ts->p_storage_r || !p_ts->p_storage_r->p_next );
    if( p_ts->p_storage_r )
        TsStorageDelete( p_ts->p_storage_r );
    vlc_mutex_unlock( &p_ts->lock );

    TsDestroy( p_ts );
}
static void TsPushCmd( ts_thread_t *p_ts, ts_cmd_t *p_cmd )
{
    vlc_mutex_lock( &p_ts->lock );

    if( !p_ts->p_storage_w || TsStorageIsFull( p_ts->p_storage_w, p_cmd ) )
    {
        ts_storage_t *p_storage = TsStorageNew( p_ts->psz_tmp_path, p_ts->i_tmp_size_max );

        if( !p_storage )
        {
            CmdClean( p_cmd );
            vlc_mutex_unlock( &p_ts->lock );
            /* TODO warn the user (but only once) */
            return;
        }

        if( !p_ts->p_storage_w )
        {
            p_ts->p_storage_r = p_ts->p_storage_w = p_storage;
        }
        else
        {
            TsStoragePack( p_ts->p_storage_w );
            p_ts->p_storage_w->p_next = p_storage;
            p_ts->p_storage_w = p_storage;
        }
    }

    /* TODO return error and warn the user (but only once) */
    TsStoragePushCmd( p_ts->p_storage_w, p_cmd, p_ts->p_storage_r == p_ts->p_storage_w );

    vlc_cond_signal( &p_ts->wait );

    vlc_mutex_unlock( &p_ts->lock );
}
static int TsPopCmdLocked( ts_thread_t *p_ts, ts_cmd_t *p_cmd, bool b_flush )
{
    vlc_assert_locked( &p_ts->lock );

    if( TsStorageIsEmpty( p_ts->p_storage_r ) )
        return VLC_EGENERIC;

    TsStoragePopCmd( p_ts->p_storage_r, p_cmd, b_flush );

    while( p_ts->p_storage_r && TsStorageIsEmpty( p_ts->p_storage_r ) )
    {
        ts_storage_t *p_next = p_ts->p_storage_r->p_next;
        if( !p_next )
            break;

        TsStorageDelete( p_ts->p_storage_r );
        p_ts->p_storage_r = p_next;
    }

    return VLC_SUCCESS;
}
static bool TsHasCmd( ts_thread_t *p_ts )
{
    bool b_cmd;

    vlc_mutex_lock( &p_ts->lock );
    b_cmd =  TsStorageIsEmpty( p_ts->p_storage_r );
    vlc_mutex_unlock( &p_ts->lock );

    return b_cmd;
}
static bool TsIsUnused( ts_thread_t *p_ts )
{
    bool b_unused;

    vlc_mutex_lock( &p_ts->lock );
    b_unused = !p_ts->b_paused &&
               p_ts->i_rate == p_ts->i_rate_source &&
               TsStorageIsEmpty( p_ts->p_storage_r );
    vlc_mutex_unlock( &p_ts->lock );

    return b_unused;
}
static int TsChangePause( ts_thread_t *p_ts, bool b_source_paused, bool b_paused, mtime_t i_date )
{
    vlc_mutex_lock( &p_ts->lock );

    int i_ret;
    if( b_paused )
    {
        assert( !b_source_paused );
        i_ret = es_out_SetPauseState( p_ts->p_out, true, true, i_date );
    }
    else
    {
        i_ret = es_out_SetPauseState( p_ts->p_out, false, false, i_date );
    }

    if( !i_ret )
    {
        if( !b_paused )
        {
            assert( p_ts->i_pause_date > 0 );

            p_ts->i_cmd_delay += i_date - p_ts->i_pause_date;
        }

        p_ts->b_paused = b_paused;
        p_ts->i_pause_date = i_date;

        vlc_cond_signal( &p_ts->wait );
    }
    vlc_mutex_unlock( &p_ts->lock );
    return i_ret;
}
static int TsChangeRate( ts_thread_t *p_ts, int i_src_rate, int i_rate )
{
    int i_ret;

    vlc_mutex_lock( &p_ts->lock );
    p_ts->i_cmd_delay += p_ts->i_rate_delay;

    p_ts->i_rate_date = -1;
    p_ts->i_rate_delay = 0;
    p_ts->i_rate = i_rate;
    p_ts->i_rate_source = i_src_rate;

    i_ret = es_out_SetRate( p_ts->p_out, i_rate, i_rate );
    vlc_mutex_unlock( &p_ts->lock );

    return i_ret;
}

static void *TsRun( void *p_data )
{
    ts_thread_t *p_ts = p_data;
    mtime_t i_buffering_date = -1;

    for( ;; )
    {
        ts_cmd_t cmd;
        mtime_t  i_deadline;
        bool b_buffering;

        /* Pop a command to execute */
        vlc_mutex_lock( &p_ts->lock );
        mutex_cleanup_push( &p_ts->lock );

        for( ;; )
        {
            const int canc = vlc_savecancel();
            b_buffering = es_out_GetBuffering( p_ts->p_out );

            if( ( !p_ts->b_paused || b_buffering ) && !TsPopCmdLocked( p_ts, &cmd, false ) )
            {
                vlc_restorecancel( canc );
                break;
            }
            vlc_restorecancel( canc );

            vlc_cond_wait( &p_ts->wait, &p_ts->lock );
        }

        if( b_buffering && i_buffering_date < 0 )
        {
            i_buffering_date = cmd.i_date;
        }
        else if( i_buffering_date > 0 )
        {
            p_ts->i_buffering_delay += i_buffering_date - cmd.i_date; /* It is < 0 */
            if( b_buffering )
                i_buffering_date = cmd.i_date;
            else
                i_buffering_date = -1;
        }

        if( p_ts->i_rate_date < 0 )
            p_ts->i_rate_date = cmd.i_date;

        p_ts->i_rate_delay = 0;
        if( p_ts->i_rate_source != p_ts->i_rate )
        {
            const mtime_t i_duration = cmd.i_date - p_ts->i_rate_date;
            p_ts->i_rate_delay = i_duration * p_ts->i_rate / p_ts->i_rate_source - i_duration;
        }
        if( p_ts->i_cmd_delay + p_ts->i_rate_delay + p_ts->i_buffering_delay < 0 && p_ts->i_rate != p_ts->i_rate_source )
        {
            const int canc = vlc_savecancel();

            /* Auto reset to rate 1.0 */
            msg_Warn( p_ts->p_input, "es out timeshift: auto reset rate to %d", p_ts->i_rate_source );

            p_ts->i_cmd_delay = 0;
            p_ts->i_buffering_delay = 0;

            p_ts->i_rate_delay = 0;
            p_ts->i_rate_date = -1;
            p_ts->i_rate = p_ts->i_rate_source;

            if( !es_out_SetRate( p_ts->p_out, p_ts->i_rate_source, p_ts->i_rate ) )
            {
                vlc_value_t val = { .i_int = p_ts->i_rate };
                /* Warn back input
                 * FIXME it is perfectly safe BUT it is ugly as it may hide a
                 * rate change requested by user */
                input_ControlPush( p_ts->p_input, INPUT_CONTROL_SET_RATE, &val );
            }

            vlc_restorecancel( canc );
        }
        i_deadline = cmd.i_date + p_ts->i_cmd_delay + p_ts->i_rate_delay + p_ts->i_buffering_delay;

        vlc_cleanup_run();

        /* Regulate the speed of command processing to the same one than
         * reading  */
        vlc_cleanup_push( cmd_cleanup_routine, &cmd );

        mwait( i_deadline );

        vlc_cleanup_pop();

        /* Execute the command  */
        const int canc = vlc_savecancel();
        switch( cmd.i_type )
        {
        case C_ADD:
            CmdExecuteAdd( p_ts->p_out, &cmd );
            CmdCleanAdd( &cmd );
            break;
        case C_SEND:
            CmdExecuteSend( p_ts->p_out, &cmd );
            CmdCleanSend( &cmd );
            break;
        case C_CONTROL:
            CmdExecuteControl( p_ts->p_out, &cmd );
            CmdCleanControl( &cmd );
            break;
        case C_DEL:
            CmdExecuteDel( p_ts->p_out, &cmd );
            break;
        default:
            assert(0);
            break;
        }
        vlc_restorecancel( canc );
    }

    return NULL;
}

/*****************************************************************************
 *
 *****************************************************************************/
static ts_storage_t *TsStorageNew( const char *psz_tmp_path, int64_t i_tmp_size_max )
{
    ts_storage_t *p_storage = calloc( 1, sizeof(ts_storage_t) );
    if( !p_storage )
        return NULL;

    /* */
    p_storage->p_next = NULL;

    /* */
    p_storage->i_file_max = i_tmp_size_max;
    p_storage->i_file_size = 0;
    p_storage->p_filew = GetTmpFile( &p_storage->psz_file, psz_tmp_path );
    if( p_storage->psz_file )
        p_storage->p_filer = vlc_fopen( p_storage->psz_file, "rb" );

    /* */
    p_storage->i_cmd_w = 0;
    p_storage->i_cmd_r = 0;
    p_storage->i_cmd_max = 30000;
    p_storage->p_cmd = malloc( p_storage->i_cmd_max * sizeof(*p_storage->p_cmd) );
    //fprintf( stderr, "\nSTORAGE name=%s size=%d KiB\n", p_storage->psz_file, p_storage->i_cmd_max * sizeof(*p_storage->p_cmd) /1024 );

    if( !p_storage->p_cmd || !p_storage->p_filew || !p_storage->p_filer )
    {
        TsStorageDelete( p_storage );
        return NULL;
    }
    return p_storage;
}
static void TsStorageDelete( ts_storage_t *p_storage )
{
    while( p_storage->i_cmd_r < p_storage->i_cmd_w )
    {
        ts_cmd_t cmd;

        TsStoragePopCmd( p_storage, &cmd, true );

        CmdClean( &cmd );
    }
    free( p_storage->p_cmd );

    if( p_storage->p_filer )
        fclose( p_storage->p_filer );
    if( p_storage->p_filew )
        fclose( p_storage->p_filew );

    if( p_storage->psz_file )
    {
        vlc_unlink( p_storage->psz_file );
        free( p_storage->psz_file );
    }

    free( p_storage );
}
static void TsStoragePack( ts_storage_t *p_storage )
{
    /* Try to release a bit of memory */
    if( p_storage->i_cmd_w >= p_storage->i_cmd_max )
        return;

    p_storage->i_cmd_max = __MAX( p_storage->i_cmd_w, 1 );

    ts_cmd_t *p_new = realloc( p_storage->p_cmd, p_storage->i_cmd_max * sizeof(*p_storage->p_cmd) );
    if( p_new )
        p_storage->p_cmd = p_new;
}
static bool TsStorageIsFull( ts_storage_t *p_storage, const ts_cmd_t *p_cmd )
{
    if( p_cmd && p_cmd->i_type == C_SEND && p_storage->i_cmd_w > 0 )
    {
        size_t i_size = sizeof(*p_cmd->u.send.p_block) + p_cmd->u.send.p_block->i_buffer;

        if( p_storage->i_file_size + i_size >= p_storage->i_file_max )
            return true;
    }
    return p_storage->i_cmd_w >= p_storage->i_cmd_max;
}
static bool TsStorageIsEmpty( ts_storage_t *p_storage )
{
    return !p_storage || p_storage->i_cmd_r >= p_storage->i_cmd_w;
}
static void TsStoragePushCmd( ts_storage_t *p_storage, const ts_cmd_t *p_cmd, bool b_flush )
{
    ts_cmd_t cmd = *p_cmd;

    assert( !TsStorageIsFull( p_storage, p_cmd ) );

    if( cmd.i_type == C_SEND )
    {
        block_t *p_block = cmd.u.send.p_block;

        cmd.u.send.p_block = NULL;
        cmd.u.send.i_offset = ftell( p_storage->p_filew );

        if( fwrite( p_block, sizeof(*p_block), 1, p_storage->p_filew ) != 1 )
        {
            block_Release( p_block );
            return;
        }
        p_storage->i_file_size += sizeof(*p_block);
        if( p_block->i_buffer > 0 )
        {
            if( fwrite( p_block->p_buffer, p_block->i_buffer, 1, p_storage->p_filew ) != 1 )
            {
                block_Release( p_block );
                return;
            }
        }
        p_storage->i_file_size += p_block->i_buffer;
        block_Release( p_block );

        if( b_flush )
            fflush( p_storage->p_filew );
    }
    p_storage->p_cmd[p_storage->i_cmd_w++] = cmd;
}
static void TsStoragePopCmd( ts_storage_t *p_storage, ts_cmd_t *p_cmd, bool b_flush )
{
    assert( !TsStorageIsEmpty( p_storage ) );

    *p_cmd = p_storage->p_cmd[p_storage->i_cmd_r++];
    if( p_cmd->i_type == C_SEND )
    {
        block_t block;

        if( !b_flush &&
            !fseek( p_storage->p_filer, p_cmd->u.send.i_offset, SEEK_SET ) &&
            fread( &block, sizeof(block), 1, p_storage->p_filer ) == 1 )
        {
            block_t *p_block = block_Alloc( block.i_buffer );
            if( p_block )
            {
                p_block->i_dts      = block.i_dts;
                p_block->i_pts      = block.i_pts;
                p_block->i_flags    = block.i_flags;
                p_block->i_length   = block.i_length;
                p_block->i_nb_samples = block.i_nb_samples;
                p_block->i_buffer = fread( p_block->p_buffer, 1, block.i_buffer, p_storage->p_filer );
            }
            p_cmd->u.send.p_block = p_block;
        }
        else
        {
            //fprintf( stderr, "TsStoragePopCmd: %m\n" );
            p_cmd->u.send.p_block = block_Alloc( 1 );
        }
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void CmdClean( ts_cmd_t *p_cmd )
{
    switch( p_cmd->i_type )
    {
    case C_ADD:
        CmdCleanAdd( p_cmd );
        break;
    case C_SEND:
        CmdCleanSend( p_cmd );
        break;
    case C_CONTROL:
        CmdCleanControl( p_cmd );
        break;
    case C_DEL:
        break;
    default:
        assert(0);
        break;
    }
}

static int CmdInitAdd( ts_cmd_t *p_cmd, es_out_id_t *p_es, const es_format_t *p_fmt, bool b_copy )
{
    p_cmd->i_type = C_ADD;
    p_cmd->i_date = mdate();
    p_cmd->u.add.p_es = p_es;
    if( b_copy )
    {
        p_cmd->u.add.p_fmt = malloc( sizeof(*p_fmt) );
        if( !p_cmd->u.add.p_fmt )
            return VLC_EGENERIC;
        es_format_Copy( p_cmd->u.add.p_fmt, p_fmt );
    }
    else
    {
        p_cmd->u.add.p_fmt = (es_format_t*)p_fmt;
    }
    return VLC_SUCCESS;
}
static void CmdExecuteAdd( es_out_t *p_out, ts_cmd_t *p_cmd )
{
    p_cmd->u.add.p_es->p_es = es_out_Add( p_out, p_cmd->u.add.p_fmt );
}
static void CmdCleanAdd( ts_cmd_t *p_cmd )
{
    es_format_Clean( p_cmd->u.add.p_fmt );
    free( p_cmd->u.add.p_fmt );
}

static void CmdInitSend( ts_cmd_t *p_cmd, es_out_id_t *p_es, block_t *p_block )
{
    p_cmd->i_type = C_SEND;
    p_cmd->i_date = mdate();
    p_cmd->u.send.p_es = p_es;
    p_cmd->u.send.p_block = p_block;
}
static int CmdExecuteSend( es_out_t *p_out, ts_cmd_t *p_cmd )
{
    block_t *p_block = p_cmd->u.send.p_block;

    p_cmd->u.send.p_block = NULL;

    if( p_block )
    {
        if( p_cmd->u.send.p_es->p_es )
            return es_out_Send( p_out, p_cmd->u.send.p_es->p_es, p_block );
        block_Release( p_block );
    }
    return VLC_EGENERIC;
}
static void CmdCleanSend( ts_cmd_t *p_cmd )
{
    if( p_cmd->u.send.p_block )
        block_Release( p_cmd->u.send.p_block );
}

static int CmdInitDel( ts_cmd_t *p_cmd, es_out_id_t *p_es )
{
    p_cmd->i_type = C_DEL;
    p_cmd->i_date = mdate();
    p_cmd->u.del.p_es = p_es;
    return VLC_SUCCESS;
}
static void CmdExecuteDel( es_out_t *p_out, ts_cmd_t *p_cmd )
{
    if( p_cmd->u.del.p_es->p_es )
        es_out_Del( p_out, p_cmd->u.del.p_es->p_es );
    free( p_cmd->u.del.p_es );
}

static int CmdInitControl( ts_cmd_t *p_cmd, int i_query, va_list args, bool b_copy )
{
    p_cmd->i_type = C_CONTROL;
    p_cmd->i_date = mdate();
    p_cmd->u.control.i_query = i_query;

    switch( i_query )
    {
    /* Pass-through control */
    case ES_OUT_SET_MODE:    /* arg1= int                            */
    case ES_OUT_SET_GROUP:   /* arg1= int                            */
    case ES_OUT_DEL_GROUP:   /* arg1=int i_group */
        p_cmd->u.control.u.i_int = (int)va_arg( args, int );
        break;

    case ES_OUT_SET_PCR:                /* arg1=int64_t i_pcr(microsecond!) (using default group 0)*/
    case ES_OUT_SET_NEXT_DISPLAY_TIME:  /* arg1=int64_t i_pts(microsecond) */
        p_cmd->u.control.u.i_i64 = (int64_t)va_arg( args, int64_t );
        break;

    case ES_OUT_SET_GROUP_PCR:          /* arg1= int i_group, arg2=int64_t i_pcr(microsecond!)*/
        p_cmd->u.control.u.int_i64.i_int = (int)va_arg( args, int );
        p_cmd->u.control.u.int_i64.i_i64 = (int64_t)va_arg( args, int64_t );
        break;

    case ES_OUT_SET_ES_SCRAMBLED_STATE:
        p_cmd->u.control.u.es_bool.p_es = (es_out_id_t*)va_arg( args, es_out_id_t * );
        p_cmd->u.control.u.es_bool.b_bool = (bool)va_arg( args, int );
        break;

    case ES_OUT_RESET_PCR:           /* no arg */
    case ES_OUT_SET_EOS:
        break;

    case ES_OUT_SET_META:        /* arg1=const vlc_meta_t* */
    case ES_OUT_SET_GROUP_META:  /* arg1=int i_group arg2=const vlc_meta_t* */
    {
        if( i_query == ES_OUT_SET_GROUP_META )
            p_cmd->u.control.u.int_meta.i_int = (int)va_arg( args, int );
        const vlc_meta_t *p_meta = va_arg( args, const vlc_meta_t * );

        if( b_copy )
        {
            p_cmd->u.control.u.int_meta.p_meta = vlc_meta_New();
            if( !p_cmd->u.control.u.int_meta.p_meta )
                return VLC_EGENERIC;
            vlc_meta_Merge( p_cmd->u.control.u.int_meta.p_meta, p_meta );
        }
        else
        {
            /* The cast is only needed to avoid warning */
            p_cmd->u.control.u.int_meta.p_meta = (vlc_meta_t*)p_meta;
        }
        break;
    }

    case ES_OUT_SET_GROUP_EPG:   /* arg1=int i_group arg2=const vlc_epg_t* */
    {
        p_cmd->u.control.u.int_epg.i_int = (int)va_arg( args, int );
        const vlc_epg_t *p_epg = va_arg( args, const vlc_epg_t * );

        if( b_copy )
        {
            p_cmd->u.control.u.int_epg.p_epg = vlc_epg_New( p_epg->psz_name );
            if( !p_cmd->u.control.u.int_epg.p_epg )
                return VLC_EGENERIC;
            for( int i = 0; i < p_epg->i_event; i++ )
            {
                vlc_epg_event_t *p_evt = p_epg->pp_event[i];

                vlc_epg_AddEvent( p_cmd->u.control.u.int_epg.p_epg,
                                  p_evt->i_start, p_evt->i_duration,
                                  p_evt->psz_name,
                                  p_evt->psz_short_description,
                                  p_evt->psz_description, 0 );
            }
            vlc_epg_SetCurrent( p_cmd->u.control.u.int_epg.p_epg,
                                p_epg->p_current ? p_epg->p_current->i_start : -1 );
        }
        else
        {
            /* The cast is only needed to avoid warning */
            p_cmd->u.control.u.int_epg.p_epg = (vlc_epg_t*)p_epg;
        }
        break;
    }

    /* Modified control */
    case ES_OUT_SET_ES:      /* arg1= es_out_id_t*                   */
    case ES_OUT_RESTART_ES:  /* arg1= es_out_id_t*                   */
    case ES_OUT_SET_ES_DEFAULT: /* arg1= es_out_id_t*                */
        p_cmd->u.control.u.p_es = (es_out_id_t*)va_arg( args, es_out_id_t * );
        break;

    case ES_OUT_SET_ES_STATE:/* arg1= es_out_id_t* arg2=bool   */
        p_cmd->u.control.u.es_bool.p_es = (es_out_id_t*)va_arg( args, es_out_id_t * );
        p_cmd->u.control.u.es_bool.b_bool = (bool)va_arg( args, int );
        break;

    case ES_OUT_SET_ES_FMT:     /* arg1= es_out_id_t* arg2=es_format_t* */
    {
        p_cmd->u.control.u.es_fmt.p_es = (es_out_id_t*)va_arg( args, es_out_id_t * );
        es_format_t *p_fmt = (es_format_t*)va_arg( args, es_format_t * );

        if( b_copy )
        {
            p_cmd->u.control.u.es_fmt.p_fmt = malloc( sizeof(*p_fmt) );
            if( !p_cmd->u.control.u.es_fmt.p_fmt )
                return VLC_EGENERIC;
            es_format_Copy( p_cmd->u.control.u.es_fmt.p_fmt, p_fmt );
        }
        else
        {
            p_cmd->u.control.u.es_fmt.p_fmt = p_fmt;
        }
        break;
    }
    case ES_OUT_SET_TIMES:
    {
        double f_position = (double)va_arg( args, double );
        mtime_t i_time = (mtime_t)va_arg( args, mtime_t );
        mtime_t i_length = (mtime_t)va_arg( args, mtime_t );

        p_cmd->u.control.u.times.f_position = f_position;
        p_cmd->u.control.u.times.i_time = i_time;
        p_cmd->u.control.u.times.i_length = i_length;
        break;
    }
    case ES_OUT_SET_JITTER:
    {
        mtime_t i_pts_delay = (mtime_t)va_arg( args, mtime_t );
        mtime_t i_pts_jitter = (mtime_t)va_arg( args, mtime_t );
        int     i_cr_average = (int)va_arg( args, int );

        p_cmd->u.control.u.jitter.i_pts_delay = i_pts_delay;
        p_cmd->u.control.u.jitter.i_pts_jitter = i_pts_jitter;
        p_cmd->u.control.u.jitter.i_cr_average = i_cr_average;
        break;
    }

    default:
        assert(0);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
static int CmdExecuteControl( es_out_t *p_out, ts_cmd_t *p_cmd )
{
    const int i_query = p_cmd->u.control.i_query;

    switch( i_query )
    {
    /* Pass-through control */
    case ES_OUT_SET_MODE:    /* arg1= int                            */
    case ES_OUT_SET_GROUP:   /* arg1= int                            */
    case ES_OUT_DEL_GROUP:   /* arg1=int i_group */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.i_int );

    case ES_OUT_SET_PCR:                /* arg1=int64_t i_pcr(microsecond!) (using default group 0)*/
    case ES_OUT_SET_NEXT_DISPLAY_TIME:  /* arg1=int64_t i_pts(microsecond) */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.i_i64 );

    case ES_OUT_SET_GROUP_PCR:          /* arg1= int i_group, arg2=int64_t i_pcr(microsecond!)*/
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.int_i64.i_int,
                                               p_cmd->u.control.u.int_i64.i_i64 );

    case ES_OUT_RESET_PCR:           /* no arg */
    case ES_OUT_SET_EOS:
        return es_out_Control( p_out, i_query );

    case ES_OUT_SET_GROUP_META:  /* arg1=int i_group arg2=const vlc_meta_t* */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.int_meta.i_int,
                                               p_cmd->u.control.u.int_meta.p_meta );

    case ES_OUT_SET_GROUP_EPG:   /* arg1=int i_group arg2=const vlc_epg_t* */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.int_epg.i_int,
                                               p_cmd->u.control.u.int_epg.p_epg );

    case ES_OUT_SET_ES_SCRAMBLED_STATE: /* arg1=int es_out_id_t* arg2=bool */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.es_bool.p_es->p_es,
                                               p_cmd->u.control.u.es_bool.b_bool );

    case ES_OUT_SET_META:  /* arg1=const vlc_meta_t* */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.int_meta.p_meta );

    /* Modified control */
    case ES_OUT_SET_ES:      /* arg1= es_out_id_t*                   */
    case ES_OUT_RESTART_ES:  /* arg1= es_out_id_t*                   */
    case ES_OUT_SET_ES_DEFAULT: /* arg1= es_out_id_t*                */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.p_es->p_es );

    case ES_OUT_SET_ES_STATE:/* arg1= es_out_id_t* arg2=bool   */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.es_bool.p_es->p_es,
                                               p_cmd->u.control.u.es_bool.b_bool );

    case ES_OUT_SET_ES_FMT:     /* arg1= es_out_id_t* arg2=es_format_t* */
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.es_fmt.p_es->p_es,
                                               p_cmd->u.control.u.es_fmt.p_fmt );

    case ES_OUT_SET_TIMES:
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.times.f_position,
                                               p_cmd->u.control.u.times.i_time,
                                               p_cmd->u.control.u.times.i_length );
    case ES_OUT_SET_JITTER:
        return es_out_Control( p_out, i_query, p_cmd->u.control.u.jitter.i_pts_delay,
                                               p_cmd->u.control.u.jitter.i_pts_jitter,
                                               p_cmd->u.control.u.jitter.i_cr_average );

    default:
        assert(0);
        return VLC_EGENERIC;
    }
}
static void CmdCleanControl( ts_cmd_t *p_cmd )
{
    if( ( p_cmd->u.control.i_query == ES_OUT_SET_GROUP_META ||
          p_cmd->u.control.i_query == ES_OUT_SET_META ) &&
        p_cmd->u.control.u.int_meta.p_meta )
    {
        vlc_meta_Delete( p_cmd->u.control.u.int_meta.p_meta );
    }
    else if( p_cmd->u.control.i_query == ES_OUT_SET_GROUP_EPG &&
             p_cmd->u.control.u.int_epg.p_epg )
    {
        vlc_epg_Delete( p_cmd->u.control.u.int_epg.p_epg );
    }
    else if( p_cmd->u.control.i_query == ES_OUT_SET_ES_FMT &&
             p_cmd->u.control.u.es_fmt.p_fmt )
    {
        es_format_Clean( p_cmd->u.control.u.es_fmt.p_fmt );
        free( p_cmd->u.control.u.es_fmt.p_fmt );
    }
}


/*****************************************************************************
 * GetTmpFile/Path:
 *****************************************************************************/
static char *GetTmpPath( char *psz_path )
{
    if( psz_path && *psz_path )
    {
        /* Make sure that the path exists and is a directory */
        struct stat s;
        const int i_ret = vlc_stat( psz_path, &s );

        if( i_ret < 0 && !vlc_mkdir( psz_path, 0600 ) )
            return psz_path;
        else if( i_ret == 0 && ( s.st_mode & S_IFDIR ) )
            return psz_path;
    }
    free( psz_path );

    /* Create a suitable path */
#if defined (_WIN32) && !VLC_WINSTORE_APP
    const DWORD dwCount = GetTempPathW( 0, NULL );
    wchar_t *psw_path = calloc( dwCount + 1, sizeof(wchar_t) );
    if( psw_path )
    {
        if( GetTempPathW( dwCount + 1, psw_path ) <= 0 )
        {
            free( psw_path );

            psw_path = _wgetcwd( NULL, 0 );
        }
    }

    psz_path = NULL;
    if( psw_path )
    {
        psz_path = FromWide( psw_path );
        while( psz_path && *psz_path && psz_path[strlen( psz_path ) - 1] == '\\' )
            psz_path[strlen( psz_path ) - 1] = '\0';

        free( psw_path );
    }

    if( !psz_path || *psz_path == '\0' )
    {
        free( psz_path );
        return strdup( "C:" );
    }
#else
    psz_path = strdup( DIR_SEP"tmp" );
#endif

    return psz_path;
}

static FILE *GetTmpFile( char **ppsz_file, const char *psz_path )
{
    char *psz_name;
    int fd;
    FILE *f;

    /* */
    *ppsz_file = NULL;
    if( asprintf( &psz_name, "%s/vlc-timeshift.XXXXXX", psz_path ) < 0 )
        return NULL;

    /* */
    fd = vlc_mkstemp( psz_name );
    *ppsz_file = psz_name;

    if( fd < 0 )
        return NULL;

    /* */
    f = fdopen( fd, "w+b" );
    if( !f )
        close( fd );

    return f;
}

