/*
 * CommandsQueue.cpp
 *****************************************************************************
 * Copyright © 2015 VideoLAN and VLC Authors
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
#include "CommandsQueue.hpp"
#include "FakeESOutID.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <algorithm>

using namespace adaptative;

enum
{
    ES_OUT_PRIVATE_COMMAND_ADD = ES_OUT_PRIVATE_START,
    ES_OUT_PRIVATE_COMMAND_DEL,
    ES_OUT_PRIVATE_COMMAND_DESTROY,
    ES_OUT_PRIVATE_COMMAND_SEND
};

AbstractCommand::AbstractCommand( int type_ )
{
    type = type_;
}

AbstractCommand::~AbstractCommand()
{

}

mtime_t AbstractCommand::getTime() const
{
    return VLC_TS_INVALID;
}

int AbstractCommand::getType() const
{
    return type;
}

EsOutSendCommand::EsOutSendCommand( FakeESOutID *p_es, block_t *p_block_ ) :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_SEND )
{
    p_fakeid = p_es;
    p_block = p_block_;
}

EsOutSendCommand::~EsOutSendCommand()
{
    if( p_block )
        block_Release( p_block );
}

void EsOutSendCommand::Execute( es_out_t *out )
{
    /* Be sure to notify Data before Sending, because UI would still not pick new ES */
    p_fakeid->notifyData();
    if( p_fakeid->realESID() &&
            es_out_Send( out, p_fakeid->realESID(), p_block ) == VLC_SUCCESS )
        p_block = NULL;
    p_fakeid->notifyData();
}

mtime_t EsOutSendCommand::getTime() const
{
    if( likely(p_block) )
        return p_block->i_dts;
    else
        return AbstractCommand::getTime();
}

EsOutDelCommand::EsOutDelCommand( FakeESOutID *p_es ) :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_DEL )
{
    p_fakeid = p_es;
}

void EsOutDelCommand::Execute( es_out_t * )
{
    p_fakeid->release();
}

EsOutAddCommand::EsOutAddCommand( FakeESOutID *p_es, const es_format_t *p_fmt ) :
    EsOutDelCommand( p_es )
{
    es_format_Copy( &fmt, p_fmt );
    fmt.i_group = 0; /* Always ignore group for adaptative */
    type = ES_OUT_PRIVATE_COMMAND_ADD;
}

EsOutAddCommand::~EsOutAddCommand()
{
    es_format_Clean( &fmt );
}

void EsOutAddCommand::Execute( es_out_t *out )
{
    /* Create the real ES on the adaptative demux */
    if( p_fakeid->realESID() == NULL )
    {
        es_out_id_t *realid = es_out_Add( out, &fmt );
        if( likely(realid) )
            p_fakeid->setRealESID( realid );
    }
}

EsOutControlPCRCommand::EsOutControlPCRCommand( int group_, mtime_t pcr_ ) :
    AbstractCommand( ES_OUT_SET_GROUP_PCR )
{
    group = group_;
    pcr = pcr_;
    type = ES_OUT_SET_GROUP_PCR;
}

void EsOutControlPCRCommand::Execute( es_out_t * )
{
    // do nothing here
}

mtime_t EsOutControlPCRCommand::getTime() const
{
    return pcr;
}

EsOutDestroyCommand::EsOutDestroyCommand() :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_DESTROY )
{
}

void EsOutDestroyCommand::Execute( es_out_t *out )
{
    es_out_Delete( out );
}

/*
 * Commands Default Factory
 */

EsOutSendCommand * CommandsFactory::createEsOutSendCommand( FakeESOutID *id, block_t *p_block )
{
    return new (std::nothrow) EsOutSendCommand( id, p_block );
}

EsOutDelCommand * CommandsFactory::createEsOutDelCommand( FakeESOutID *id )
{
    return new (std::nothrow) EsOutDelCommand( id );
}

EsOutAddCommand * CommandsFactory::createEsOutAddCommand( FakeESOutID *id, const es_format_t *fmt )
{
    return new (std::nothrow) EsOutAddCommand( id, fmt );
}

EsOutControlPCRCommand * CommandsFactory::createEsOutControlPCRCommand( int group, mtime_t pcr )
{
    return new (std::nothrow) EsOutControlPCRCommand( group, pcr );
}

EsOutDestroyCommand * CommandsFactory::createEsOutDestroyCommand()
{
    return new (std::nothrow) EsOutDestroyCommand();
}

/*
 * Commands Queue management
 */
CommandsQueue::CommandsQueue()
{
    bufferinglevel = VLC_TS_INVALID;
    b_drop = false;
    vlc_mutex_init( &lock );
}

CommandsQueue::~CommandsQueue()
{
    Abort( false );
    vlc_mutex_destroy( &lock );
}

static bool compareCommands( AbstractCommand *a, AbstractCommand *b )
{
    return (a->getTime() < b->getTime() && a->getTime() != VLC_TS_INVALID);
}

void CommandsQueue::Schedule( AbstractCommand *command )
{
    vlc_mutex_lock( &lock );
    if( b_drop )
    {
        delete command;
    }
    else if( command->getType() == ES_OUT_SET_GROUP_PCR )
    {
        bufferinglevel = command->getTime();
        FlushLocked();
        commands.push_back( command );
    }
    else
    {
        incoming.push_back( command );
    }
    vlc_mutex_unlock( &lock );
}

void CommandsQueue::Process( es_out_t *out, mtime_t barrier )
{
    vlc_mutex_lock( &lock );
    while( !commands.empty() && commands.front()->getTime() <= barrier )
    {
        AbstractCommand *command = commands.front();
        commands.pop_front();
        command->Execute( out );
        delete command;
    }
    vlc_mutex_unlock( &lock );
}

void CommandsQueue::FlushLocked()
{
    /* reorder all blocks by time between 2 PCR and merge with main list */
    incoming.sort( compareCommands );
    commands.splice( commands.end(), incoming );
}

void CommandsQueue::Flush()
{
    vlc_mutex_lock( &lock );
    FlushLocked();
    vlc_mutex_unlock( &lock );
}

void CommandsQueue::Abort( bool b_reset )
{
    vlc_mutex_lock( &lock );
    commands.splice( commands.end(), incoming );
    while( !commands.empty() )
    {
        delete commands.front();
        commands.pop_front();
    }

    if( b_reset )
        bufferinglevel = VLC_TS_INVALID;
    vlc_mutex_unlock( &lock );
}

bool CommandsQueue::isEmpty() const
{
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    bool b_empty = commands.empty() && incoming.empty();
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return b_empty;
}

void CommandsQueue::setDrop( bool b )
{
    vlc_mutex_lock( &lock );
    b_drop = b;
    vlc_mutex_unlock( &lock );
}

mtime_t CommandsQueue::getBufferingLevel() const
{
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    mtime_t buffer = bufferinglevel;
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return buffer;
}

mtime_t CommandsQueue::getFirstDTS() const
{
    mtime_t i_dts = VLC_TS_INVALID;
    vlc_mutex_lock( const_cast<vlc_mutex_t *>(&lock) );
    std::list<AbstractCommand *>::const_iterator it;
    for( it = commands.begin(); it != commands.end(); ++it )
    {
        if( (*it)->getTime() > VLC_TS_INVALID )
        {
            i_dts = (*it)->getTime();
            break;
        }
    }
    vlc_mutex_unlock( const_cast<vlc_mutex_t *>(&lock) );
    return i_dts;
}
