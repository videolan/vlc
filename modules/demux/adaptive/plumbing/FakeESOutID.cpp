/*
 * FakeESOutID.cpp
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

#include "FakeESOutID.hpp"
#include "FakeESOut.hpp"

using namespace adaptive;

FakeESOutID::FakeESOutID( FakeESOut *fakeesout, const es_format_t *p_fmt )
    : fakeesout( fakeesout )
    , p_real_es_id( NULL )
    , pending_delete( false )
{
    es_format_Copy( &fmt, p_fmt );
}

FakeESOutID::~FakeESOutID()
{
    es_format_Clean( &fmt );
}

void FakeESOutID::setRealESID( es_out_id_t *real_es_id )
{
   p_real_es_id = real_es_id;
}

void FakeESOutID::notifyData()
{
    fakeesout->gc();
}

void FakeESOutID::create()
{
    fakeesout->createOrRecycleRealEsID( this );
}

void FakeESOutID::release()
{
    fakeesout->recycle( this );
}

es_out_id_t * FakeESOutID::realESID()
{
    return p_real_es_id;
}

const es_format_t *FakeESOutID::getFmt() const
{
    return &fmt;
}

bool FakeESOutID::isCompatible( const FakeESOutID *p_other ) const
{
    if( p_other->fmt.i_cat != fmt.i_cat )
        return false;

    switch(fmt.i_codec)
    {
        case VLC_CODEC_H264:
        case VLC_CODEC_HEVC:
        case VLC_CODEC_VC1:
                return true;

        default:
            if(fmt.i_cat == AUDIO_ES)
            {
                /* Reject audio streams with different or unknown rates */
                if(fmt.audio.i_rate != p_other->fmt.audio.i_rate || !fmt.audio.i_rate)
                    return false;
            }

            return es_format_IsSimilar( &p_other->fmt, &fmt ) &&
                   !p_other->fmt.i_extra && !fmt.i_extra;
    }
}

void FakeESOutID::setScheduledForDeletion()
{
    pending_delete = true;
}

bool FakeESOutID::scheduledForDeletion() const
{
    return pending_delete;
}
