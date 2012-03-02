/*****************************************************************************
 * customwidgets.hpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2004 Daniel Molkentin <molkentin@kde.org>
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * The "ClickLineEdit" control is based on code by  Daniel Molkentin
 * <molkentin@kde.org> for libkdepim
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

#ifndef _CUSTOMWIDGETS_H_
#define _CUSTOMWIDGETS_H_

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QSpinBox>
#include <QList>
#include <QTimer>
#include <QToolButton>

class QPixmap;

class QFramelessButton : public QPushButton
{
    Q_OBJECT
public:
    QFramelessButton( QWidget *parent = NULL );
    virtual QSize sizeHint() const { return iconSize(); }
protected:
    virtual void paintEvent( QPaintEvent * event );
};

class QToolButtonExt : public QToolButton
{
    Q_OBJECT
public:
    QToolButtonExt( QWidget *parent = 0, int ms = 0 );
private:
    bool shortClick;
    bool longClick;
private slots:
    void releasedSlot();
    void clickedSlot();
signals:
    void shortClicked();
    void longClicked();
};

class QElidingLabel : public QLabel
{
public:
    QElidingLabel( const QString &s = QString(),
                      Qt::TextElideMode mode = Qt::ElideRight,
                      QWidget * parent = NULL );
    void setElideMode( Qt::TextElideMode );
protected:
    virtual void paintEvent( QPaintEvent * event );
private:
    Qt::TextElideMode elideMode;
};


class QVLCStackedWidget : public QStackedWidget
{
public:
    QVLCStackedWidget( QWidget *parent ) : QStackedWidget( parent ) { }
    QSize minimumSizeHint () const
    {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};

class QVLCDebugLevelSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    QVLCDebugLevelSpinBox( QWidget *parent ) : QSpinBox( parent ) { };
protected:
    virtual QString textFromValue( int ) const;
    /* QVLCDebugLevelSpinBox is read-only */
    virtual int valueFromText( const QString& ) const { return -1; }
};

class AnimatedIcon : public QLabel
{
    /** An animated pixmap
     * Use this widget to display an animated icon based on a series of
     * pixmaps. The pixmaps will be stored in memory and should be kept small.
     * First, create the widget, add frames and then start playing. Looping
     * is supported.
     * Frames #1 to #n are displayed at regular intervals when playing.
     * Frame #0 is the idle frame, displayed when the icon is not animated.
     * If not #0 frame has been specified, the last frame will be shown when
     * idle.
     **/

    Q_OBJECT

public:
    /** Create an empty AnimatedIcon */
    AnimatedIcon( QWidget *parent );
    virtual ~AnimatedIcon();

    /** Adds a frame to play in the loop.
     * @param pixmap The QPixmap to display. Data will be copied internally.
     * @param index If -1, append the frame. If 0, replace the idle frame.
     *              Otherwise, insert the frame at the given position.
     **/
    void addFrame( const QPixmap &pixmap, int index = -1 );

    /** Play the animation (or restart it)
     * @param loops Number of times to play the loop. 0 means stop, while -1
     *              means play forever. When stopped, the frame #0 will be
     *              displayed until play() is called again.
     * @param interval Delay between frames, in milliseconds (minimum 20ms)
     * @note If isPlaying() is true, then restart the animation from frame #1
     **/
    void play( int loops = 1, int interval = 200 );

    /** Stop playback. Same as play(0). */
    inline void stop()
    {
        play( 0 );
    }

    /** Is the animation currently running? */
    inline bool isPlaying()
    {
        return mTimer.isActive();
    }

private:
    QTimer mTimer;
    QPixmap *mIdleFrame;
    QList<QPixmap*> mFrames; // Keeps deep copies of all the frames
    int mCurrentFrame, mRemainingLoops;

private slots:
    /** Slot connected to the timeout() signal of our internal timer */
    void onTimerTick();
};

class SpinningIcon : public AnimatedIcon
{
    /** This spinning icon, to the colors of the VLC cone, will show
     * that there is some background activity running
     **/

    Q_OBJECT

public:
    SpinningIcon( QWidget *parent, bool noIdleFrame = false );
};

/* VLC Key/Wheel hotkeys interactions */

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( QInputEvent* e );
int qtEventToVLCKey( QKeyEvent *e );
int qtWheelEventToVLCKey( QWheelEvent *e );
QString VLCKeyToString( unsigned val );

#endif
