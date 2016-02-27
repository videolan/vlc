/*****************************************************************************
 * info_widgets.cpp : Widgets for info panels
 ****************************************************************************
 * Copyright (C) 2013 the VideoLAN team
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

#include "qt.hpp"
#include "info_widgets.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPolygonF>
#include <QGraphicsPolygonItem>
#include <QGraphicsLineItem>
#include <QVectorIterator>

#define STATS_LENGTH 60
#define ADD_LABEL(row, color, text) \
label = new QLabel( QString( "<font color=\"%1\">%2</font>" ) \
                    .arg( color.name() ) \
                    .arg( text ) \
                    ); \
layout->addWidget( label, row, 0, 1, 1, 0 );

VLCStatsView::VLCStatsView( QWidget *parent ) : QGraphicsView( parent )
{
    QColor history(0, 0, 0, 255),
        total(237, 109, 0, 160),
        content(109, 237, 0, 160);

    scale( 1.0, -1.0 ); /* invert our Y axis */
    setOptimizationFlags( QGraphicsView::DontAdjustForAntialiasing );
    setAlignment( Qt::AlignLeft );
    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    viewScene = new QGraphicsScene( this );
    historyShape = viewScene->addPolygon( QPolygonF(), QPen(Qt::NoPen),
                                             QBrush(history) );
    totalbitrateShape = viewScene->addPolygon( QPolygonF(), QPen(Qt::NoPen),
                                           QBrush(total) );
    setScene( viewScene );
    reset();

    QPen linepen( Qt::DotLine );
    linepen.setCosmetic( true );
    linepen.setBrush( QBrush( QColor( 33, 33, 33 ) ) );
    for ( int i=0; i<3; i++ )
        rulers[i] = viewScene->addLine( QLineF(), linepen );
}
#undef ADD_LABEL

void VLCStatsView::reset()
{
    historymergepointer = 0;
    blocksize = 4;
    valuesaccumulator = 0;
    valuesaccumulatorcount = 0;
    historyShape->setPolygon( QPolygonF() );
    totalbitrateShape->setPolygon( QPolygonF() );
}

void VLCStatsView::addValue( float value )
{
    value /= 1000;

    QPolygonF shape = totalbitrateShape->polygon();
    if ( shape.count() > ( STATS_LENGTH + 2 ) ) /* keep only STATS_LENGTH samples */
    {
        shape.remove( 1 );
        for(int i=1; i<( STATS_LENGTH + 2 ); i++)
            ( (QPointF &) shape.at(i) ).setX( i - 1 ); /*move back values*/
    }

    int count = shape.count();
    if ( count == 0 )
    {
        shape << QPointF( 0, 0 ); /* begin and close shape */
        shape << QPointF( count, 0 );
    }

    shape.insert( shape.end() - 1, QPointF( count, value ) );
    ( (QPointF &) shape.last() ).setX( count );
    totalbitrateShape->setPolygon( shape );

    addHistoryValue( value );

    QRectF maxsizes = scene()->itemsBoundingRect();
    maxsizes.setRight( STATS_LENGTH );
    fitInView( maxsizes ); /* fix viewport */
    drawRulers( maxsizes );
}

void VLCStatsView::drawRulers( const QRectF &maxsizes )
{
    float height = maxsizes.height() * 1000;
    int texp = 0;
    while( height > 1.0 ) { height /= 10; texp++; }
    height = 1.0;
    while( texp-- ) height *= 10;
    for ( int i=0; i<3; i++ )
    {
        float y = ( height / 5 ) * ( i + 1 ) / 1000;
        rulers[i]->setLine( QLineF( 0, y, STATS_LENGTH, y ) );
    }
}

void VLCStatsView::addHistoryValue( float value )
{
    /* We keep a full history by creating virtual blocks for inserts, growing
       by power of 2 when no more space is available. At this time, we also
       free space by agregating the oldest values 2 by 2.
       Each shown value finally being a mean of blocksize samples.
    */
    bool doinsert = false;
    int next_blocksize = blocksize;
    QPolygonF shape = historyShape->polygon();
    int count = shape.count();
    if ( count == 0 )
    {
        shape << QPointF( 0, 0 ); /* begin and close shape */
        shape << QPointF( count, 0 );
    }

    valuesaccumulator += ( value / blocksize );
    valuesaccumulatorcount++;

    if ( valuesaccumulatorcount == blocksize )
    {
        valuesaccumulator = 0;
        valuesaccumulatorcount = 0;
        doinsert = true;
    }

    if ( doinsert )
    {
        if ( count > ( STATS_LENGTH + 2 ) )
        {
            float y = 0;
            y += ((QPointF &) shape.at( historymergepointer + 1 )).y();
            y += ((QPointF &) shape.at( historymergepointer + 2 )).y();
            y /= 2;

            /* merge */
            shape.remove( historymergepointer + 2 );
            ( (QPointF &) shape.at( historymergepointer + 1 ) ).setY( y );
            for(int i=historymergepointer +1; i<( STATS_LENGTH + 2 ); i++)
                ( (QPointF &) shape.at(i) ).setX( i - 1 ); /*move back values*/
            historymergepointer++;
            if ( historymergepointer > ( STATS_LENGTH - 1 ) )
            {
                historymergepointer = 0;
                next_blocksize = ( blocksize << 1 );
            }
        }

        shape.insert( shape.end() - 1, QPointF( count, value ) );
        ( (QPointF &) shape.last() ).setX( count );
    }
    else
        ( (QPointF &) shape.last() ).setX( count - 1 );

    historyShape->setPolygon( shape );

    blocksize = next_blocksize;
}
