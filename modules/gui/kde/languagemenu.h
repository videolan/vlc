/*****************************************************************************
 * languagemenu.h: the KLanguageMenuAction class
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
 *
 * Author: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#include <kaction.h>
#include "common.h"
class KLanguageMenuAction : public KRadioAction
{
    Q_OBJECT
public:
    KLanguageMenuAction(intf_thread_t*, const QString&, es_descriptor_t *, QObject *);
    ~KLanguageMenuAction();
signals:
    void toggled( bool, es_descriptor_t *);
public slots:
    void setChecked( bool );
private:
    es_descriptor_t *p_es;
    intf_thread_t    *p_intf;
};
