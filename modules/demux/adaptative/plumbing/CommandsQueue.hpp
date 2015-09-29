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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_atomic.h>

#include <list>

namespace adaptative
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

    /* Factory so we can alter behaviour and filter on execution */
    class CommandsFactory
    {
        public:
            virtual ~CommandsFactory() {}
            virtual EsOutSendCommand * createEsOutSendCommand( FakeESOutID *, block_t * );
            virtual EsOutDelCommand * createEsOutDelCommand( FakeESOutID * );
            virtual EsOutAddCommand * createEsOutAddCommand( FakeESOutID * );
            virtual EsOutControlPCRCommand * createEsOutControlPCRCommand( int, mtime_t );
            virtual EsOutControlResetPCRCommand * creatEsOutControlResetPCRCommand();
            virtual EsOutDestroyCommand * createEsOutDestroyCommand();
    };

    /* Queuing for doing all the stuff in order */
    class CommandsQueue
    {
        public:
            CommandsQueue();
            ~CommandsQueue();
            void Schedule( AbstractCommand * );
            mtime_t Process( es_out_t *out, mtime_t );
            void Abort( bool b_reset );
            void Commit();
            bool isEmpty() const;
            void setDrop( bool );
            mtime_t getBufferingLevel() const;
            mtime_t getFirstDTS() const;

        private:
            std::list<AbstractCommand *> incoming;
            std::list<AbstractCommand *> commands;
            mtime_t bufferinglevel;
            bool b_drop;
    };
}

#endif /* COMMANDSQUEUE_HPP_ */
