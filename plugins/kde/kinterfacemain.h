/***************************************************************************
                          kinterfacemain.h  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _KINTERFACEMAIN_H_
#define _KINTERFACEMAIN_H_

#define MODULE_NAME kde
#include "intf_plugin.h"

#include <kmainwindow.h>
#include <kurl.h>
#include <qdragobject.h>
#include <qstring.h>
#include <qwidget.h>

class KDiskDialog;
class KNetDialog;
class KRecentFilesAction;
class KTitleMenu;
class KToggleAction;
class KVLCSlider;

/**Main Window for the KDE vlc interface
  *@author andres
  */

class KInterfaceMain : public KMainWindow  {
		Q_OBJECT
	public:
		KInterfaceMain(intf_thread_t *p_intf, QWidget *parent=0,
				const char *name=0);
		~KInterfaceMain();

  public slots:
    /** open a file and load it into the document*/
    void slotFileOpen();
    /** opens a file from the recent files menu */
    void slotFileOpenRecent(const KURL& url);
    /** asks for saving if the file is modified, then closes the actual file and window*/
    void slotFileClose();
    /** closes all open windows by calling close() on each memberList item until the list is empty, then quits the application.
     * If queryClose() returns false because the user canceled the saveModified() dialog, the closing breaks.
     */
    void slotFileQuit();
    /** toggles the toolbar
     */
    void slotViewToolBar();
    /** toggles the statusbar
     */
    void slotViewStatusBar();
    /** changes the statusbar contents for the standard label permanently, used to indicate current actions.
     * @param text the text that is displayed in the statusbar
     */
    void slotStatusMsg( const QString &text );

	protected:
		/** initializes the KActions of the application */
		void initActions();
		/** sets up the statusbar for the main window by initialzing a statuslabel.
		 */
		void initStatusBar();

		virtual void dragEnterEvent( QDragEnterEvent *event );
		virtual void dropEvent( QDropEvent *event );

	private slots:
		/** we use this to manage the communication with the vlc core */
		void slotManage();

		/** this slot is called when we drag the position seek bar */
		void slotSliderMoved( int position );

		/** called every time the slider changes values */
		void slotSliderChanged( int position );

		void slotOpenDisk();
		void slotOpenStream();

		void slotPlay();
		void slotPause();
		void slotStop();
		void slotBackward();
		void slotForward();
		void slotSlow();
		void slotFast();

  private:

		intf_thread_t	*fInterfaceThread;

		/** to call p_intf->pf_manage every now and then */
		QTimer			*fTimer;

		/** slider which works well with user movement */
		KVLCSlider	*fSlider;

		/** open dvd/vcd */
		KDiskDialog	*fDiskDialog;

		/** open net stream */
		KNetDialog		*fNetDialog;

		KTitleMenu		*fTitleMenu;

		// KAction pointers to enable/disable actions
		KAction						*fileOpen;
		KAction						*diskOpen;
		KAction						*streamOpen;
		KRecentFilesAction		*fileOpenRecent;
		KAction						*fileClose;
		KAction						*fileQuit;
		KToggleAction				*viewToolBar;
		KToggleAction				*viewStatusBar;
		KAction						*play;
		KAction						*pause;
		KAction						*stop;
		KAction						*backward;
		KAction						*forward;
		KAction						*slow;
		KAction						*fast;

};

#endif /* _KINTERFACEMAIN_H_ */
