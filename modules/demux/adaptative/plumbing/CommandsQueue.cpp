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
#include "FakeESOut.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <algorithm>

using namespace adaptative;

enum
{
    ES_OUT_PRIVATE_COMMAND_ADD = ES_OUT_PRIVATE_START,
    ES_OUT_PRIVATE_COMMAND_DEL,
    ES_OUT_PRIVATE_COMMAND_DESTROY,
    ES_OUT_PRIVATE_COMMAND_SEND,
    ES_OUT_PRIVATE_COMMAND_DISCONTINUITY
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

AbstractFakeEsCommand::AbstractFakeEsCommand( int type, FakeESOutID *p_es ) :
    AbstractCommand( type )
{
    p_fakeid = p_es;
}

EsOutSendCommand::EsOutSendCommand( FakeESOutID *p_es, block_t *p_block_ ) :
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_SEND, p_es )
{
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
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_DEL, p_es )
{
}

void EsOutDelCommand::Execute( es_out_t * )
{
    p_fakeid->release();
}

EsOutAddCommand::EsOutAddCommand( FakeESOutID *p_es ) :
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_ADD, p_es )
{
}

EsOutAddCommand::~EsOutAddCommand()
{
}

void EsOutAddCommand::Execute( es_out_t * )
{
    /* Create the real ES on the adaptative demux */
    p_fakeid->create();
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

void EsOutDestroyCommand::Execute( es_out_t * )
{
}

EsOutControlResetPCRCommand::EsOutControlResetPCRCommand() :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_DISCONTINUITY )
{
}

void EsOutControlResetPCRCommand::Execute( es_out_t * )
{
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

EsOutAddCommand * CommandsFactory::createEsOutAddCommand( FakeESOutID *id )
{
    return new (std::nothrow) EsOutAddCommand( id );
}

EsOutControlPCRCommand * CommandsFactory::createEsOutControlPCRCommand( int group, mtime_t pcr )
{
    return new (std::nothrow) EsOutControlPCRCommand( group, pcr );
}

EsOutDestroyCommand * CommandsFactory::createEsOutDestroyCommand()
{
    return new (std::nothrow) EsOutDestroyCommand();
}

EsOutControlResetPCRCommand * CommandsFactory::creatEsOutControlResetPCRCommand()
{
    return new (std::nothrow) EsOutControlResetPCRCommand();
}

/*
 * Commands Queue management
 */
CommandsQueue::CommandsQueue()
{
    bufferinglevel = VLC_TS_INVALID;
    b_drop = false;
}

CommandsQueue::~CommandsQueue()
{
    Abort( false );
}

static bool compareCommands( AbstractCommand *a, AbstractCommand *b )
{
    return (a->getTime() < b->getTime() && a->getTime() != VLC_TS_INVALID);
}

void CommandsQueue::Schedule( AbstractCommand *command )
{
    if( b_drop )
    {
        delete command;
    }
    else if( command->getType() == ES_OUT_SET_GROUP_PCR )
    {
        bufferinglevel = command->getTime();
        Commit();
        commands.push_back( command );
    }
    else
    {
        incoming.push_back( command );
    }
}

mtime_t CommandsQueue::Process( es_out_t *out, mtime_t barrier )
{
    mtime_t lastdts = barrier;
    bool b_datasent = false;

    while( !commands.empty() && commands.front()->getTime() <= barrier )
    {
        AbstractCommand *command = commands.front();
        /* We need to have PCR set for stream before Deleting ES,
         * or it will get stuck if any waiting decoder buffer */
        if(command->getType() == ES_OUT_PRIVATE_COMMAND_DEL && b_datasent)
            break;

        if(command->getType() == ES_OUT_PRIVATE_COMMAND_DISCONTINUITY && b_datasent)
            break;

        if(command->getType() == ES_OUT_PRIVATE_COMMAND_SEND)
        {
            lastdts = command->getTime();
            b_datasent = true;
        }

        commands.pop_front();
        command->Execute( out );
        delete command;
    }
    return lastdts;
}

void CommandsQueue::Commit()
{
    /* reorder all blocks by time between 2 PCR and merge with main list */
    incoming.sort( compareCommands );
    commands.splice( commands.end(), incoming );
}

void CommandsQueue::Abort( bool b_reset )
{
    commands.splice( commands.end(), incoming );
    while( !commands.empty() )
    {
        delete commands.front();
        commands.pop_front();
    }

    if( b_reset )
        bufferinglevel = VLC_TS_INVALID;
}

bool CommandsQueue::isEmpty() const
{
    return commands.empty() && incoming.empty();
}

void CommandsQueue::setDrop( bool b )
{
    b_drop = b;
}

mtime_t CommandsQueue::getBufferingLevel() const
{
    return bufferinglevel;
}

mtime_t CommandsQueue::getFirstDTS() const
{
    mtime_t i_dts = VLC_TS_INVALID;
    std::list<AbstractCommand *>::const_iterator it;
    for( it = commands.begin(); it != commands.end(); ++it )
    {
        if( (*it)->getTime() > VLC_TS_INVALID )
        {
            i_dts = (*it)->getTime();
            break;
        }
    }
    return i_dts;
}
