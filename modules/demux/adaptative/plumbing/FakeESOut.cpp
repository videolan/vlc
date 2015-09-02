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
#include "FakeESOut.hpp"
#include "FakeESOutID.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>

using namespace adaptative;

FakeESOut::FakeESOut( es_out_t *es, CommandsFactory *factory )
{
    real_es_out = es;
    fakeesout = new es_out_t;

    fakeesout->pf_add = esOutAdd_Callback;
    fakeesout->pf_control = esOutControl_Callback;
    fakeesout->pf_del = esOutDel_Callback;
    fakeesout->pf_destroy = esOutDestroy_Callback;
    fakeesout->pf_send = esOutSend_Callback;
    fakeesout->p_sys = (es_out_sys_t*) this;

    commandsFactory = factory;
    timestamps_offset = 0;
    extrainfo = NULL;

    vlc_mutex_init( &lock );
}

es_out_t * FakeESOut::getEsOut()
{
    return fakeesout;
}

FakeESOut::~FakeESOut()
{
    delete commandsFactory;

    recycleAll();
    gc();

    free( fakeesout );

    vlc_mutex_destroy( &lock );
}

void FakeESOut::setTimestampOffset(mtime_t offset)
{
    vlc_mutex_lock(&lock);
    timestamps_offset = offset;
    vlc_mutex_unlock(&lock);
}

void FakeESOut::setExtraInfoProvider( ExtraFMTInfoInterface *extra )
{
    extrainfo = extra;
}

FakeESOutID * FakeESOut::createOrRecycle( const es_format_t *p_fmt )
{
    FakeESOutID *es_id = NULL;
    std::list<FakeESOutID *>::iterator it;

    es_format_t fmtcopy;
    es_format_Init( &fmtcopy, 0, 0 );
    es_format_Copy( &fmtcopy, p_fmt );

    if( extrainfo )
        extrainfo->fillExtraFMTInfo( &fmtcopy );

    vlc_mutex_lock( &lock );
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        if ( (*it)->isCompatible( &fmtcopy ) )
        {
            es_id = *it;
            recycle_candidates.erase( it );
            break;
        }
    }
    vlc_mutex_unlock( &lock );

    if( es_id == NULL )
        es_id = new (std::nothrow) FakeESOutID( this, &fmtcopy );

    if( es_id )
        fakeesidlist.push_back( es_id );

    es_format_Clean( &fmtcopy );

    return es_id;
}

mtime_t FakeESOut::getTimestampOffset() const
{
    mtime_t offset;
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    offset = timestamps_offset;
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return offset;
}

size_t FakeESOut::esCount() const
{
    size_t i_count = 0;
    std::list<FakeESOutID *>::const_iterator it;
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
        if( (*it)->realESID() )
            i_count++;
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return i_count;
}
void FakeESOut::recycleAll()
{
    vlc_mutex_lock( &lock );
    recycle_candidates.splice( recycle_candidates.end(), fakeesidlist );
    vlc_mutex_unlock( &lock );
}

void FakeESOut::gc()
{
    if( recycle_candidates.empty() )
        return;

    vlc_mutex_lock( &lock );
    std::list<FakeESOutID *>::iterator it;
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        if( (*it)->realESID() )
            es_out_Del( real_es_out, (*it)->realESID() );
        delete *it;
    }
    recycle_candidates.clear();
    vlc_mutex_unlock( &lock );
}

bool FakeESOut::hasSelectedEs() const
{
    bool b_selected = false;
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end() && !b_selected; ++it )
    {
        FakeESOutID *esID = *it;
        if( esID->realESID() )
            es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, esID->realESID(), &b_selected );
    }
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return b_selected;
}

bool FakeESOut::restarting() const
{
    return !recycle_candidates.empty();
}

void FakeESOut::recycle( FakeESOutID *id )
{
    vlc_mutex_lock( &lock );
    fakeesidlist.remove( id );
    recycle_candidates.push_back( id );
    vlc_mutex_unlock( &lock );
}

/* Static callbacks */
/* Always pass Fake ES ID to slave demuxes, it is just an opaque struct to them */
es_out_id_t * FakeESOut::esOutAdd_Callback(es_out_t *fakees, const es_format_t *p_fmt)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    /* Feed the slave demux/stream_Demux with FakeESOutID struct,
     * we'll create real ES later on main demux on execution */
    FakeESOutID *es_id = me->createOrRecycle( p_fmt );
    if( likely(es_id) )
    {
        AbstractCommand *command = me->commandsFactory->createEsOutAddCommand( es_id, p_fmt );
        if( likely(command) )
        {
            me->commandsqueue.Schedule( command );
            return reinterpret_cast<es_out_id_t *>(es_id);
        }
        else
        {
            delete es_id;
        }
    }
    return NULL;
}

int FakeESOut::esOutSend_Callback(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    mtime_t offset = me->getTimestampOffset();
    if( p_block->i_dts > VLC_TS_INVALID )
    {
        p_block->i_dts += offset;
        if( p_block->i_pts > VLC_TS_INVALID )
                p_block->i_pts += offset;
    }
    AbstractCommand *command = me->commandsFactory->createEsOutSendCommand( es_id, p_block );
    if( likely(command) )
    {
        me->commandsqueue.Schedule( command );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

void FakeESOut::esOutDel_Callback(es_out_t *fakees, es_out_id_t *p_es)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    AbstractCommand *command = me->commandsFactory->createEsOutDelCommand( es_id );
    if( likely(command) )
        me->commandsqueue.Schedule( command );
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
                i_group = static_cast<int>(va_arg( args, int ));
            else
                i_group = 0;
            int64_t  pcr = static_cast<int64_t>(va_arg( args, int64_t ));
            pcr += me->getTimestampOffset();
            AbstractCommand *command = me->commandsFactory->createEsOutControlPCRCommand( i_group, pcr );
            if( likely(command) )
            {
                me->commandsqueue.Schedule( command );
                return VLC_SUCCESS;
            }
        }
        break;

        /* For others, we don't have the delorean, so always lie */
        case ES_OUT_GET_ES_STATE:
        {
            static_cast<void*>(va_arg( args, es_out_id_t * ));
            bool *pb = static_cast<bool *>(va_arg( args, bool * ));
            *pb = true;
            // ft
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
    AbstractCommand *command = me->commandsFactory->createEsOutDestroyCommand();
    if( likely(command) )
        me->commandsqueue.Schedule( command );
}
/* !Static callbacks */
