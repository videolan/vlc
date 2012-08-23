/*****************************************************************************
 * sout_widgets.hpp : Widgets for stream output destination boxes
 ****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef SOUT_WIDGETS_H
#define SOUT_WIDGETS_H

#include "qt4.hpp"

#include <QGroupBox>

class QLineEdit;
class QLabel;
class QSpinBox;

class SoutInputBox : public QGroupBox
{
    public:
        SoutInputBox( QWidget *_parent = NULL, const QString& mrl = "" );

        void setMRL( const QString& );
    private:
        QLineEdit *sourceLine;
        QLabel *sourceValueLabel;

};

class VirtualDestBox : public QWidget
{
    Q_OBJECT
    public:
        VirtualDestBox( QWidget *_parent = NULL ) : QWidget( _parent ){}
        virtual QString getMRL( const QString& ) = 0;
    protected:
        QString mrl;
    signals:
        void mrlUpdated();
};

class FileDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        FileDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *fileEdit;
    private slots:
        void fileBrowse();
};

class HTTPDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        HTTPDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *HTTPEdit;
        QSpinBox *HTTPPort;
};

class MMSHDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        MMSHDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *MMSHEdit;
        QSpinBox *MMSHPort;
};

class RTSPDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        RTSPDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *RTSPEdit;
        QSpinBox *RTSPPort;
};

class UDPDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        UDPDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *UDPEdit;
        QSpinBox *UDPPort;
};

class RTPDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        RTPDestBox( QWidget *_parent = NULL, const char *mux = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *RTPEdit;
        QSpinBox *RTPPort;
        QLineEdit *SAPName;
        QString mux;
};

class ICEDestBox: public VirtualDestBox
{
    Q_OBJECT
    public:
        ICEDestBox( QWidget *_parent = NULL );
        virtual QString getMRL( const QString& );
    private:
        QLineEdit *ICEEdit;
        QLineEdit *ICEMountEdit;
        QLineEdit *ICEPassEdit;
        QSpinBox *ICEPort;
};



#endif
