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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "CommandsQueue.hpp"
#include "FakeESOutID.hpp"
#include "FakeESOut.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <vlc_meta.h>
#include <algorithm>
#include <set>

using namespace adaptive;

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

const void * EsOutSendCommand::esIdentifier() const
{
    return static_cast<const void *>(p_fakeid);
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
    /* Create the real ES on the adaptive demux */
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

EsOutMetaCommand::EsOutMetaCommand( int i_group, vlc_meta_t *p_meta ) :
    AbstractCommand( ES_OUT_SET_GROUP_META )
{
    group = i_group;
    this->p_meta = p_meta;
}

EsOutMetaCommand::~EsOutMetaCommand()
{
    if( p_meta )
        vlc_meta_Delete( p_meta );
}

void EsOutMetaCommand::Execute( es_out_t *out )
{
    es_out_Control( out, ES_OUT_SET_GROUP_META, group, p_meta );
}

/*
 * Commands Default Factory
 */

EsOutSendCommand * CommandsFactory::createEsOutSendCommand( FakeESOutID *id, block_t *p_block ) const
{
    return new (std::nothrow) EsOutSendCommand( id, p_block );
}

EsOutDelCommand * CommandsFactory::createEsOutDelCommand( FakeESOutID *id ) const
{
    return new (std::nothrow) EsOutDelCommand( id );
}

EsOutAddCommand * CommandsFactory::createEsOutAddCommand( FakeESOutID *id ) const
{
    return new (std::nothrow) EsOutAddCommand( id );
}

EsOutControlPCRCommand * CommandsFactory::createEsOutControlPCRCommand( int group, mtime_t pcr ) const
{
    return new (std::nothrow) EsOutControlPCRCommand( group, pcr );
}

EsOutDestroyCommand * CommandsFactory::createEsOutDestroyCommand() const
{
    return new (std::nothrow) EsOutDestroyCommand();
}

EsOutControlResetPCRCommand * CommandsFactory::creatEsOutControlResetPCRCommand() const
{
    return new (std::nothrow) EsOutControlResetPCRCommand();
}

EsOutMetaCommand * CommandsFactory::createEsOutMetaCommand( int group, const vlc_meta_t *p_meta ) const
{
    vlc_meta_t *p_dup = vlc_meta_New();
    if( p_dup )
    {
        vlc_meta_Merge( p_dup, p_meta );
        return new (std::nothrow) EsOutMetaCommand( group, p_dup );
    }
    return NULL;
}

/*
 * Commands Queue management
 */
CommandsQueue::CommandsQueue( CommandsFactory *f )
{
    bufferinglevel = VLC_TS_INVALID;
    pcr = VLC_TS_INVALID;
    b_drop = false;
    b_draining = false;
    b_eof = false;
    commandsFactory = f;
    vlc_mutex_init(&lock);
}

CommandsQueue::~CommandsQueue()
{
    Abort( false );
    delete commandsFactory;
    vlc_mutex_destroy(&lock);
}

static bool compareCommands( AbstractCommand *a, AbstractCommand *b )
{
    return (a->getTime() < b->getTime() && a->getTime() != VLC_TS_INVALID);
}

void CommandsQueue::Schedule( AbstractCommand *command )
{
    vlc_mutex_lock(&lock);
    if( b_drop )
    {
        delete command;
    }
    else if( command->getType() == ES_OUT_SET_GROUP_PCR )
    {
        bufferinglevel = command->getTime();
        LockedCommit();
        commands.push_back( command );
    }
    else
    {
        incoming.push_back( command );
    }
    vlc_mutex_unlock(&lock);
}

const CommandsFactory * CommandsQueue::factory() const
{
    return commandsFactory;
}

mtime_t CommandsQueue::Process( es_out_t *out, mtime_t barrier )
{
    mtime_t lastdts = barrier;
    std::set<const void *> disabled_esids;
    bool b_datasent = false;

    /* We need to filter the current commands list
       We need to return on discontinuity or reset events if data was sent
       We must lookup every packet until end or PCR matching barrier,
       because packets of multiple stream can arrive delayed (with intermidiate pcr)
       ex: for a target time of 2, you must dequeue <= 2 until >= PCR2
       A0,A1,A2,B0,PCR0,B1,B2,PCR2,B3,A3,PCR3
    */
    std::list<AbstractCommand *> output;
    std::list<AbstractCommand *> in;

    vlc_mutex_lock(&lock);

    in.splice( in.end(), commands );

    while( !in.empty() )
    {
        AbstractCommand *command = in.front();

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_DEL && b_datasent )
            break;

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_DISCONTINUITY && b_datasent )
            break;

        if(command->getType() == ES_OUT_SET_GROUP_PCR && command->getTime() > barrier )
            break;

        in.pop_front();
        b_datasent = true;

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_SEND )
        {
            EsOutSendCommand *sendcommand = dynamic_cast<EsOutSendCommand *>(command);
            /* We need a stream identifier to send NON DATED data following DATA for the same ES */
            const void *id = (sendcommand) ? sendcommand->esIdentifier() : 0;

            /* Not for now */
            if( command->getTime() > barrier ) /* Not for now */
            {
                /* ensure no more non dated for that ES is sent
                 * since we're sure that data is above barrier */
                disabled_esids.insert( id );
                commands.push_back( command );
            }
            else if( command->getTime() == VLC_TS_INVALID )
            {
                if( disabled_esids.find( id ) == disabled_esids.end() )
                    output.push_back( command );
                else
                    commands.push_back( command );
            }
            else /* Falls below barrier, send */
            {
                output.push_back( command );
            }
        }
        else output.push_back( command ); /* will discard below */
    }

    /* push remaining ones if broke above */
    commands.splice( commands.end(), in );

    if(commands.empty() && b_draining)
        b_draining = false;

    /* Now execute our selected commands */
    while( !output.empty() )
    {
        AbstractCommand *command = output.front();
        output.pop_front();

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_SEND )
        {
            mtime_t dts = command->getTime();
            if( dts != VLC_TS_INVALID )
                lastdts = dts;
        }

        command->Execute( out );
        delete command;
    }
    pcr = lastdts; /* Warn! no PCR update/lock release until execution */

    vlc_mutex_unlock(&lock);

    return lastdts;
}

void CommandsQueue::LockedCommit()
{
    /* reorder all blocks by time between 2 PCR and merge with main list */
    incoming.sort( compareCommands );
    commands.splice( commands.end(), incoming );
}

void CommandsQueue::Commit()
{
    vlc_mutex_lock(&lock);
    LockedCommit();
    vlc_mutex_unlock(&lock);
}

void CommandsQueue::Abort( bool b_reset )
{
    vlc_mutex_lock(&lock);
    commands.splice( commands.end(), incoming );
    while( !commands.empty() )
    {
        delete commands.front();
        commands.pop_front();
    }

    if( b_reset )
    {
        bufferinglevel = VLC_TS_INVALID;
        pcr = VLC_TS_INVALID;
        b_draining = false;
        b_eof = false;
    }
    vlc_mutex_unlock(&lock);
}

bool CommandsQueue::isEmpty() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    bool b_empty = commands.empty() && incoming.empty();
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_empty;
}

void CommandsQueue::setDrop( bool b )
{
    vlc_mutex_lock(&lock);
    b_drop = b;
    vlc_mutex_unlock(&lock);
}

void CommandsQueue::setDraining()
{
    vlc_mutex_lock(&lock);
    LockedSetDraining();
    vlc_mutex_unlock(&lock);
}

bool CommandsQueue::isDraining() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    bool b = b_draining;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b;
}

void CommandsQueue::setEOF( bool b )
{
    vlc_mutex_lock(&lock);
    b_eof = b;
    if( b_eof )
        LockedSetDraining();
    else
        b_draining = false;
    vlc_mutex_unlock(&lock);
}

bool CommandsQueue::isEOF() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    bool b = b_eof;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b;
}

mtime_t CommandsQueue::getDemuxedAmount() const
{
    return bufferinglevel - getFirstDTS();
}

mtime_t CommandsQueue::getBufferingLevel() const
{
    mtime_t i_buffer;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    i_buffer = bufferinglevel;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return i_buffer;
}

mtime_t CommandsQueue::getFirstDTS() const
{
    std::list<AbstractCommand *>::const_iterator it;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    mtime_t i_firstdts = pcr;
    for( it = commands.begin(); it != commands.end(); ++it )
    {
        const mtime_t i_dts = (*it)->getTime();
        if( i_dts > VLC_TS_INVALID )
        {
            if( i_dts < i_firstdts || i_firstdts == VLC_TS_INVALID )
                i_firstdts = i_dts;
            break;
        }
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return i_firstdts;
}

void CommandsQueue::LockedSetDraining()
{
    LockedCommit();
    b_draining = !commands.empty();
}

mtime_t CommandsQueue::getPCR() const
{
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    mtime_t i_pcr = pcr;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return i_pcr;
}
