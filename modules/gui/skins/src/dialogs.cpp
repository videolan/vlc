/*****************************************************************************
 * dialogs.cpp: Handles all the different dialog boxes we provide.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dialogs.cpp,v 1.10 2003/07/17 17:30:40 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "event.h"
#include "banks.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "window.h"
#include "vlcproc.h"
#include "skin_common.h"
#include "dialogs.h"

/* Callback prototype */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );

//---------------------------------------------------------------------------
// Implementation of Dialogs class
//---------------------------------------------------------------------------
Dialogs::Dialogs( intf_thread_t *_p_intf )
{
    /* Errors while loading the dialogs provider are not fatal.
     * Dialogs just won't be available. */

    p_intf = _p_intf;
    p_intf->p_sys->p_dialogs = this;
    b_popup_change = VLC_FALSE;

    /* Allocate descriptor */
    p_provider = (intf_thread_t *)vlc_object_create( p_intf, VLC_OBJECT_INTF );
    if( p_provider == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return;
    }

    p_module = module_Need( p_provider, "dialogs provider", NULL );
    if( p_module == NULL )
    {
        msg_Err( p_intf, "no suitable dialogs provider found" );
        vlc_object_destroy( p_provider );
        p_provider = NULL;
        return;
    }

    /* Initialize dialogs provider
     * (returns as soon as initialization is done) */
    if( p_provider->pf_run ) p_provider->pf_run( p_provider );
}

Dialogs::~Dialogs()
{
    if( p_provider && p_module )
    {
        module_Unneed( p_provider, p_module );
        vlc_object_destroy( p_provider );
    }
}

void Dialogs::ShowDialog( intf_thread_t *p_intf, int i_dialog_event,
                          int i_arg )
{
}

void Dialogs::ShowOpen( bool b_play )
{
    if( p_provider && p_provider->pf_show_dialog )
        p_provider->pf_show_dialog( p_provider, INTF_DIALOG_FILE, b_play );
}

void Dialogs::ShowOpenSkin()
{
    if( p_provider && p_provider->pf_show_dialog )
        p_provider->pf_show_dialog( p_provider, INTF_DIALOG_FILE, 0 );
}

void Dialogs::ShowMessages()
{
    if( p_provider && p_provider->pf_show_dialog )
        p_provider->pf_show_dialog( p_provider, INTF_DIALOG_MESSAGES, 0 );
}

void Dialogs::ShowPrefs()
{
    if( p_provider && p_provider->pf_show_dialog )
        p_provider->pf_show_dialog( p_provider, INTF_DIALOG_PREFS, 0 );
}

void Dialogs::ShowFileInfo()
{
    if( p_provider && p_provider->pf_show_dialog )
        p_provider->pf_show_dialog( p_provider, INTF_DIALOG_FILEINFO, 0 );
}

void Dialogs::ShowPopup()
{
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    Dialogs *p_dialogs = (Dialogs *)param;
    p_dialogs->ShowPopup();

    return VLC_SUCCESS;
}
