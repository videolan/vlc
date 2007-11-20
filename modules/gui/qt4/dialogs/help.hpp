/*****************************************************************************
 * Help.hpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _HELP_DIALOG_H_
#define _HELP_DIALOG_H_

#include <vlc_update.h>

#include "util/qvlcframe.hpp"

class QCheckBox;

class HelpDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static HelpDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new HelpDialog( p_intf );
        return instance;
    }
    virtual ~HelpDialog();

private:
    HelpDialog( intf_thread_t *);
    static HelpDialog *instance;
public slots:
    void close();
};


class AboutDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static AboutDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new AboutDialog( p_intf);
        return instance;
    }
    virtual ~AboutDialog();

private:
    AboutDialog( intf_thread_t *);
    static AboutDialog *instance;
public slots:
    void close();
};


class UpdateDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static UpdateDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new UpdateDialog( p_intf);
        return instance;
    }
    virtual ~UpdateDialog();

private:
    UpdateDialog( intf_thread_t *);
    static UpdateDialog *instance;
    QCheckBox *checkInfo;
    QCheckBox *checkSource;
    QCheckBox *checkBinary;
    QCheckBox *checkPlugin;
    update_t *p_update;
private slots:
    void close();
    void update();
};


#endif
