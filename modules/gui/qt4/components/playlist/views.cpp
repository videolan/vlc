/*****************************************************************************
 * views.cpp : Views for the Playlist
 ****************************************************************************
 * Copyright © 2010 the VideoLAN team
 * $Id$
 *
 * Authors:         Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "components/playlist/views.hpp"
#include "components/playlist/playlist_model.hpp" /* PLModel */
#include "components/playlist/sorting.h"          /* Columns List */
#include "input_manager.hpp"                      /* THEMIM */

#include <QPainter>
#include <QRect>
#include <QStyleOptionViewItem>
#include <QFontMetrics>
#include <QDrag>
#include <QDragMoveEvent>
#include <QMetaType>
#include <QHeaderView>

#include "assert.h"

/* ICON_SCALER comes currently from harrison-stetson method, so good value */
#define ICON_SCALER         16
#define ART_RADIUS          5
#define SPACER              5

void AbstractPlViewItemDelegate::paintBackground(
    QPainter *painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    /* FIXME: This does not indicate item selection in all QStyles, so for the time being we
       have to draw it ourselves, to ensure visible effect of selection on all platforms */
    /* QApplication::style()->drawPrimitive( QStyle::PE_PanelItemViewItem, &option,
                                            painter ); */

    painter->save();
    QRect r = option.rect.adjusted( 0, 0, -1, -1 );
    if( option.state & QStyle::State_Selected )
    {
        painter->setBrush( option.palette.color( QPalette::Highlight ) );
        painter->setPen( option.palette.color( QPalette::Highlight ).darker( 150 ) );
        painter->drawRect( r );
    }
    else if( index.data( PLModel::IsCurrentRole ).toBool() )
    {
        painter->setBrush( QBrush( Qt::lightGray ) );
        painter->setPen( QColor( Qt::darkGray ) );
        painter->drawRect( r );
    }
    if( option.state & QStyle::State_MouseOver )
    { /* requires WA_hover on viewport */
        painter->setOpacity( 0.5 );
        painter->setPen( Qt::NoPen );
        painter->setBrush( option.palette.color( QPalette::Highlight ).lighter( 150 ) );
        painter->drawRect( option.rect );
    }
    painter->restore();
}

void PlIconViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QString title = VLCModel::getMeta( index, COLUMN_TITLE );
    QString artist = VLCModel::getMeta( index, COLUMN_ARTIST );

    QFont font( index.data( Qt::FontRole ).value<QFont>() );
    font.setPointSize( __MAX( font.pointSize() + i_zoom, 4 ) );
    font.setBold( index.data( PLModel::IsCurrentRole ).toBool() );
    painter->setFont( font );
    QFontMetrics fm = painter->fontMetrics();

    int averagewidth = fm.averageCharWidth();
    QSize rectSize = option.rect.size();
    int art_width = averagewidth * ICON_SCALER;
    int art_height = averagewidth * ICON_SCALER;

    QPixmap artPix = VLCModel::getArtPixmap( index, QSize( art_width, art_height) );

    paintBackground( painter, option, index );

    painter->save();

    QRect artRect( option.rect.x() + ( rectSize.width() - artPix.width() ) / 2,
                   option.rect.y() - averagewidth*3 + ( rectSize.height() - artPix.height() ) / 2,
                   artPix.width(), artPix.height() );

    // Draw the drop shadow
    painter->save();
    painter->setOpacity( 0.7 );
    painter->setBrush( QBrush( Qt::darkGray ) );
    painter->setPen( Qt::NoPen );
    painter->drawRoundedRect( artRect.adjusted( 0, 0, 2, 2 ), ART_RADIUS, ART_RADIUS );
    painter->restore();

    // Draw the art pixmap
    QPainterPath artRectPath;
    artRectPath.addRoundedRect( artRect, ART_RADIUS, ART_RADIUS );
    painter->setClipPath( artRectPath );
    painter->drawPixmap( artRect, artPix );
    painter->setClipping( false );

    if( option.state & QStyle::State_Selected )
        painter->setPen( option.palette.color( QPalette::HighlightedText ) );


    //Draw children indicator
    if( !index.data( PLModel::IsLeafNodeRole ).toBool() )
    {
        QRect r( option.rect );
        r.setSize( QSize( 25, 25 ) );
        r.translate( 5, 5 );
        if( index.data( PLModel::IsCurrentsParentNodeRole ).toBool() )
        {
            painter->setOpacity( 0.75 );
            QPainterPath nodeRectPath;
            nodeRectPath.addRoundedRect( r, 4, 4 );
            painter->fillPath( nodeRectPath, option.palette.color( QPalette::Highlight ) );
            painter->setOpacity( 1.0 );
        }
        QPixmap dirPix( ":/type/node" );
        QRect r2( dirPix.rect() );
        r2.moveCenter( r.center() );
        painter->drawPixmap( r2, dirPix );
    }

    // Draw title
    font.setItalic( true );

    QRect textRect;
    textRect.setRect( option.rect.x() , artRect.bottom() + fm.height()/2, option.rect.width(), fm.height() );

    painter->drawText( textRect,
                      fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                      QTextOption( Qt::AlignCenter ) );

    // Draw artist
    painter->setPen( painter->pen().color().lighter( 150 ) );
    font.setItalic( false );
    painter->setFont( font );
    fm = painter->fontMetrics();

    textRect.moveTop( textRect.bottom() + 1 );

    painter->drawText(  textRect,
                        fm.elidedText( artist, Qt::ElideRight, textRect.width() ),
                        QTextOption( Qt::AlignCenter ) );

    painter->restore();
}

QSize PlIconViewItemDelegate::sizeHint ( const QStyleOptionViewItem &, const QModelIndex & index ) const
{
    QFont f( index.data( Qt::FontRole ).value<QFont>() );
    f.setPointSize( __MAX( f.pointSize() + i_zoom, 4 ) );
    f.setBold( true );
    QFontMetrics fm( f );
    int textHeight = fm.height();
    int averagewidth = fm.averageCharWidth();
    QSize sz ( averagewidth * ICON_SCALER + 4 * SPACER,
               averagewidth * ICON_SCALER + 4 * SPACER + 2 * textHeight + 1 );
    return sz;
}


#define LISTVIEW_ART_SIZE 45

void PlListViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QString title = VLCModel::getMeta( index, COLUMN_TITLE );
    QString duration = VLCModel::getMeta( index, COLUMN_DURATION );
    if( !duration.isEmpty() ) title += QString(" [%1]").arg( duration );

    QString artist = VLCModel::getMeta( index, COLUMN_ARTIST );
    QString album = VLCModel::getMeta( index, COLUMN_ALBUM );
    QString trackNum = VLCModel::getMeta( index, COLUMN_TRACK_NUMBER );
    QString artistAlbum = artist;
    if( !album.isEmpty() )
    {
        if( !artist.isEmpty() ) artistAlbum += ": ";
        artistAlbum += album;
        if( !trackNum.isEmpty() ) artistAlbum += QString( " [#%1]" ).arg( trackNum );
    }

    QPixmap artPix = VLCModel::getArtPixmap( index, QSize( LISTVIEW_ART_SIZE, LISTVIEW_ART_SIZE ) );

    //Draw selection rectangle and current playing item indication
    paintBackground( painter, option, index );

    QRect artRect( artPix.rect() );
    artRect.moveCenter( QPoint( artRect.center().x() + 3,
                                option.rect.center().y() ) );
    //Draw album art
    painter->drawPixmap( artRect, artPix );

    //Start drawing text
    painter->save();

    if( option.state & QStyle::State_Selected )
        painter->setPen( option.palette.color( QPalette::HighlightedText ) );

    QTextOption textOpt( Qt::AlignVCenter | Qt::AlignLeft );
    textOpt.setWrapMode( QTextOption::NoWrap );

    QFont f( index.data( Qt::FontRole ).value<QFont>() );

    //Draw title info
    f.setItalic( true );
    f.setPointSize( __MAX( f.pointSize() + i_zoom, 4 ) );
    f.setBold( index.data( PLModel::IsCurrentRole ).toBool() );
    painter->setFont( f );
    QFontMetrics fm( painter->fontMetrics() );

    QRect textRect = option.rect.adjusted( LISTVIEW_ART_SIZE + 10, 0, -10, 0 );
    if( !artistAlbum.isEmpty() )
    {
        textRect.setHeight( fm.height() );
        textRect.moveBottom( option.rect.center().y() - 2 );
    }

    //Draw children indicator
    if( !index.data( PLModel::IsLeafNodeRole ).toBool() )
    {
        QPixmap dirPix = QPixmap( ":/type/node" );
        painter->drawPixmap( QPoint( textRect.x(), textRect.center().y() - dirPix.height() / 2 ),
                             dirPix );
        textRect.setLeft( textRect.x() + dirPix.width() + 5 );
    }

    painter->drawText( textRect,
                       fm.elidedText( title, Qt::ElideRight, textRect.width() ),
                       textOpt );

    // Draw artist and album info
    if( !artistAlbum.isEmpty() )
    {
        f.setItalic( false );
        painter->setFont( f );
        fm = painter->fontMetrics();

        textRect.moveTop( textRect.bottom() + 4 );
        textRect.setLeft( textRect.x() + 20 );

        painter->drawText( textRect,
                           fm.elidedText( artistAlbum, Qt::ElideRight, textRect.width() ),
                           textOpt );
    }

    painter->restore();
}

QSize PlListViewItemDelegate::sizeHint ( const QStyleOptionViewItem &, const QModelIndex & ) const
{
    QFont f;
    f.setBold( true );
    QFontMetrics fm( f );
    int height = qMax( LISTVIEW_ART_SIZE, 2 * fm.height() + 4 ) + 6;
    return QSize( 0, height );
}


void PlTreeViewItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    if ( index.data( PLModel::IsCurrentRole ).toBool() )
    {
        QStyleOptionViewItem myoptions = option;
        myoptions.font.setBold( true );
        AbstractPlViewItemDelegate::paint( painter, myoptions, index );
    }
    else
    {
        AbstractPlViewItemDelegate::paint( painter, option, index );
    }
}

static inline void plViewStartDrag( QAbstractItemView *view, const Qt::DropActions & supportedActions )
{
    QDrag *drag = new QDrag( view );
    drag->setPixmap( QPixmap( ":/noart64" ) );
    drag->setMimeData( view->model()->mimeData(
        view->selectionModel()->selectedIndexes() ) );
    drag->exec( supportedActions );
}

static void plViewDragMoveEvent( QAbstractItemView *, QDragMoveEvent * event )
{
    if( event->keyboardModifiers() & Qt::ControlModifier &&
        event->possibleActions() & Qt::CopyAction )
        event->setDropAction( Qt::CopyAction );
    else event->acceptProposedAction();
}

PlIconView::PlIconView( PLModel *, QWidget *parent ) : QListView( parent )
{
    PlIconViewItemDelegate *delegate = new PlIconViewItemDelegate( this );

    setViewMode( QListView::IconMode );
    setMovement( QListView::Static );
    setResizeMode( QListView::Adjust );
    setWrapping( true );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setDragEnabled(true);
    setAttribute( Qt::WA_MacShowFocusRect, false );
    viewport()->setAttribute( Qt::WA_Hover );
    /* dropping in QListView::IconMode does not seem to work */
    //setAcceptDrops( true );
    //setDropIndicatorShown(true);

    setItemDelegate( delegate );
}

void PlIconView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

void PlIconView::dragMoveEvent ( QDragMoveEvent * event )
{
    plViewDragMoveEvent( this, event );
    QAbstractItemView::dragMoveEvent( event );
}

bool PlIconView::viewportEvent ( QEvent * event )
{
    if ( event->type() == QEvent::ToolTip )
    {
        event->ignore();
        return true;
    }
    else if ( event->type() == QEvent::Wheel )
    {
        QWheelEvent *wEvent = static_cast<QWheelEvent *>(event);
        if( wEvent->modifiers() & Qt::ControlModifier ) {
            event->ignore();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent( event );
}

PlListView::PlListView( PLModel *, QWidget *parent ) : QListView( parent )
{
    setViewMode( QListView::ListMode );
    setUniformItemSizes( true );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setAlternatingRowColors( true );
    setDragEnabled(true);
    setAcceptDrops( true );
    setDropIndicatorShown(true);

    PlListViewItemDelegate *delegate = new PlListViewItemDelegate( this );
    setItemDelegate( delegate );
    setAttribute( Qt::WA_MacShowFocusRect, false );
    viewport()->setAttribute( Qt::WA_Hover );
}

void PlListView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

void PlListView::dragMoveEvent ( QDragMoveEvent * event )
{
    plViewDragMoveEvent( this, event );
    QAbstractItemView::dragMoveEvent( event );
}

void PlListView::keyPressEvent( QKeyEvent *event )
{
    //If the space key is pressed, override the standard list behaviour to allow pausing
    //to proceed.
    if ( event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Space )
        QWidget::keyPressEvent( event );
    //Otherwise, just do as usual.
    else
        QListView::keyPressEvent( event );
}

bool PlListView::viewportEvent ( QEvent * event )
{
    if ( event->type() == QEvent::ToolTip )
    {
        event->ignore();
        return true;
    }
    else if ( event->type() == QEvent::Wheel )
    {
        QWheelEvent *wEvent = static_cast<QWheelEvent *>(event);
        if( wEvent->modifiers() & Qt::ControlModifier ) {
            event->ignore();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent( event );
}

PlTreeView::PlTreeView( PLModel *, QWidget *parent ) : QTreeView( parent )
{
    setItemDelegate( new PlTreeViewItemDelegate( this ) );

    setIconSize( QSize( 20, 20 ) );
    setAlternatingRowColors( true );
    setAnimated( true );
    setUniformRowHeights( true );
    setSortingEnabled( true );
    setAttribute( Qt::WA_MacShowFocusRect, false );
    viewport()->setAttribute( Qt::WA_Hover );
    header()->setSortIndicator( -1 , Qt::AscendingOrder );
    header()->setSortIndicatorShown( true );
#if QT_VERSION >= 0x050000
    header()->setSectionsClickable( true );
#else
    header()->setClickable( true );
#endif
    header()->setContextMenuPolicy( Qt::CustomContextMenu );

    setSelectionBehavior( QAbstractItemView::SelectRows );
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setDragEnabled( true );
    setAcceptDrops( true );
    setDropIndicatorShown( true );
    setContextMenuPolicy( Qt::CustomContextMenu );
}

void PlTreeView::setModel( QAbstractItemModel * model )
{
    QTreeView::setModel( model );
    CONNECT( this, expanded( const QModelIndex & ),
             model, ensureArtRequested( const QModelIndex & ) );
}

void PlTreeView::startDrag ( Qt::DropActions supportedActions )
{
    plViewStartDrag( this, supportedActions );
}

void PlTreeView::dragMoveEvent ( QDragMoveEvent * event )
{
    plViewDragMoveEvent( this, event );
    QAbstractItemView::dragMoveEvent( event );
}

void PlTreeView::keyPressEvent( QKeyEvent *event )
{
    //If the space key is pressed, override the standard list behaviour to allow pausing
    //to proceed.
    if ( event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Space )
        QWidget::keyPressEvent( event );
    //Otherwise, just do as usual.
    else
        QTreeView::keyPressEvent( event );
}

#include <QHBoxLayout>
PicFlowView::PicFlowView( PLModel *p_model, QWidget *parent ) : QAbstractItemView( parent )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setMargin( 0 );
    picFlow = new PictureFlow( this, p_model );
    layout->addWidget( picFlow );
    picFlow->setSlideSize(QSize( 4*LISTVIEW_ART_SIZE, 3*LISTVIEW_ART_SIZE) );
    setSelectionMode( QAbstractItemView::SingleSelection );
}

void PicFlowView::setModel( QAbstractItemModel *model )
{
    QAbstractItemView::setModel( model );
    picFlow->setModel( model );
}

int PicFlowView::horizontalOffset() const
{
    return 0;
}

int PicFlowView::verticalOffset() const
{
    return 0;
}

QRect PicFlowView::visualRect(const QModelIndex & ) const
{
    return QRect( QPoint(0,0), picFlow->slideSize() );
}

void PicFlowView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint)
{
     int currentIndex = picFlow->centerIndex();
     if( qAbs( currentIndex - index.row()) > 20 )
     {
        /* offset is offset from target index toward currentIndex */
        int offset = -19;
        if( index.row() > currentIndex )
            offset = 19;
        picFlow->setCenterIndex( index.row() + offset );
        picFlow->showSlide( index.row() );
     }
     else
        picFlow->showSlide( index.row() );
}

QModelIndex PicFlowView::indexAt(const QPoint &) const
{
    return QModelIndex();
    // No idea, PictureFlow doesn't provide anything to help this
}

QModelIndex PicFlowView::moveCursor(QAbstractItemView::CursorAction, Qt::KeyboardModifiers)
{
    return QModelIndex();
}

bool PicFlowView::isIndexHidden(const QModelIndex &index) const
{
    int currentIndex = picFlow->centerIndex();
    if( index.row()-5 <= currentIndex &&
        index.row()+5 >= currentIndex )
        return false;
    else
        return true;
}

QRegion PicFlowView::visualRegionForSelection(const QItemSelection &) const
{
    return QRect();
}

void PicFlowView::setSelection(const QRect &, QFlags<QItemSelectionModel::SelectionFlag>)
{
    // No selection possible
}

void PicFlowView::dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight )
{
    int currentIndex = picFlow->centerIndex();
    for(int i = topLeft.row(); i<=bottomRight.row(); i++ )
    {
        if( i-5 <= currentIndex &&
            i+5 >= currentIndex )
        {
            picFlow->render();
            return;
        }
    }
}

void PicFlowView::playItem( int i_item )
{
    emit activated( model()->index(i_item, 0) );
}

bool PicFlowView::viewportEvent ( QEvent * event )
{
    if ( event->type() == QEvent::ToolTip )
    {
        event->ignore();
        return true;
    }
    else if ( event->type() == QEvent::Wheel )
    {
        QWheelEvent *wEvent = static_cast<QWheelEvent *>(event);
        if( wEvent->modifiers() & Qt::ControlModifier ) {
            event->ignore();
            return true;
        }
    }
    return QAbstractItemView::viewportEvent( event );
}
