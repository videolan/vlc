/*****************************************************************************
 * dialogs.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef DIALOGS_HPP
#define DIALOGS_HPP

#include "skin_common.hpp"
#include <string>

struct interaction_dialog_t ;

// Dialogs provider
class Dialogs: public SkinObject
{
public:
    /// Get the instance of Dialogs (or NULL if initialization failed)
    static Dialogs *instance( intf_thread_t *pIntf );

    /// Delete the instance of Dialogs
    static void destroy( intf_thread_t *pIntf );

    /// Show the Change Skin dialog
    void showChangeSkin();

    /// Show the Load Playlist dialog
    void showPlaylistLoad();

    /// Show the Save Playlist dialog
    void showPlaylistSave();

    /** Show the Quick Open File dialog.
     *  If play is false, just add the item in the playlist
     */
    void showFileSimple( bool play );

    /** Show the Open File dialog
     *  If play is false, just add the item in the playlist
     */
    void showFile( bool play );

    /** Show the Open Directory dialog
     *  If play is false, just add the item in the playlist
     */
    void showDirectory( bool play );

    /** Show the Open Disc dialog
     *  If play is false, just add the item in the playlist
     */
    void showDisc( bool play );

    /** Show the Open Network Stream dialog
     *  If play is false, just add the item in the playlist
     */
    void showNet( bool play );

    /// Show the Messages dialog
    void showMessages();

    /// Show the Preferences dialog
    void showPrefs();

    /// Show the FileInfo dialog
    void showFileInfo();

    /// Show the Streaming Wizard dialog
    void showStreamingWizard();

    /// Show the Playlist
    void showPlaylist();

    /// Show a popup menu
    void showPopupMenu( bool bShow, int popupType );

    /// Show an interaction dialog
    void showInteraction( interaction_dialog_t * );

private:
    // Private because it's a singleton
    Dialogs( intf_thread_t *pIntf );
    ~Dialogs();

    /// DlgCallback is the type of the callbacks of the open/save dialog
    typedef void DlgCallback( intf_dialog_args_t *pArg );

    /// Possible flags for the open/save dialog
    enum flags_t
    {
        kOPEN     = 0x01,
        kSAVE     = 0x02,
        kMULTIPLE = 0x04
    };

    /// Initialization method
    bool init();

    /** Show a generic open/save dialog, initialized with the given
     *  parameters
     *  The 'flags' parameter is a logical or of the flags_t values
     */
    void showFileGeneric( const string &rTitle, const string &rExtensions,
                          DlgCallback callback, int flags );

    /// Callback for the Change Skin dialog
    static void showChangeSkinCB( intf_dialog_args_t *pArg );

    /// Callback for the Load Playlist dialog
    static void showPlaylistLoadCB( intf_dialog_args_t *pArg );

    /// Callback for the Save Playlist dialog
    static void showPlaylistSaveCB( intf_dialog_args_t *pArg );

    /// Dialogs provider module
    intf_thread_t *m_pProvider;
    module_t *m_pModule;
};


#endif
