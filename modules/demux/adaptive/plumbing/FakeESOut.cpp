/*
 * FakeESOut.cpp
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include "FakeESOut.hpp"
#include "FakeESOutID.hpp"
#include "CommandsQueue.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <cassert>

using namespace adaptive;

namespace adaptive
{
    class EsOutCallbacks
    {
        public:
            /* static callbacks for demuxer */
            static es_out_id_t *es_out_Add( es_out_t *, input_source_t *, const es_format_t * );
            static int es_out_Send( es_out_t *, es_out_id_t *, block_t * );
            static void es_out_Del( es_out_t *, es_out_id_t * );
            static int es_out_Control( es_out_t *, input_source_t *in, int, va_list );
            static void es_out_Destroy( es_out_t * );
            static const struct es_out_callbacks cbs;
            struct Private
            {
                AbstractFakeEsOut *fake;
                es_out_t es_out;
            };
    };
}

const struct es_out_callbacks EsOutCallbacks::cbs =
{
    EsOutCallbacks::es_out_Add,
    EsOutCallbacks::es_out_Send,
    EsOutCallbacks::es_out_Del,
    EsOutCallbacks::es_out_Control,
    EsOutCallbacks::es_out_Destroy,
    nullptr,
};

es_out_id_t * EsOutCallbacks::es_out_Add(es_out_t *fakees, input_source_t *, const es_format_t *p_fmt)
{
    AbstractFakeEsOut *me = container_of(fakees, Private, es_out)->fake;
    return me->esOutAdd(p_fmt);
}

int EsOutCallbacks::es_out_Send(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    AbstractFakeEsOut *me = container_of(fakees, Private, es_out)->fake;
    return me->esOutSend(p_es, p_block);
}

void EsOutCallbacks::es_out_Del(es_out_t *fakees, es_out_id_t *p_es)
{
    AbstractFakeEsOut *me = container_of(fakees, Private, es_out)->fake;
    me->esOutDel(p_es);
}

int EsOutCallbacks::es_out_Control(es_out_t *fakees, input_source_t *, int i_query, va_list args)
{
    AbstractFakeEsOut *me = container_of(fakees, Private, es_out)->fake;
    return me->esOutControl(i_query, args);
}

void EsOutCallbacks::es_out_Destroy(es_out_t *fakees)
{
    AbstractFakeEsOut *me = container_of(fakees, Private, es_out)->fake;
    me->esOutDestroy();
}

AbstractFakeEsOut::AbstractFakeEsOut()
{
    EsOutCallbacks::Private *priv = new EsOutCallbacks::Private;
    priv->fake = this;
    priv->es_out.cbs = &EsOutCallbacks::cbs;
    esoutpriv = priv;
}

AbstractFakeEsOut::~AbstractFakeEsOut()
{
    delete reinterpret_cast<EsOutCallbacks::Private *>(esoutpriv);
}

AbstractFakeEsOut::operator es_out_t *()
{
    return & reinterpret_cast<EsOutCallbacks::Private *>(esoutpriv)->es_out;
}

FakeESOut::LockedFakeEsOut::LockedFakeEsOut(FakeESOut &q)
{
    p = &q;
    vlc_mutex_lock(&p->lock);
}

FakeESOut::LockedFakeEsOut::~LockedFakeEsOut()
{
    vlc_mutex_unlock(&p->lock);
}

FakeESOut::LockedFakeEsOut::operator es_out_t*()
{
    return (es_out_t *) *p;
}

FakeESOut & FakeESOut::LockedFakeEsOut::operator *()
{
    return *p;
}

FakeESOut * FakeESOut::LockedFakeEsOut::operator ->()
{
    return p;
}

FakeESOut::FakeESOut( es_out_t *es, CommandsQueue *queue )
    : AbstractFakeEsOut()
    , real_es_out( es )
    , extrainfo( NULL )
    , commandsqueue( queue )
    , timestamps_offset( 0 )
{
    associated.b_timestamp_set = false;
    expected.b_timestamp_set = false;
    timestamp_first = 0;
    priority = ES_PRIORITY_SELECTABLE_MIN;

    vlc_mutex_init(&lock);
}

FakeESOut::LockedFakeEsOut FakeESOut::WithLock()
{
    return LockedFakeEsOut(*this);
}

CommandsQueue * FakeESOut::commandsQueue()
{
    return commandsqueue;
}

FakeESOut::~FakeESOut()
{
    recycleAll();
    gc();

    delete commandsqueue;
}

void FakeESOut::resetTimestamps()
{
    setExpectedTimestamp(-1);
    setAssociatedTimestamp(-1);
}

bool FakeESOut::getStartTimestamps( vlc_tick_t *pi_mediats, vlc_tick_t *pi_demuxts )
{
    if(!expected.b_timestamp_set)
        return false;
    *pi_demuxts = timestamp_first;
    *pi_mediats = expected.timestamp;
    return true;
}

void FakeESOut::setExpectedTimestamp(vlc_tick_t ts)
{
    if(ts < 0)
    {
        expected.b_timestamp_set = false;
        timestamps_offset = 0;
    }
    else if(!expected.b_timestamp_set)
    {
        expected.b_timestamp_set = true;
        expected.timestamp = ts;
        expected.b_offset_calculated = false;
    }
}

void FakeESOut::setAssociatedTimestamp(vlc_tick_t ts)
{
    if(ts < 0)
    {
        associated.b_timestamp_set = false;
        timestamps_offset = 0;
    }
    else if(!associated.b_timestamp_set)
    {
        associated.b_timestamp_set = true;
        associated.timestamp = ts;
        associated.b_offset_calculated = false;
    }
}

void FakeESOut::setExtraInfoProvider( ExtraFMTInfoInterface *extra )
{
    extrainfo = extra;
}

FakeESOutID * FakeESOut::createNewID( const es_format_t *p_fmt )
{
    es_format_t fmtcopy;
    es_format_Init( &fmtcopy, p_fmt->i_cat, p_fmt->i_codec );
    es_format_Copy( &fmtcopy, p_fmt );
    fmtcopy.i_group = 0; /* Always ignore group for adaptive */
    fmtcopy.i_id = -1;

    fmtcopy.i_priority = priority;

    if( extrainfo )
        extrainfo->fillExtraFMTInfo( &fmtcopy );

    FakeESOutID *es_id = new (std::nothrow) FakeESOutID( this, &fmtcopy );

    es_format_Clean( &fmtcopy );

    return es_id;
}

void FakeESOut::createOrRecycleRealEsID( FakeESOutID *es_id )
{
    std::list<FakeESOutID *>::iterator it;
    es_out_id_t *realid = NULL;

    /* declared ES must are temporary until real ES decl */
    recycle_candidates.insert(recycle_candidates.begin(), declared.begin(), declared.end());
    declared.clear();

    bool b_preexisting = false;
    bool b_select = false;
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        FakeESOutID *cand = *it;
        if ( cand->isCompatible( es_id ) )
        {
            realid = cand->realESID();
            cand->setRealESID( NULL );
            delete *it;
            recycle_candidates.erase( it );
            break;
        }
        else if( cand->getFmt()->i_cat == es_id->getFmt()->i_cat && cand->realESID() )
        {
            b_preexisting = true;
            /* We need to enforce same selection when not reused
               Otherwise the es will select any other compatible track
               and will end this in a activate/select loop when reactivating a track */
            if( !b_select )
                es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, cand->realESID(), &b_select );
        }
    }

    if( !realid )
    {
        es_format_t fmt;
        es_format_Copy( &fmt, es_id->getFmt() );
        if( b_preexisting && !b_select ) /* was not previously selected on other format */
            fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;
        else
            fmt.i_priority = priority;//ES_PRIORITY_SELECTABLE_MIN;
        realid = es_out_Add( real_es_out, &fmt );
        if( b_preexisting && b_select ) /* was previously selected on other format */
            es_out_Control( real_es_out, ES_OUT_SET_ES, realid );
        es_format_Clean( &fmt );
    }

    es_id->setRealESID( realid );
}

void FakeESOut::setPriority(int p)
{
    priority = p;
}

size_t FakeESOut::esCount() const
{
    if(!declared.empty())
        return declared.size();

    size_t i_count = 0;
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
        if( (*it)->realESID() )
            i_count++;
    return i_count;
}

void FakeESOut::schedulePCRReset()
{
    AbstractCommand *command = commandsqueue->factory()->creatEsOutControlResetPCRCommand();
    if( likely(command) )
        commandsqueue->Schedule( command );
}

void FakeESOut::scheduleAllForDeletion()
{
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
    {
        FakeESOutID *es_id = *it;
        if(!es_id->scheduledForDeletion())
        {
            AbstractCommand *command = commandsqueue->factory()->createEsOutDelCommand( es_id );
            if( likely(command) )
            {
                commandsqueue->Schedule( command );
                es_id->setScheduledForDeletion();
            }
        }
    }
}

void FakeESOut::recycleAll()
{
    /* Only used when demux is killed and commands queue is cancelled */
    commandsqueue->Abort( true );
    assert(commandsqueue->isEmpty());
    recycle_candidates.splice( recycle_candidates.end(), fakeesidlist );
}

void FakeESOut::gc()
{
    recycle_candidates.insert(recycle_candidates.begin(), declared.begin(), declared.end());
    declared.clear();

    if( recycle_candidates.empty() )
    {
        return;
    }

    std::list<FakeESOutID *>::iterator it;
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        if( (*it)->realESID() )
        {
            es_out_Control( real_es_out, ES_OUT_SET_ES_STATE, (*it)->realESID(), false );
            es_out_Del( real_es_out, (*it)->realESID() );
        }
        delete *it;
    }
    recycle_candidates.clear();
}

bool FakeESOut::hasSelectedEs() const
{
    bool b_selected = false;
    std::list<FakeESOutID *> const * lists[2] = {&declared, &fakeesidlist};
    std::list<FakeESOutID *>::const_iterator it;
    for(int i=0; i<2; i++)
    {
        for( it=lists[i]->begin(); it!=lists[i]->end() && !b_selected; ++it )
        {
            FakeESOutID *esID = *it;
            if( esID->realESID() )
                es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, esID->realESID(), &b_selected );
        }
    }
    return b_selected;
}

bool FakeESOut::decodersDrained()
{
    bool b_empty = true;
    es_out_Control( real_es_out, ES_OUT_GET_EMPTY, &b_empty );
    return b_empty;
}

bool FakeESOut::restarting() const
{
    bool b = !recycle_candidates.empty();
    return b;
}

void FakeESOut::recycle( FakeESOutID *id )
{
    fakeesidlist.remove( id );
    recycle_candidates.push_back( id );
}

vlc_tick_t FakeESOut::fixTimestamp(vlc_tick_t ts)
{
    if(ts != VLC_TICK_INVALID)
    {
        if(associated.b_timestamp_set)
        {
            /* Some streams (ex: HLS) have a timestamp mapping
               embedded in some header or metadata (ID3 crap for HLS).
               In that case it needs to remap original timestamp. */
            if(!associated.b_offset_calculated)
            {
                timestamps_offset = associated.timestamp - ts;
                associated.b_offset_calculated = true;
                timestamp_first = ts + timestamps_offset;
            }
        }
        else if(expected.b_timestamp_set)
        {
            /* Some streams (ex: smooth mp4 without TFDT)
             * do not have proper timestamps and will always start 0.
             * In that case we need to enforce playlist time */
            if(!expected.b_offset_calculated)
            {
                if(ts < VLC_TICK_FROM_SEC(1)) /* Starting 0 */
                    timestamps_offset = expected.timestamp - ts;
                else
                    timestamps_offset = 0;
                expected.b_offset_calculated = true;
                timestamp_first = ts + timestamps_offset;
            }
        }
        ts += timestamps_offset;
    }
    return ts;
}

void FakeESOut::declareEs(const es_format_t *fmt)
{
    /* Declared ES are only visible until stream data flows.
       They are then recycled to create the real ES. */
    if(!recycle_candidates.empty() || !fakeesidlist.empty())
    {
        assert(recycle_candidates.empty());
        assert(fakeesidlist.empty());
        return;
    }

    FakeESOutID *fakeid = createNewID(fmt);
    if( likely(fakeid) )
    {
        es_out_id_t *realid = es_out_Add( real_es_out, fakeid->getFmt() );
        if( likely(realid) )
        {
            fakeid->setRealESID(realid);
            declared.push_front(fakeid);
        }
        else delete fakeid;
    }
}

/* Static callbacks */
/* Always pass Fake ES ID to slave demuxes, it is just an opaque struct to them */
es_out_id_t * FakeESOut::esOutAdd(const es_format_t *p_fmt)
{
    vlc_mutex_locker locker(&lock);

    if( p_fmt->i_cat != VIDEO_ES && p_fmt->i_cat != AUDIO_ES && p_fmt->i_cat != SPU_ES )
        return NULL;

    /* Feed the slave demux/stream_Demux with FakeESOutID struct,
     * we'll create real ES later on main demux on execution */
    FakeESOutID *es_id = createNewID( p_fmt );
    if( likely(es_id) )
    {
        assert(!es_id->scheduledForDeletion());
        AbstractCommand *command = commandsqueue->factory()->createEsOutAddCommand( es_id );
        if( likely(command) )
        {
            fakeesidlist.push_back(es_id);
            commandsqueue->Schedule( command );
            return reinterpret_cast<es_out_id_t *>(es_id);
        }
        else
        {
            delete es_id;
        }
    }
    return NULL;
}

int FakeESOut::esOutSend(es_out_id_t *p_es, block_t *p_block)
{
    vlc_mutex_locker locker(&lock);

    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    assert(!es_id->scheduledForDeletion());

    p_block->i_dts = fixTimestamp( p_block->i_dts );
    p_block->i_pts = fixTimestamp( p_block->i_pts );

    AbstractCommand *command = commandsqueue->factory()->createEsOutSendCommand( es_id, p_block );
    if( likely(command) )
    {
        commandsqueue->Schedule( command );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

void FakeESOut::esOutDel(es_out_id_t *p_es)
{
    vlc_mutex_locker locker(&lock);

    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    AbstractCommand *command = commandsqueue->factory()->createEsOutDelCommand( es_id );
    if( likely(command) )
    {
        es_id->setScheduledForDeletion();
        commandsqueue->Schedule( command );
    }
}

int FakeESOut::esOutControl(int i_query, va_list args)
{
    vlc_mutex_locker locker(&lock);

    switch( i_query )
    {
        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            int i_group;
            if( i_query == ES_OUT_SET_GROUP_PCR )
                i_group = va_arg( args, int );
            else
                i_group = 0;
            vlc_tick_t  pcr = va_arg( args, vlc_tick_t );
            pcr = fixTimestamp( pcr );
            AbstractCommand *command = commandsqueue->factory()->createEsOutControlPCRCommand( i_group, pcr );
            if( likely(command) )
            {
                commandsqueue->Schedule( command );
                return VLC_SUCCESS;
            }
        }
        break;

        case ES_OUT_SET_GROUP_META:
        {
            static_cast<void>(va_arg( args, int )); /* ignore group */
            const vlc_meta_t *p_meta = va_arg( args, const vlc_meta_t * );
            AbstractCommand *command = commandsqueue->factory()->createEsOutMetaCommand( -1, p_meta );
            if( likely(command) )
            {
                commandsqueue->Schedule( command );
                return VLC_SUCCESS;
            }
        }
        break;

        /* For others, we don't have the delorean, so always lie */
        case ES_OUT_GET_ES_STATE:
        {
            static_cast<void>(va_arg( args, es_out_id_t * ));
            bool *pb = va_arg( args, bool * );
            *pb = true;
            return VLC_SUCCESS;
        }

        case ES_OUT_SET_ES:
        case ES_OUT_SET_ES_DEFAULT:
        case ES_OUT_SET_ES_STATE:
            return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

void FakeESOut::esOutDestroy()
{
    vlc_mutex_locker locker(&lock);

    AbstractCommand *command = commandsqueue->factory()->createEsOutDestroyCommand();
    if( likely(command) )
        commandsqueue->Schedule( command );
}
/* !Static callbacks */
