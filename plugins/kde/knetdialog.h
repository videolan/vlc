/***************************************************************************
                          knetdialog.h  -  description
                             -------------------
    begin                : Mon Apr 9 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _KNETDIALOG_H_
#define _KNETDIALOG_H_

#include <qwidget.h>
#include <kdialogbase.h>

class QVButtonGroup;
class QRadioButton;
class QSpinBox;
class KLineEdit;

/**
  *@author andres
  */

class KNetDialog : public KDialogBase  {
		Q_OBJECT
	public:
		KNetDialog(QWidget *parent=0, const char *name=0);
		~KNetDialog();

		QString	protocol() const;
		QString	server() const;
		int			port() const;

	private:
		QVButtonGroup	*fButtonGroup;
		QRadioButton		*fTSButton;
		QRadioButton		*fRTPButton;
		QRadioButton		*fHTTPButton;
		KLineEdit				*fAddress;
		QSpinBox				*fPort;

};

#endif /* _KNETDIALOG_H_ */
