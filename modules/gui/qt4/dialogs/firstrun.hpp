/*****************************************************************************
 * firstrun : First Run dialogs
 ****************************************************************************
 * Copyright © 2009 VideoLAN
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt4.hpp"

#include <QWidget>
class ConfigControl;
class FirstRun : public QWidget
{
    Q_OBJECT
    public:
        FirstRun( QWidget *, intf_thread_t * );
    private:
        QList<ConfigControl *> controlsList;
        intf_thread_t *p_intf;
        void buildPrivDialog();
    private slots:
        void save();
};

