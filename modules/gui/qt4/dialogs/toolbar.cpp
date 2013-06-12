/*****************************************************************************
 * toolbar.cpp : ToolbarEdit dialog
 ****************************************************************************
 * Copyright (C) 2008-2009 the VideoLAN team
 * $Id$
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/toolbar.hpp"

/* Widgets */
#include "util/input_slider.hpp"
#include "util/customwidgets.hpp"
#include "components/interface_widgets.hpp"
#include "util/buttons/DeckButtonsLayout.hpp"
#include "util/buttons/BrowseButton.hpp"
#include "util/buttons/RoundButton.hpp"

#include "qt4.hpp"
#include "input_manager.hpp"
#include <vlc_vout.h>                       /* vout_thread_t for aspect ratio combobox */

#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QSpinBox>
#include <QRubberBand>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMimeData>

#include <assert.h>

ToolbarEditDialog::ToolbarEditDialog( QWidget *_w, intf_thread_t *_p_intf)
                  : QVLCDialog( _w,  _p_intf )
{
    setWindowTitle( qtr( "Toolbars Editor" ) );
    setWindowRole( "vlc-toolbars-editor" );
    QGridLayout *mainLayout = new QGridLayout( this );
    setMinimumWidth( 600 );
    setAttribute( Qt::WA_DeleteOnClose );

    /* main GroupBox */
    QGroupBox *widgetBox = new QGroupBox( qtr( "Toolbar Elements") , this );
    widgetBox->setSizePolicy( QSizePolicy::Preferred,
                              QSizePolicy::MinimumExpanding );
    QGridLayout *boxLayout = new QGridLayout( widgetBox );

    QLabel *styleLabel = new QLabel( qtr( "Next widget style:" ) );
    flatBox = new QCheckBox( qtr( "Flat Button" ) );
    bigBox = new QCheckBox( qtr( "Big Button" ) );
    shinyBox = new QCheckBox( qtr( "Native Slider" ) );

    boxLayout->addWidget( new WidgetListing( p_intf, this ), 0, 0, 1, -1);
    boxLayout->addWidget( styleLabel, 1, 0 );
    boxLayout->addWidget( flatBox, 1, 1 );
    boxLayout->addWidget( bigBox, 1, 2 );
    boxLayout->addWidget( shinyBox, 2, 1 );
    mainLayout->addWidget( widgetBox, 0, 0, 5, 1 );


    /* Main ToolBar */
    QGroupBox *mainToolbarBox = new QGroupBox( qtr( "Main Toolbar" ), this );
    QGridLayout *mainTboxLayout = new QGridLayout( mainToolbarBox );

    QLabel *label = new QLabel( qtr( "Toolbar position:" ) );
    mainTboxLayout->addWidget(label, 0, 0, 1, 2);

    positionCombo = new QComboBox;
    positionCombo->addItem( qtr( "Under the Video" ), QVariant( 0 ) );
    positionCombo->addItem( qtr( "Above the Video" ), QVariant( 1 ) );
    positionCombo->setCurrentIndex( positionCombo->findData(
                getSettings()->value( "MainWindow/ToolbarPos", 0 ).toInt() ) );
    mainTboxLayout->addWidget( positionCombo, 0, 2, 1, 1 );

    QLabel *line1Label = new QLabel( qtr("Line 1:") );
    QString line1 = getSettings()->value( "MainWindow/MainToolbar1",
                                          MAIN_TB1_DEFAULT ).toString();
    controller1 = new DroppingController( p_intf, line1,
            this );
    mainTboxLayout->addWidget( line1Label, 1, 0, 1, 1 );
    mainTboxLayout->addWidget( controller1, 1, 1, 1, 2 );

    QLabel *line2Label = new QLabel( qtr("Line 2:") );
    QString line2 = getSettings()->value( "MainWindow/MainToolbar2",
                                          MAIN_TB2_DEFAULT ).toString();
    controller2 = new DroppingController( p_intf, line2,
            this );
    mainTboxLayout->addWidget( line2Label, 2, 0, 1, 1 );
    mainTboxLayout->addWidget( controller2, 2, 1, 1, 2);

    /* Advanced ToolBar */
    QLabel *advLabel = new QLabel( qtr( "Advanced Widget toolbar:" ) );
    QString lineA = getSettings()->value( "MainWindow/AdvToolbar",
                                          ADV_TB_DEFAULT ).toString();
    controllerA = new DroppingController( p_intf, lineA,
            this );
    mainTboxLayout->addWidget( advLabel, 3, 0, 1, 2 );
    mainTboxLayout->addWidget( controllerA, 3, 2, 1, 1 );

    mainLayout->addWidget( mainToolbarBox, 0, 1, 1, -1 );

    /* TimeToolBar */
    QGroupBox *timeToolbarBox = new QGroupBox( qtr( "Time Toolbar" ) , this );
    QGridLayout *timeTboxLayout = new QGridLayout( timeToolbarBox );

    QString line = getSettings()->value( "MainWindow/InputToolbar",
                                         INPT_TB_DEFAULT ).toString();
    controller = new DroppingController( p_intf, line,
            this );
    timeTboxLayout->addWidget( controller, 0, 0, 1, -1 );

    mainLayout->addWidget( timeToolbarBox, 1, 1, 1, -1 );

    /* FSCToolBar */
    QGroupBox *FSCToolbarBox = new QGroupBox( qtr( "Fullscreen Controller" ),
                                              this );
    QGridLayout *FSCTboxLayout = new QGridLayout( FSCToolbarBox );

    QString lineFSC = getSettings()->value( "MainWindow/FSCtoolbar",
                                            FSC_TB_DEFAULT ).toString();
    controllerFSC = new DroppingController( p_intf,
            lineFSC, this );
    FSCTboxLayout->addWidget( controllerFSC, 0, 0, 1, -1 );

    mainLayout->addWidget( FSCToolbarBox, 2, 1, 1, -1 );

    /* Profile */
    QGroupBox *profileBox = new QGroupBox( qtr( "Profile" ), this );
    QGridLayout *profileBoxLayout = new QGridLayout( profileBox );

    profileCombo = new QComboBox;
    QLabel *profileLabel = new QLabel( qtr( "Select profile:" ), this );

    QToolButton *newButton = new QToolButton;
    newButton->setIcon( QIcon( ":/new" ) );
    newButton->setToolTip( qtr("New profile") );
    QToolButton *deleteButton = new QToolButton;
    deleteButton->setIcon( QIcon( ":/toolbar/clear" ) );
    deleteButton->setToolTip( qtr( "Delete the current profile" ) );

    profileBoxLayout->addWidget( profileLabel, 0, 0 );
    profileBoxLayout->addWidget( profileCombo, 0, 1 );
    profileBoxLayout->addWidget( newButton, 0, 2 );
    profileBoxLayout->addWidget( deleteButton, 0, 3 );

    mainLayout->addWidget( profileBox, 3, 1, 1, -1 );

    /* Fill combos */
    int i_size = getSettings()->beginReadArray( "ToolbarProfiles" );
    for( int i = 0; i < i_size; i++ )
    {
        getSettings()->setArrayIndex(i);
        profileCombo->addItem( getSettings()->value( "ProfileName" ).toString(),
                               getSettings()->value( "Value" ).toString() );
    }
    getSettings()->endArray();

    /* Load defaults ones if we have no combos */
    /* We could decide that we load defaults on first launch of the dialog
       or when the combo is back to 0. I choose the second solution, because some clueless
       user might hit on delete a bit too much, but discussion is opened. -- jb */
    if( i_size == 0 )
    {
        profileCombo->addItem( PROFILE_NAME_6, QString( VALUE_6 ) );
        profileCombo->addItem( PROFILE_NAME_1, QString( VALUE_1 ) );
        profileCombo->addItem( PROFILE_NAME_2, QString( VALUE_2 ) );
        profileCombo->addItem( PROFILE_NAME_3, QString( VALUE_3 ) );
        profileCombo->addItem( PROFILE_NAME_4, QString( VALUE_4 ) );
        profileCombo->addItem( PROFILE_NAME_5, QString( VALUE_5 ) );
    }
    profileCombo->setCurrentIndex( -1 );

    /* Buttons */
    QDialogButtonBox *okCancel = new QDialogButtonBox;
    QPushButton *okButton = new QPushButton( qtr( "Cl&ose" ), this );
    okButton->setDefault( true );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ), this );
    okCancel->addButton( okButton, QDialogButtonBox::AcceptRole );
    okCancel->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( deleteButton, deleteProfile() );
    BUTTONACT( newButton, newProfile() );
    CONNECT( profileCombo, currentIndexChanged( int ), this, changeProfile( int ) );
    BUTTONACT( okButton, close() );
    BUTTONACT( cancelButton, cancel() );
    mainLayout->addWidget( okCancel, 5, 2 );
}


ToolbarEditDialog::~ToolbarEditDialog()
{
    getSettings()->beginWriteArray( "ToolbarProfiles" );
    for( int i = 0; i < profileCombo->count(); i++ )
    {
        getSettings()->setArrayIndex(i);
        getSettings()->setValue( "ProfileName", profileCombo->itemText( i ) );
        getSettings()->setValue( "Value", profileCombo->itemData( i ) );
    }
    getSettings()->endArray();
}

void ToolbarEditDialog::newProfile()
{
    bool ok;
    QString name =  QInputDialog::getText( this, qtr( "Profile Name" ),
                 qtr( "Please enter the new profile name." ), QLineEdit::Normal, 0, &ok );
    if( !ok ) return;

    QString temp = QString::number( positionCombo->currentIndex() );
    temp += "|" + controller1->getValue();
    temp += "|" + controller2->getValue();
    temp += "|" + controllerA->getValue();
    temp += "|" + controller->getValue();
    temp += "|" + controllerFSC->getValue();

    profileCombo->addItem( name, temp );
    profileCombo->setCurrentIndex( profileCombo->count() - 1 );
}

void ToolbarEditDialog::deleteProfile()
{
    profileCombo->removeItem( profileCombo->currentIndex() );
}

void ToolbarEditDialog::changeProfile( int i )
{
    QStringList qs_list = profileCombo->itemData( i ).toString().split( "|" );
    if( qs_list.count() < 6 )
        return;

    positionCombo->setCurrentIndex( positionCombo->findData( qs_list[0].toInt() ) );
    controller1->resetLine( qs_list[1] );
    controller2->resetLine( qs_list[2] );
    controllerA->resetLine( qs_list[3] );
    controller->resetLine( qs_list[4] );
    controllerFSC->resetLine( qs_list[5] );
}

void ToolbarEditDialog::close()
{
    getSettings()->setValue( "MainWindow/ToolbarPos",
            positionCombo->itemData( positionCombo->currentIndex() ).toInt() );
    getSettings()->setValue( "MainWindow/MainToolbar1", controller1->getValue() );
    getSettings()->setValue( "MainWindow/MainToolbar2", controller2->getValue() );
    getSettings()->setValue( "MainWindow/AdvToolbar", controllerA->getValue() );
    getSettings()->setValue( "MainWindow/InputToolbar", controller->getValue() );
    getSettings()->setValue( "MainWindow/FSCtoolbar", controllerFSC->getValue() );
    getSettings()->sync();
    accept();
}

void ToolbarEditDialog::cancel()
{
    reject();
}

/************************************************
 *  Widget Listing:
 * Creation of the list of drawed lovely buttons
 ************************************************/
WidgetListing::WidgetListing( intf_thread_t *p_intf, QWidget *_parent )
              : QListWidget( _parent )
{
    /* We need the parent to know the options checked */
    parent = qobject_cast<ToolbarEditDialog *>(_parent);
    assert( parent );

    /* Normal options */
    setViewMode( QListView::IconMode );
    setSpacing( 8 );
    setGridSize( QSize(90, 50) );
    setWrapping( true );
    setWordWrap( true );
    setTextElideMode( Qt::ElideNone );
    setDragEnabled( true );

    /* All the buttons do not need a special rendering */
    for( int i = 0; i < BUTTON_MAX; i++ )
    {
        QListWidgetItem *widgetItem = new QListWidgetItem( this );
        widgetItem->setText( qtr( nameL[i] ) );
        QPixmap pix( iconL[i] );
        widgetItem->setIcon( pix.scaled( 16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
        widgetItem->setData( Qt::UserRole, QVariant( i ) );
        widgetItem->setToolTip( widgetItem->text() );
        addItem( widgetItem );
    }

    /* Spacers are yet again a different thing */
    QListWidgetItem *widgetItem = new QListWidgetItem( QIcon( ":/toolbar/space" ),
            qtr( "Spacer" ), this );
    widgetItem->setData( Qt::UserRole, WIDGET_SPACER );
    widgetItem->setToolTip( widgetItem->text() );
    addItem( widgetItem );

    widgetItem = new QListWidgetItem( QIcon( ":/toolbar/space" ),
            qtr( "Expanding Spacer" ), this );
    widgetItem->setData( Qt::UserRole, WIDGET_SPACER_EXTEND );
    widgetItem->setToolTip( widgetItem->text() );
    addItem( widgetItem );

    /**
     * For all other widgets, we create then, do a pseudo rendering in
     * a pixmaps for the view, and delete the object
     *
     * A lot of code is retaken from the Abstract, but not exactly...
     * So, rewrite.
     * They are better ways to deal with this, but I doubt that this is
     * necessary. If you feel like you have the time, be my guest.
     * --
     * jb
     **/
    for( int i = SPLITTER; i < SPECIAL_MAX; i++ )
    {
        QWidget *widget = NULL;
        QListWidgetItem *widgetItem = new QListWidgetItem( this );
        switch( i )
        {
        case SPLITTER:
            {
                QFrame *line = new QFrame( this );
                line->setFrameShape( QFrame::VLine );
                line->setFrameShadow( QFrame::Raised );
                line->setLineWidth( 0 ); line->setMidLineWidth( 1 );
                widget = line;
            }
            widgetItem->setText( qtr("Splitter") );
            break;
        case INPUT_SLIDER:
            {
                SeekSlider *slider = new SeekSlider( Qt::Horizontal, this );
                widget = slider;
            }
            widgetItem->setText( qtr("Time Slider") );
            break;
        case VOLUME:
            {
                SoundWidget *snd = new SoundWidget( this, p_intf,
                        parent->getOptions() & WIDGET_SHINY );
                widget = snd;
            }
            widgetItem->setText( qtr("Volume") );
            break;
        case VOLUME_SPECIAL:
            {
                QListWidgetItem *widgetItem = new QListWidgetItem( this );
                widgetItem->setText( qtr("Small Volume") );
                widgetItem->setIcon( QIcon( ":/toolbar/volume-medium" ) );
                widgetItem->setData( Qt::UserRole, QVariant( i ) );
                addItem( widgetItem );
            }
            continue;
        case TIME_LABEL:
            {
                QLabel *timeLabel = new QLabel( "12:42/2:12:42", this );
                widget = timeLabel;
            }
            widgetItem->setText( qtr("Time") );
            break;
        case MENU_BUTTONS:
            {
                QWidget *discFrame = new QWidget( this );
                //discFrame->setLineWidth( 1 );
                QHBoxLayout *discLayout = new QHBoxLayout( discFrame );
                discLayout->setSpacing( 0 ); discLayout->setMargin( 0 );

                QToolButton *prevSectionButton = new QToolButton( discFrame );
                prevSectionButton->setIcon( QIcon( ":/toolbar/dvd_prev" ) );
                prevSectionButton->setToolTip( qtr("Previous chapter") );
                discLayout->addWidget( prevSectionButton );

                QToolButton *menuButton = new QToolButton( discFrame );
                menuButton->setIcon( QIcon( ":/toolbar/dvd_menu" ) );
                menuButton->setToolTip( qtr("Go to the DVD menu") );
                discLayout->addWidget( menuButton );

                QToolButton *nextButton = new QToolButton( discFrame );
                nextButton->setIcon( QIcon( ":/toolbar/dvd_next" ) );
                nextButton->setToolTip( qtr("Next chapter") );
                discLayout->addWidget( nextButton );

                widget = discFrame;
            }
            widgetItem->setText( qtr("DVD menus") );
            break;
        case TELETEXT_BUTTONS:
            {
                QWidget *telexFrame = new QWidget( this );
                QHBoxLayout *telexLayout = new QHBoxLayout( telexFrame );
                telexLayout->setSpacing( 0 ); telexLayout->setMargin( 0 );

                QToolButton *telexOn = new QToolButton( telexFrame );
                telexOn->setIcon( QIcon( ":/toolbar/tv" ) );
                telexLayout->addWidget( telexOn );

                QToolButton *telexTransparent = new QToolButton;
                telexTransparent->setIcon( QIcon( ":/toolbar/tvtelx" ) );
                telexTransparent->setToolTip( qtr("Teletext transparency") );
                telexLayout->addWidget( telexTransparent );

                QSpinBox *telexPage = new QSpinBox;
                telexLayout->addWidget( telexPage );

                widget = telexFrame;
            }
            widgetItem->setText( qtr("Teletext") );
            break;
        case ADVANCED_CONTROLLER:
            {
                AdvControlsWidget *advControls = new AdvControlsWidget( p_intf, this );
                widget = advControls;
            }
            widgetItem->setText( qtr("Advanced Buttons") );
            break;
        case PLAYBACK_BUTTONS:
            {
                widget = new QWidget;
                DeckButtonsLayout *layout = new DeckButtonsLayout( widget );
                BrowseButton *prev = new BrowseButton( widget, BrowseButton::Backward );
                BrowseButton *next = new BrowseButton( widget );
                RoundButton *play = new RoundButton( widget );
                layout->setBackwardButton( prev );
                layout->setForwardButton( next );
                layout->setRoundButton( play );
            }
            widgetItem->setText( qtr("Playback Buttons") );
            break;
        case ASPECT_RATIO_COMBOBOX:
            widget = new AspectRatioComboBox( p_intf );
            widgetItem->setText( qtr("Aspect ratio selector") );
            break;
        case SPEED_LABEL:
            widget = new SpeedLabel( p_intf, this );
            widgetItem->setText( qtr("Speed selector") );
            break;
        case TIME_LABEL_ELAPSED:
            widget = new QLabel( "2:42", this );
            widgetItem->setText( qtr("Elapsed time") );
            break;
        case TIME_LABEL_REMAINING:
            widget = new QLabel( "-2:42", this );
            widgetItem->setText( qtr("Total/Remaining time") );
            break;
        default:
            msg_Warn( p_intf, "This should not happen %i", i );
            break;
        }

        if( widget == NULL ) continue;


        widgetItem->setIcon( QIcon( QPixmap::grabWidget( widget ) ) );
        widgetItem->setToolTip( widgetItem->text() );
        widget->hide();
        widgetItem->setData( Qt::UserRole, QVariant( i ) );

        addItem( widgetItem );
        delete widget;
    }
}

void WidgetListing::startDrag( Qt::DropActions /*supportedActions*/ )
{
    QListWidgetItem *item = currentItem();

    QByteArray itemData;
    QDataStream dataStream( &itemData, QIODevice::WriteOnly );

    int i_type = item->data( Qt::UserRole ).toInt();
    int i_option = parent->getOptions();
    dataStream << i_type << i_option;

    /* Create a new dragging event */
    QDrag *drag = new QDrag( this );

    /* With correct mimedata */
    QMimeData *mimeData = new QMimeData;
    mimeData->setData( "vlc/button-bar", itemData );
    drag->setMimeData( mimeData );

    /* And correct pixmap */
    QPixmap aPixmap = item->icon().pixmap( QSize( 22, 22 ) );
    drag->setPixmap( aPixmap );
    drag->setHotSpot( QPoint( 20, 20 ) );

    /* We want to keep a copy */
    drag->exec( Qt::CopyAction | Qt::MoveAction );
}

/*
 * The special controller with drag'n drop abilities.
 * We don't do this in the main controller, since we don't want the OverHead
 * to propagate there too
 */
DroppingController::DroppingController( intf_thread_t *_p_intf,
                                        const QString& line,
                                        QWidget *_parent )
                   : AbstractController( _p_intf, _parent )
{
    RTL_UNAFFECTED_WIDGET
    rubberband = NULL;
    b_draging = false;
    setAcceptDrops( true );
    controlLayout = new QHBoxLayout( this );
    controlLayout->setSpacing( 5 );
    controlLayout->setMargin( 0 );
    setFrameShape( QFrame::StyledPanel );
    setFrameShadow( QFrame::Raised );
    setMinimumHeight( 20 );

    parseAndCreate( line, controlLayout );
}

void DroppingController::resetLine( const QString& line )
{
    hide();
    QLayoutItem *child;
    while( (child = controlLayout->takeAt( 0 ) ) != 0 )
    {
        child->widget()->hide();
        delete child;
    }

    parseAndCreate( line, controlLayout );
    show();
}

/* Overloading the AbstractController one, because we don't manage the
   Spacing items in the same ways */
void DroppingController::createAndAddWidget( QBoxLayout *newControlLayout,
                                             int i_index,
                                             buttonType_e i_type,
                                             int i_option )
{
    doubleInt *value = new doubleInt;
    value->i_type = i_type;
    value->i_option = i_option;

    /* Special case for SPACERS, who aren't QWidgets */
    if( i_type == WIDGET_SPACER || i_type == WIDGET_SPACER_EXTEND )
    {
        QLabel *label = new QLabel( this );
        label->setPixmap( QPixmap( ":/toolbar/space" ) );
        if( i_type == WIDGET_SPACER_EXTEND )
        {
            label->setSizePolicy( QSizePolicy::MinimumExpanding,
                    QSizePolicy::Preferred );

            /* Create a box around it */
            label->setFrameStyle( QFrame::Panel | QFrame::Sunken );
            label->setLineWidth ( 1 );
            label->setAlignment( Qt::AlignCenter );
        }
        else
            label->setSizePolicy( QSizePolicy::Fixed,
                    QSizePolicy::Preferred );

        /* Install event Filter for drag'n drop */
        label->installEventFilter( this );
        newControlLayout->insertWidget( i_index, label );
    }

    /* Normal Widgets */
    else
    {
        QWidget *widg = createWidget( i_type, i_option );
        if( !widg ) return;

        /* Install the Event Filter in order to catch the drag */
        widg->setParent( this );
        widg->installEventFilter( this );

        /* We are in a complex widget, we need to stop events on children too */
        if( i_type >= VOLUME && i_type < SPECIAL_MAX )
        {
            QList<QObject *>children = widg->children();

            QObject *child;
            foreach( child, children )
            {
                QWidget *childWidg;
                if( ( childWidg = qobject_cast<QWidget *>( child ) ) )
                {
                    child->installEventFilter( this );
                    childWidg->setEnabled( true );
                }
            }

            /* Decorating the frames when possible */
            QFrame *frame;
            if( i_type >= MENU_BUTTONS  /* Don't bother to check for volume */
                && ( frame = qobject_cast<QFrame *>( widg ) ) != NULL )
            {
                frame->setFrameStyle( QFrame::Panel | QFrame::Raised );
                frame->setLineWidth ( 1 );
            }
        }

        /* Some Widgets are deactivated at creation */
        widg->setEnabled( true );
        widg->show();
        newControlLayout->insertWidget( i_index, widg );
    }

    /* QList and QBoxLayout don't act the same with insert() */
    if( i_index < 0 ) i_index = newControlLayout->count() - 1;

    widgetList.insert( i_index, value );
}

DroppingController::~DroppingController()
{
    qDeleteAll( widgetList );
    widgetList.clear();
}

QString DroppingController::getValue()
{
    QString qs = "";

    for( int i = 0; i < controlLayout->count(); i++ )
    {
        doubleInt *dI = widgetList.at( i );
        assert( dI );

        qs.append( QString::number( dI->i_type ) );
        if( dI->i_option ) qs.append( "-" + QString::number( dI->i_option ) );
        qs.append( ';' );
    }
    return qs;
}

void DroppingController::dragEnterEvent( QDragEnterEvent * event )
{
    if( event->mimeData()->hasFormat( "vlc/button-bar" ) )
        event->accept();
    else
        event->ignore();
}

void DroppingController::dragMoveEvent( QDragMoveEvent *event )
{
    QPoint origin = event->pos();

    int i_pos = getParentPosInLayout( origin );
    bool b_end = false;

    /* Both sides of the frame */
    if( i_pos == -1 )
    {
        if( rubberband ) rubberband->hide();
        return;
    }

    /* Last item is special because of underlying items */
    if( i_pos >= controlLayout->count() )
    {
        i_pos--;
        b_end = true;
    }

    /* Query the underlying item for size && middles */
    QLayoutItem *tempItem = controlLayout->itemAt( i_pos ); assert( tempItem );
    QWidget *temp = tempItem->widget(); assert( temp );

    /* Position assignment */
    origin.ry() = 0;
    origin.rx() = temp->x() - 2;

    if( b_end ) origin.rx() += temp->width();

    if( !rubberband )
        rubberband = new QRubberBand( QRubberBand::Line, this );
    rubberband->setGeometry( origin.x(), origin.y(), 4, height() );
    rubberband->show();
}

inline int DroppingController::getParentPosInLayout( QPoint point )
{
    point.ry() = height() / 2 ;
    QPoint origin = mapToGlobal ( point );

    QWidget *tempWidg = QApplication::widgetAt( origin );
    if( tempWidg == NULL )
        return -1;

    int i = controlLayout->indexOf( tempWidg );
    if( i == -1 )
    {
        i = controlLayout->indexOf( tempWidg->parentWidget() );
        tempWidg = tempWidg->parentWidget();
    }

    /* Return the nearest position */
    if( ( point.x() - tempWidg->x()  > tempWidg->width() / 2 ) && i != -1 )
        i++;

    //    msg_Dbg( p_intf, "%i", i);
    return i;
}

void DroppingController::dropEvent( QDropEvent *event )
{
    int i = getParentPosInLayout( event->pos() );

    QByteArray data = event->mimeData()->data( "vlc/button-bar" );
    QDataStream dataStream(&data, QIODevice::ReadOnly);

    int i_option = 0, i_type = 0;
    dataStream >> i_type >> i_option;

    createAndAddWidget( controlLayout, i, (buttonType_e)i_type, i_option );

    /* Hide by precaution, you don't exactly know what could have happened in
       between */
    if( rubberband ) rubberband->hide();
}

void DroppingController::dragLeaveEvent ( QDragLeaveEvent * event )
{
    if( rubberband ) rubberband->hide();
    event->accept();
}

/**
 * Overloading doAction to block any action
 **/
void DroppingController::doAction( int i )
{
    VLC_UNUSED( i );
}

bool DroppingController::eventFilter( QObject *obj, QEvent *event )
{
    switch( event->type() )
    {
        case QEvent::MouseButtonPress:
            b_draging = true;
            return true;
        case QEvent::MouseButtonRelease:
            b_draging = false;
            return true;
        case QEvent::MouseMove:
            {
            if( !b_draging ) return true;
            QWidget *widg = static_cast<QWidget*>(obj);

            QByteArray itemData;
            QDataStream dataStream( &itemData, QIODevice::WriteOnly );

            int i = -1;
            i = controlLayout->indexOf( widg );
            if( i == -1 )
            {
                i = controlLayout->indexOf( widg->parentWidget() );
                widg = widg->parentWidget();
                /* NOTE: be extra-careful Now with widg access */
            }

            if( i == -1 ) return true;
            i_dragIndex = i;

            doubleInt *dI = widgetList.at( i );

            int i_type = dI->i_type;
            int i_option = dI->i_option;
            dataStream << i_type << i_option;

            /* With correct mimedata */
            QMimeData *mimeData = new QMimeData;
            mimeData->setData( "vlc/button-bar", itemData );

            QDrag *drag = new QDrag( widg );
            drag->setMimeData( mimeData );

            /* Remove before the drag to not mess DropEvent,
               that will createAndAddWidget */
            widgetList.removeAt( i );
            controlLayout->removeWidget( widg );
            widg->hide();

            /* Start the effective drag */
            drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
            b_draging = false;
            }
            return true;

        case QEvent::MouseButtonDblClick:
        case QEvent::EnabledChange:
        case QEvent::Hide:
        case QEvent::HideToParent:
        case QEvent::Move:
        case QEvent::ZOrderChange:
            return true;
        default:
            return false;
    }
}
