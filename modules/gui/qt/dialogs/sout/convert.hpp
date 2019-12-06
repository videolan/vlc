/*****************************************************************************
 * convert.hpp : GotoTime dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#ifndef QVLC_CONVERT_DIALOG_H_
#define QVLC_CONVERT_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"

class QLineEdit;
class QCheckBox;
class QRadioButton;
class QPushButton;
class VLCProfileSelector;

class ConvertDialog : public QVLCDialog
{
    Q_OBJECT
public:
    ConvertDialog( QWidget *, intf_thread_t *, const QStringList& );
    virtual ~ConvertDialog(){}

    QStringList getMrls() {return mrls;}

private:
    QLineEdit *fileLine;

    QCheckBox *displayBox, *deinterBox, *appendBox;
    QRadioButton *dumpRadio;
    QPushButton *okButton;
    VLCProfileSelector *profile;

    const QStringList *incomingMRLs;
    const bool singleFileSelected;
    QUrl outgoingMRL;
    QStringList mrls;

private slots:
    void close() Q_DECL_OVERRIDE;
    void cancel() Q_DECL_OVERRIDE;
    void fileBrowse();
    void setDestinationFileExtension();
    void validate();
};

#endif
