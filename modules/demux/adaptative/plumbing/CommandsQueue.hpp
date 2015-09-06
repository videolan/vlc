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

    class EsOutSendCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutSendCommand();
            virtual void Execute( es_out_t *out );
            virtual mtime_t getTime() const;

        protected:
            EsOutSendCommand( FakeESOutID *, block_t * );
            FakeESOutID *p_fakeid;
            block_t *p_block;
    };

    class EsOutDelCommand : public AbstractCommand
    {
        friend class CommandsFactory;
        public:
            virtual void Execute( es_out_t *out );

        protected:
            EsOutDelCommand( FakeESOutID * );
            FakeESOutID *p_fakeid;
    };

    class EsOutAddCommand : public EsOutDelCommand
    {
        friend class CommandsFactory;
        public:
            virtual ~EsOutAddCommand();
            virtual void Execute( es_out_t *out );

        protected:
            EsOutAddCommand( FakeESOutID *, const es_format_t * );
            es_format_t fmt;
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

    /* Factory so we can alter behaviour and filter on execution */
    class CommandsFactory
    {
        public:
            virtual ~CommandsFactory() {}
            virtual EsOutSendCommand * createEsOutSendCommand( FakeESOutID *, block_t * );
            virtual EsOutDelCommand * createEsOutDelCommand( FakeESOutID * );
            virtual EsOutAddCommand * createEsOutAddCommand( FakeESOutID *, const es_format_t * );
            virtual EsOutControlPCRCommand * createEsOutControlPCRCommand( int, mtime_t );
            virtual EsOutDestroyCommand * createEsOutDestroyCommand();
    };

    /* Queuing for doing all the stuff in order */
    class CommandsQueue
    {
        public:
            CommandsQueue();
            ~CommandsQueue();
            void Schedule( AbstractCommand * );
            void Process( es_out_t *out, mtime_t );
            void Abort( bool b_reset );
            void Flush();
            bool isEmpty() const;
            void setDrop( bool );
            mtime_t getBufferingLevel() const;
            mtime_t getFirstDTS() const;

        private:
            std::list<AbstractCommand *> incoming;
            std::list<AbstractCommand *> commands;
            void FlushLocked();
            mtime_t bufferinglevel;
            bool b_drop;
            vlc_mutex_t lock;
    };
}

#endif /* COMMANDSQUEUE_HPP_ */
