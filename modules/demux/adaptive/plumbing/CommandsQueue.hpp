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
            virtual mtime_t getTime() const;
            int getType() const;

        protected:
            AbstractCommand( int );
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
            virtual mtime_t getTime() const override;

        protected:
            EsOutSendCommand( AbstractFakeESOutID *, block_t * );
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
            virtual mtime_t getTime() const override;

        protected:
            EsOutControlPCRCommand( int, mtime_t );
            int group;
            mtime_t pcr;
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

    /* Factory so we can alter behaviour and filter on execution */
    class CommandsFactory
    {
        public:
            virtual ~CommandsFactory() {}
            virtual EsOutSendCommand * createEsOutSendCommand( AbstractFakeESOutID *, block_t * ) const;
            virtual EsOutDelCommand * createEsOutDelCommand( AbstractFakeESOutID * ) const;
            virtual EsOutAddCommand * createEsOutAddCommand( AbstractFakeESOutID * ) const;
            virtual EsOutControlPCRCommand * createEsOutControlPCRCommand( int, mtime_t ) const;
            virtual EsOutControlResetPCRCommand * creatEsOutControlResetPCRCommand() const;
            virtual EsOutDestroyCommand * createEsOutDestroyCommand() const;
            virtual EsOutMetaCommand * createEsOutMetaCommand( AbstractFakeEsOut *, int, const vlc_meta_t * ) const;
            virtual EsOutMilestoneCommand * createEsOutMilestoneCommand( AbstractFakeEsOut * ) const;
    };

    using Queueentry = std::pair<uint64_t, AbstractCommand *>;

    class AbstractCommandsQueue
    {
        public:
            AbstractCommandsQueue();
            virtual ~AbstractCommandsQueue() = default;
            virtual void Schedule( AbstractCommand *, EsType = EsType::Other ) = 0;
            virtual mtime_t Process( mtime_t )  = 0;
            virtual void Abort( bool b_reset )  = 0;
            virtual void Commit()  = 0;
            virtual bool isEmpty() const = 0;
            void setDrop( bool );
            virtual void setDraining();
            void setEOF( bool );
            bool isDraining() const;
            bool isEOF() const;
            virtual mtime_t getDemuxedAmount(mtime_t) const  = 0;
            virtual mtime_t getBufferingLevel() const  = 0;
            virtual mtime_t getFirstDTS() const  = 0;
            virtual mtime_t getPCR() const = 0;

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
            virtual mtime_t Process( mtime_t ) override;
            virtual void Abort( bool b_reset ) override;
            virtual void Commit() override;
            virtual bool isEmpty() const override;
            virtual void setDraining() override;
            virtual mtime_t getDemuxedAmount(mtime_t) const override;
            virtual mtime_t getBufferingLevel() const override;
            virtual mtime_t getFirstDTS() const override;
            virtual mtime_t getPCR() const override;

        private:
            void LockedCommit();
            void LockedSetDraining();
            std::list<Queueentry> incoming;
            std::list<Queueentry> commands;
            mtime_t bufferinglevel;
            mtime_t pcr;
            uint64_t nextsequence;
    };
}

#endif /* COMMANDSQUEUE_HPP_ */
