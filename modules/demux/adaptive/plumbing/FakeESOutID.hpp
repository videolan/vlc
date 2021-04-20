/*
 * FakeESOutID.hpp
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
#ifndef FAKEESOUTID_HPP
#define FAKEESOUTID_HPP

#include <vlc_common.h>
#include <vlc_es.h>

namespace adaptive
{
    class FakeESOut;

    enum class EsType
    {
        Video,
        Audio,
        Other,
    };

    class AbstractFakeESOutID
    {
        public:
            virtual ~AbstractFakeESOutID() = default;
            virtual es_out_id_t * realESID() = 0;
            virtual void create() = 0;
            virtual void release() = 0;
            virtual void sendData(block_t *) = 0;
            virtual EsType esType() const = 0;
    };

    class FakeESOutID : public AbstractFakeESOutID
    {
        public:
            FakeESOutID( FakeESOut *, const es_format_t * );
            virtual ~FakeESOutID();
            void setRealESID( es_out_id_t * );
            virtual es_out_id_t * realESID() override;
            const es_format_t *getFmt() const;
            virtual void create() override;
            virtual void release() override;
            virtual void sendData(block_t *) override;
            virtual EsType esType() const override;
            bool isCompatible( const FakeESOutID * ) const;
            /* Ensure we won't issue delete command twice */
            void setScheduledForDeletion();
            bool scheduledForDeletion() const;

        private:
            FakeESOut *fakeesout;
            es_out_id_t *p_real_es_id;
            es_format_t fmt;
            bool pending_delete;
    };
}

#endif // FAKEESOUTID_HPP
