/*****************************************************************************
 * preferences.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                            /* strerror() */
#include <stdio.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "wince.h"

#include <winuser.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#include <vlc_config_cat.h>

#include "preferences_widgets.h"

#define GENERAL_ID 1242
#define PLUGIN_ID 1243
#define CAPABILITY_ID 1244

/*****************************************************************************
 * Event Table.
 *****************************************************************************/

/* IDs for the controls and the menu commands */
enum
{
    Notebook_Event,
    MRL_Event,
    ResetAll_Event,
    Advanced_Event
};

/*****************************************************************************
 * Classes declarations.
 *****************************************************************************/
class ConfigTreeData;
class PrefsTreeCtrl
{
public:

    PrefsTreeCtrl() { }
    PrefsTreeCtrl( intf_thread_t *_p_intf, PrefsDialog *p_prefs_dialog,
                   HWND hwnd, HINSTANCE _hInst );
    virtual ~PrefsTreeCtrl();

    void ApplyChanges();
    /*void CleanChanges();*/

    void OnSelectTreeItem( LPNM_TREEVIEW pnmtv, HWND parent, HINSTANCE hInst );
        
    ConfigTreeData *FindModuleConfig( ConfigTreeData *config_data );

    HWND hwndTV;

private:
    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;
    vlc_bool_t b_advanced;

    HTREEITEM general_item;
    HTREEITEM plugins_item;
};

class PrefsPanel
{
public:

    PrefsPanel() { }
    PrefsPanel( HWND parent, HINSTANCE hInst, intf_thread_t *_p_intf,
                PrefsDialog *, int i_object_id, char *, char * );
    virtual ~PrefsPanel() {}

    void Hide();
    void Show();

    HWND config_window;

    int oldvalue;
    int maxvalue;

    void ApplyChanges();

private:
    intf_thread_t *p_intf;
    PrefsDialog *p_prefs_dialog;

    vlc_bool_t b_advanced;

    HWND label;

    vector<ConfigControl *> config_array;
};

class ConfigTreeData
{
public:

    ConfigTreeData() { b_submodule = 0; panel = NULL; psz_section = NULL;
                       psz_help = NULL; }
    virtual ~ConfigTreeData() { if( panel ) delete panel;
                                if( psz_section) free(psz_section);
                                if( psz_help) free(psz_help); }

    vlc_bool_t b_submodule;

    PrefsPanel *panel;
    int i_object_id;
    char *psz_section;
    char *psz_help;
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PrefsDialog::PrefsDialog( intf_thread_t *_p_intf, HINSTANCE _hInst )
{
    /* Initializations */
    p_intf = _p_intf;
    hInst = _hInst;
    prefs_tree = NULL;
}

/***********************************************************************

FUNCTION: 
  WndProc

PURPOSE: 
  Processes messages sent to the main window.

***********************************************************************/
LRESULT PrefsDialog::WndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    SHINITDLGINFO shidi;
    SHMENUBARINFO mbi;
    RECT rcClient;

    switch( msg )
    {
    case WM_INITDIALOG:
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN |
            SHIDIF_FULLSCREENNOMENUBAR;//SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hwnd;
        SHInitDialog( &shidi );

        //Create the menubar
        memset( &mbi, 0, sizeof (SHMENUBARINFO) );
        mbi.cbSize     = sizeof (SHMENUBARINFO);
        mbi.hwndParent = hwnd;
        mbi.dwFlags    = SHCMBF_EMPTYBAR;
        mbi.hInstRes   = hInst;

        if( !SHCreateMenuBar(&mbi) )
        {
            MessageBox(hwnd, _T("SHCreateMenuBar Failed"), _T("Error"), MB_OK);
            //return -1;
        }

        hwndCB = mbi.hwndMB;

        // Get the client area rect to put the panels in
        GetClientRect(hwnd, &rcClient);

        /* Create the buttons */            
        advanced_checkbox =
            CreateWindow( _T("BUTTON"), _T("Advanced options"),
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                        5, 10, 15, 15, hwnd, NULL, hInst, NULL );
        SendMessage( advanced_checkbox, BM_SETCHECK, BST_UNCHECKED, 0 );

        advanced_label = CreateWindow( _T("STATIC"), _T("Advanced options"),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        5 + 15 + 5, 10, 110, 15,
                        hwnd, NULL, hInst, NULL);

        if( config_GetInt( p_intf, "advanced" ) )
        {
            SendMessage( advanced_checkbox, BM_SETCHECK, BST_CHECKED, 0 );
            /*dummy_event.SetInt(TRUE);
              OnAdvanced( dummy_event );*/
        }

        reset_button = CreateWindow( _T("BUTTON"), _T("Reset All"),
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        rcClient.right - 5 - 80, 10 - 3, 80, 15 + 6,
                        hwnd, NULL, hInst, NULL );

        /* Create the preferences tree control */
        prefs_tree = new PrefsTreeCtrl( p_intf, this, hwnd, hInst );

        UpdateWindow( hwnd );
        break;

    case WM_CLOSE:
        EndDialog( hwnd, LOWORD( wp ) );
        break;

    case WM_COMMAND:
        if( LOWORD(wp) == IDOK )
        {
            OnOk();
            EndDialog( hwnd, LOWORD( wp ) );
        }
        break;

    case WM_NOTIFY:

        if( lp && prefs_tree &&
            ((LPNMHDR)lp)->hwndFrom == prefs_tree->hwndTV &&
            ((LPNMHDR)lp)->code == TVN_SELCHANGED )
        {
            prefs_tree->OnSelectTreeItem( (NM_TREEVIEW FAR *)(LPNMHDR)lp,
                                          hwnd, hInst );
        }
        break;

    case WM_VSCROLL:
    {
        TVITEM tvi = {0};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = TreeView_GetSelection( prefs_tree->hwndTV );
	if( !tvi.hItem ) break;

        if( !TreeView_GetItem( prefs_tree->hwndTV, &tvi ) ) break;

        ConfigTreeData *config_data =
            prefs_tree->FindModuleConfig( (ConfigTreeData *)tvi.lParam );
        if( config_data && hwnd == config_data->panel->config_window ) 
        {
            int dy;
            RECT rc;
            GetWindowRect( hwnd, &rc);
            int newvalue = config_data->panel->oldvalue;
            switch ( GET_WM_VSCROLL_CODE(wp,lp) ) 
            {
            case SB_BOTTOM       : newvalue = 0; break;
            case SB_TOP          : newvalue = config_data->panel->maxvalue; break;
            case SB_LINEDOWN     : newvalue += 10; break;
            case SB_PAGEDOWN     : newvalue += rc.bottom - rc.top - 25; break; // faux ! une page c'est la longueur réelle de notebook
            case SB_LINEUP       : newvalue -= 10; break;
            case SB_PAGEUP       : newvalue -= rc.bottom - rc.top - 25; break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK   : newvalue = GET_WM_VSCROLL_POS(wp,lp); break;
            }
            newvalue = max(0,min(config_data->panel->maxvalue,newvalue));
            SetScrollPos( hwnd,SB_VERT,newvalue,TRUE);//SB_CTL si hwnd=hwndScrollBar, SB_VERT si window
            dy = config_data->panel->oldvalue - newvalue;

            ScrollWindowEx( hwnd, 0, dy, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN );
            UpdateWindow ( hwnd);

            config_data->panel->oldvalue = newvalue;                                
        }
        break;
    }

    default:
        break;
    }

    return FALSE;
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/

/*****************************************************************************
 * Events methods.
 *****************************************************************************/
void PrefsDialog::OnOk( void )
{
    prefs_tree->ApplyChanges();
    config_SaveConfigFile( p_intf, NULL );
}

/*****************************************************************************
 * PrefsTreeCtrl class definition.
 *****************************************************************************/
PrefsTreeCtrl::PrefsTreeCtrl( intf_thread_t *_p_intf,
                              PrefsDialog *_p_prefs_dialog, HWND hwnd,
                              HINSTANCE hInst )
{
    vlc_list_t      *p_list;
    module_t        *p_module;
    module_config_t *p_item;
    int i_index;

    INITCOMMONCONTROLSEX iccex;
    RECT rcClient;
    TVITEM tvi = {0}; 
    TVINSERTSTRUCT tvins = {0}; 
    HTREEITEM hPrev;

    size_t i_capability_count = 0;
    size_t i_child_index;

    HTREEITEM capability_item;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;
    b_advanced = VLC_FALSE;

    /* Create a tree view */
    // Initialize the INITCOMMONCONTROLSEX structure.
    iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccex.dwICC = ICC_TREEVIEW_CLASSES;

    // Registers Statusbar control classes from the common control dll
    InitCommonControlsEx( &iccex );

    // Get the client area rect to put the tv in
    GetClientRect(hwnd, &rcClient);

    // Create the tree-view control.
    hwndTV = CreateWindowEx( 0, WC_TREEVIEW, NULL,
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES |
        TVS_LINESATROOT | TVS_HASBUTTONS,
        5, 10 + 2*(15 + 10) + 105 + 5, rcClient.right - 5 - 5, 6*15,
        hwnd, NULL, hInst, NULL );

    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;

    /* List the plugins */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    if( !p_list ) return;

    /*
     * Build a tree of the main options
     */
    ConfigTreeData *config_data = new ConfigTreeData;
    config_data->i_object_id = GENERAL_ID;
    config_data->psz_help = strdup("nothing");//strdup( GENERAL_HELP );
    config_data->psz_section = strdup( GENERAL_TITLE );
    tvi.pszText = _T("General settings");
    tvi.cchTextMax = lstrlen(_T("General settings"));
    tvi.lParam = (long)config_data;
    tvins.item = tvi;
    tvins.hInsertAfter = TVI_FIRST;
    tvins.hParent = TVI_ROOT;

    // Add the item to the tree-view control.
    hPrev = (HTREEITEM) TreeView_InsertItem( hwndTV, &tvins);
    general_item = hPrev;

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_module->psz_object_name, "main" ) )
            break;
    }
    if( i_index < p_list->i_count )
    {
        /* We found the main module */

        /* Enumerate config categories and store a reference so we can
         * generate their config panel them when it is asked by the user. */
        p_item = p_module->p_config;

        if( p_item ) do
        {
            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
                ConfigTreeData *config_data = new ConfigTreeData;
                config_data->psz_section = strdup( p_item->psz_text );
                if( p_item->psz_longtext )
                {
                    config_data->psz_help =
                        strdup( p_item->psz_longtext );
                }
                else
                {
                    config_data->psz_help = NULL;
                }
                config_data->i_object_id = p_module->i_object_id;

                /* Add the category to the tree */
                // Set the text of the item. 
                tvi.pszText = _FROMMB(p_item->psz_text); 
                tvi.cchTextMax = _tcslen(tvi.pszText);
                tvi.lParam = (long)config_data;
                tvins.item = tvi;
                tvins.hInsertAfter = hPrev; 
                tvins.hParent = general_item; //level 3
    
                // Add the item to the tree-view control. 
                hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );

                break;
            }
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );

        TreeView_SortChildren( hwndTV, general_item, 0 );
    }
        
    /*
     * Build a tree of all the plugins
     */
    config_data = new ConfigTreeData;
    config_data->i_object_id = PLUGIN_ID;
    config_data->psz_help = strdup("nothing");//strdup( PLUGIN_HELP );
    config_data->psz_section = strdup("nothing");//strdup( PLUGIN_TITLE );
    tvi.pszText = _T("Modules");
    tvi.cchTextMax = lstrlen(_T("Modules"));
    tvi.lParam = (long)config_data;
    tvins.item = tvi;
    tvins.hInsertAfter = TVI_LAST;
    tvins.hParent = TVI_ROOT;

    // Add the item to the tree-view control.
    hPrev = (HTREEITEM) TreeView_InsertItem( hwndTV, &tvins);
    plugins_item = hPrev;

    i_capability_count = 0;
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        i_child_index = 0;

        p_module = (module_t *)p_list->p_values[i_index].p_object;

        /* Exclude the main module */
        if( !strcmp( p_module->psz_object_name, "main" ) )
            continue;

        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */
        if( p_module->b_submodule )
            p_item = ((module_t *)p_module->p_parent)->p_config;
        else
            p_item = p_module->p_config;

        if( !p_item ) continue;
        do
        {
            if( p_item->i_type & CONFIG_ITEM )
                break;
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );
        if( p_item->i_type == CONFIG_HINT_END ) continue;

        /* Find the capability child item */
        /*long cookie; size_t i_child_index;*/
        capability_item = TreeView_GetChild( hwndTV, plugins_item );
        while( capability_item != 0 )
        {
            TVITEM capability_tvi = {0};
            TCHAR psz_text[256];
            i_child_index++;

            capability_tvi.mask = TVIF_TEXT;
            capability_tvi.pszText = psz_text;
            capability_tvi.cchTextMax = 256;
            capability_tvi.hItem = capability_item;
            TreeView_GetItem( hwndTV, &capability_tvi );
            if( !strcmp( _TOMB(capability_tvi.pszText),
                         p_module->psz_capability ) ) break;
 
            capability_item =
                TreeView_GetNextSibling( hwndTV, capability_item );
        }

        if( i_child_index == i_capability_count &&
            p_module->psz_capability && *p_module->psz_capability )
        {
            /* We didn't find it, add it */
            ConfigTreeData *config_data = new ConfigTreeData;
            config_data->psz_section =
                strdup( GetCapabilityHelp( p_module->psz_capability , 1 ) );
            config_data->psz_help =
                strdup( GetCapabilityHelp( p_module->psz_capability , 2 ) );
            config_data->i_object_id = CAPABILITY_ID;
            tvi.pszText = _FROMMB(p_module->psz_capability);
            tvi.cchTextMax = _tcslen(tvi.pszText);
            tvi.lParam = (long)config_data;
            tvins.item = tvi;
            tvins.hInsertAfter = plugins_item; 
            tvins.hParent = plugins_item;// level 3

            // Add the item to the tree-view control. 
            capability_item = (HTREEITEM) TreeView_InsertItem( hwndTV, &tvins);

            i_capability_count++;
        }

        /* Add the plugin to the tree */
        ConfigTreeData *config_data = new ConfigTreeData;
        config_data->b_submodule = p_module->b_submodule;
        config_data->i_object_id = p_module->b_submodule ?
            ((module_t *)p_module->p_parent)->i_object_id :
            p_module->i_object_id;
        config_data->psz_help = NULL;
        tvi.pszText = _FROMMB(p_module->psz_object_name);
        tvi.cchTextMax = _tcslen(tvi.pszText);
        tvi.lParam = (long)config_data;
        tvins.item = tvi;
        tvins.hInsertAfter = capability_item; 
        tvins.hParent = capability_item;// level 4

        // Add the item to the tree-view control. 
        TreeView_InsertItem( hwndTV, &tvins );
    }

    /* Sort all this mess */
    /*long cookie; size_t i_child_index;*/
    TreeView_SortChildren( hwndTV, plugins_item, 0 );
    capability_item = TreeView_GetChild( hwndTV, plugins_item );
    while( capability_item != 0 )
    {
        TreeView_SortChildren( hwndTV, capability_item, 0 );
        capability_item = TreeView_GetNextSibling( hwndTV, capability_item );
    }

    /* Clean-up everything */
    vlc_list_release( p_list );

    TreeView_Expand( hwndTV, general_item, TVE_EXPANDPARTIAL |TVE_EXPAND );
}

PrefsTreeCtrl::~PrefsTreeCtrl()
{
}

void PrefsTreeCtrl::ApplyChanges()
{
    /*long cookie, cookie2;*/
    ConfigTreeData *config_data;

    /* Apply changes to the main module */
    HTREEITEM item = TreeView_GetChild( hwndTV, general_item );
    while( item != 0 )
    {
        TVITEM tvi = {0};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = item;
        TreeView_GetItem( hwndTV, &tvi );
        config_data = (ConfigTreeData *)tvi.lParam;
        if( config_data && config_data->panel )
        {
            config_data->panel->ApplyChanges();
        }

        item = TreeView_GetNextSibling( hwndTV, item );
    }

    /* Apply changes to the plugins */
    item = TreeView_GetChild( hwndTV, plugins_item );
    while( item != 0 )
    {
        HTREEITEM item2 = TreeView_GetChild( hwndTV, item );
        while( item2 != 0 )
        {       
            TVITEM tvi = {0};
            tvi.mask = TVIF_PARAM;
            tvi.hItem = item2;
            TreeView_GetItem( hwndTV, &tvi );
            config_data = (ConfigTreeData *)tvi.lParam;
            if( config_data && config_data->panel )
            {
                config_data->panel->ApplyChanges();
            }
            item2 = TreeView_GetNextSibling( hwndTV, item2 );
        }
        item = TreeView_GetNextSibling( hwndTV, item );
    }
}

ConfigTreeData *PrefsTreeCtrl::FindModuleConfig( ConfigTreeData *config_data )
{
    /* We need this complexity because submodules don't have their own config
     * options. They use the parent module ones. */

    if( !config_data || !config_data->b_submodule )
    {
        return config_data;
    }

    /*long cookie, cookie2;*/
    ConfigTreeData *config_new;
    HTREEITEM item = TreeView_GetChild( hwndTV, plugins_item );
    while( item != 0 )
    {
        HTREEITEM item2 = TreeView_GetChild( hwndTV, item );
        while( item2 != 0 )
        {       
            TVITEM tvi = {0};
            tvi.mask = TVIF_PARAM;
            tvi.hItem = item2;
            TreeView_GetItem( hwndTV, &tvi );
            config_new = (ConfigTreeData *)tvi.lParam;
            if( config_new && !config_new->b_submodule &&
                config_new->i_object_id == config_data->i_object_id )
            {
                return config_new;
            }
            item2 = TreeView_GetNextSibling( hwndTV, item2 );
        }
        item = TreeView_GetNextSibling( hwndTV, item );
    }

    /* Found nothing */
    return NULL;
}

void PrefsTreeCtrl::OnSelectTreeItem( LPNM_TREEVIEW pnmtv, HWND parent,
                                      HINSTANCE hInst )
{
    ConfigTreeData *config_data = NULL;

    if( pnmtv->itemOld.hItem )
        config_data = FindModuleConfig( (ConfigTreeData *)pnmtv->itemOld.lParam );

    if( config_data && config_data->panel )
    {
        config_data->panel->Hide();
    }

    /* Don't use event.GetItem() because we also send fake events */
    TVITEM tvi = {0};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = TreeView_GetSelection( hwndTV );
    TreeView_GetItem( hwndTV, &tvi );
    config_data = FindModuleConfig( (ConfigTreeData *)tvi.lParam );
    if( config_data )
    {
        if( !config_data->panel )
        {
            /* The panel hasn't been created yet. Let's do it. */
            config_data->panel =
                new PrefsPanel( parent, hInst, p_intf, p_prefs_dialog,
                                config_data->i_object_id,
                                config_data->psz_section,
                                config_data->psz_help );
        }
        else
        {
            config_data->panel->Show();
        }
    }
}

/*****************************************************************************
 * PrefsPanel class definition.
 *****************************************************************************/
PrefsPanel::PrefsPanel( HWND parent, HINSTANCE hInst, intf_thread_t *_p_intf,
                        PrefsDialog *_p_prefs_dialog,
                        int i_object_id, char *psz_section, char *psz_help )
{
    module_config_t *p_item;
    module_t *p_module = NULL;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;

    b_advanced = VLC_TRUE;

    if( i_object_id == PLUGIN_ID || i_object_id == GENERAL_ID ||
        i_object_id == CAPABILITY_ID )
    {
        label = CreateWindow( _T("STATIC"), _FROMMB(psz_section),
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              5, 10 + (15 + 10), 200, 15,
                              parent, NULL, hInst, NULL );
        config_window = NULL;
    }
    else
    {
        /* Get a pointer to the module */
        p_module = (module_t *)vlc_object_get( p_intf,  i_object_id );
        if( p_module->i_object_type != VLC_OBJECT_MODULE )
        {
            /* 0OOoo something went really bad */
            return;
        }

        /* Enumerate config options and add corresponding config boxes
         * (submodules don't have config options, they are stored in the
         *  parent module) */
        if( p_module->b_submodule )
            p_item = ((module_t *)p_module->p_parent)->p_config;
        else
            p_item = p_module->p_config;

        /* Find the category if it has been specified */
        if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY )
        {
            while( !(p_item->i_type == CONFIG_HINT_CATEGORY) ||
                   strcmp( psz_section, p_item->psz_text ) )
            {
                if( p_item->i_type == CONFIG_HINT_END ) break;
                p_item++;
            }
        }

        /* Add a head title to the panel */
        label = CreateWindow( _T("STATIC"), _FROMMB(psz_section ?
                        p_item->psz_text : p_module->psz_longname),
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        5, 10 + (15 + 10), 250, 15,
                        parent, NULL, hInst, NULL );

        WNDCLASS wc;
        memset( &wc, 0, sizeof(wc) );
        wc.style          = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc    = (WNDPROC) _p_prefs_dialog->BaseWndProc;
        wc.cbClsExtra     = 0;
        wc.cbWndExtra     = 0;
        wc.hInstance      = hInst;
        wc.hIcon          = 0;
        wc.hCursor        = 0;
        wc.hbrBackground  = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName   = 0;
        wc.lpszClassName  = _T("PrefsPanelClass");
        RegisterClass(&wc);

        RECT rc;
        GetWindowRect( parent, &rc);
        config_window = CreateWindow( _T("PrefsPanelClass"),
                        _T("config_window"),
                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
                        5, 10 + 2*(15 + 10), rc.right - 5 - 7, 105,
                        parent, NULL, hInst, (void *) _p_prefs_dialog );

        int y_pos = 5;
        if( p_item ) do
        {
            /* If a category has been specified, check we finished the job */
            if( psz_section && p_item->i_type == CONFIG_HINT_CATEGORY &&
                strcmp( psz_section, p_item->psz_text ) )
                break;

            ConfigControl *control =
                CreateConfigControl( VLC_OBJECT(p_intf),
                                     p_item, config_window,
                                     hInst, &y_pos );

            /* Don't add items that were not recognized */
            if( control == NULL ) continue;

            /* Add the config data to our array so we can keep a trace of it */
            config_array.push_back( control );
        }
        while( p_item->i_type != CONFIG_HINT_END && p_item++ );
                
        GetWindowRect( config_window, &rc);
        maxvalue = y_pos - (rc.bottom - rc.top) + 5;
        oldvalue = 0;
        SetScrollRange( config_window, SB_VERT, 0, maxvalue, TRUE );
    }
}

void PrefsPanel::Hide()
{
    ShowWindow( label, SW_HIDE );
    if( config_window ) ShowWindow( config_window, SW_HIDE );
}

void PrefsPanel::Show()
{
    ShowWindow( label, SW_SHOW );
    if( config_window ) ShowWindow( config_window, SW_SHOW );
}

void PrefsPanel::ApplyChanges()
{
    vlc_value_t val;

    for( size_t i = 0; i < config_array.size(); i++ )
    {
        ConfigControl *control = config_array[i];

        switch( control->GetType() )
        {
        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:
        case CONFIG_ITEM_MODULE:
            config_PutPsz( p_intf, control->GetName(),
                           control->GetPszValue() );
            break;
        case CONFIG_ITEM_KEY:
            /* So you don't need to restart to have the changes take effect */
            val.i_int = control->GetIntValue();
            var_Set( p_intf->p_vlc, control->GetName(), val );
        case CONFIG_ITEM_INTEGER:
        case CONFIG_ITEM_BOOL:
            config_PutInt( p_intf, control->GetName(),
                           control->GetIntValue() );
            break;
        case CONFIG_ITEM_FLOAT:
            config_PutFloat( p_intf, control->GetName(),
                             control->GetFloatValue() );
            break;
        }
    }
}
