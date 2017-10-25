/*****************************************************************************
 * chromaprint.cpp: Fingerprinter helper class
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#include "chromaprint.hpp"
#include <vlc_fingerprinter.h>
#include <vlc_modules.h>

Chromaprint::Chromaprint( intf_thread_t *_p_intf ) : p_intf( _p_intf )
{
    Q_ASSERT( p_intf );
    p_fingerprinter = fingerprinter_Create( VLC_OBJECT( p_intf ) );
    if ( p_fingerprinter )
        var_AddCallback( p_fingerprinter, "results-available", results_available, this );
}

int Chromaprint::results_available( vlc_object_t *, const char *, vlc_value_t, vlc_value_t , void *param )
{
    Chromaprint *me = (Chromaprint *) param;
    me->finish();
    return 0;
}

fingerprint_request_t * Chromaprint::fetchResults()
{
    return p_fingerprinter->pf_getresults( p_fingerprinter );
}

void Chromaprint::apply( fingerprint_request_t *p_r, size_t i_id )
{
    p_fingerprinter->pf_apply( p_r, i_id );
}

bool Chromaprint::enqueue( input_item_t *p_item )
{
    if ( ! p_fingerprinter ) return false;
    fingerprint_request_t *p_r = fingerprint_request_New( p_item );
    if ( ! p_r ) return false;
    mtime_t t = input_item_GetDuration( p_item );
    if ( t ) p_r->i_duration = (unsigned int) ( t / 1000000 );
    if( p_fingerprinter->pf_enqueue( p_fingerprinter, p_r ) != 0 )
    {
        fingerprint_request_Delete( p_r );
        return false;
    }
    return true;
}

bool Chromaprint::isSupported( QString uri )
{
    if ( !module_exists( "stream_out_chromaprint" ) )
        return false;
    else
    return ( uri.startsWith( "file://" ) || uri.startsWith( "/" ) );
}

Chromaprint::~Chromaprint()
{
    if ( p_fingerprinter )
        fingerprinter_Destroy( p_fingerprinter );
}
