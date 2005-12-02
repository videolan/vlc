/*****************************************************************************
 * subtitles.hpp: private wxWindows interface description
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifndef _WXVLC_SUBTITLES_H_
#define _WXVLC_SUBTITLES_H_

#include "wxwidgets.hpp"
#include <wx/spinctrl.h>

namespace wxvlc
{
/* Subtitles File Dialog */
class SubsFileDialog: public wxDialog
{
public:
    /* Constructor */
    SubsFileDialog( intf_thread_t *p_intf, wxWindow *p_parent );
    virtual ~SubsFileDialog();

    wxComboBox *file_combo;
    wxComboBox *encoding_combo;
    wxComboBox *size_combo;
    wxComboBox *align_combo;
    wxSpinCtrl *fps_spinctrl;
    wxSpinCtrl *delay_spinctrl;

private:
    /* Event handlers (these functions should _not_ be virtual) */
    void OnOk( wxCommandEvent& event );
    void OnCancel( wxCommandEvent& event );
    void OnFileBrowse( wxCommandEvent& event );

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
    wxWindow *p_parent;
};

};

#endif

