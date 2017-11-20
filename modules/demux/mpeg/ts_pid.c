/*****************************************************************************
 * ts_pid.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "ts_pid.h"
#include "ts_streams.h"

#include "ts.h"

#include <assert.h>
#include <stdlib.h>

#define PID_ALLOC_CHUNK 16

void ts_pid_list_Init( ts_pid_list_t *p_list )
{
    p_list->dummy.i_pid = 8191;
    p_list->dummy.i_flags = FLAG_SEEN;
    p_list->base_si.i_pid = 0x1FFB;
    p_list->pp_all = NULL;
    p_list->i_all = 0;
    p_list->i_all_alloc = 0;
    p_list->i_last_pid = 0;
    p_list->p_last = NULL;
}

void ts_pid_list_Release( demux_t *p_demux, ts_pid_list_t *p_list )
{
    for( int i = 0; i < p_list->i_all; i++ )
    {
        ts_pid_t *pid = p_list->pp_all[i];
#ifndef NDEBUG
        if( pid->type != TYPE_FREE )
            msg_Err( p_demux, "PID %d type %d not freed refcount %d", pid->i_pid, pid->type, pid->i_refcount );
#else
        VLC_UNUSED(p_demux);
#endif
        free( pid );
    }
    free( p_list->pp_all );
}

struct searchkey
{
    int16_t i_pid;
    ts_pid_t **pp_last;
};

static int ts_bsearch_searchkey_Compare( void *key, void *other )
{
    struct searchkey *p_key = (struct searchkey *) key;
    ts_pid_t *p_pid = *((ts_pid_t **) other);
    p_key->pp_last = (ts_pid_t **) other;
    return ( p_key->i_pid >= p_pid->i_pid ) ? p_key->i_pid - p_pid->i_pid : -1;
}

ts_pid_t * ts_pid_Get( ts_pid_list_t *p_list, uint16_t i_pid )
{
    switch( i_pid )
    {
        case 0:
            return &p_list->pat;
        case 0x1FFB:
            return &p_list->base_si;
        case 0x1FFF:
            return &p_list->dummy;
        default:
            if( p_list->i_last_pid == i_pid )
                return p_list->p_last;
        break;
    }

    size_t i_index = 0;
    ts_pid_t *p_pid = NULL;

    if( p_list->pp_all )
    {
        struct searchkey pidkey;
        pidkey.i_pid = i_pid;
        pidkey.pp_last = NULL;

        ts_pid_t **pp_pidk = bsearch( &pidkey, p_list->pp_all, p_list->i_all,
                                      sizeof(ts_pid_t *), ts_bsearch_searchkey_Compare );
        if ( pp_pidk )
            p_pid = *pp_pidk;
        else
            i_index = (pidkey.pp_last - p_list->pp_all); /* Last visited index */
    }

    if( p_pid == NULL )
    {
        if( p_list->i_all >= p_list->i_all_alloc )
        {
            ts_pid_t **p_realloc = realloc( p_list->pp_all,
                                            (p_list->i_all_alloc + PID_ALLOC_CHUNK) * sizeof(ts_pid_t *) );
            if( !p_realloc )
            {
                abort();
                //return NULL;
            }
            p_list->pp_all = p_realloc;
            p_list->i_all_alloc += PID_ALLOC_CHUNK;
        }

        p_pid = calloc( 1, sizeof(*p_pid) );
        if( !p_pid )
        {
            abort();
            //return NULL;
        }

        p_pid->i_cc  = 0xff;
        p_pid->i_pid = i_pid;

        /* Do insertion based on last bsearch mid point */
        if( p_list->i_all )
        {
            if( p_list->pp_all[i_index]->i_pid < i_pid )
                i_index++;

            memmove( &p_list->pp_all[i_index + 1],
                    &p_list->pp_all[i_index],
                    (p_list->i_all - i_index) * sizeof(ts_pid_t *) );
        }

        p_list->pp_all[i_index] = p_pid;
        p_list->i_all++;

    }

    p_list->p_last = p_pid;
    p_list->i_last_pid = i_pid;

    return p_pid;
}

ts_pid_t * ts_pid_Next( ts_pid_list_t *p_list, ts_pid_next_context_t *p_ctx )
{
    if( likely(p_list->i_all && p_ctx) )
    {
        if( p_ctx->i_pos < p_list->i_all )
            return p_list->pp_all[p_ctx->i_pos++];
    }
    return NULL;
}

static void PIDReset( ts_pid_t *pid )
{
    assert(pid->i_refcount == 0);
    pid->i_cc       = 0xff;
    pid->i_flags    &= ~FLAG_SCRAMBLED;
    pid->type = TYPE_FREE;
}

bool PIDSetup( demux_t *p_demux, ts_pid_type_t i_type, ts_pid_t *pid, ts_pid_t *p_parent )
{
    if( pid == p_parent || pid->i_pid == 0x1FFF )
        return false;

    if( pid->i_refcount == 0 )
    {
        assert( pid->type == TYPE_FREE );
        switch( i_type )
        {
        case TYPE_FREE: /* nonsense ?*/
            PIDReset( pid );
            return true;

        case TYPE_CAT:
            return true;

        case TYPE_PAT:
            PIDReset( pid );
            pid->u.p_pat = ts_pat_New( p_demux );
            if( !pid->u.p_pat )
                return false;
            break;

        case TYPE_PMT:
            PIDReset( pid );
            pid->u.p_pmt = ts_pmt_New( p_demux );
            if( !pid->u.p_pmt )
                return false;
            break;

        case TYPE_STREAM:
            PIDReset( pid );
            pid->u.p_stream = ts_stream_New( p_demux, p_parent->u.p_pmt );
            if( !pid->u.p_stream )
                return false;
            break;

        case TYPE_SI:
            PIDReset( pid );
            pid->u.p_si = ts_si_New( p_demux );
            if( !pid->u.p_si )
                return false;
            break;

        case TYPE_PSIP:
            PIDReset( pid );
            pid->u.p_psip = ts_psip_New( p_demux );
            if( !pid->u.p_psip )
                return false;
            break;

        default:
            assert(false);
            break;
        }

        pid->i_refcount++;
        pid->type = i_type;
    }
    else if( pid->type == i_type && pid->i_refcount < UINT16_MAX )
    {
        pid->i_refcount++;
    }
    else
    {
        if( pid->type != TYPE_FREE )
            msg_Warn( p_demux, "Tried to redeclare pid %d with another type", pid->i_pid );
        return false;
    }

    return true;
}

void PIDRelease( demux_t *p_demux, ts_pid_t *pid )
{
    if( pid->i_refcount == 0 )
    {
        assert( pid->type == TYPE_FREE );
        return;
    }
    else if( pid->i_refcount == 1 )
    {
        pid->i_refcount--;
    }
    else if( pid->i_refcount > 1 )
    {
        assert( pid->type != TYPE_FREE && pid->type != TYPE_PAT );
        pid->i_refcount--;
    }

    if( pid->i_refcount == 0 )
    {
        switch( pid->type )
        {
        default:
        case TYPE_FREE: /* nonsense ?*/
            assert( pid->type != TYPE_FREE );
            break;

        case TYPE_CAT:
            break;

        case TYPE_PAT:
            ts_pat_Del( p_demux, pid->u.p_pat );
            pid->u.p_pat = NULL;
            break;

        case TYPE_PMT:
            ts_pmt_Del( p_demux, pid->u.p_pmt );
            pid->u.p_pmt = NULL;
            break;

        case TYPE_STREAM:
            ts_stream_Del( p_demux, pid->u.p_stream );
            pid->u.p_stream = NULL;
            break;

        case TYPE_SI:
            ts_si_Del( p_demux, pid->u.p_si );
            pid->u.p_si = NULL;
            break;

        case TYPE_PSIP:
            ts_psip_Del( p_demux, pid->u.p_psip );
            pid->u.p_psip = NULL;
            break;
        }

        SetPIDFilter( p_demux->p_sys, pid, false );
        PIDReset( pid );
    }
}

int UpdateHWFilter( demux_sys_t *p_sys, ts_pid_t *p_pid )
{
    if( !p_sys->b_access_control )
        return VLC_EGENERIC;

    return vlc_stream_Control( p_sys->stream, STREAM_SET_PRIVATE_ID_STATE,
                           p_pid->i_pid, !!(p_pid->i_flags & FLAG_FILTERED) );
}

int SetPIDFilter( demux_sys_t *p_sys, ts_pid_t *p_pid, bool b_selected )
{
    if( b_selected )
        p_pid->i_flags |= FLAG_FILTERED;
    else
        p_pid->i_flags &= ~FLAG_FILTERED;

    return UpdateHWFilter( p_sys, p_pid );
}
