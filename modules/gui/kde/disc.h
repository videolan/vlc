/***************************************************************************
                          disc.h  -  description
                             -------------------
    begin                : Sat Apr 7 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef KDE_DISC_H
#define KDE_DISC_H

#include "common.h"
#include <kdialogbase.h>
#include <qstring.h>

class QVButtonGroup;
class QRadioButton;
class QSpinBox;
class KLineEdit;

/**
  *@author andres
  */

class KDiskDialog : public KDialogBase
{
    Q_OBJECT
    public: 
        KDiskDialog( QWidget *parent=0, const char *name=0 );
        ~KDiskDialog();

        QString    type() const;
        QString    device() const;
        int        title() const;
        int        chapter() const;

    private:

        QVButtonGroup    *fButtonGroup;
        QRadioButton     *fDVDButton;
        QRadioButton     *fVCDButton;
        QSpinBox         *fTitle;
        QSpinBox         *fChapter;
        KLineEdit        *fLineEdit;

};

#endif
