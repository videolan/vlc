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

#ifndef _SOUT_DIALOG_H_
#define _SOUT_DIALOG_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "ui/sout.h"
#include "util/qvlcframe.hpp"

class QPushButton;
class QCheckBox;
class QGridLayout;
class QTextEdit;

class SoutDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static SoutDialog* getInstance( QWidget *parent, intf_thread_t *p_intf,
                                    bool transcode_only )
    {
        if( !instance )
            instance = new SoutDialog( parent, p_intf, transcode_only );
        else
        {
            /* Recenter the dialog on the parent */
            instance->setParent( parent, Qt::Dialog );
            instance->b_transcode_only = transcode_only;
            instance->toggleSout();
        }
        return instance;
    }

    virtual ~SoutDialog(){}

    QString getMrl(){ return mrl; }

private:
    Ui::Sout ui;
    static SoutDialog *instance;
    SoutDialog( QWidget* parent, intf_thread_t *,
                bool _transcode_only = false );
    QPushButton *okButton;
    QString mrl;
    bool b_transcode_only;

public slots:
    void updateMRL();

private slots:
    void ok();
    void cancel();
    void toggleSout();
    void setOptions();
    void fileBrowse();
    void setVTranscodeOptions( bool );
    void setATranscodeOptions( bool );
    void setSTranscodeOptions( bool );
    void setRawOptions( bool );
    void changeUDPandRTPmess( bool );
    void RTPtoggled( bool );
};

#endif
