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

FakeESOut::FakeESOut( es_out_t *es, CommandsQueue *queue )
    : real_es_out( es )
    , extrainfo( NULL )
    , commandsqueue( queue )
    , fakeesout( new es_out_t )
    , timestamps_offset( 0 )
    , timestamps_expected( 0 )
    , timestamps_check_done( false )
{
    fakeesout->pf_add = esOutAdd_Callback;
    fakeesout->pf_control = esOutControl_Callback;
    fakeesout->pf_del = esOutDel_Callback;
    fakeesout->pf_destroy = esOutDestroy_Callback;
    fakeesout->pf_send = esOutSend_Callback;
    fakeesout->p_sys = (es_out_sys_t*) this;

    vlc_mutex_init(&lock);
}

es_out_t * FakeESOut::getEsOut()
{
    return fakeesout;
}

FakeESOut::~FakeESOut()
{
    recycleAll();
    gc();

    delete fakeesout;
    vlc_mutex_destroy(&lock);
}

void FakeESOut::setExpectedTimestampOffset(mtime_t offset)
{
    vlc_mutex_lock(&lock);
    timestamps_offset = 0;
    timestamps_expected = offset;
    timestamps_check_done = false;
    vlc_mutex_unlock(&lock);
}

void FakeESOut::setTimestampOffset(mtime_t offset)
{
    vlc_mutex_lock(&lock);
    timestamps_offset = offset;
    timestamps_check_done = true;
    vlc_mutex_unlock(&lock);
}

void FakeESOut::setExtraInfoProvider( ExtraFMTInfoInterface *extra )
{
    vlc_mutex_lock(&lock);
    extrainfo = extra;
    vlc_mutex_unlock(&lock);
}

FakeESOutID * FakeESOut::createNewID( const es_format_t *p_fmt )
{
    es_format_t fmtcopy;
    es_format_Init( &fmtcopy, p_fmt->i_cat, p_fmt->i_codec );
    es_format_Copy( &fmtcopy, p_fmt );
    fmtcopy.i_group = 0; /* Always ignore group for adaptive */
    fmtcopy.i_id = -1;

    vlc_mutex_lock(&lock);

    if( extrainfo )
        extrainfo->fillExtraFMTInfo( &fmtcopy );

    FakeESOutID *es_id = new (std::nothrow) FakeESOutID( this, &fmtcopy );
    if(likely(es_id))
        fakeesidlist.push_back( es_id );

    vlc_mutex_unlock(&lock);

    es_format_Clean( &fmtcopy );

    return es_id;
}

void FakeESOut::createOrRecycleRealEsID( FakeESOutID *es_id )
{
    std::list<FakeESOutID *>::iterator it;
    es_out_id_t *realid = NULL;

    vlc_mutex_lock(&lock);

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
            /* We need to enforce same selection when not reused
               Otherwise the es will select any other compatible track
               and will end this in a activate/select loop when reactivating a track */
            es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, cand->realESID(), &b_select );
            break;
        }
    }

    if( !realid )
    {
        realid = es_out_Add( real_es_out, es_id->getFmt() );
        if( b_select )
            es_out_Control( real_es_out, ES_OUT_SET_ES, realid );
    }

    es_id->setRealESID( realid );

    vlc_mutex_unlock(&lock);
}

mtime_t FakeESOut::getTimestampOffset() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    mtime_t time = timestamps_offset;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return time;
}

size_t FakeESOut::esCount() const
{
    size_t i_count = 0;
    std::list<FakeESOutID *>::const_iterator it;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
        if( (*it)->realESID() )
            i_count++;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
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
    vlc_mutex_lock(&lock);
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
    vlc_mutex_unlock(&lock);
}

void FakeESOut::recycleAll()
{
    /* Only used when demux is killed and commands queue is cancelled */
    commandsqueue->Abort( true );
    assert(commandsqueue->isEmpty());
    vlc_mutex_lock(&lock);
    recycle_candidates.splice( recycle_candidates.end(), fakeesidlist );
    vlc_mutex_unlock(&lock);
}

void FakeESOut::gc()
{
    vlc_mutex_lock(&lock);
    if( recycle_candidates.empty() )
    {
        vlc_mutex_unlock(&lock);
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
    vlc_mutex_unlock(&lock);
}

bool FakeESOut::hasSelectedEs() const
{
    bool b_selected = false;
    std::list<FakeESOutID *>::const_iterator it;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end() && !b_selected; ++it )
    {
        FakeESOutID *esID = *it;
        if( esID->realESID() )
            es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, esID->realESID(), &b_selected );
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
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
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    bool b = !recycle_candidates.empty();
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b;
}

void FakeESOut::recycle( FakeESOutID *id )
{
    vlc_mutex_lock(&lock);
    fakeesidlist.remove( id );
    recycle_candidates.push_back( id );
    vlc_mutex_unlock(&lock);
}

/* Static callbacks */
/* Always pass Fake ES ID to slave demuxes, it is just an opaque struct to them */
es_out_id_t * FakeESOut::esOutAdd_Callback(es_out_t *fakees, const es_format_t *p_fmt)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;

    if( p_fmt->i_cat != VIDEO_ES && p_fmt->i_cat != AUDIO_ES && p_fmt->i_cat != SPU_ES )
        return NULL;

    /* Feed the slave demux/stream_Demux with FakeESOutID struct,
     * we'll create real ES later on main demux on execution */
    FakeESOutID *es_id = me->createNewID( p_fmt );
    if( likely(es_id) )
    {
        assert(!es_id->scheduledForDeletion());
        AbstractCommand *command = me->commandsqueue->factory()->createEsOutAddCommand( es_id );
        if( likely(command) )
        {
            me->commandsqueue->Schedule( command );
            return reinterpret_cast<es_out_id_t *>(es_id);
        }
        else
        {
            delete es_id;
        }
    }
    return NULL;
}

void FakeESOut::checkTimestampsStart(mtime_t i_start)
{
    if( i_start == VLC_TS_INVALID )
        return;

    vlc_mutex_lock(&lock);
    if( !timestamps_check_done )
    {
        if( i_start < CLOCK_FREQ ) /* Starts 0 */
            timestamps_offset = timestamps_expected;
        timestamps_check_done = true;
    }
    vlc_mutex_unlock(&lock);
}

int FakeESOut::esOutSend_Callback(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    assert(!es_id->scheduledForDeletion());

    me->checkTimestampsStart( p_block->i_dts );

    mtime_t offset = me->getTimestampOffset();
    if( p_block->i_dts > VLC_TS_INVALID )
    {
        p_block->i_dts += offset;
        if( p_block->i_pts > VLC_TS_INVALID )
                p_block->i_pts += offset;
    }
    AbstractCommand *command = me->commandsqueue->factory()->createEsOutSendCommand( es_id, p_block );
    if( likely(command) )
    {
        me->commandsqueue->Schedule( command );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

void FakeESOut::esOutDel_Callback(es_out_t *fakees, es_out_id_t *p_es)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    AbstractCommand *command = me->commandsqueue->factory()->createEsOutDelCommand( es_id );
    if( likely(command) )
    {
        es_id->setScheduledForDeletion();
        me->commandsqueue->Schedule( command );
    }
}

int FakeESOut::esOutControl_Callback(es_out_t *fakees, int i_query, va_list args)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;

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
            int64_t  pcr = va_arg( args, int64_t );
            me->checkTimestampsStart( pcr );
            pcr += me->getTimestampOffset();
            AbstractCommand *command = me->commandsqueue->factory()->createEsOutControlPCRCommand( i_group, pcr );
            if( likely(command) )
            {
                me->commandsqueue->Schedule( command );
                return VLC_SUCCESS;
            }
        }
        break;

        case ES_OUT_SET_GROUP_META:
        {
            static_cast<void>(va_arg( args, int )); /* ignore group */
            const vlc_meta_t *p_meta = va_arg( args, const vlc_meta_t * );
            AbstractCommand *command = me->commandsqueue->factory()->createEsOutMetaCommand( -1, p_meta );
            if( likely(command) )
            {
                me->commandsqueue->Schedule( command );
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

void FakeESOut::esOutDestroy_Callback(es_out_t *fakees)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    AbstractCommand *command = me->commandsqueue->factory()->createEsOutDestroyCommand();
    if( likely(command) )
        me->commandsqueue->Schedule( command );
}
/* !Static callbacks */
