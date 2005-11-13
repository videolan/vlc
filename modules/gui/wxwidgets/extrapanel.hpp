/*****************************************************************************
 * extrapanel.hpp: Headers for the extra panel window
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12670 2005-09-25 11:16:31Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _WXVLC_EXTRAPANEL_H_
#define _WXVLC_EXTRAPANEL_H_

#include "wxwidgets.hpp"
#include <wx/notebook.h>

namespace wxvlc
{
    /* Extended panel */
    class ExtraPanel: public wxPanel
    {
    public:
        /* Constructor */
        ExtraPanel( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~ExtraPanel();

        /// \todo Check access level for these
        wxStaticBox *adjust_box;
        wxButton *restoredefaults_button;
        wxSlider *brightness_slider;
        wxSlider *contrast_slider;
        wxSlider *saturation_slider;
        wxSlider *hue_slider;
        wxSlider *gamma_slider;

        wxStaticBox *other_box;
        wxComboBox *ratio_combo;

        char *psz_bands;
        float f_preamp;
        vlc_bool_t b_update;
    private:
        /* General layout */
        wxPanel *VideoPanel( wxWindow * );
        wxPanel *EqzPanel( wxWindow * );
        wxPanel *AudioPanel( wxWindow * );
        wxNotebook *notebook;
    
        /* Equalizer */
        wxCheckBox *eq_chkbox;
        wxCheckBox *eq_2p_chkbox;
        wxButton *eq_restoredefaults_button;
        wxSlider *smooth_slider;
        wxStaticText *smooth_text;
        wxSlider *preamp_slider;
        wxStaticText * preamp_text;
        int i_smooth;
        wxSlider *band_sliders[10];
        wxStaticText *band_texts[10];
        int i_values[10];
    
        void OnEnableEqualizer( wxCommandEvent& );
        void OnRestoreDefaults( wxCommandEvent& );
        void OnChangeEqualizer( wxScrollEvent& );
        void OnEqSmooth( wxScrollEvent& );
        void OnPreamp( wxScrollEvent& );
        void OnEq2Pass( wxCommandEvent& );
        void OnEqRestore( wxCommandEvent& );

        /* Video */
        void OnEnableAdjust( wxCommandEvent& );
        void OnAdjustUpdate( wxScrollEvent& );
        void OnRatio( wxCommandEvent& );
        void OnFiltersInfo( wxCommandEvent& );
        void OnSelectFilter( wxCommandEvent& );

        /* Audio */
        void OnHeadphone( wxCommandEvent& );
        void OnNormvol( wxCommandEvent& );
        void OnNormvolSlider( wxScrollEvent& );

        void CheckAout();
        void OnIdle( wxIdleEvent& );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        vlc_bool_t b_my_update;
        wxWindow *p_parent;
    };
};
#if 0
/* Extended Window  */
class ExtraWindow: public wxFrame
{
public:
    /* Constructor */
    ExtraWindow( intf_thread_t *p_intf, wxWindow *p_parent, wxPanel *panel );
    virtual ~ExtraWindow();

private:

    wxPanel *panel;

    DECLARE_EVENT_TABLE();

    intf_thread_t *p_intf;
};
#endif

#endif
