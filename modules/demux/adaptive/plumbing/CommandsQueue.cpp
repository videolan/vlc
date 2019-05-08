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

vlc_tick_t AbstractCommand::getTime() const
{
    return VLC_TICK_INVALID;
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

vlc_tick_t EsOutSendCommand::getTime() const
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

EsOutControlPCRCommand::EsOutControlPCRCommand( int group_, vlc_tick_t pcr_ ) :
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

vlc_tick_t EsOutControlPCRCommand::getTime() const
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

EsOutControlPCRCommand * CommandsFactory::createEsOutControlPCRCommand( int group, vlc_tick_t pcr ) const
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
#if 0
/* For queue printing/debugging */
std::ostream& operator<<(std::ostream& ostr, const std::list<AbstractCommand *>& list)
{
    for (auto &i : list) {
        ostr << "[" << i->getType() << "]" << SEC_FROM_VLC_TICK(i->getTime()) << " ";
    }
    return ostr;
}
#endif

CommandsQueue::CommandsQueue( CommandsFactory *f )
{
    bufferinglevel = VLC_TICK_INVALID;
    pcr = VLC_TICK_INVALID;
    b_drop = false;
    b_draining = false;
    b_eof = false;
    commandsFactory = f;
}

CommandsQueue::~CommandsQueue()
{
    Abort( false );
    delete commandsFactory;
}

static bool compareCommands( AbstractCommand *a, AbstractCommand *b )
{
    if(a->getTime() == b->getTime())
    {
        /* Reorder the initial clock PCR setting PCR0 DTS0 PCR0 DTS1 PCR1
           so it appears after the block, avoiding it not being output */
        if(a->getType() == ES_OUT_SET_GROUP_PCR &&
           b->getType() == ES_OUT_PRIVATE_COMMAND_SEND)
            return false;
        return true;
    }
    else return (a->getTime() < b->getTime() && a->getTime() != VLC_TICK_INVALID);
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
        LockedCommit();
        commands.push_back( command );
    }
    else
    {
        incoming.push_back( command );
    }
}

const CommandsFactory * CommandsQueue::factory() const
{
    return commandsFactory;
}

vlc_tick_t CommandsQueue::Process( es_out_t *out, vlc_tick_t barrier )
{
    vlc_tick_t lastdts = barrier;
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
            else if( command->getTime() == VLC_TICK_INVALID )
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
            vlc_tick_t dts = command->getTime();
            if( dts != VLC_TICK_INVALID )
                lastdts = dts;
        }

        command->Execute( out );
        delete command;
    }
    pcr = lastdts; /* Warn! no PCR update/lock release until execution */


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
    LockedCommit();
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
    {
        bufferinglevel = VLC_TICK_INVALID;
        pcr = VLC_TICK_INVALID;
        b_draining = false;
        b_eof = false;
    }
}

bool CommandsQueue::isEmpty() const
{
    bool b_empty = commands.empty() && incoming.empty();
    return b_empty;
}

void CommandsQueue::setDrop( bool b )
{
    b_drop = b;
}

void CommandsQueue::setDraining()
{
    LockedSetDraining();
}

bool CommandsQueue::isDraining() const
{
    bool b = b_draining;
    return b;
}

void CommandsQueue::setEOF( bool b )
{
    b_eof = b;
    if( b_eof )
        LockedSetDraining();
    else
        b_draining = false;
}

bool CommandsQueue::isEOF() const
{
    bool b = b_eof;
    return b;
}

vlc_tick_t CommandsQueue::getDemuxedAmount() const
{
    return bufferinglevel - getFirstDTS();
}

vlc_tick_t CommandsQueue::getBufferingLevel() const
{
    vlc_tick_t i_buffer;
    i_buffer = bufferinglevel;
    return i_buffer;
}

vlc_tick_t CommandsQueue::getFirstDTS() const
{
    std::list<AbstractCommand *>::const_iterator it;
    vlc_tick_t i_firstdts = pcr;
    for( it = commands.begin(); it != commands.end(); ++it )
    {
        const vlc_tick_t i_dts = (*it)->getTime();
        if( i_dts != VLC_TICK_INVALID )
        {
            if( i_dts < i_firstdts || i_firstdts == VLC_TICK_INVALID )
                i_firstdts = i_dts;
            break;
        }
    }
    return i_firstdts;
}

void CommandsQueue::LockedSetDraining()
{
    LockedCommit();
    b_draining = !commands.empty();
}

vlc_tick_t CommandsQueue::getPCR() const
{
    vlc_tick_t i_pcr = pcr;
    return i_pcr;
}
