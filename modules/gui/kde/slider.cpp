/***************************************************************************
                          slider.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/
/***************************************************************************
    shamelessly copied from noatun's excellent interface
****************************************************************************/

#include "slider.h"

KVLCSlider::KVLCSlider(QWidget * parent, const char * name) :
    QSlider(parent,name), pressed(false)
{
}

KVLCSlider::KVLCSlider(Orientation o, QWidget * parent, const char * name) :
    QSlider(o,parent,name), pressed(false)
{
}

KVLCSlider::KVLCSlider(int minValue, int maxValue, int pageStep, int value,
                       Orientation o, QWidget * parent, const char * name) :
    QSlider(minValue, maxValue, pageStep, value, o, parent,name), pressed(false)
{
}

void KVLCSlider::setValue(int i)
{
    if ( !pressed )
    {
        QSlider::setValue( i );
    }
}

void KVLCSlider::mousePressEvent( QMouseEvent *e )
{
    if ( e->button() != RightButton )
    {
        pressed=true;
        QSlider::mousePressEvent( e );
    }
}

void KVLCSlider::mouseReleaseEvent( QMouseEvent *e )
{
    pressed=false;
    QSlider::mouseReleaseEvent( e );
    emit userChanged( value() );
}
