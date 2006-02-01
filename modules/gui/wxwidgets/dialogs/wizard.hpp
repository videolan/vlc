/*****************************************************************************
 * wizard.hpp: Stream wizard headers
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _WXVLC_WIZARD_H_
#define _WXVLC_WIZARD_H_

#include "wxwidgets.hpp"
#include <wx/wizard.h>

namespace wxvlc
{
    /* Wizard */
    class WizardDialog : public wxWizard
    {
    public:
        /* Constructor */
        WizardDialog( intf_thread_t *, wxWindow *p_parent, char *, int, int );
        virtual ~WizardDialog();
        void SetTranscode( char const *vcodec, int vb, char const *acodec,
                            int ab);
        void SetMrl( const char *mrl );
        void SetTTL( int i_ttl );
        void SetPartial( int, int );
        void SetStream( char const *method, char const *address );
        void SetTranscodeOut( char const *address );
        void SetAction( int i_action );
        int  GetAction();
        void SetSAP( bool b_enabled, const char *psz_name );
        void SetMux( char const *mux );
        void Run();
        int i_action;
        char *method;

    protected:
        int vb,ab;
        int i_from, i_to, i_ttl;
        char *vcodec , *acodec , *address , *mrl , *mux ;
        char *psz_sap_name;
        bool b_sap;
        DECLARE_EVENT_TABLE();

        intf_thread_t *p_intf;
    };
};

#endif
