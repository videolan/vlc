/*****************************************************************************
 * intf.h: Qt interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <qapplication.h>
#include <qmainwindow.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qwhatsthis.h>
#include <qpushbutton.h>
#include <qfiledialog.h>
#include <qslider.h>
#include <qlcdnumber.h>
#include <qmenubar.h>
#include <qstatusbar.h>
#include <qmessagebox.h>
#include <qlabel.h> 
#include <qtimer.h> 
#include <qiconset.h> 

#include <qvbox.h>
#include <qhbox.h>

/*****************************************************************************
 * Local Qt slider class
 *****************************************************************************/
class IntfSlider : public QSlider
{
    Q_OBJECT

public:
    IntfSlider( intf_thread_t *, QWidget * );  /* Constructor and destructor */
    ~IntfSlider();

    bool b_free;                                     /* Is the slider free ? */

    int  oldvalue   ( void ) { return i_oldvalue; };
    void setOldValue( int i_value ) { i_oldvalue = i_value; };

private slots:
    void SlideStart ( void ) { b_free = FALSE; };
    void SlideStop  ( void ) { b_free = TRUE; };

private:
    intf_thread_t *p_intf;
    int  i_oldvalue;
};

/*****************************************************************************
 * Local Qt interface window class
 *****************************************************************************/
class IntfWindow : public QMainWindow
{
    Q_OBJECT

public:
    IntfWindow( intf_thread_t * );
    ~IntfWindow();

private slots:
    void Manage ( void );

    void FileOpen  ( void );
    void FileQuit  ( void );

    void PlaybackPlay  ( void );
    void PlaybackPause ( void );
    void PlaybackSlow  ( void );
    void PlaybackFast  ( void );

    void PlaylistPrev  ( void );
    void PlaylistNext  ( void );

    void DateDisplay  ( int );
    void About ( void );

    void Unimplemented( void ) { msg_Warn( p_intf, "unimplemented" ); };

private:
    intf_thread_t *p_intf;

    IntfSlider *p_slider;

    QToolBar   *p_toolbar;
    QPopupMenu *p_popup;
    QLabel     *p_date;
};

