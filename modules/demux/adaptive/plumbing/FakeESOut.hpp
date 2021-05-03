/*
 * FakeESOut.hpp
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN and VLC Authors
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
#ifndef FAKEESOUT_HPP
#define FAKEESOUT_HPP

#include <vlc_common.h>
#include <list>

namespace adaptive
{
    class ExtraFMTInfoInterface
    {
        public:
            virtual void fillExtraFMTInfo( es_format_t * ) const = 0;
            virtual ~ExtraFMTInfoInterface() = default;
    };

    class AbstractCommandsQueue;
    class CommandsFactory;
    class AbstractFakeESOutID;
    class FakeESOutID;

    class AbstractFakeEsOut
    {
        friend class EsOutCallbacks;
        public:
            AbstractFakeEsOut();
            virtual ~AbstractFakeEsOut();
            operator es_out_t*();
            virtual void milestoneReached() = 0;
            /* Used by FakeES ID */
            virtual void recycle( AbstractFakeESOutID * ) = 0;
            virtual void createOrRecycleRealEsID( AbstractFakeESOutID * ) = 0;
            virtual void setPriority(int) = 0;
            virtual void sendData( AbstractFakeESOutID *, block_t * ) = 0;
            virtual void sendMeta( int, const vlc_meta_t * ) = 0;

        private:
            void *esoutpriv;
            virtual es_out_id_t *esOutAdd( const es_format_t * ) = 0;
            virtual int esOutSend( es_out_id_t *, block_t * ) = 0;
            virtual void esOutDel( es_out_id_t * ) = 0;
            virtual int esOutControl( int, va_list ) = 0;
            virtual void esOutDestroy() = 0;
    };

    class FakeESOut : public AbstractFakeEsOut
    {
        public:
            class LockedFakeEsOut
            {
                friend class FakeESOut;
                public:
                    ~LockedFakeEsOut();
                    operator es_out_t*();
                    FakeESOut & operator*();
                    FakeESOut * operator->();
                private:
                    FakeESOut *p;
                    LockedFakeEsOut(FakeESOut &q);
            };
            FakeESOut( es_out_t *, AbstractCommandsQueue *, CommandsFactory * );
            virtual ~FakeESOut();
            LockedFakeEsOut WithLock();
            AbstractCommandsQueue * commandsQueue();
            CommandsFactory *commandsFactory() const;
            void setAssociatedTimestamp( vlc_tick_t );
            void setExpectedTimestamp( vlc_tick_t );
            void resetTimestamps();
            bool getStartTimestamps( vlc_tick_t *, vlc_tick_t * );
            size_t esCount() const;
            bool hasSelectedEs() const;
            bool decodersDrained();
            bool restarting() const;
            void setExtraInfoProvider( ExtraFMTInfoInterface * );
            vlc_tick_t fixTimestamp(vlc_tick_t);
            void declareEs( const es_format_t * );

            virtual void milestoneReached() override;

            /* Used by FakeES ID */
            virtual void recycle( AbstractFakeESOutID *id ) override;
            virtual void createOrRecycleRealEsID( AbstractFakeESOutID * ) override;
            virtual void setPriority(int) override;
            virtual void sendData( AbstractFakeESOutID *, block_t * ) override;
            virtual void sendMeta( int, const vlc_meta_t * ) override;

            /**/
            void scheduleNecessaryMilestone();
            void schedulePCRReset();
            void scheduleAllForDeletion(); /* Queue Del commands for non Del issued ones */
            void recycleAll(); /* Cancels all commands and send fakees for recycling */
            void gc();

        private:
            friend class LockedFakeESOut;
            vlc_mutex_t lock;
            virtual es_out_id_t *esOutAdd( const es_format_t * ) override;
            virtual int esOutSend( es_out_id_t *, block_t * ) override;
            virtual void esOutDel( es_out_id_t * ) override;
            virtual int esOutControl( int, va_list ) override;
            virtual void esOutDestroy() override;
            es_out_t *real_es_out;
            FakeESOutID * createNewID( const es_format_t * );
            ExtraFMTInfoInterface *extrainfo;
            AbstractCommandsQueue *commandsqueue;
            CommandsFactory *commandsfactory;
            struct
            {
                vlc_tick_t timestamp;
                bool b_timestamp_set;
                bool b_offset_calculated;
            } associated, expected;
            vlc_tick_t timestamp_first;
            vlc_tick_t timestamps_offset;
            int priority;
            bool b_in_commands_group;
            std::list<FakeESOutID *> fakeesidlist;
            std::list<FakeESOutID *> recycle_candidates;
            std::list<FakeESOutID *> declared;
    };

}
#endif // FAKEESOUT_HPP
