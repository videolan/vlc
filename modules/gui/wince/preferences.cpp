/*****************************************************************************
 * preferences.cpp : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

#include "wince.h"

#include <winuser.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>

#include <vlc_config_cat.h>

#include "preferences_widgets.h"

#define TYPE_CATEGORY 0
#define TYPE_CATSUBCAT 1  /* Category with embedded subcategory */
#define TYPE_SUBCATEGORY 2
#define TYPE_MODULE 3

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
    bool b_advanced;

    HTREEITEM general_item;
    HTREEITEM plugins_item;
};

class PrefsPanel
{
public:

    PrefsPanel() { }
    PrefsPanel( HWND parent, HINSTANCE hInst, intf_thread_t *_p_intf,
                PrefsDialog *, module_t *p_module, char *, char *, ConfigTreeData * );
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

    bool b_advanced;

    HWND label;

    vector<ConfigControl *> config_array;
};

class ConfigTreeData
{
public:

    ConfigTreeData() { b_submodule = 0; panel = NULL; psz_name = NULL;
                       psz_help = NULL; }
    virtual ~ConfigTreeData() { delete panel;
                                free( psz_name );
                                free( psz_help ); }

    bool b_submodule;

    PrefsPanel *panel;
    module_t *p_module;
    int i_object_id;
    int i_subcat_id;
    int i_type;
    char *psz_name;
    char *psz_help;
};

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
PrefsDialog::PrefsDialog( intf_thread_t *p_intf, CBaseWindow *p_parent,
                          HINSTANCE h_inst )
  :  CBaseWindow( p_intf, p_parent, h_inst )
{
    /* Initializations */
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

    case WM_SETFOCUS:
        SHFullScreen( hwnd, SHFS_SHOWSIPBUTTON );
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
            case SB_PAGEDOWN     : newvalue += rc.bottom - rc.top - 25; break; // wrong! one page is notebook actual length
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
    module_t        *p_module = NULL;
    module_config_t *p_item;

    INITCOMMONCONTROLSEX iccex;
    RECT rcClient;
    TVITEM tvi = {0};
    TVINSERTSTRUCT tvins = {0};
    HTREEITEM hPrev;

    size_t i_capability_count = 0;
    size_t i_child_index;

    HTREEITEM category_item, subcategory_item;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;
    b_advanced = false;

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

    /*
     * Build a tree of the main options
     */
    ConfigTreeData *config_data = new ConfigTreeData;
    config_data->i_object_id = TYPE_CATEGORY;
    config_data->psz_help = strdup(MAIN_HELP);
    config_data->psz_name = strdup( "General" );
    tvi.pszText = _T("General settings");
    tvi.cchTextMax = lstrlen(_T("General settings"));
    tvi.lParam = (long)config_data;
    tvins.item = tvi;
    tvins.hInsertAfter = TVI_FIRST;
    tvins.hParent = TVI_ROOT;

    // Add the item to the tree-view control.
    hPrev = (HTREEITEM) TreeView_InsertItem( hwndTV, &tvins);
    general_item = hPrev;

    /* Build the categories list */
    p_module = module_get_main();

    unsigned int confsize;
    const char *psz_help;
    module_config_t * p_config;

    /* Enumerate config categories and store a reference so we can
     * generate their config panel when it is asked by the user. */
    p_config = module_config_get (p_module, &confsize);

    for( size_t i = 0; i < confsize; i++ )
    {
    /* Work on a new item */
        module_config_t *p_item = p_config + i;

            switch( p_item->i_type )
            {
                case CONFIG_CATEGORY:
                    if( p_item->value.i == -1 )   break; // Don't display it
                    config_data = new ConfigTreeData;
                    config_data->psz_name = strdup( config_CategoryNameGet(p_item->value.i ) );
                    psz_help = config_CategoryHelpGet( p_item->value.i );
                    if( psz_help )
                    {
                        config_data->psz_help = wraptext( strdup( psz_help ), 72 );
                    }
                    else
                    {
                        config_data->psz_help = NULL;
                    }

                    config_data->i_type = TYPE_CATEGORY;
                    config_data->i_object_id = p_item->value.i;
                    config_data->p_module =  p_module;
                    tvi.pszText = _FROMMB(config_data->psz_name);
                    tvi.cchTextMax = _tcslen(tvi.pszText);

                    /* Add the category to the tree */
                    tvi.lParam = (long)config_data;
                    tvins.item = tvi;
                    tvins.hInsertAfter = hPrev;
                    tvins.hParent = general_item; //level 3

                    // Add the item to the tree-view control.
                    hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );
                    break;

                case CONFIG_SUBCATEGORY:
                    if( p_item->value.i == -1 ) break; // Don't display it
                    /* Special case: move the "general" subcategories to their parent category */
                    if(config_data && p_item->value.i == SUBCAT_VIDEO_GENERAL ||
                        p_item->value.i == SUBCAT_ADVANCED_MISC ||
                        p_item->value.i == SUBCAT_INPUT_GENERAL ||
                        p_item->value.i == SUBCAT_INTERFACE_GENERAL ||
                        p_item->value.i == SUBCAT_SOUT_GENERAL||
                        p_item->value.i == SUBCAT_PLAYLIST_GENERAL||
                        p_item->value.i == SUBCAT_AUDIO_GENERAL )
                    {

                        config_data->i_type = TYPE_CATSUBCAT;
                        config_data->i_subcat_id = p_item->value.i;
                        free( config_data->psz_name );
                        config_data->psz_name = strdup( config_CategoryNameGet( p_item->value.i ) );

                        free( config_data->psz_help );
                        const char *psz_help = config_CategoryHelpGet( p_item->value.i );
                        if( psz_help )
                        {
                            config_data->psz_help = wraptext( strdup( psz_help ), 72 );
                        }
                        else
                        {
                            config_data->psz_help = NULL;
                        }
                        continue;
                    }

                    config_data = new ConfigTreeData;

                    config_data->psz_name = strdup(  config_CategoryNameGet( p_item->value.i ) );
                    psz_help = config_CategoryHelpGet( p_item->value.i );
                    if( psz_help )
                    {
                        config_data->psz_help = wraptext( strdup( psz_help ), 72 );
                    }
                    else
                    {
                        config_data->psz_help = NULL;
                    }
                    config_data->i_type = TYPE_SUBCATEGORY;
                    config_data->i_object_id = p_item->value.i;

                    tvi.pszText = _FROMMB(config_data->psz_name);
                    tvi.cchTextMax = _tcslen(tvi.pszText);

                    tvi.lParam = (long)config_data;
                    tvins.item = tvi;
                    tvins.hInsertAfter = hPrev;
                    tvins.hParent = hPrev;

                    // Add the item to the tree-view control.
                    TreeView_InsertItem( hwndTV, &tvins );
                    break;

            }
        }
    TreeView_SortChildren( hwndTV, general_item, 0 );
    module_config_free( p_config );

    /* List the plugins */
    module_t **p_list = module_list_get( NULL );

    /*
    * Build a tree of all the plugins
    */
    for( size_t i_index = 0; p_list[i_index]; i_index++ )
    {
        /* Take every module */
        p_module = p_list[i_index];

        /* Exclude the main module */
        if( module_is_main( p_module ) )
            continue;

        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */

        unsigned int confsize;
        i_child_index = 0;
        int i_category = 0, i_subcategory = 0, i_options = 0;
        bool b_options = false;

        p_config = module_config_get( (module_t *) p_module,&confsize);

        /* Loop through the configurations items */
        for( size_t i = 0; i < confsize; i++ )
        {
            module_config_t *p_item = p_config + i;
            if( p_item->i_type == CONFIG_CATEGORY )
                i_category = p_item->value.i;
            else if( p_item->i_type == CONFIG_SUBCATEGORY )
                i_subcategory = p_item->value.i;

            if( p_item->i_type & CONFIG_ITEM )
                b_options = true;

            if( b_options && i_category && i_subcategory )
                break;
        }
        module_config_free (p_config);

        /* Dummy item, please proceed */
        if( !b_options || i_category == 0 || i_subcategory == 0 ) continue;

        category_item = TreeView_GetChild( hwndTV, general_item );
        while(category_item != 0)
        {
            TVITEM category_tvi = {0};

            category_tvi.mask = TVIF_PARAM;
            category_tvi.lParam = NULL;
            category_tvi.hItem = category_item;
            TreeView_GetItem( hwndTV, &category_tvi );

            ConfigTreeData * data = (ConfigTreeData *)category_tvi.lParam;

            if( data->i_object_id == i_category )
            {
                subcategory_item = TreeView_GetChild( hwndTV, category_item );

                while(subcategory_item != 0)
                {
                    TVITEM subcategory_tvi = {0};

                    subcategory_tvi.mask = TVIF_PARAM;
                    subcategory_tvi.lParam = NULL;
                    subcategory_tvi.hItem = subcategory_item;
                    TreeView_GetItem( hwndTV, &subcategory_tvi );

                    ConfigTreeData * subdata = (ConfigTreeData *)subcategory_tvi.lParam;

                    if( subdata->i_object_id == i_subcategory )
                    {
                        config_data = new ConfigTreeData;

                        config_data->psz_name = strdup( module_get_object( p_module ) );
                        config_data->psz_help = NULL;
                        config_data->i_type = TYPE_MODULE;
                        config_data->i_object_id = p_item->value.i;
                        config_data->p_module = p_module;

                        tvi.pszText = _FROMMB(module_get_name( p_module, false ));
                        tvi.cchTextMax = _tcslen(tvi.pszText);

                        tvi.lParam = (long)config_data;
                        tvins.item = tvi;
                        tvins.hInsertAfter = subcategory_item;
                        tvins.hParent = subcategory_item;

                // Add the item to the tree-view control.
                        hPrev = (HTREEITEM)TreeView_InsertItem( hwndTV, &tvins );
                        break;
                    }
                    subcategory_item = TreeView_GetNextSibling( hwndTV, subcategory_item );
                }

                break;
            }

            category_item = TreeView_GetNextSibling( hwndTV, category_item );
        }

    }

    /* Sort all this mess */
    TreeView_SortChildren( hwndTV, general_item, 0 );
    category_item = TreeView_GetChild( hwndTV, general_item );
    while( category_item != 0 )
    {
        TreeView_SortChildren( hwndTV, category_item, 0 );
        category_item = TreeView_GetNextSibling( hwndTV, category_item );
    }

    /* Clean-up everything */
    module_list_free( p_list );

    TreeView_Expand( hwndTV, general_item, TVE_EXPANDPARTIAL |TVE_EXPAND );
}

PrefsTreeCtrl::~PrefsTreeCtrl()
{
}

void PrefsTreeCtrl::ApplyChanges()
{
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
    item = TreeView_GetChild( hwndTV, general_item );
    while( item != 0 )
    {
        HTREEITEM item2 = TreeView_GetChild( hwndTV, item );
        while( item2 != 0 )
        {
            HTREEITEM item3 = TreeView_GetChild( hwndTV, item2 );
            while(item3 !=0)
            {
                TVITEM tvi = {0};
                tvi.mask = TVIF_PARAM;
                tvi.hItem = item3;
                TreeView_GetItem( hwndTV, &tvi );
                config_data = (ConfigTreeData *)tvi.lParam;
                if( config_data && config_data->panel )
                {
                    config_data->panel->ApplyChanges();
                }
                item3 = TreeView_GetNextSibling( hwndTV, item3 );
            }
            item2 = TreeView_GetNextSibling( hwndTV, item2 );
        }
        item = TreeView_GetNextSibling( hwndTV, item );
    }
}

ConfigTreeData *PrefsTreeCtrl::FindModuleConfig( ConfigTreeData *config_data )
{
    if( !config_data || !config_data->p_module )
    {
        return config_data;
    }

    ConfigTreeData *config_new;
    HTREEITEM item = TreeView_GetChild( hwndTV, general_item );
    while( item != 0 )
    {
        HTREEITEM item2 = TreeView_GetChild( hwndTV, item );
        while( item2 != 0 )
        {
            HTREEITEM item3 = TreeView_GetChild( hwndTV, item2 );
            while( item3 != 0 )
            {
                TVITEM tvi = {0};
                tvi.mask = TVIF_PARAM;
                tvi.hItem = item3;
                TreeView_GetItem( hwndTV, &tvi );
                config_new = (ConfigTreeData *)tvi.lParam;
                if( config_new && config_new->p_module == config_data->p_module )
                {
                    return config_new;
                }
                item3 = TreeView_GetNextSibling( hwndTV, item3 );
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
                                config_data->p_module,
                                config_data->psz_name,
                                config_data->psz_help, config_data );
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
                        module_t *p_module, char *psz_name, char *psz_help, ConfigTreeData * config_data )
{
    module_config_t *p_item, *p_config, *p_end;

    /* Initializations */
    p_intf = _p_intf;
    p_prefs_dialog = _p_prefs_dialog;
    module_t **p_list;

    b_advanced = true;

    if( config_data->i_type == TYPE_CATEGORY )
    {
        label = CreateWindow( _T("STATIC"), _FROMMB(psz_name),
                              WS_CHILD | WS_VISIBLE | SS_LEFT,
                              5, 10 + (15 + 10), 200, 15,
                              parent, NULL, hInst, NULL );
        config_window = NULL;
    }
    else
    {
        /* Get a pointer to the module */
        if( config_data->i_type == TYPE_MODULE )
        {
            p_module = config_data->p_module;
        }
        else
        {
            /* List the plugins */
            size_t i_index;
            bool b_found = false;
            p_list = module_list_get( NULL );

            for( i_index = 0; p_list[i_index]; i_index++ )
            {
                p_module = p_list[i_index];
                if( !strcmp( module_get_object(p_module), "main" ) )
                {
                    b_found = true;
                    break;
                }
            }
            if( !p_module && !b_found )
            {
                msg_Warn( p_intf, "unable to create preferences "
                        "(main module not found)" );
                return;
            }
        }

        /* Enumerate config options and add corresponding config boxes
         * (submodules don't have config options, they are stored in the
         *  parent module) */
        unsigned confsize;
        p_config = module_config_get( (module_t *) p_module,&confsize);

        p_item = p_config;
        p_end = p_config + confsize;

        /* Find the category if it has been specified */
        if( config_data->i_type == TYPE_SUBCATEGORY ||
            config_data->i_type == TYPE_CATSUBCAT )
        {
            for( ; p_item && p_item < p_end ; p_item++ )
            {
                if( p_item->i_type == CONFIG_SUBCATEGORY &&
                    ( config_data->i_type == TYPE_SUBCATEGORY &&
                    p_item->value.i == config_data->i_object_id ) ||
                    ( config_data->i_type == TYPE_CATSUBCAT &&
                    p_item->value.i == config_data->i_subcat_id ) )
                {
                    break;
                }
            }
        }

        /* Add a head title to the panel */
        const char *psz_head;
        if( config_data->i_type == TYPE_SUBCATEGORY ||
            config_data->i_type == TYPE_CATSUBCAT )
        {
            psz_head = config_data->psz_name;
            p_item++;
        }
        else
        {
            psz_head = module_GetLongName(p_module);
        }

        label = CreateWindow( _T("STATIC"), _FROMMB(psz_head ?
                psz_head : _("Unknown")),
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
        for( ; p_item && p_item < p_end ; p_item++ )
        {
            /* If a category has been specified, check we finished the job */
            if( ( ( config_data->i_type == TYPE_SUBCATEGORY &&
                    p_item->value.i != config_data->i_object_id ) ||
                    ( config_data->i_type == TYPE_CATSUBCAT  &&
                    p_item->value.i != config_data->i_subcat_id ) ) &&
                    (p_item->i_type == CONFIG_CATEGORY ||
                    p_item->i_type == CONFIG_SUBCATEGORY ) )
                break;

            ConfigControl *control = NULL;

            control = CreateConfigControl( VLC_OBJECT(p_intf),
                                     p_item, config_window,
                                     hInst, &y_pos );

            /* Don't add items that were not recognized */
            if( control == NULL ) continue;

            /* Add the config data to our array so we can keep a trace of it */
            config_array.push_back( control );
        }
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
            var_Set( p_intf->p_libvlc, control->GetName(), val );
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
