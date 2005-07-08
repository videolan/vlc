/*****************************************************************************
 * preferences.h: includes for the preferences window
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no> Mon Aug 12 2002
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
#ifndef _KDE_PREFERENCES_H_
#define _KDE_PREFERENCES_H_
#include "common.h"
#include <kdialogbase.h>

#include "QConfigItem.h"
class KPreferences : KDialogBase
{
    Q_OBJECT
 public:
    KPreferences(intf_thread_t *p_intf, const char *psz_module_name,
                 QWidget *parent, const QString &caption=QString::null);
    ~KPreferences();
    bool isConfigureable(QString module);

 public slots:
    void slotApply();
    void slotOk();
    void slotUser1();

 private:
    intf_thread_t *p_intf;
};
#endif
