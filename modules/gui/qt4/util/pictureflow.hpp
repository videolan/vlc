/*
  PictureFlow - animated image show widget
  http://pictureflow.googlecode.com
    and
  http://libqxt.org  <foundation@libqxt.org>

  Copyright (C) 2009 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2008 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)

  Copyright (C) Qxt Foundation. Some rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef PICTUREFLOW_H
#define PICTUREFLOW_H

#include <QApplication>
#include <QCache>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QList>

#include "../components/playlist/playlist_model.hpp" /* getArtPixmap etc */


class PictureFlowPrivate;

/*!
  Class PictureFlow implements an image show widget with animation effect
  like Apple's CoverFlow (in iTunes and iPod). Images are arranged in form
  of slides, one main slide is shown at the center with few slides on
  the left and right sides of the center slide. When the next or previous
  slide is brought to the front, the whole slides flow to the right or
  the right with smooth animation effect; until the new slide is finally
  placed at the center.

 */
class PictureFlow : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QSize slideSize READ slideSize WRITE setSlideSize)
    Q_PROPERTY(int slideCount READ slideCount)
    Q_PROPERTY(int centerIndex READ centerIndex WRITE setCenterIndex)

public:

    enum ReflectionEffect {
        NoReflection,
        PlainReflection,
        BlurredReflection
    };

    /*!
      Creates a new PictureFlow widget.
    */
    PictureFlow(QWidget* parent = 0, VLCModel *model = 0);

    /*!
      Destroys the widget.
    */
    ~PictureFlow();

    void setModel(QAbstractItemModel * model);
    QAbstractItemModel * model();

    /*!
      Returns the background color.
    */
    QColor backgroundColor() const;

    /*!
      Sets the background color. By default it is black.
    */
    void setBackgroundColor(const QColor& c);

    /*!
      Returns the dimension of each slide (in pixels).
    */
    QSize slideSize() const;

    /*!
      Sets the dimension of each slide (in pixels).
    */
    void setSlideSize(QSize size);

    /*!
      Returns the total number of slides.
    */
    int slideCount() const;

    /*!
      Returns the index of slide currently shown in the middle of the viewport.
    */
    int centerIndex() const;

    /*!
      Returns the effect applied to the reflection.
    */
    ReflectionEffect reflectionEffect() const;

    /*!
      Sets the effect applied to the reflection. The default is PlainReflection.
    */
    void setReflectionEffect(ReflectionEffect effect);


public slots:

    /*!
      Sets slide to be shown in the middle of the viewport. No animation
      effect will be produced, unlike using showSlide.
    */
    void setCenterIndex(int index);

    /*!
      Clears all slides.
    */
    void clear();

    /*!
      Shows previous slide using animation effect.
    */
    void showPrevious();

    /*!
      Shows next slide using animation effect.
    */
    void showNext();

    /*!
      Go to specified slide using animation effect.
    */
    void showSlide(int index);

    /*!
      Rerender the widget. Normally this function will be automatically invoked
      whenever necessary, e.g. during the transition animation.
    */
    void render();

    /*!
      Schedules a rendering update. Unlike render(), this function does not cause
      immediate rendering.
    */
    void triggerRender();

signals:
    void centerIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent *event);
    void keyPressEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent* event);

private slots:
    void updateAnimation();

private:
    PictureFlowPrivate* d;
};

// for fixed-point arithmetic, we need minimum 32-bit long
// long long (64-bit) might be useful for multiplication and division
typedef long PFreal;
#define PFREAL_SHIFT 10
#define PFREAL_ONE (1 << PFREAL_SHIFT)

#define IANGLE_MAX 1024
#define IANGLE_MASK 1023

inline PFreal fmul(PFreal a, PFreal b)
{
    return ((long long)(a))*((long long)(b)) >> PFREAL_SHIFT;
}

inline PFreal fdiv(PFreal num, PFreal den)
{
    long long p = (long long)(num) << (PFREAL_SHIFT * 2);
    long long q = p / (long long)den;
    long long r = q >> PFREAL_SHIFT;

    return r;
}

inline PFreal fsin(int iangle)
{
    // warning: regenerate the table if IANGLE_MAX and PFREAL_SHIFT are changed!
    static const PFreal tab[] = {
        3,    103,    202,    300,    394,    485,    571,    652,
        726,    793,    853,    904,    947,    980,   1004,   1019,
        1023,   1018,   1003,    978,    944,    901,    849,    789,
        721,    647,    566,    479,    388,    294,    196,     97,
        -4,   -104,   -203,   -301,   -395,   -486,   -572,   -653,
        -727,   -794,   -854,   -905,   -948,   -981,  -1005,  -1020,
        -1024,  -1019,  -1004,   -979,   -945,   -902,   -850,   -790,
        -722,   -648,   -567,   -480,   -389,   -295,   -197,    -98,
        3
    };

    while (iangle < 0)
        iangle += IANGLE_MAX;
    iangle &= IANGLE_MASK;

    int i = (iangle >> 4);
    PFreal p = tab[i];
    PFreal q = tab[(i+1)];
    PFreal g = (q - p);
    return p + g *(iangle - i*16) / 16;
}

inline PFreal fcos(int iangle)
{
    return fsin(iangle + (IANGLE_MAX >> 2));
}

struct SlideInfo {
    int slideIndex;
    int angle;
    PFreal cx;
    PFreal cy;
    int blend;
};

class PictureFlow;

class PictureFlowState
{
public:
    PictureFlowState();
    ~PictureFlowState();

    void reposition();
    void reset();

    QRgb backgroundColor;
    int slideWidth;
    int slideHeight;
    PictureFlow::ReflectionEffect reflectionEffect;

    int angle;
    int spacing;
    PFreal offsetX;
    PFreal offsetY;

    VLCModel *model;
    SlideInfo centerSlide;
    QVector<SlideInfo> leftSlides;
    QVector<SlideInfo> rightSlides;
    int centerIndex;
};

class PictureFlowAnimator
{
public:
    PictureFlowAnimator();
    PictureFlowState* state;

    void start(int slide);
    void stop(int slide);
    void update();

    int target;
    int step;
    int frame;
    QTimer animateTimer;
};

class PictureFlowAbstractRenderer
{
public:
    PictureFlowAbstractRenderer(): state(0), dirty(false), widget(0) {}
    virtual ~PictureFlowAbstractRenderer() {}

    PictureFlowState* state;
    bool dirty;
    QWidget* widget;

    virtual void init() = 0;
    virtual void paint() = 0;
};

class PictureFlowSoftwareRenderer: public PictureFlowAbstractRenderer
{
public:
    PictureFlowSoftwareRenderer();
    ~PictureFlowSoftwareRenderer();

    virtual void init();
    virtual void paint();

private:
    QSize size;
    QRgb bgcolor;
    int effect;
    QImage buffer;
    QVector<PFreal> rays;
    QImage* blankSurface;

    void render();
    void renderSlides();
    QRect renderSlide(const SlideInfo &slide, int col1 = -1, int col2 = -1);
    QImage* surface(QModelIndex);
    QHash<QString, QImage*> cache;
};

class PictureFlowPrivate : public QObject
{
    Q_OBJECT
public:
    PictureFlowState* state;
    PictureFlowAnimator* animator;
    PictureFlowAbstractRenderer* renderer;
    QTimer triggerTimer;
    void setModel(QAbstractItemModel * model);
    void clear();
    void triggerRender();
    void insertSlide(int index, const QImage& image);
    void replaceSlide(int index, const QImage& image);
    void removeSlide(int index);
    void setCurrentIndex(QModelIndex index);
    void showSlide(int index);

    int picrole;
    int textrole;
    int piccolumn;
    int textcolumn;

    void reset();

    QList<QPersistentModelIndex> modelmap;
    QPersistentModelIndex currentcenter;

    QPoint lastgrabpos;
    QModelIndex rootindex;

public Q_SLOTS:
    void columnsAboutToBeInserted(const QModelIndex & parent, int start, int end);
    void columnsAboutToBeRemoved(const QModelIndex & parent, int start, int end);
    void columnsInserted(const QModelIndex & parent, int start, int end);
    void columnsRemoved(const QModelIndex & parent, int start, int end);
    void dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
    void headerDataChanged(Qt::Orientation orientation, int first, int last);
    void layoutAboutToBeChanged();
    void layoutChanged();
    void modelAboutToBeReset();
    void modelReset();
    void rowsAboutToBeInserted(const QModelIndex & parent, int start, int end);
    void rowsAboutToBeRemoved(const QModelIndex & parent, int start, int end);
    void rowsInserted(const QModelIndex & parent, int start, int end);
    void rowsRemoved(const QModelIndex & parent, int start, int end);
};


#endif // PICTUREFLOW_H

