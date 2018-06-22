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
    ES_OUT_PRIVATE_COMMAND_DISCONTINUITY,
    ES_OUT_PRIVATE_COMMAND_MILESTONE,
    ES_OUT_PRIVATE_COMMAND_PROGRESS,
};

AbstractCommand::AbstractCommand( int type_ )
{
    type = type_;
}

AbstractCommand::~AbstractCommand()
{

}

const Times & AbstractCommand::getTimes() const
{
    return times;
}

int AbstractCommand::getType() const
{
    return type;
}

AbstractFakeEsCommand::AbstractFakeEsCommand( int type, AbstractFakeESOutID *p_es ) :
    AbstractCommand( type )
{
    p_fakeid = p_es;
}

const void * AbstractFakeEsCommand::esIdentifier() const
{
    return static_cast<const void *>(p_fakeid);
}

EsOutSendCommand::EsOutSendCommand( AbstractFakeESOutID *p_es,
                                    const SegmentTimes &t, block_t *p_block_ ) :
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_SEND, p_es )
{
    p_block = p_block_;
    times = Times(t, p_block_->i_dts );
}

EsOutSendCommand::~EsOutSendCommand()
{
    if( p_block )
        block_Release( p_block );
}

void EsOutSendCommand::Execute()
{
    p_fakeid->sendData( p_block );
    p_block = nullptr;
}

EsOutDelCommand::EsOutDelCommand( AbstractFakeESOutID *p_es ) :
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_DEL, p_es )
{
}

void EsOutDelCommand::Execute( )
{
    p_fakeid->release();
}

EsOutAddCommand::EsOutAddCommand( AbstractFakeESOutID *p_es ) :
    AbstractFakeEsCommand( ES_OUT_PRIVATE_COMMAND_ADD, p_es )
{
}

EsOutAddCommand::~EsOutAddCommand()
{
}

void EsOutAddCommand::Execute( )
{
    /* Create the real ES on the adaptive demux */
    p_fakeid->create();
}

EsOutControlPCRCommand::EsOutControlPCRCommand( int group_,
                                                const SegmentTimes &t, vlc_tick_t pcr )
    : AbstractCommand( ES_OUT_SET_GROUP_PCR )
{
    group = group_;
    times = Times( t, pcr );
    type = ES_OUT_SET_GROUP_PCR;
}

void EsOutControlPCRCommand::Execute( )
{
    // do nothing here
}

EsOutDestroyCommand::EsOutDestroyCommand() :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_DESTROY )
{
}

void EsOutDestroyCommand::Execute( )
{
}

EsOutControlResetPCRCommand::EsOutControlResetPCRCommand() :
    AbstractCommand( ES_OUT_PRIVATE_COMMAND_DISCONTINUITY )
{
}

void EsOutControlResetPCRCommand::Execute( )
{
}

EsOutMetaCommand::EsOutMetaCommand( AbstractFakeEsOut *out,
                                    int i_group, vlc_meta_t *p_meta ) :
    AbstractCommand( ES_OUT_SET_GROUP_META )
{
    group = i_group;
    this->out = out;
    this->p_meta = p_meta;
}

EsOutMetaCommand::~EsOutMetaCommand()
{
    if( p_meta )
        vlc_meta_Delete( p_meta );
}

void EsOutMetaCommand::Execute()
{
    out->sendMeta( group, p_meta );
}

EsOutMilestoneCommand::EsOutMilestoneCommand( AbstractFakeEsOut *out )
    : AbstractCommand( ES_OUT_PRIVATE_COMMAND_MILESTONE )
{
    this->out = out;
}

void EsOutMilestoneCommand::Execute()
{
    out->milestoneReached();
}

EsOutMediaProgressCommand::EsOutMediaProgressCommand(const SegmentTimes &t)
    : AbstractCommand( ES_OUT_PRIVATE_COMMAND_PROGRESS )
{
    times = Times(t, VLC_TS_INVALID);
}

void EsOutMediaProgressCommand::Execute()
{

}

/*
 * Commands Default Factory
 */

EsOutSendCommand * CommandsFactory::createEsOutSendCommand( AbstractFakeESOutID *id,
                                                            const SegmentTimes &t,
                                                            block_t *p_block ) const
{
    return new (std::nothrow) EsOutSendCommand( id, t, p_block );
}

EsOutDelCommand * CommandsFactory::createEsOutDelCommand( AbstractFakeESOutID *id ) const
{
    return new (std::nothrow) EsOutDelCommand( id );
}

EsOutAddCommand * CommandsFactory::createEsOutAddCommand( AbstractFakeESOutID *id ) const
{
    return new (std::nothrow) EsOutAddCommand( id );
}

EsOutControlPCRCommand * CommandsFactory::createEsOutControlPCRCommand( int group,
                                                                        const SegmentTimes &t,
                                                                        vlc_tick_t pcr ) const
{
    return new (std::nothrow) EsOutControlPCRCommand( group, t, pcr );
}

EsOutDestroyCommand * CommandsFactory::createEsOutDestroyCommand() const
{
    return new (std::nothrow) EsOutDestroyCommand();
}

EsOutControlResetPCRCommand * CommandsFactory::creatEsOutControlResetPCRCommand() const
{
    return new (std::nothrow) EsOutControlResetPCRCommand();
}

EsOutMetaCommand * CommandsFactory::createEsOutMetaCommand( AbstractFakeEsOut *out, int group,
                                                            const vlc_meta_t *p_meta ) const
{
    vlc_meta_t *p_dup = vlc_meta_New();
    if( p_dup )
    {
        vlc_meta_Merge( p_dup, p_meta );
        return new (std::nothrow) EsOutMetaCommand( out, group, p_dup );
    }
    return nullptr;
}

EsOutMilestoneCommand * CommandsFactory::createEsOutMilestoneCommand( AbstractFakeEsOut *out ) const
{
    return new (std::nothrow) EsOutMilestoneCommand( out );
}

EsOutMediaProgressCommand * CommandsFactory::createEsOutMediaProgressCommand( const SegmentTimes &t ) const
{
    try {
        return new EsOutMediaProgressCommand( t );
    } catch(...) { return nullptr; }
}

/*
 * Commands Queue management
 */
#if 0
/* For queue printing/debugging */
std::ostream& operator<<(std::ostream& ostr, const std::list<AbstractCommand *>& list)
{
    for (auto &i : list) {
<<<<<<< HEAD
        ostr << "[" << i->getType() << "]" << (i->getTime() / CLOCK_FREQ) << " ";
=======
        ostr << "[" << i->getType() << "]" << SEC_FROM_VLC_TICK(i->getTimes().continuous) << " ";
>>>>>>> 7f9221f31f (demux: adaptive: propagate and interpolate asynchronous times)
    }
    return ostr;
}
#endif

AbstractCommandsQueue::AbstractCommandsQueue()
{
    b_drop = false;
    b_draining = false;
    b_eof = false;
}

void AbstractCommandsQueue::setDrop( bool b )
{
    b_drop = b;
}

void AbstractCommandsQueue::setDraining()
{
    b_draining = true;
}

bool AbstractCommandsQueue::isDraining() const
{
    return b_draining;
}

void AbstractCommandsQueue::setEOF( bool b )
{
    b_eof = b;
    if( b_eof )
        setDraining();
    else
        b_draining = false;
}

bool AbstractCommandsQueue::isEOF() const
{
    return b_eof;
}

CommandsQueue::CommandsQueue()
    : AbstractCommandsQueue()
{
    bufferinglevel = Times();
    nextsequence = 0;
}

CommandsQueue::~CommandsQueue()
{
    Abort( false );
}

static bool compareCommands( const Queueentry &a, const Queueentry &b )
{
    if(a.second->getTimes().continuous == b.second->getTimes().continuous)
    {
        /* Reorder the initial clock PCR setting PCR0 DTS0 PCR0 DTS1 PCR1
           so it appears after the block, avoiding it not being output */
        if(a.second->getType() == ES_OUT_SET_GROUP_PCR &&
           b.second->getType() == ES_OUT_PRIVATE_COMMAND_SEND &&
           a.first < b.first)
            return false;

        return a.first < b.first;
    }
    else if (a.second->getTimes().continuous == VLC_TS_INVALID ||
             b.second->getTimes().continuous == VLC_TS_INVALID)
    {
        return a.first < b.first;
    }
    else
    {
        return a.second->getTimes().continuous < b.second->getTimes().continuous;
    }
}

void CommandsQueue::Schedule( AbstractCommand *command, EsType )
{
    if( b_drop )
    {
        delete command;
    }
    else if( command->getType() == ES_OUT_PRIVATE_COMMAND_PROGRESS )
    {
        const Times &times = command->getTimes();
        bufferinglevel_media = times.segment;
        delete command;
    }
    else if( command->getType() == ES_OUT_SET_GROUP_PCR )
    {
        if(command->getTimes().continuous != VLC_TS_INVALID)
            bufferinglevel = command->getTimes();
        LockedCommit();
        commands.push_back( Queueentry(nextsequence++, command) );
    }
    else
    {
        incoming.push_back( Queueentry(nextsequence++, command) );
    }
}

Times CommandsQueue::Process( Times barrier )
{
    Times lastdts = barrier;
    std::set<const void *> disabled_esids;
    bool b_datasent = false;

    /* We need to filter the current commands list
       We need to return on discontinuity or reset events if data was sent
       We must lookup every packet until end or PCR matching barrier,
       because packets of multiple stream can arrive delayed (with intermidiate pcr)
       ex: for a target time of 2, you must dequeue <= 2 until >= PCR2
       A0,A1,A2,B0,PCR0,B1,B2,PCR2,B3,A3,PCR3
    */
    std::list<Queueentry> output;
    std::list<Queueentry> in;


    in.splice( in.end(), commands );

    while( !in.empty() )
    {
        Queueentry entry = in.front();
        AbstractCommand *command = entry.second;

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_DEL && b_datasent )
            break;

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_DISCONTINUITY && b_datasent )
            break;

        if(command->getType() == ES_OUT_SET_GROUP_PCR && command->getTimes().continuous > barrier.continuous )
            break;

        in.pop_front();
        b_datasent = true;

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_SEND )
        {
            EsOutSendCommand *sendcommand = dynamic_cast<EsOutSendCommand *>(command);
            /* We need a stream identifier to send NON DATED data following DATA for the same ES */
            const void *id = (sendcommand) ? sendcommand->esIdentifier() : 0;

            /* Not for now */
            if( command->getTimes().continuous > barrier.continuous ) /* Not for now */
            {
                /* ensure no more non dated for that ES is sent
                 * since we're sure that data is above barrier */
                disabled_esids.insert( id );
                commands.push_back( entry );
            }
            else if( command->getTimes().continuous == VLC_TS_INVALID )
            {
                if( disabled_esids.find( id ) == disabled_esids.end() )
                    output.push_back( entry );
                else
                    commands.push_back( entry );
            }
            else /* Falls below barrier, send */
            {
                output.push_back( entry );
            }
        }
        else output.push_back( entry ); /* will discard below */
    }

    /* push remaining ones if broke above */
    commands.splice( commands.end(), in );

    if(commands.empty() && b_draining)
        b_draining = false;

    /* Now execute our selected commands */
    while( !output.empty() )
    {
        AbstractCommand *command = output.front().second;
        output.pop_front();

        if( command->getType() == ES_OUT_PRIVATE_COMMAND_SEND )
        {
            Times times = command->getTimes();
            if( times.continuous != VLC_TS_INVALID )
                lastdts = times;
        }

        command->Execute();
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
        delete commands.front().second;
        commands.pop_front();
    }

    if( b_reset )
    {
        bufferinglevel = Times();
        bufferinglevel_media = SegmentTimes();
        pcr = Times();
        b_draining = false;
        b_eof = false;
    }
}

bool CommandsQueue::isEmpty() const
{
    return commands.empty() && incoming.empty();
}

void CommandsQueue::setDraining()
{
    LockedSetDraining();
}

Times CommandsQueue::getDemuxedAmount(Times from) const
{
    Times bufferingstart = getFirstTimes();
    if( bufferinglevel.continuous == VLC_TS_INVALID ||
        bufferingstart.continuous == VLC_TS_INVALID ||
        from.continuous == VLC_TS_INVALID ||
        from.continuous > bufferinglevel.continuous )
        return Times(SegmentTimes(0,0),0); /* durations */

    Times t = bufferinglevel;
    t.offsetBy( - from.continuous );
    return t;
}

Times CommandsQueue::getDemuxedMediaAmount(const Times &from) const
{
    if(from.continuous == VLC_TS_INVALID ||
       bufferinglevel_media.media == VLC_TS_INVALID ||
       from.segment.media > bufferinglevel_media.media)
        return Times(SegmentTimes(0,0),0);  /* durations */
    Times t = from;
    t.offsetBy( bufferinglevel_media.media - from.segment.media - from.segment.media );
    return t;
}

Times CommandsQueue::getBufferingLevel() const
{
    return bufferinglevel;
}

Times CommandsQueue::getFirstTimes() const
{
    Times first = pcr;
    std::list<Queueentry>::const_iterator it;
    for( it = commands.begin(); it != commands.end(); ++it )
    {
        const Times times = (*it).second->getTimes();
        if( times.continuous != VLC_TS_INVALID )
        {
            if( times.continuous < first.continuous || first.continuous == VLC_TS_INVALID )
                first = times;
            break;
        }
    }
    return first;
}

void CommandsQueue::LockedSetDraining()
{
    LockedCommit();
    b_draining = !commands.empty();
}

Times CommandsQueue::getPCR() const
{
    return pcr;
}

