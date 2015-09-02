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
#include "FakeESOutID.hpp"
#include "FakeESOut.hpp"

using namespace adaptative;

FakeESOutID::FakeESOutID( FakeESOut *fakeesout_, const es_format_t *p_fmt )
{
    p_real_es_id = NULL;
    fakeesout = fakeesout_;
    es_format_Init( &fmt, 0, 0 );
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

void FakeESOutID::release()
{
    fakeesout->recycle( this );
}

es_out_id_t * FakeESOutID::realESID()
{
    return p_real_es_id;
}

bool FakeESOutID::isCompatible( const es_format_t *p_fmt ) const
{
    return es_format_IsSimilar( p_fmt, &fmt ) &&
           p_fmt->i_extra == fmt.i_extra &&
          !memcmp( p_fmt->p_extra, fmt.p_extra, p_fmt->i_extra );
}
