/*****************************************************************************
 * vlm_panel.hpp: Header for the VLM panel
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: wxwidgets.h 12502 2005-09-09 19:38:01Z gbazin $
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

#ifndef _WXVLC_VLMPANEL_H_
#define _WXVLC_VLMPANEL_H_

#include "wxwidgets.hpp"
#include <wx/notebook.h>

#include <vector>
using namespace std;


class VLMWrapper;
class VLMStream;
class VLMBroadcastStream;
class VLMVODStream;

namespace wxvlc
{
    class VLMStreamPanel;
    class VLMBroadcastStreamPanel;
    class VLMVODStreamPanel;

    /** This class is the panel to add or edit a VLM stream
     * It can be embedded in the main VLM panel or on a separate frame
     */
    class VLMAddStreamPanel : public wxPanel
    {
    public:
        VLMAddStreamPanel( intf_thread_t *, wxWindow *, VLMWrapper *,
                           vlc_bool_t, vlc_bool_t );
        virtual ~VLMAddStreamPanel();
        void Load( VLMStream *);
    private:
        void OnCreate( wxCommandEvent& );
        void OnClear( wxCommandEvent& );

        wxTextCtrl *name_text;
        wxTextCtrl *input_text;
        wxTextCtrl *output_text;

        wxCheckBox *enabled_checkbox;
        wxCheckBox *loop_checkbox;

        intf_thread_t *p_intf;
        VLMWrapper *p_vlm;


        vlc_bool_t b_edit, b_broadcast;

        DECLARE_EVENT_TABLE();
    };

    /**
     * This class is the main VLM Manager panel
     */
    class VLMPanel : public wxPanel
    {
    public:
        VLMPanel( intf_thread_t *p_intf, wxWindow * );
        virtual ~VLMPanel();

        void Update();

    protected:

    private:
        VLMWrapper *p_vlm;
        intf_thread_t *p_intf;

        wxTimer timer;
        void OnTimer( wxTimerEvent &);
        /** Notebook */
        wxNotebook *p_notebook;
        DECLARE_EVENT_TABLE();

        /* Broadcast stuff */
        vector<VLMBroadcastStreamPanel *> broadcasts;
        wxPanel *broadcasts_panel;
        wxBoxSizer *broadcasts_sizer;
        wxScrolledWindow *scrolled_broadcasts;
        wxBoxSizer *scrolled_broadcasts_sizer;
        wxPanel *BroadcastPanel( wxWindow *);
        wxPanel *AddBroadcastPanel( wxPanel *);
        void AppendBroadcast( VLMBroadcastStream *);
        void RemoveBroadcast( VLMBroadcastStreamPanel *);

        /* VOD stuff */
        vector<VLMVODStreamPanel *> vods;
        wxPanel *vods_panel;
        wxBoxSizer *vods_sizer;
        wxScrolledWindow *scrolled_vods;
        wxBoxSizer *scrolled_vods_sizer;
        wxPanel *VODPanel( wxWindow *);
        wxPanel *AddVODPanel( wxPanel *);
        void AppendVOD( VLMVODStream *);
        void RemoveVOD( VLMVODStreamPanel *);

    };

    /** This class is the standard VLM frame
     * It only consists of the VLM panel
     */
    class VLMFrame: public wxFrame
    {
    public:
        VLMFrame( intf_thread_t *p_intf, wxWindow * );
        virtual ~VLMFrame();

        void Update();
    private:
        VLMPanel *vlm_panel;
        DECLARE_EVENT_TABLE();
        void OnClose( wxCloseEvent& );
    };

    /** This class is the edit dialog for a stream
     * It only consists of the VLM edit panel
     */
    class VLMEditStreamFrame: public wxFrame
    {
    public:
        VLMEditStreamFrame( intf_thread_t *p_intf, wxWindow *,
                            VLMWrapper * , vlc_bool_t, VLMStream * );
        virtual ~VLMEditStreamFrame();

    private:
        VLMAddStreamPanel *vlm_panel;
    };
};

#endif
