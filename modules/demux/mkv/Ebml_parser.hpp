
/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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
#ifndef VLC_MKV_EBML_PARSER_HPP_
#define VLC_MKV_EBML_PARSER_HPP_

#include "mkv.hpp"

namespace mkv {

/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
class EbmlParser
{
  public:
    EbmlParser( EbmlStream *es, EbmlElement *el_start, demux_t *p_demux );
    ~EbmlParser( void );

    void reconstruct( EbmlStream*, EbmlElement*, demux_t*);

    void Up( void );
    void Down( void );
    void Reset( demux_t *p_demux );
    EbmlElement *Get( bool allow_overshoot = true );
    void        Keep( void );
    void        Unkeep( void );

    int  GetLevel( void ) const;

    /* Is the provided element presents in our upper elements */
    bool IsTopPresent( EbmlElement * ) const;

  private:
    static const int M_EL_MAXSIZE = 10;

    demux_t     *p_demux;
    EbmlStream  *m_es;
    int          mi_level;
    EbmlElement *m_el[M_EL_MAXSIZE];

    EbmlElement *m_got;

    int          mi_user_level;
    bool         mb_keep;
    /* Allow dummy/unknown EBML elements */
    bool         mb_dummy;
};

} // namespace

#endif
