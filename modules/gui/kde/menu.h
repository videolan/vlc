/***************************************************************************
                          menu.h  -  description
                             -------------------
    begin                : Thu Apr 12 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _KDE_MENU_H_
#define _KDE_MENU_H_

#include "common.h"

#include <qwidget.h>
#include <kpopupmenu.h>

class KActionMenu;

/**
  *@author andres
  */

class KTitleMenu : public KPopupMenu
{
    Q_OBJECT
    public: 
        KTitleMenu( intf_thread_t *p_intf, QWidget *parent=0,
                    const char *name=0 );
        ~KTitleMenu();

    private:
        intf_thread_t      *fInterfaceThread;
        KActionMenu        *fLanguageList;

    private slots: // Private slots
        /** this method regenerates the popup menu */
        void regenerateSlot();

        /** this method is called when the user selects a language */
        void languageSelectedSlot();

};

#endif /* _KDE_MENU_H_ */
