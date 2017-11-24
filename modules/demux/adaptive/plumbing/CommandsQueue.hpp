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

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_atomic.h>

#include <list>

namespace adaptive
{
    class FakeESOut;
    class FakeESOutID;

    class AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~AbstractCommand();
            virtual void Execute( es_out_t * ) = 0;
            virtual mtime_t getTime() const;
            int getType() const;

        protected:
            AbstractCommand( int );
            int type;
    };

    class AbstractFakeEsCommand : public AbstractCommand
    {
        protected:
            AbstractFakeEsCommand( int, FakeESOutID * );
            FakeESOutID *p_fakeid;
    };

    class EsOutSendCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutSendCommand();
            virtual void Execute( es_out_t *out );
            virtual mtime_t getTime() const;
            const void * esIdentifier() const;

        protected:
            EsOutSendCommand( FakeESOutID *, block_t * );
            block_t *p_block;
    };

    class EsOutDelCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );

        protected:
            EsOutDelCommand( FakeESOutID * );
    };

    class EsOutAddCommand : public AbstractFakeEsCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutAddCommand();
            virtual void Execute( es_out_t *out );

        protected:
            EsOutAddCommand( FakeESOutID * );
    };

    class EsOutControlPCRCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );
            virtual mtime_t getTime() const;

        protected:
            EsOutControlPCRCommand( int, mtime_t );
            int group;
            mtime_t pcr;
    };

    class EsOutDestroyCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );

        protected:
            EsOutDestroyCommand();
    };

    class EsOutControlResetPCRCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );

        protected:
            EsOutControlResetPCRCommand();
    };

    class EsOutMetaCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );

        protected:
            EsOutMetaCommand( int, vlc_meta_t * );
            virtual ~EsOutMetaCommand();
            int group;
            vlc_meta_t *p_meta;
    };

    /* Factory so we can alter behaviour and filter on execution */
    class CommandsFactory
    {
        public:
            virtual ~CommandsFactory() {}
            virtual EsOutSendCommand * createEsOutSendCommand( FakeESOutID *, block_t * ) const;
            virtual EsOutDelCommand * createEsOutDelCommand( FakeESOutID * ) const;
            virtual EsOutAddCommand * createEsOutAddCommand( FakeESOutID * ) const;
            virtual EsOutControlPCRCommand * createEsOutControlPCRCommand( int, mtime_t ) const;
            virtual EsOutControlResetPCRCommand * creatEsOutControlResetPCRCommand() const;
            virtual EsOutDestroyCommand * createEsOutDestroyCommand() const;
            virtual EsOutMetaCommand * createEsOutMetaCommand( int, const vlc_meta_t * ) const;
    };

    /* Queuing for doing all the stuff in order */
    class CommandsQueue
    {
        public:
            CommandsQueue( CommandsFactory * );
            ~CommandsQueue();
            const CommandsFactory * factory() const;
            void Schedule( AbstractCommand * );
            mtime_t Process( es_out_t *out, mtime_t );
            void Abort( bool b_reset );
            void Commit();
            bool isEmpty() const;
            void setDrop( bool );
            void setDraining();
            void setEOF( bool );
            bool isDraining() const;
            bool isEOF() const;
            mtime_t getDemuxedAmount() const;
            mtime_t getBufferingLevel() const;
            mtime_t getFirstDTS() const;
            mtime_t getPCR() const;

        private:
            CommandsFactory *commandsFactory;
            vlc_mutex_t lock;
            void LockedCommit();
            void LockedSetDraining();
            std::list<AbstractCommand *> incoming;
            std::list<AbstractCommand *> commands;
            mtime_t bufferinglevel;
            mtime_t pcr;
            bool b_draining;
            bool b_drop;
            bool b_eof;
    };
}

#endif /* COMMANDSQUEUE_HPP_ */
