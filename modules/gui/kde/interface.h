/***************************************************************************
                          interface.h  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _KDE_INTERFACE_H_
#define _KDE_INTERFACE_H_

#include "common.h"

#include <kaction.h>
#include <kmainwindow.h>
#include <kapplication.h>
#include <kurl.h>
#include <qdragobject.h>
#include <qstring.h>
#include <qwidget.h>
#include "messages.h"
class KThread;

class KDiskDialog;
class KNetDialog;
class KRecentFilesAction;
class KTitleMenu;
class KToggleAction;
class KVLCSlider;

/**Main Window for the KDE vlc interface
  *@author andres
  */

class KInterface : public KMainWindow
{
    Q_OBJECT
    public:
        KInterface(intf_thread_t *p_intf, QWidget *parent=0,
                   const char *name="VLC");
        ~KInterface();

    public slots:
        /** open a file and load it into the document*/
        void slotFileOpen();
        /** opens a file from the recent files menu */
        void slotFileOpenRecent(const KURL& url);
        /** closes all open windows by calling close() on each
         * memberList item until the list is empty, then quits the
         * application.  If queryClose() returns false because the
         * user canceled the saveModified() dialog, the closing
         * breaks.
         */
        void slotFileQuit();
        void slotShowPreferences();

        /** toggles the toolbar
         */
        void slotViewToolBar();
        /** toggles the statusbar
         */
        void slotViewStatusBar();
        /** changes the statusbar contents for the standard label
         * permanently, used to indicate current actions.
         * @param text the text that is displayed in the statusbar
         */
        void slotStatusMsg( const QString &text );
        void slotShowMessages();
        void slotShowInfo();
        void slotSetLanguage( bool, es_descriptor_t * );

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
        void slotSliderMoved( int );

        /** called every time the slider changes values */
        void slotSliderChanged( int position );

        void slotUpdateLanguages();
        
        void slotOpenDisk();
        void slotOpenStream();

        void slotBackward();
        void slotStop();
        void slotPlay();
        void slotPause();
        void slotSlow();
        void slotFast();
        void slotPrev();
        void slotNext();

  private:
        void languageMenus( KActionMenu *, es_descriptor_t *, int );

        intf_thread_t    *p_intf;
        KMessagesWindow *p_messagesWindow;

        /** to call p_intf->pf_manage every now and then */
        QTimer            *fTimer;

        /** slider which works well with user movement */
        KVLCSlider    *fSlider;

        /** open dvd/vcd */
        KDiskDialog    *fDiskDialog;

        /** open net stream */
        KNetDialog        *fNetDialog;

        KTitleMenu        *fTitleMenu;

        // KAction pointers to enable/disable actions
        KAction             *fileOpen;
        KAction             *diskOpen;
        KAction             *streamOpen;
        KRecentFilesAction  *fileOpenRecent;
        KAction             *fileQuit;
        KToggleAction       *viewToolBar;
        KToggleAction       *viewStatusBar;
        KAction             *backward;
        KAction             *stop;
        KAction             *play;
        KAction             *pause;
        KAction             *slow;
        KAction             *fast;
        KAction             *prev;
        KAction             *next;
        KAction             *messages;
        KAction             *preferences;
        KAction             *info;
        KActionMenu         *languages;
        KActionMenu         *subtitles;
        KActionCollection   *languageCollection;
        KActionCollection   *subtitleCollection;
        KActionMenu         *program;
        KActionMenu         *title;
        KActionMenu         *chapter;
};

/*****************************************************************************
 * intf_sys_t: description and status of KDE interface
 *****************************************************************************/
struct intf_sys_t
{
    KApplication *p_app;
    KInterface   *p_window;
    KAboutData   *p_about;
    int b_playing;

    input_thread_t *p_input;
    msg_subscription_t *p_msg;
};

#endif /* _KDE_INTERFACE_H_ */
