/*****************************************************************************
 * dialogs.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialogs.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
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

#ifndef DIALOGS_HPP
#define DIALOGS_HPP

#include "skin_common.hpp"
#include <string>


// Dialogs provider
class Dialogs: public SkinObject
{
    public:
        /// Get the instance of Dialogs.
        /// Returns NULL if initialization failed
        static Dialogs *instance( intf_thread_t *pIntf );

        /// Delete the instance of Dialogs
        static void destroy( intf_thread_t *pIntf );

        /// Show the Change Skin dialog.
        void showChangeSkin();

        /// Show the Quick Open File dialog.
        /// If play is false, just add the item in the playlist
        void showFileSimple( bool play );

        /// Show the Open File dialog.
        /// If play is false, just add the item in the playlist
        void showFile( bool play );

        /// Show the Open Disc dialog
        /// If play is false, just add the item in the playlist
        void showDisc( bool play );

        /// Show the Open Network Stream dialog
        /// If play is false, just add the item in the playlist
        void showNet( bool play );

        /// Show the Messages dialog
        void showMessages();

        /// Show the Preferences dialog
        void showPrefs();

        /// Show the FileInfo dialog
        void showFileInfo();

        /// Show the popup menu
        void showPopupMenu( bool bShow );

        // XXX: This is a kludge! In fact, the file name retrieved when
        // changing the skin should be returned directly to the command, but
        // the dialog provider mechanism doesn't allow it.
        /// Store temporarily a file name
        void setThemeFile( const string &themeFile ) { m_theme = themeFile; }
        /// Get a previously saved file name
        const string &getThemeFile() const { return m_theme; }

    private:
        // Private because it's a singleton
        Dialogs( intf_thread_t *pIntf );
        ~Dialogs();

        /// Initialization method
        bool init();

        /// Dialogs provider module
        intf_thread_t *m_pProvider;
        module_t *m_pModule;

        /// Name of a theme file, obtained via the ChangeSkin dialog
        string m_theme;
};


#endif
