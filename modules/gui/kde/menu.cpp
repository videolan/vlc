/***************************************************************************
                          menu.cpp  -  description
                             -------------------
    begin                : Thu Apr 12 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#include "interface.h"
#include "menu.h"

#include <kaction.h>
#include <klocale.h>

KTitleMenu::KTitleMenu( intf_thread_t *p_intf, QWidget *parent, const char *name ) : KPopupMenu( parent, name )
{
    fInterfaceThread = p_intf;
    connect( this, SIGNAL( aboutToShow() ), this, SLOT( regenerateSlot() ) );
    fLanguageList = new KActionMenu( "Language", 0, this );
}

KTitleMenu::~KTitleMenu()
{
}

void KTitleMenu::regenerateSlot()
{
    // removal of elements and disconnection of signal/slots happen transparently on delete
    delete fLanguageList;
    fLanguageList = new KActionMenu( "Language", 0, this );

    int i_item = 0;
    vlc_mutex_lock( &fInterfaceThread->p_sys->p_input->stream.stream_lock );

    for( unsigned int i = 0 ;
         i < fInterfaceThread->p_sys->p_input->stream.i_es_number ;
         i++ )
    {
        if( fInterfaceThread->p_sys->p_input->stream.pp_es[i]->i_cat /* == i_cat */ )
        {
            i_item++;
            QString language( fInterfaceThread->p_sys->p_input->stream.pp_es[i]->psz_desc );
            if ( QString::null == language )
            {
                language += i18n( "Language" );
                language += " " + i_item;
            }
            KRadioAction *action = new KRadioAction( language, 0, this, "language_action" );
            fLanguageList->insert( action );

            if( /* p_es == */ fInterfaceThread->p_sys->p_input->stream.pp_es[i] )
            {
                /* don't lose p_item when we append into menu */
                //p_item_active = p_item;
            }
        }
    }

    vlc_mutex_unlock( &fInterfaceThread->p_sys->p_input->stream.stream_lock );

#if 0
    /* link the new menu to the menubar item */
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( p_root ), p_menu );

    /* acitvation will call signals so we can only do it
     * when submenu is attached to menu - to get intf_window */
    if( p_item_active != NULL )
    {
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( p_item_active ),
                                        TRUE );
    }
#endif

    /* be sure that menu is sensitive if non empty */
    if ( i_item > 0 )
    {
        fLanguageList->setEnabled( true );
    }
}

/** this method is called when the user selects a language */
void KTitleMenu::languageSelectedSlot()
{
}
