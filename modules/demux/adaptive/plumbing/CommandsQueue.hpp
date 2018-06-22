/*
 * CommandsQueue.hpp
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
#ifndef COMMANDSQUEUE_HPP_
#define COMMANDSQUEUE_HPP_

#include "FakeESOutID.hpp"
#include "../Time.hpp"

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_atomic.h>

#include <list>

namespace adaptive
{
    class AbstractFakeEsOut;

    class AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~AbstractCommand();
            virtual void Execute( ) = 0;
            virtual const Times & getTimes() const;
            int getType() const;

        protected:
            AbstractCommand( int );
            Times times;
            int type;
    };

    class AbstractFakeEsCommand : public AbstractCommand
    {
        public:
            const void * esIdentifier() const;

        protected:
            AbstractFakeEsCommand( int, AbstractFakeESOutID * );
            AbstractFakeESOutID *p_fakeid;
    };

    class EsOutSendCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutSendCommand();
            virtual void Execute( ) override;

        protected:
            EsOutSendCommand( AbstractFakeESOutID *, const SegmentTimes &, block_t * );
            block_t *p_block;
    };

    class EsOutDelCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutDelCommand( AbstractFakeESOutID * );
    };

    class EsOutAddCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutAddCommand();
            virtual void Execute() override;

        protected:
            EsOutAddCommand( AbstractFakeESOutID * );
    };

    class EsOutControlPCRCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutControlPCRCommand( int, const SegmentTimes &, vlc_tick_t );
            int group;
    };

    class EsOutDestroyCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutDestroyCommand();
    };

    class EsOutControlResetPCRCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutControlResetPCRCommand();
    };

    class EsOutMetaCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutMetaCommand( AbstractFakeEsOut *, int, vlc_meta_t * );
            virtual ~EsOutMetaCommand();
            AbstractFakeEsOut *out;
            int group;
            vlc_meta_t *p_meta;
    };

    class EsOutMilestoneCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutMilestoneCommand( AbstractFakeEsOut * );
            AbstractFakeEsOut *out;
    };

    class EsOutMediaProgressCommand : public AbstractCommand
    {
         friend class CommandsFactory;
        public:
            virtual void Execute() override;

        protected:
            EsOutMediaProgressCommand( const SegmentTimes & );
    };

    /* Factory so we can alter behaviour and filter on execution */
    class CommandsFactory
    {
        public:
            virtual ~CommandsFactory() {}
            virtual EsOutSendCommand * createEsOutSendCommand( AbstractFakeESOutID *,
                                                               const SegmentTimes &,
                                                               block_t * ) const;
            virtual EsOutDelCommand * createEsOutDelCommand( AbstractFakeESOutID * ) const;
            virtual EsOutAddCommand * createEsOutAddCommand( AbstractFakeESOutID * ) const;
            virtual EsOutControlPCRCommand * createEsOutControlPCRCommand( int,
                                                                           const SegmentTimes &,
                                                                           vlc_tick_t ) const;
            virtual EsOutControlResetPCRCommand * creatEsOutControlResetPCRCommand() const;
            virtual EsOutDestroyCommand * createEsOutDestroyCommand() const;
            virtual EsOutMetaCommand * createEsOutMetaCommand( AbstractFakeEsOut *, int, const vlc_meta_t * ) const;
            virtual EsOutMilestoneCommand * createEsOutMilestoneCommand( AbstractFakeEsOut * ) const;
            virtual EsOutMediaProgressCommand * createEsOutMediaProgressCommand( const SegmentTimes & ) const;
    };

    using Queueentry = std::pair<uint64_t, AbstractCommand *>;

    class AbstractCommandsQueue
    {
        public:
            AbstractCommandsQueue();
            virtual ~AbstractCommandsQueue() = default;
            virtual void Schedule( AbstractCommand *, EsType = EsType::Other ) = 0;
            virtual Times Process( Times )  = 0;
            virtual void Abort( bool b_reset )  = 0;
            virtual void Commit()  = 0;
            virtual bool isEmpty() const = 0;
            void setDrop( bool );
            virtual void setDraining();
            void setEOF( bool );
            bool isDraining() const;
            bool isEOF() const;
            virtual Times getDemuxedAmount(Times) const  = 0;
            virtual Times getDemuxedMediaAmount(const Times &) const = 0;
            virtual Times getBufferingLevel() const  = 0;
            virtual Times getFirstTimes() const  = 0;
            virtual Times getPCR() const = 0;

        protected:
            bool b_draining;
            bool b_drop;
            bool b_eof;
    };

    /* Queuing for doing all the stuff in order */
    class CommandsQueue : public AbstractCommandsQueue
    {
        public:
            CommandsQueue();
            virtual ~CommandsQueue();
            virtual void Schedule( AbstractCommand *, EsType = EsType::Other ) override;
            virtual Times Process( Times ) override;
            virtual void Abort( bool b_reset ) override;
            virtual void Commit() override;
            virtual bool isEmpty() const override;
            virtual void setDraining() override;
            virtual Times getDemuxedAmount(Times) const override;
            virtual Times getDemuxedMediaAmount(const Times &) const override;
            virtual Times getBufferingLevel() const override;
            virtual Times getFirstTimes() const override;
            virtual Times getPCR() const override;

        private:
            void LockedCommit();
            void LockedSetDraining();
            std::list<Queueentry> incoming;
            std::list<Queueentry> commands;
            SegmentTimes bufferinglevel_media;
            Times bufferinglevel;
            Times pcr;
            uint64_t nextsequence;
    };
}

#endif /* COMMANDSQUEUE_HPP_ */
