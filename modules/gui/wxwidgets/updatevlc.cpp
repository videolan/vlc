/*****************************************************************************
 * updatevlc.cpp : Check for VLC updates dialog
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <wx/progdlg.h>

#include "wxwidgets.h"

#include "vlc_block.h"
#include "vlc_stream.h"
#include "vlc_xml.h"

/* define UPDATE_VLC_OS and UPDATE_VLC_ARCH */
/* todo : move this somewhere else (isn't wx specific) */

#ifdef WIN32
#   define UPDATE_VLC_OS "windows"
#   define UPDATE_VLC_ARCH "i386"
#else
#ifdef SYS_DARWIN
#   define UPDATE_VLC_OS "macosx"
#   define UPDATE_VLC_ARCH "ppc"
#else
#   define UPDATE_VLC_OS "*"
#   define UPDATE_VLC_ARCH "*"
#endif
#endif

/* arch == "*" and os == "*" concern non OS or arch specific stuff */

#define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status"
#define UPDATE_VLC_MIRRORS_URL "http://update.videolan.org/mirrors"

#define UPDATE_VLC_DOWNLOAD_BUFFER_SIZE 2048

class UpdatesTreeItem : public wxTreeItemData
{
    public:
        UpdatesTreeItem( wxString _url ):wxTreeItemData()
        {
            url = _url;
        }
        wxString url;
};

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Close_Event,
    CheckForUpdate_Event,
    MirrorChoice_Event,
    UpdatesTreeActivate_Event
};

BEGIN_EVENT_TABLE(UpdateVLC, wxFrame)
    /* Button events */
    EVT_BUTTON(wxID_OK, UpdateVLC::OnButtonClose)
    EVT_BUTTON(CheckForUpdate_Event, UpdateVLC::OnCheckForUpdate)

    /* Choice events */
    EVT_CHOICE(MirrorChoice_Event, UpdateVLC::OnMirrorChoice)

    /* Tree events */
    EVT_TREE_ITEM_ACTIVATED(UpdatesTreeActivate_Event, UpdateVLC::OnUpdatesTreeActivate)

    /* Hide the window when the user closes the window */
    EVT_CLOSE(UpdateVLC::OnClose)

END_EVENT_TABLE()

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
UpdateVLC::UpdateVLC( intf_thread_t *_p_intf, wxWindow *p_parent ):
    wxFrame( p_parent, -1, wxU(_("Check for updates ...")), wxDefaultPosition,
             wxDefaultSize, wxDEFAULT_FRAME_STYLE )
{
    /* Initializations */
    p_intf = _p_intf;
    release_type = wxT( "stable" );
    SetIcon( *p_intf->p_sys->p_icon );
    SetAutoLayout( TRUE );

    /* Create a panel to put everything in */
    wxPanel *panel = new wxPanel( this, -1 );
    panel->SetAutoLayout( TRUE );

    updates_tree =
        new wxTreeCtrl( panel, UpdatesTreeActivate_Event, wxDefaultPosition,
                        wxSize( 400, 200 ),
                        wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxSUNKEN_BORDER );
    updates_tree->AddRoot( wxU(_("root" )), -1, -1, NULL );

    /* Place everything in sizers */
    wxBoxSizer *main_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *panel_sizer = new wxBoxSizer( wxVERTICAL );
    wxBoxSizer *subpanel_sizer = new wxBoxSizer( wxHORIZONTAL );
    panel_sizer->Add( updates_tree, 1, wxGROW | wxALL, 5 );
    wxButton *update_button =
        new wxButton( panel, CheckForUpdate_Event,
                      wxU(_("Check for updates now !")) );
    subpanel_sizer->Add( update_button, 0, wxALL, 5 );
//wxChoice constructor prototype changes with 2.5
#if wxCHECK_VERSION(2,5,0)
    wxArrayString *choices_array = new wxArrayString();
    choices_array->Add( wxT("") );
    mirrors_choice =
        new wxChoice( panel, MirrorChoice_Event, wxDefaultPosition,
                      wxSize( 200, -1 ), *choices_array );
#else
    wxString choices_array = wxT("");
    mirrors_choice =
        new wxChoice( panel, -1, wxDefaultPosition,
                      wxSize( 200, -1 ),1, *choices_array );
#endif
    subpanel_sizer->Add( mirrors_choice, 0, wxALL, 5 );
    subpanel_sizer->Layout();
    panel_sizer->Add( subpanel_sizer, 0, wxALL , 0 );
    panel_sizer->Layout();
    panel->SetSizerAndFit( panel_sizer );
    main_sizer->Add( panel, 1, wxALL | wxGROW, 0 );
    main_sizer->Layout();
    SetSizerAndFit( main_sizer );

    UpdateMirrorsChoice();
    UpdateUpdatesTree();
}


UpdateVLC::~UpdateVLC()
{
}

/* this function gets all the info from the xml files hosted on
http://update.videolan.org/ and stores it in appropriate lists */
void UpdateVLC::GetData()
{
    stream_t *p_stream = NULL;
    char *psz_eltname = NULL;
    char *psz_name = NULL;
    char *psz_value = NULL;
    char *psz_eltvalue = NULL;
    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;
    bool b_os = false;
    bool b_arch = false;

    struct update_file_t tmp_file;
    struct update_version_t tmp_version;
    std::list<update_version_t>::iterator it;
    std::list<update_file_t>::iterator it_files;

    struct update_mirror_t tmp_mirror;

    p_xml = xml_Create( p_intf );
    if( !p_xml )
    {
        msg_Err( p_intf, "Failed to open XML parser" );
        // FIXME : display error message in dialog
        return;
    }

    p_stream = stream_UrlNew( p_intf, UPDATE_VLC_STATUS_URL );
    if( !p_stream )
    {
        msg_Err( p_intf, "Failed to open %s for reading",
                 UPDATE_VLC_STATUS_URL );
        // FIXME : display error message in dialog
        return;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_intf, "Failed to open %s for parsing",
                 UPDATE_VLC_STATUS_URL );
        // FIXME : display error message in dialog
        return;
    }

    /* empty tree */
    m_versions.clear();

    /* build tree */
    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            // Error
            case -1:
                // TODO : print message
                return;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO : print message
                    return;
                }
                msg_Dbg( p_intf, "element name : %s", psz_eltname );
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        // TODO : print message
                        free( psz_eltname );
                        return;
                    }
                    msg_Dbg( p_intf, "  attribute %s = %s",
                             psz_name, psz_value );
                    if( b_os && b_arch )
                    {
                        if( strcmp( psz_eltname, "version" ) == 0 )
                        {
                            if( !strcmp( psz_name, "type" ) )
                                tmp_version.type = wxU( psz_value );
                            if( !strcmp( psz_name, "major" ) )
                                tmp_version.major = wxU( psz_value );
                            if( !strcmp( psz_name, "minor" ) )
                                tmp_version.minor = wxU( psz_value );
                            if( !strcmp( psz_name, "revision" ) )
                                tmp_version.revision = wxU( psz_value );
                            if( !strcmp( psz_name, "extra" ) )
                                tmp_version.extra = wxU( psz_value );
                        }
                        if( !strcmp( psz_eltname, "file" ) )
                        {
                            if( !strcmp( psz_name, "type" ) )
                                tmp_file.type = wxU( psz_value );
                            if( !strcmp( psz_name, "md5" ) )
                                tmp_file.md5 = wxU( psz_value );
                            if( !strcmp( psz_name, "size" ) )
                                tmp_file.size = wxU( psz_value );
                            if( !strcmp( psz_name, "url" ) )
                                tmp_file.url = wxU( psz_value );
                        }
                    }
                    if( !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, UPDATE_VLC_OS )
                           || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "os" ) )
                    {
                        b_os = true;
                    }
                    if( b_os && !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, UPDATE_VLC_ARCH )
                           || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "arch" ) )
                    {
                        b_arch = true;
                    }
                    free( psz_name );
                    free( psz_value );
                }
                if( ( b_os && b_arch && strcmp( psz_eltname, "arch" ) ) )
                {
                    if( !strcmp( psz_eltname, "version" ) )
                    {
                        it = m_versions.begin();
                        while( it != m_versions.end() )
                        {
                            if( it->type == tmp_version.type
                                && it->major == tmp_version.major
                                && it->minor == tmp_version.minor
                                && it->revision == tmp_version.revision
                                && it->extra == tmp_version.extra )
                            {
                                break;
                            }
                            it++;
                        }
                        if( it == m_versions.end() )
                        {
                            m_versions.push_back( tmp_version );
                            it = m_versions.begin();
                            while( it != m_versions.end() )
                            {
                                if( it->type == tmp_version.type
                                    && it->major == tmp_version.major
                                    && it->minor == tmp_version.minor
                                    && it->revision == tmp_version.revision
                                    && it->extra == tmp_version.extra )
                                {
                                    break;
                                }
                                it++;
                            }
                        }
                        tmp_version.type = wxT( "" );
                        tmp_version.major = wxT( "" );
                        tmp_version.minor = wxT( "" );
                        tmp_version.revision = wxT( "" );
                        tmp_version.extra = wxT( "" );
                    }
                    if( !strcmp( psz_eltname, "file" ) )
                    {
                        it->m_files.push_back( tmp_file );
                        tmp_file.type = wxT( "" );
                        tmp_file.md5 = wxT( "" );
                        tmp_file.size = wxT( "" );
                        tmp_file.url = wxT( "" );
                        tmp_file.description = wxT( "" );
                    }
                }
                free( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO : print message
                    return;
                }
                msg_Dbg( p_intf, "element end : %s", psz_eltname );
                if( !strcmp( psz_eltname, "os" ) )
                    b_os = false;
                if( !strcmp( psz_eltname, "arch" ) )
                    b_arch = false;
                free( psz_eltname );
                break;

            case XML_READER_TEXT:
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                msg_Dbg( p_intf, "  text : %s", psz_eltvalue );
                /* This doesn't look safe ... but it works */
                if( !m_versions.empty() )
                    if( !it->m_files.empty() )
                        it->m_files.back().description = wxU( psz_eltvalue );
                free( psz_eltvalue );
                break;
        }
    }

    if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_stream ) stream_Delete( p_stream );

    p_stream = stream_UrlNew( p_intf, UPDATE_VLC_MIRRORS_URL );
    if( !p_stream )
    {
        msg_Err( p_intf, "Failed to open %s for reading",
                 UPDATE_VLC_MIRRORS_URL );
        // FIXME : display error message in dialog
        return;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_intf, "Failed to open %s for parsing",
                 UPDATE_VLC_MIRRORS_URL );
        // FIXME : display error message in dialog
        return;
    }
    /* empty list */
    m_mirrors.clear();

    /* build list */
    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            // Error
            case -1:
                // TODO : print message
                return;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO : print message
                    return;
                }
                msg_Dbg( p_intf, "element name : %s", psz_eltname );
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        // TODO : print message
                        free( psz_eltname );
                        return;
                    }
                    msg_Dbg( p_intf, "  attribute %s = %s",
                             psz_name, psz_value );
                    if( !strcmp( psz_eltname, "mirror" ) )
                    {
                        if( !strcmp( psz_name, "name" ) )
                            tmp_mirror.name = wxU( psz_value );
                        if( !strcmp( psz_name, "location" ) )
                            tmp_mirror.location = wxU( psz_value );
                    }
                    if( !strcmp( psz_eltname, "url" ) )
                    {
                        if( !strcmp( psz_name, "type" ) )
                            tmp_mirror.type = wxU( psz_value );
                        if( !strcmp( psz_name, "base" ) )
                            tmp_mirror.base_url = wxU( psz_value );
                    }
                    free( psz_name );
                    free( psz_value );
                }
                if( !strcmp( psz_eltname, "url" ) )
                {
                    m_mirrors.push_back( tmp_mirror );
                    tmp_mirror.type = wxT( "" );
                    tmp_mirror.base_url = wxT( "" );
                }
                free( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO : print message
                    return;
                }
                msg_Dbg( p_intf, "element end : %s", psz_eltname );
                if( !strcmp( psz_eltname, "mirror" ) )
                {
                    tmp_mirror.name = wxT( "" );
                    tmp_mirror.location = wxT( "" );
                }
                free( psz_eltname );
                break;

            case XML_READER_TEXT:
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                msg_Dbg( p_intf, "  text : %s", psz_eltvalue );
                free( psz_eltvalue );
                break;
        }
    }


    if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_stream ) stream_Delete( p_stream );
    if( p_xml ) xml_Delete( p_xml );
}

void UpdateVLC::UpdateUpdatesTree()
{
    wxTreeItemId parent;
    std::list<update_version_t>::iterator it;
    std::list<update_file_t>::iterator it_files;
    std::list<update_mirror_t>::iterator it_mirrors;

    int selection = mirrors_choice->GetSelection();
    wxString base_url = wxT( "" );

    if( selection-- )
    {
        it_mirrors = m_mirrors.begin();
        while( it_mirrors != m_mirrors.end() && selection )
        {
            it_mirrors++;
            selection--;
        }
        if( it_mirrors != m_mirrors.end() ) base_url = it_mirrors->base_url;
    }

    /* empty tree */
    updates_tree->DeleteAllItems();

    /* build tree */
    parent=updates_tree->AddRoot( wxU(_("root" )), -1, -1, NULL );
    updates_tree->AppendItem( parent,
                             wxT( "Current version : VLC media player "PACKAGE_VERSION_MAJOR"."PACKAGE_VERSION_MINOR"."PACKAGE_VERSION_REVISION"-"PACKAGE_VERSION_EXTRA ),
                             -1, -1, new UpdatesTreeItem( wxT( "" ) ));
    it = m_versions.begin();
    while( it != m_versions.end() )
    {
        if( atoi((const char *)it->major.mb_str()) <
            atoi(PACKAGE_VERSION_MAJOR)
         || ( atoi((const char *)it->major.mb_str()) ==
              atoi(PACKAGE_VERSION_MAJOR)
         && ( atoi((const char *)it->minor.mb_str()) <
               atoi(PACKAGE_VERSION_MINOR)
         || ( atoi((const char *)it->minor.mb_str()) ==
              atoi(PACKAGE_VERSION_MINOR)
         && ( atoi((const char *)it->revision.mb_str()) <
               atoi(PACKAGE_VERSION_REVISION)
         || ( atoi((const char *)it->revision.mb_str()) ==
              atoi(PACKAGE_VERSION_REVISION) ) ) ) ) ) )
        {
            /* version is older or equal tu current version.
            FIXME : how do we handle the extra version number ? */
            it++;
            continue;
        }
        wxTreeItemId cat = updates_tree->AppendItem( parent,
                         wxT("New Version : VLC media player ")
                         + it->major + wxT(".")
                         + it->minor + wxT(".") + it->revision + wxT("-")
                         + it->extra + wxT(" (") + it->type + wxT(")"),
                         -1, -1, new UpdatesTreeItem( wxT( "" ) ));
        it_files = it->m_files.begin();
        while( it_files != it->m_files.end() )
        {
            wxString url = (it_files->url[0]=='/' ? base_url : wxT( "" ) )
                           + it_files->url;
            wxTreeItemId file =
                updates_tree->AppendItem( cat, it_files->description,
                   -1, -1, new UpdatesTreeItem( url ) );
            updates_tree->AppendItem( file,
                wxU(_("type : ")) + it_files->type,
                -1, -1, new UpdatesTreeItem( url ));
            updates_tree->AppendItem( file, wxU(_("URL : ")) + url,
                                      -1, -1, new UpdatesTreeItem( url ));
            if( it_files->size != wxT( "" ) )
                updates_tree->AppendItem( file,
                    wxU(_("file size : ")) + it_files->size,
                    -1, -1, new UpdatesTreeItem( url ));
            if( it_files->md5 != wxT( "" ) )
                updates_tree->AppendItem( file,
                    wxU(_("file md5 hash : ")) + it_files->md5,
                    -1, -1, new UpdatesTreeItem( url ));
            it_files ++;
        }
        it ++;
        updates_tree->Expand( cat );
        updates_tree->Expand( parent );
    }
}

void UpdateVLC::UpdateMirrorsChoice()
{
    std::list<update_mirror_t>::iterator it_mirrors;

    mirrors_choice->Clear();
    mirrors_choice->Append( wxU(_("Choose a mirror")) );
    it_mirrors = m_mirrors.begin();
    while( it_mirrors != m_mirrors.end() )
    {
        mirrors_choice->Append( it_mirrors->name + wxT(" (")
                                + it_mirrors->location + wxT(") [")
                                + it_mirrors->type + wxT("]") );
        it_mirrors++;
    }
    mirrors_choice->SetSelection( 0 );
}

/*void UpdateVLC::UpdateUpdateVLC()
{
    UpdateUpdatesTree();
    UpdateMirrorsChoice();
}*/

void UpdateVLC::OnButtonClose( wxCommandEvent& event )
{
    wxCloseEvent cevent;
    OnClose(cevent);
}

void UpdateVLC::OnClose( wxCloseEvent& WXUNUSED(event) )
{
    Hide();
}

void UpdateVLC::OnCheckForUpdate( wxCommandEvent& event )
{
    GetData();
    UpdateMirrorsChoice();
    UpdateUpdatesTree();
}

void UpdateVLC::OnMirrorChoice( wxCommandEvent& event )
{
    UpdateUpdatesTree();
}

void UpdateVLC::OnUpdatesTreeActivate( wxTreeEvent& event )
{
    wxString url =
      ((UpdatesTreeItem *)(updates_tree->GetItemData(event.GetItem())))->url;
    if( url != wxT( "" ) ? url[0] != '/' : false )
    {
        wxFileDialog *filedialog =
            new wxFileDialog( updates_tree, wxU(_("Save file ...")),
                              wxT(""), url.AfterLast( '/' ), wxT("*.*"),
                              wxSAVE | wxOVERWRITE_PROMPT );
        if( filedialog->ShowModal() == wxID_OK )
        {
            DownloadFile( url, filedialog->GetPath() );
        }
    }
}

void UpdateVLC::DownloadFile( wxString url, wxString dst )
{
    char *psz_local = ToLocale( dst.mb_str() );
    msg_Dbg( p_intf, "Downloading %s to %s",
             (const char *)url.mb_str(), psz_local );

    stream_t *p_stream = NULL;
    p_stream = stream_UrlNew( p_intf, (const char *)url.mb_str() );
    if( !p_stream )
    {
        msg_Err( p_intf, "Failed to open %s for reading", (const char *)url.mb_str() );
        // FIXME : display error message in dialog
        return;
    }

    FILE *p_file = NULL;
    p_file = fopen( psz_local, "w" );
    if( !p_file )
    {
        msg_Err( p_intf, "Failed to open %s for writing", psz_local );
        // FIXME : display error message in dialog
        return;
    }
    LocaleFree( psz_local );

    int i_progress = 0;
    wxProgressDialog *progressdialog =
        new wxProgressDialog( wxU(_("Downloading...")),
        wxU(wxT("Src: ") +url + wxT("\nDst: ") +dst ),
        (int)(stream_Size(p_stream)/UPDATE_VLC_DOWNLOAD_BUFFER_SIZE), NULL,
        wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE
        | wxPD_CAN_ABORT );

    void *buffer = (void *)malloc( UPDATE_VLC_DOWNLOAD_BUFFER_SIZE );
    while( stream_Read( p_stream, buffer, UPDATE_VLC_DOWNLOAD_BUFFER_SIZE ) )
    {
        fwrite( buffer, UPDATE_VLC_DOWNLOAD_BUFFER_SIZE, 1, p_file);
        if( !progressdialog->Update(++i_progress) )
        {
            free( buffer );
            fclose( p_file );
            if( p_stream ) stream_Delete( p_stream );
            progressdialog->Destroy();
            msg_Warn( p_intf, "User aborted download" );
            return;
        }
    }
    progressdialog->Destroy();
    msg_Dbg( p_intf, "Download finished" );
    free( buffer );
    fclose( p_file );
    if( p_stream ) stream_Delete( p_stream );
}
