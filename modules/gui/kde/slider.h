/***************************************************************************
                          slider.h  -  description
                             -------------------
    begin                : Sun Apr 03 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/
/***************************************************************************
        shamelessly copied from noatun's excellent interface
****************************************************************************/
#ifndef _KDE_SLIDER_H_
#define _KDE_SLIDER_H_

#include <qslider.h>

/**
 * This slider can be changed by the vlc while not dragged by the user
 */
class KVLCSlider : public QSlider
{
    Q_OBJECT
    public:
        KVLCSlider(QWidget * parent, const char * name=0);
        KVLCSlider(Orientation, QWidget * parent, const char * name=0);
        KVLCSlider(int minValue, int maxValue, int pageStep, int value,
                   Orientation, QWidget * parent, const char * name=0);

    signals:
    /**
     * emmited only when the user changes the value by hand
     */
    void userChanged( int value );

    public slots:
        virtual void setValue( int );

    protected:
        virtual void mousePressEvent( QMouseEvent * e );
        virtual void mouseReleaseEvent( QMouseEvent * e );

    private:
        bool pressed; // set this to true when the user drags the slider
};

#endif /* _KDE_SLIDER_H_ */
