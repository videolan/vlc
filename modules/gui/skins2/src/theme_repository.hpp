/*****************************************************************************
 * theme_repository.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef THEME_REPOSITORY_HPP
#define THEME_REPOSITORY_HPP

#include "skin_common.hpp"


/// Singleton object handling the list of available themes
class ThemeRepository: public SkinObject
{
    public:
        /// Get the instance of ThemeRepository
        /// Returns NULL if the initialization of the object failed
        static ThemeRepository *instance( intf_thread_t *pIntf );

        /// Delete the instance of ThemeRepository
        static void destroy( intf_thread_t *pIntf );

    protected:
        // Protected because it is a singleton
        ThemeRepository( intf_thread_t *pIntf );
        virtual ~ThemeRepository();

    private:
        /// Identifier for the special menu entry
        static const char *kOpenDialog;

       /// Look for themes in a directory
        void parseDirectory( const string &rDir );

        /// Callback for menu item selection
        static int changeSkin( vlc_object_t *pThis, char const *pCmd,
                               vlc_value_t oldval, vlc_value_t newval,
                               void *pData );
};


#endif
