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
    };

    class CommandsQueue;
    class FakeESOutID;

    class FakeESOut
    {
        public:
            FakeESOut( es_out_t *, CommandsQueue * );
            ~FakeESOut();
            es_out_t * getEsOut();
            void setTimestampOffset( mtime_t );
            void setExpectedTimestampOffset(mtime_t);
            size_t esCount() const;
            bool hasSelectedEs() const;
            bool decodersDrained();
            bool restarting() const;
            void setExtraInfoProvider( ExtraFMTInfoInterface * );
            void checkTimestampsStart(mtime_t);

            /* Used by FakeES ID */
            void recycle( FakeESOutID *id );
            void createOrRecycleRealEsID( FakeESOutID * );

            /**/
            void schedulePCRReset();
            void scheduleAllForDeletion(); /* Queue Del commands for non Del issued ones */
            void recycleAll(); /* Cancels all commands and send fakees for recycling */
            void gc();

            /* static callbacks for demuxer */
            static es_out_id_t *esOutAdd_Callback( es_out_t *, const es_format_t * );
            static int esOutSend_Callback( es_out_t *, es_out_id_t *, block_t * );
            static void esOutDel_Callback( es_out_t *, es_out_id_t * );
            static int esOutControl_Callback( es_out_t *, int, va_list );
            static void esOutDestroy_Callback( es_out_t * );

        private:
            vlc_mutex_t lock;
            es_out_t *real_es_out;
            FakeESOutID * createNewID( const es_format_t * );
            ExtraFMTInfoInterface *extrainfo;
            mtime_t getTimestampOffset() const;
            CommandsQueue *commandsqueue;
            es_out_t *fakeesout;
            mtime_t timestamps_offset;
            mtime_t timestamps_expected;
            bool timestamps_check_done;
            std::list<FakeESOutID *> fakeesidlist;
            std::list<FakeESOutID *> recycle_candidates;
    };

}
#endif // FAKEESOUT_HPP
