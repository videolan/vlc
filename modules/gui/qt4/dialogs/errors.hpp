/*****************************************************************************
 * errors.hpp : Errors
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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

#ifndef QVLC_ERRORS_DIALOG_H_
#define QVLC_ERRORS_DIALOG_H_ 1

#include "util/qvlcframe.hpp"

class QPushButton;
class QCheckBox;
class QGridLayout;
class QTextEdit;

class ErrorsDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static ErrorsDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new ErrorsDialog( (QWidget *)p_intf->p_sys->p_mi, p_intf );
        return instance;
    }
    virtual ~ErrorsDialog() {};

    void addError( const QString&, const QString& );
    /*void addWarning( QString, QString );*/
private:
    ErrorsDialog( QWidget *parent, intf_thread_t * );
    static ErrorsDialog *instance;
    void add( bool, const QString&, const QString& );

    QCheckBox *stopShowing;
    QTextEdit *messages;
private slots:
    void close();
    void clear();
    void dontShow();
};

#endif
