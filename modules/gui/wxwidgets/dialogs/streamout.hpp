/*****************************************************************************
 * streamout.hpp: Stream output dialog
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

#ifndef _WXVLC_STREAMOUT_H_
#define _WXVLC_STREAMOUT_H_

#include "wxwidgets.hpp"
#include <wx/spinctrl.h>

namespace wxvlc
{
    enum
    {
        PLAY_ACCESS_OUT = 0,
        FILE_ACCESS_OUT,
        HTTP_ACCESS_OUT,
        MMSH_ACCESS_OUT,
        RTP_ACCESS_OUT,
        UDP_ACCESS_OUT,
        ACCESS_OUT_NUM
    };

    enum
    {
        TS_ENCAPSULATION = 0,
        PS_ENCAPSULATION,
        MPEG1_ENCAPSULATION,
        OGG_ENCAPSULATION,
        ASF_ENCAPSULATION,
        MP4_ENCAPSULATION,
        MOV_ENCAPSULATION,
        WAV_ENCAPSULATION,
        RAW_ENCAPSULATION,
        AVI_ENCAPSULATION,
        ENCAPS_NUM
    };

    enum
    {
        ANN_MISC_SOUT = 0,
        TTL_MISC_SOUT,
        MISC_SOUT_NUM
    };

    class SoutDialog: public wxDialog
    {
    public:
        /* Constructor */
        SoutDialog( intf_thread_t *p_intf, wxWindow *p_parent );
        virtual ~SoutDialog();

        wxArrayString GetOptions();

    private:
        void UpdateMRL();
        wxPanel *AccessPanel( wxWindow* parent );
        wxPanel *MiscPanel( wxWindow* parent );
        wxPanel *EncapsulationPanel( wxWindow* parent );
        wxPanel *TranscodingPanel( wxWindow* parent );
        void    ParseMRL();

        /* Event handlers (these functions should _not_ be virtual) */
        void OnOk( wxCommandEvent& event );
        void OnCancel( wxCommandEvent& event );
        void OnMRLChange( wxCommandEvent& event );
        void OnAccessTypeChange( wxCommandEvent& event );

        /* Event handlers for the file access output */
        void OnFileChange( wxCommandEvent& event );
        void OnFileBrowse( wxCommandEvent& event );
        void OnFileDump( wxCommandEvent& event );

        /* Event handlers for the net access output */
        void OnNetChange( wxCommandEvent& event );

        /* Event specific to the announce address */
        void OnAnnounceGroupChange( wxCommandEvent& event );
        void OnAnnounceAddrChange( wxCommandEvent& event );

        /* Event handlers for the encapsulation panel */
        void OnEncapsulationChange( wxCommandEvent& event );

        /* Event handlers for the transcoding panel */
        void OnTranscodingEnable( wxCommandEvent& event );
        void OnTranscodingChange( wxCommandEvent& event );

        /* Event handlers for the misc panel */
        void OnSAPMiscChange( wxCommandEvent& event );

        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
        wxWindow *p_parent;

        wxComboBox *mrl_combo;

        /* Controls for the access outputs */
        wxPanel *access_panel;
        wxPanel *access_subpanels[ACCESS_OUT_NUM];
        wxCheckBox *access_checkboxes[ACCESS_OUT_NUM];

        int i_access_type;

        wxComboBox *file_combo;
        wxCheckBox *dump_checkbox;
        wxSpinCtrl *net_ports[ACCESS_OUT_NUM];
        wxTextCtrl *net_addrs[ACCESS_OUT_NUM];

        /* Controls for the SAP announces and TTL setting */
        wxPanel *misc_panel;
        wxPanel *misc_subpanels[MISC_SOUT_NUM];
        wxCheckBox *sap_checkbox;
        wxTextCtrl *announce_group;
        wxTextCtrl *announce_addr;
        wxSpinCtrl *ttl_spin;

        /* Controls for the encapsulation */
        wxPanel *encapsulation_panel;
        wxRadioButton *encapsulation_radios[ENCAPS_NUM];
        int i_encapsulation_type;

        /* Controls for transcoding */
        wxPanel *transcoding_panel;
        wxCheckBox *video_transc_checkbox;
        wxComboBox *video_codec_combo;
        wxComboBox *audio_codec_combo;
        wxCheckBox *audio_transc_checkbox;
        wxComboBox *video_bitrate_combo;
        wxComboBox *audio_bitrate_combo;
        wxComboBox *audio_channels_combo;
        wxComboBox *video_scale_combo;
        wxComboBox *subtitles_codec_combo;
        wxCheckBox *subtitles_transc_checkbox;
        wxCheckBox *subtitles_overlay_checkbox;

        /* Misc controls */
        wxCheckBox *sout_all_checkbox;
    };

};

#endif
