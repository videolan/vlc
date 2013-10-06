/*****************************************************************************
 * sout.hpp : Stream output dialog ( old-style, ala WX )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_SOUT_DIALOG_H_
#define QVLC_SOUT_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h> /* Gettext functions */

#include "ui/sout.h"
#include "util/qvlcframe.hpp"

#include <QWizard>

class QPushButton;
class QToolButton;
class QCheckBox;
class QGridLayout;
class QTextEdit;

class SoutMrl
{
public:
    SoutMrl( const QString& head = "")
    {
        mrl = head;
        b_first = true;
        b_has_bracket = false;
    }

    QString getMrl()
    {
        return mrl;
    }

    void begin( const QString& module )
    {
        if( !b_first )
            mrl += ":";
        b_first = false;

        mrl += module;
        b_has_bracket = false;
    }
    void end()
    {
        if( b_has_bracket )
            mrl += "}";
    }
    void option( const QString& option, const QString& value = "" )
    {
        if( !b_has_bracket )
            mrl += "{";
        else
            mrl += ",";
        b_has_bracket = true;

        mrl += option;

        if( !value.isEmpty() )
        {
            char *psz = config_StringEscape( qtu(value) );
            if( psz )
            {
                mrl += "=" + qfu( psz );
                free( psz );
            }
        }
    }
    void option( const QString& name, const int i_value, const int i_precision = 10 )
    {
        option( name, QString::number( i_value, i_precision ) );
    }
    void option( const QString& name, const double f_value )
    {
        option( name, QString::number( f_value ) );
    }

    void option( const QString& name, const QString& base, const int i_value, const int i_precision = 10 )
    {
        option( name, base + ":" + QString::number( i_value, i_precision ) );
    }

private:
    QString mrl;
    bool b_has_bracket;
    bool b_first;
};


class SoutDialog : public QWizard
{
    Q_OBJECT
public:
    SoutDialog( QWidget* parent, intf_thread_t *, const QString& mrl = "");
    virtual ~SoutDialog(){}

    QString getMrl(){ return mrl; }

protected:
    virtual void done( int );
private:
    Ui::Sout ui;

    QString mrl;
    QPushButton *okButton;

    intf_thread_t* p_intf;

public slots:
    void updateMRL();

private slots:
    void closeTab( int );
    void addDest();
};

#endif
