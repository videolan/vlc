/*****************************************************************************
 * theme_loader.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: theme_loader.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef THEME_LOADER_HPP
#define THEME_LOADER_HPP

#include "skin_common.hpp"
#include <string>


class ThemeLoader: public SkinObject
{
    public:
        ThemeLoader( intf_thread_t *pIntf ): SkinObject( pIntf ) {}
        virtual ~ThemeLoader() {}

        bool load( const string &fileName );

    private:
#if defined( HAVE_ZLIB_H )
        bool extractTarGz( const string &tarFile, const string &rootDir );
        bool extract( const string &fileName );
        void deleteTempFiles( const string &path );
#endif
        bool parse( const string &xmlFile );
};

#endif
