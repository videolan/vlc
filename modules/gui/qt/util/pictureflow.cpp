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

#include "pictureflow.hpp"

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <QHash>
#include "../components/playlist/playlist_model.hpp" /* getArtPixmap etc */
#include "../components/playlist/sorting.h"          /* Columns List */
#include "input_manager.hpp"


// ------------- PictureFlowState ---------------------------------------

PictureFlowState::PictureFlowState():
        backgroundColor(qRgba(0,0,0,0)), slideWidth(150), slideHeight(120),
        reflectionEffect(PictureFlow::BlurredReflection), centerIndex(0)
{
}

PictureFlowState::~PictureFlowState()
{
}

// readjust the settings, call this when slide dimension is changed
void PictureFlowState::reposition()
{
    angle = 70 * IANGLE_MAX / 360;  // approx. 70 degrees tilted

    offsetX = slideWidth / 2 * (PFREAL_ONE - fcos(angle));
    offsetY = slideWidth / 2 * fsin(angle);
    offsetX += slideWidth * PFREAL_ONE;
    offsetY += slideWidth * PFREAL_ONE / 4;
    spacing = 40;
}

// adjust slides so that they are in "steady state" position
void PictureFlowState::reset()
{
    centerSlide.angle = 0;
    centerSlide.cx = 0;
    centerSlide.cy = 0;
    centerSlide.slideIndex = centerIndex;
    centerSlide.blend = 256;

    leftSlides.resize(6);
    for (int i = 0; i < (int)leftSlides.count(); i++) {
        SlideInfo& si = leftSlides[i];
        si.angle = angle;
        si.cx = -(offsetX + spacing * i * PFREAL_ONE);
        si.cy = offsetY;
        si.slideIndex = centerIndex - 1 - i;
        si.blend = 256;
        if (i == (int)leftSlides.count() - 2)
            si.blend = 128;
        if (i == (int)leftSlides.count() - 1)
            si.blend = 0;
    }

    rightSlides.resize(6);
    for (int i = 0; i < (int)rightSlides.count(); i++) {
        SlideInfo& si = rightSlides[i];
        si.angle = -angle;
        si.cx = offsetX + spacing * i * PFREAL_ONE;
        si.cy = offsetY;
        si.slideIndex = centerIndex + 1 + i;
        si.blend = 256;
        if (i == (int)rightSlides.count() - 2)
            si.blend = 128;
        if (i == (int)rightSlides.count() - 1)
            si.blend = 0;
    }
}

// ------------- PictureFlowAnimator  ---------------------------------------

PictureFlowAnimator::PictureFlowAnimator():
        state(0), target(0), step(0), frame(0)
{
}

void PictureFlowAnimator::start(int slide)
{
    target = slide;
    if (!animateTimer.isActive() && state) {
        step = (target < state->centerSlide.slideIndex) ? -1 : 1;
        animateTimer.start(30);
    }
}

void PictureFlowAnimator::stop(int slide)
{
    step = 0;
    target = slide;
    frame = slide << 16;
    animateTimer.stop();
}

void PictureFlowAnimator::update()
{
    if (!animateTimer.isActive())
        return;
    if (step == 0)
        return;
    if (!state)
        return;

    int speed = 16384/2;

#if 1
    // deaccelerate when approaching the target
    const int max = 2 * 65536;

    int fi = frame;
    fi -= (target << 16);
    if (fi < 0)
        fi = -fi;
    fi = qMin(fi, max);

    int ia = IANGLE_MAX * (fi - max / 2) / (max * 2);
    speed = 512 + 16384 * (PFREAL_ONE + fsin(ia)) / PFREAL_ONE;
#endif

    frame += speed * step;

    int index = frame >> 16;
    int pos = frame & 0xffff;
    int neg = 65536 - pos;
    int tick = (step < 0) ? neg : pos;
    PFreal ftick = (tick * PFREAL_ONE) >> 16;

    if (step < 0)
        index++;

    if (state->centerIndex != index) {
        state->centerIndex = index;
        frame = index << 16;
        state->centerSlide.slideIndex = state->centerIndex;
        for (int i = 0; i < (int)state->leftSlides.count(); i++)
            state->leftSlides[i].slideIndex = state->centerIndex - 1 - i;
        for (int i = 0; i < (int)state->rightSlides.count(); i++)
            state->rightSlides[i].slideIndex = state->centerIndex + 1 + i;
    }

    state->centerSlide.angle = (step * tick * state->angle) >> 16;
    state->centerSlide.cx = -step * fmul(state->offsetX, ftick);
    state->centerSlide.cy = fmul(state->offsetY, ftick);

    if (state->centerIndex == target) {
        stop(target);
        state->reset();
        return;
    }

    for (int i = 0; i < (int)state->leftSlides.count(); i++) {
        SlideInfo& si = state->leftSlides[i];
        si.angle = state->angle;
        si.cx = -(state->offsetX + state->spacing * i * PFREAL_ONE + step * state->spacing * ftick);
        si.cy = state->offsetY;
    }

    for (int i = 0; i < (int)state->rightSlides.count(); i++) {
        SlideInfo& si = state->rightSlides[i];
        si.angle = -state->angle;
        si.cx = state->offsetX + state->spacing * i * PFREAL_ONE - step * state->spacing * ftick;
        si.cy = state->offsetY;
    }

    if (step > 0) {
        PFreal ftick = (neg * PFREAL_ONE) >> 16;
        state->rightSlides[0].angle = -(neg * state->angle) >> 16;
        state->rightSlides[0].cx = fmul(state->offsetX, ftick);
        state->rightSlides[0].cy = fmul(state->offsetY, ftick);
    } else {
        PFreal ftick = (pos * PFREAL_ONE) >> 16;
        state->leftSlides[0].angle = (pos * state->angle) >> 16;
        state->leftSlides[0].cx = -fmul(state->offsetX, ftick);
        state->leftSlides[0].cy = fmul(state->offsetY, ftick);
    }

    // must change direction ?
    if (target < index) if (step > 0)
            step = -1;
    if (target > index) if (step < 0)
            step = 1;

    // the first and last slide must fade in/fade out
    int nleft = state->leftSlides.count();
    int nright = state->rightSlides.count();
    int fade = pos / 256;

    for (int index = 0; index < nleft; index++) {
        int blend = 256;
        if (index == nleft - 1)
            blend = (step > 0) ? 0 : 128 - fade / 2;
        if (index == nleft - 2)
            blend = (step > 0) ? 128 - fade / 2 : 256 - fade / 2;
        if (index == nleft - 3)
            blend = (step > 0) ? 256 - fade / 2 : 256;
        state->leftSlides[index].blend = blend;
    }
    for (int index = 0; index < nright; index++) {
        int blend = (index < nright - 2) ? 256 : 128;
        if (index == nright - 1)
            blend = (step > 0) ? fade / 2 : 0;
        if (index == nright - 2)
            blend = (step > 0) ? 128 + fade / 2 : fade / 2;
        if (index == nright - 3)
            blend = (step > 0) ? 256 : 128 + fade / 2;
        state->rightSlides[index].blend = blend;
    }

}

// ------------- PictureFlowSoftwareRenderer ---------------------------------------

PictureFlowSoftwareRenderer::PictureFlowSoftwareRenderer():
        PictureFlowAbstractRenderer(), size(0, 0), bgcolor(0), effect(-1), blankSurface(0)
{
}

PictureFlowSoftwareRenderer::~PictureFlowSoftwareRenderer()
{
    buffer = QImage();
    qDeleteAll( cache.values() );
    delete blankSurface;
}

void PictureFlowSoftwareRenderer::paint()
{
    if (!widget)
        return;

    if (widget->size() != size)
        init();

    if (state->backgroundColor != bgcolor) {
        bgcolor = state->backgroundColor;
    }

    if ((int)(state->reflectionEffect) != effect) {
        effect = (int)state->reflectionEffect;
    }

    if (dirty)
        render();

    QPainter painter(widget);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QPoint(0, 0), buffer);
}

void PictureFlowSoftwareRenderer::init()
{
    if (!widget)
        return;

    blankSurface = 0;

    size = widget->size();
    int ww = size.width();
    int wh = size.height();
    int w = (ww + 1) / 2;
    int h = (wh + 1) / 2;

    buffer = QImage(ww, wh, QImage::Format_ARGB32);
    buffer.fill(bgcolor);

    rays.resize(w*2);
    for (int i = 0; i < w; i++) {
        PFreal gg = ((PFREAL_ONE >> 1) + i * PFREAL_ONE) / (2 * h);
        rays[w-i-1] = -gg;
        rays[w+i] = gg;
    }

    dirty = true;
}

// TODO: optimize this with lookup tables
static QRgb blendColor(QRgb c1, QRgb c2, int blend)
{
    unsigned int a,r,g,b,as,ad;
    if(blend>255)
        blend=255;
    as=(qAlpha(c1)*blend)/256;
    ad=qAlpha(c2);
    a=as+((255-as)*ad)/256;
    if(a>0)
    {
        r=(as*qRed(c1)+((255-as)*ad*qRed(c2))/256)/a;
        g=(as*qGreen(c1)+((255-as)*ad*qGreen(c2))/256)/a;
        b=(as*qBlue(c1)+((255-as)*ad*qBlue(c2))/256)/a;
    }
    else
    {
        r=g=b=0;
    }
    return qRgba(r, g, b, a);
}


static QImage* prepareSurface(const QImage* slideImage, int w, int h, QRgb bgcolor,
                              PictureFlow::ReflectionEffect reflectionEffect, QModelIndex index)
{
    Q_UNUSED(bgcolor);
    Qt::TransformationMode mode = Qt::SmoothTransformation;
    QImage img = slideImage->scaled(w, h, Qt::KeepAspectRatio, mode);

    // slightly larger, to accomodate for the reflection
    int hs = h * 2;
    int hofs = h / 3;

    // offscreen buffer: black is sweet
    QImage* result = new QImage(hs, w, QImage::Format_ARGB32);
    QFont font( index.data( Qt::FontRole ).value<QFont>() );
    QPainter imagePainter( result );
    QTransform rotation;
    imagePainter.setFont( font );
    rotation.rotate(90);
    rotation.scale(1,-1);
    rotation.translate( 0, hofs );
    QRgb bg=qRgba(0, 0, 0, 0);
    result->fill(bg);

    // transpose the image, this is to speed-up the rendering
    // because we process one column at a time
    // (and much better and faster to work row-wise, i.e in one scanline)
    /*
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            result->setPixel(hofs + y, x, img.pixel(x, y));
    */
    if (reflectionEffect != PictureFlow::NoReflection) {
        // create the reflection
        int ht = hs - h - hofs;
        int hte = ht;
        for (int x = 0; x < w; x++)
        {
            QRgb *line = (QRgb*)(result->scanLine( x ));
            int xw=img.width(),yw=img.height();
            QRgb color;
            for (int y = 0; y < ht; y++) {
                color=bg;
                int x0=x-(w-xw)/2;
                int y0=yw - y - 1+(h-yw)/2;
                if(x0>=0 && x0<xw && y0>=0 && y0<yw)
                    color = img.pixel(x0, y0);
                line[h+hofs+y] = blendColor( color, bg, 128*(hte-y)/hte );
                //result->setPixel(h + hofs + y, x, blendColor(color, bgcolor, 128*(hte - y) / hte));
            }
        }

        if (reflectionEffect == PictureFlow::BlurredReflection) {
            // blur the reflection everything first
            // Based on exponential blur algorithm by Jani Huhtanen
            QRect rect(hs / 2, 0, hs / 2, w);
            rect &= result->rect();

            int r1 = rect.top();
            int r2 = rect.bottom();
            int c1 = rect.left();
            int c2 = rect.right();

            int bpl = result->bytesPerLine();
            int rgba[4];
            unsigned char* p;

            // how many times blur is applied?
            // for low-end system, limit this to only 1 loop
            for (int loop = 0; loop < 2; loop++) {
                for (int col = c1; col <= c2; col++) {
                    p = result->scanLine(r1) + col * 4;
                    for (int i = 0; i < 3; i++)
                        rgba[i] = p[i] << 4;

                    p += bpl;
                    for (int j = r1; j < r2; j++, p += bpl)
                        for (int i = 0; i < 3; i++)
                            p[i] = (rgba[i] += (((p[i] << 4) - rgba[i])) >> 1) >> 4;
                }

                for (int row = r1; row <= r2; row++) {
                    p = result->scanLine(row) + c1 * 4;
                    for (int i = 0; i < 3; i++)
                        rgba[i] = p[i] << 4;

                    p += 4;
                    for (int j = c1; j < c2; j++, p += 4)
                        for (int i = 0; i < 3; i++)
                            p[i] = (rgba[i] += (((p[i] << 4) - rgba[i])) >> 1) >> 4;
                }

                for (int col = c1; col <= c2; col++) {
                    p = result->scanLine(r2) + col * 4;
                    for (int i = 0; i < 3; i++)
                        rgba[i] = p[i] << 4;

                    p -= bpl;
                    for (int j = r1; j < r2; j++, p -= bpl)
                        for (int i = 0; i < 3; i++)
                            p[i] = (rgba[i] += (((p[i] << 4) - rgba[i])) >> 1) >> 4;
                }

                for (int row = r1; row <= r2; row++) {
                    p = result->scanLine(row) + c2 * 4;
                    for (int i = 0; i < 3; i++)
                        rgba[i] = p[i] << 4;

                    p -= 4;
                    for (int j = c1; j < c2; j++, p -= 4)
                        for (int i = 0; i < 3; i++)
                            p[i] = (rgba[i] += (((p[i] << 4) - rgba[i])) >> 1) >> 4;
                }
            }
        }
    }
    // overdraw to leave only the reflection blurred (but not the actual image)
    imagePainter.setTransform( rotation );
    imagePainter.drawImage( (w-img.width())/2, (h-img.height())/2, img );
    imagePainter.setBrush( QColor(bg));//QBrush( Qt::lightGray ) );
    imagePainter.setPen( QColor( Qt::lightGray ) );
    QFontMetrics fm = imagePainter.fontMetrics();
    imagePainter.setPen( QColor( Qt::darkGray ) );
    imagePainter.drawText( 0+1, 1+h-fm.height()*2, VLCModel::getMeta( index, COLUMN_TITLE ) );
    imagePainter.setPen( QColor( Qt::lightGray ) );
    imagePainter.drawText( 0, h-fm.height()*2, VLCModel::getMeta( index, COLUMN_TITLE ) );
    imagePainter.setPen( QColor( Qt::darkGray ) );
    imagePainter.drawText( 0+1, 1+h-fm.height()*1, VLCModel::getMeta( index, COLUMN_ARTIST ) );
    imagePainter.setPen( QColor( Qt::lightGray ) );
    imagePainter.drawText( 0, h-fm.height()*1, VLCModel::getMeta( index, COLUMN_ARTIST ) );

    return result;
}

QImage* PictureFlowSoftwareRenderer::surface(QModelIndex index)
{
    if (!state || !index.isValid())
        return 0;

    QImage* img = new QImage(VLCModel::getArtPixmap( index,
                                         QSize( state->slideWidth, state->slideHeight ) ).toImage());

    QImage* sr = prepareSurface(img, state->slideWidth, state->slideHeight, bgcolor, state->reflectionEffect, index );

    delete img;
    return sr;
}

// Renders a slide to offscreen buffer. Returns a rect of the rendered area.
// col1 and col2 limit the column for rendering.
QRect PictureFlowSoftwareRenderer::renderSlide(const SlideInfo &slide, int col1, int col2)
{
    int blend = slide.blend;
    if (!blend)
        return QRect();

    QModelIndex index;

    QString artURL;

    VLCModel *m = static_cast<VLCModel*>( state->model );

    if( m )
    {
        index = m->index( slide.slideIndex, 0, m->currentIndex().parent() );
        if( !index.isValid() )
            return QRect();
        artURL = m->data( index, COLUMN_COVER ).toString();
    }

    QString key = QString("%1%2%3%4").arg(VLCModel::getMeta( index, COLUMN_TITLE )).arg( VLCModel::getMeta( index, COLUMN_ARTIST ) ).arg(index.data( VLCModel::CURRENT_ITEM_ROLE ).toBool() ).arg( artURL );

    QImage* src;
    if( cache.contains( key ) )
       src = cache.value( key );
    else
    {
       src = surface( index );
       cache.insert( key, src );
    }
    if (!src)
        return QRect();

    QRect rect(0, 0, 0, 0);

    int sw = src->height();
    int sh = src->width();
    int h = buffer.height();
    int w = buffer.width();

    if (col1 > col2) {
        int c = col2;
        col2 = col1;
        col1 = c;
    }

    col1 = (col1 >= 0) ? col1 : 0;
    col2 = (col2 >= 0) ? col2 : w - 1;
    col1 = qMin(col1, w - 1);
    col2 = qMin(col2, w - 1);

    int zoom = 100;
    int distance = h * 100 / zoom;
    PFreal sdx = fcos(slide.angle);
    PFreal sdy = fsin(slide.angle);
    PFreal xs = slide.cx - state->slideWidth * sdx / 2;
    PFreal ys = slide.cy - state->slideWidth * sdy / 2;
    PFreal dist = distance * PFREAL_ONE;

    int xi = qMax((PFreal)0, ((w * PFREAL_ONE / 2) + fdiv(xs * h, dist + ys)) >> PFREAL_SHIFT);
    if (xi >= w)
    {
        return rect;
    }

    bool flag = false;
    rect.setLeft(xi);
    for (int x = qMax(xi, col1); x <= col2; x++) {
        PFreal hity = 0;
        PFreal fk = rays[x];
        if (sdy) {
            fk = fk - fdiv(sdx, sdy);
            hity = -fdiv((rays[x] * distance - slide.cx + slide.cy * sdx / sdy), fk);
        }

        dist = distance * PFREAL_ONE + hity;
        if (dist < 0)
            continue;

        PFreal hitx = fmul(dist, rays[x]);
        PFreal hitdist = fdiv(hitx - slide.cx, sdx);

        int column = sw / 2 + (hitdist >> PFREAL_SHIFT);
        if (column >= sw)
            break;
        if (column < 0)
            continue;

        rect.setRight(x);
        if (!flag)
            rect.setLeft(x);
        flag = true;

        int y1 = h / 2;
        int y2 = y1 + 1;
        QRgb* pixel1 = (QRgb*)(buffer.scanLine(y1)) + x;
        QRgb* pixel2 = (QRgb*)(buffer.scanLine(y2)) + x;
        QRgb pixelstep = pixel2 - pixel1;

        int center = (sh / 2);
        int dy = dist / h;
        int p1 = center * PFREAL_ONE - dy / 2;
        int p2 = center * PFREAL_ONE + dy / 2;

        const QRgb *ptr = (const QRgb*)(src->scanLine(column));
            while ((y1 >= 0) && (y2 < h) && (p1 >= 0)) {
                QRgb c1 = ptr[p1 >> PFREAL_SHIFT];
                QRgb c2 = ptr[p2 >> PFREAL_SHIFT];
                *pixel1 = blendColor(c1, *pixel1+0*bgcolor, blend);
                *pixel2 = blendColor(c2, *pixel2+0*bgcolor, blend);
                p1 -= dy;
                p2 += dy;
                y1--;
                y2++;
                pixel1 -= pixelstep;
                pixel2 += pixelstep;
            }
    }

    rect.setTop(0);
    rect.setBottom(h - 1);
    return rect;
}

void PictureFlowSoftwareRenderer::renderSlides()
{
    int nleft = state->leftSlides.count();
    int nright = state->rightSlides.count();

    for (int index = nleft-1; index >= 0; index--) {
        renderSlide(state->leftSlides[index]);
    }
    for (int index = nright-1; index >= 0; index--) {
        renderSlide(state->rightSlides[index]);
    }
    renderSlide(state->centerSlide);
}

// Render the slides. Updates only the offscreen buffer.
void PictureFlowSoftwareRenderer::render()
{
    buffer.fill(state->backgroundColor);
    renderSlides();
    dirty = false;
}

// -----------------------------------------


PictureFlow::PictureFlow(QWidget* parent, QAbstractItemModel* _p_model): QWidget(parent)
{
    d = new PictureFlowPrivate;
    d->picrole = Qt::DecorationRole;
    d->textrole = Qt::DisplayRole;
    d->piccolumn = 0;
    d->textcolumn = 0;

    d->state = new PictureFlowState;
    d->state->model = 0;
    d->state->reset();
    d->state->reposition();

    d->renderer = new PictureFlowSoftwareRenderer;
    d->renderer->state = d->state;
    d->renderer->widget = this;
    d->renderer->init();

    d->animator = new PictureFlowAnimator;
    d->animator->state = d->state;
    QObject::connect(&d->animator->animateTimer, SIGNAL(timeout()), this, SLOT(updateAnimation()));

    QObject::connect(&d->triggerTimer, SIGNAL(timeout()), this, SLOT(render()));

    setAttribute(Qt::WA_StaticContents, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    d->setModel(_p_model);
}

PictureFlow::~PictureFlow()
{
    delete d->renderer;
    delete d->animator;
    delete d->state;
    delete d;
}

/*!
    Sets the \a model.

    \bold {Note:} The view does not take ownership of the model unless it is the
    model's parent object because it may be shared between many different views.
 */
void PictureFlow::setModel(QAbstractItemModel * model)
{
    d->setModel(model);
    //d->state->model=(VLCModel*)model;
    d->state->reset();
    d->state->reposition();
    d->renderer->init();
    triggerRender();
}

/*!
    Returns the model.
 */
QAbstractItemModel * PictureFlow::model()
{
    return d->state->model;
}

int PictureFlow::slideCount() const
{
    return d->state->model->rowCount( d->state->model->currentIndex().parent() );
}

QColor PictureFlow::backgroundColor() const
{
    return QColor(d->state->backgroundColor);
}

void PictureFlow::setBackgroundColor(const QColor& c)
{
    d->state->backgroundColor = c.rgba();
    triggerRender();
}

QSize PictureFlow::slideSize() const
{
    return QSize(d->state->slideWidth, d->state->slideHeight);
}

void PictureFlow::setSlideSize(QSize size)
{
    d->state->slideWidth = size.width();
    d->state->slideHeight = size.height();
    d->state->reposition();
    triggerRender();
}

PictureFlow::ReflectionEffect PictureFlow::reflectionEffect() const
{
    return d->state->reflectionEffect;
}

void PictureFlow::setReflectionEffect(ReflectionEffect effect)
{
    d->state->reflectionEffect = effect;
    triggerRender();
}

int PictureFlow::centerIndex() const
{
    return d->state->centerIndex;
}

void PictureFlow::setCenterIndex(int index)
{
    index = qMin(index, slideCount() - 1);
    index = qMax(index, 0);
    d->state->centerIndex = index;
    d->state->reset();
    d->animator->stop(index);
    triggerRender();
}

void PictureFlow::clear()
{
    d->state->reset();
    triggerRender();
}

void PictureFlow::render()
{
    d->renderer->dirty = true;
    update();
}

void PictureFlow::triggerRender()
{
    d->triggerTimer.setSingleShot(true);
    d->triggerTimer.start(0);
}

void PictureFlow::showPrevious()
{
    int step = d->animator->step;
    int center = d->state->centerIndex;

    if (step > 0)
        d->animator->start(center);

    if (step == 0)
        if (center > 0)
            d->animator->start(center - 1);

    if (step < 0)
        d->animator->target = qMax(0, center - 2);
}

void PictureFlow::showNext()
{
    int step = d->animator->step;
    int center = d->state->centerIndex;

    if (step < 0)
        d->animator->start(center);

    if (step == 0)
        if (center < slideCount() - 1)
            d->animator->start(center + 1);

    if (step > 0)
        d->animator->target = qMin(center + 2, slideCount() - 1);
}

void PictureFlow::showSlide(int index)
{
    index = qMax(index, 0);
    index = qMin(slideCount() - 1, index);
    if (index < 0 || index == d->state->centerSlide.slideIndex)
        return;

    d->animator->start(index);
}

void PictureFlow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left) {
        if (event->modifiers() == Qt::ControlModifier)
            showSlide(centerIndex() - 10);
        else
            showPrevious();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Right) {
        if (event->modifiers() == Qt::ControlModifier)
            showSlide(centerIndex() + 10);
        else
            showNext();
        event->accept();
        return;
    }

    event->ignore();
}

void PictureFlow::mousePressEvent(QMouseEvent* event)
{
    if (event->x() > width() / 2 + d->state->slideWidth/2 )
        showNext();
    else if (event->x() < width() / 2 - d->state->slideWidth/2 )
        showPrevious();
    else if ( d->state->model->rowCount()>0 && d->state->model->currentIndex().row() != d->state->centerIndex )
    {
        if(d->state->model->hasIndex( d->state->centerIndex, 0, d->state->model->currentIndex().parent() ))
        {
            QModelIndex i=d->state->model->index( d->state->centerIndex, 0, d->state->model->currentIndex().parent() );
            d->state->model->activateItem( i );
        }
    }
}

void PictureFlow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    d->renderer->paint();
}

void PictureFlow::resizeEvent(QResizeEvent* event)
{
    triggerRender();
    QWidget::resizeEvent(event);
}

void PictureFlow::wheelEvent(QWheelEvent * event)
{
    if (event->orientation() == Qt::Horizontal)
    {
        event->ignore();
    }
    else
    {
        int numSteps = -((event->delta() / 8) / 15);

        if (numSteps > 0)
        {
            for (int i = 0;i < numSteps;i++)
            {
                showNext();
            }
        }
        else
        {
            for (int i = numSteps;i < 0;i++)
            {
                showPrevious();
            }
        }
        event->accept();
    }
}

void PictureFlow::updateAnimation()
{
    int old_center = d->state->centerIndex;
    d->animator->update();
    triggerRender();
    if (d->state->centerIndex != old_center)
        emit centerIndexChanged(d->state->centerIndex);
}




void PictureFlowPrivate::columnsAboutToBeInserted(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

}

void PictureFlowPrivate::columnsAboutToBeRemoved(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

}

void PictureFlowPrivate::columnsInserted(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

}

void PictureFlowPrivate::columnsRemoved(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
}

void PictureFlowPrivate::dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);

    if (topLeft.parent() != rootindex)
        return;

    if (bottomRight.parent() != rootindex)
        return;


    int start = topLeft.row();
    int end = bottomRight.row();

    for (int i = start;i <= end;i++)
        replaceSlide(i, qvariant_cast<QImage>(state->model->data(state->model->index(i, piccolumn, rootindex), picrole)));
}

void PictureFlowPrivate::headerDataChanged(Qt::Orientation orientation, int first, int last)
{
    Q_UNUSED(orientation);
    Q_UNUSED(first);
    Q_UNUSED(last);
}

void PictureFlowPrivate::layoutAboutToBeChanged()
{
}

void PictureFlowPrivate::layoutChanged()
{
    reset();
    setCurrentIndex(currentcenter);
}

void PictureFlowPrivate::modelAboutToBeReset()
{
}

void PictureFlowPrivate::modelReset()
{
    reset();
}

void PictureFlowPrivate::rowsAboutToBeInserted(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
}

void PictureFlowPrivate::rowsAboutToBeRemoved(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
}

void PictureFlowPrivate::rowsInserted(const QModelIndex & parent, int start, int end)
{
    if (rootindex != parent)
        return;
    for (int i = start;i <= end;i++)
    {
        QModelIndex idx = state->model->index(i, piccolumn, rootindex);
        insertSlide(i, qvariant_cast<QImage>(state->model->data(idx, picrole)));
        modelmap.insert(i, idx);
    }
}

void PictureFlowPrivate::rowsRemoved(const QModelIndex & parent, int start, int end)
{
    if (rootindex != parent)
        return;
    for (int i = start;i <= end;i++)
    {
        removeSlide(i);
        modelmap.removeAt(i);
    }
}

void PictureFlowPrivate::setModel(QAbstractItemModel * m)
{
    if (state->model)
    {
        disconnect(state->model, SIGNAL(columnsAboutToBeInserted(const QModelIndex & , int , int)),
                   this, SLOT(columnsAboutToBeInserted(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(columnsAboutToBeRemoved(const QModelIndex & , int , int)),
                   this, SLOT(columnsAboutToBeRemoved(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(columnsInserted(const QModelIndex & , int , int)),
                   this, SLOT(columnsInserted(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(columnsRemoved(const QModelIndex & , int , int)),
                   this, SLOT(columnsRemoved(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(dataChanged(const QModelIndex & , const QModelIndex &)),
                   this, SLOT(dataChanged(const QModelIndex & , const QModelIndex &)));
        disconnect(state->model, SIGNAL(headerDataChanged(Qt::Orientation , int , int)),
                   this, SLOT(headerDataChanged(Qt::Orientation , int , int)));
        disconnect(state->model, SIGNAL(layoutAboutToBeChanged()),
                   this, SLOT(layoutAboutToBeChanged()));
        disconnect(state->model, SIGNAL(layoutChanged()),
                   this, SLOT(layoutChanged()));
        disconnect(state->model, SIGNAL(modelAboutToBeReset()),
                   this, SLOT(modelAboutToBeReset()));
        disconnect(state->model, SIGNAL(modelReset()),
                   this, SLOT(modelReset()));
        disconnect(state->model, SIGNAL(rowsAboutToBeInserted(const QModelIndex & , int , int)),
                   this, SLOT(rowsAboutToBeInserted(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(rowsAboutToBeRemoved(const QModelIndex & , int , int)),
                   this, SLOT(rowsAboutToBeRemoved(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(rowsInserted(const QModelIndex & , int , int)),
                   this, SLOT(rowsInserted(const QModelIndex & , int , int)));
        disconnect(state->model, SIGNAL(rowsRemoved(const QModelIndex & , int , int)),
                   this, SLOT(rowsRemoved(const QModelIndex & , int , int)));
    }

    state->model = (VLCModel*)m;
    if (state->model)
    {
        rootindex = state->model->parent(QModelIndex());

        connect(state->model, SIGNAL(columnsAboutToBeInserted(const QModelIndex & , int , int)),
                this, SLOT(columnsAboutToBeInserted(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(columnsAboutToBeRemoved(const QModelIndex & , int , int)),
                this, SLOT(columnsAboutToBeRemoved(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(columnsInserted(const QModelIndex & , int , int)),
                this, SLOT(columnsInserted(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(columnsRemoved(const QModelIndex & , int , int)),
                this, SLOT(columnsRemoved(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(dataChanged(const QModelIndex & , const QModelIndex &)),
                this, SLOT(dataChanged(const QModelIndex & , const QModelIndex &)));
        connect(state->model, SIGNAL(headerDataChanged(Qt::Orientation , int , int)),
                this, SLOT(headerDataChanged(Qt::Orientation , int , int)));
        connect(state->model, SIGNAL(layoutAboutToBeChanged()),
                this, SLOT(layoutAboutToBeChanged()));
        connect(state->model, SIGNAL(layoutChanged()),
                this, SLOT(layoutChanged()));
        connect(state->model, SIGNAL(modelAboutToBeReset()),
                this, SLOT(modelAboutToBeReset()));
        connect(state->model, SIGNAL(modelReset()),
                this, SLOT(modelReset()));
        connect(state->model, SIGNAL(rowsAboutToBeInserted(const QModelIndex & , int , int)),
                this, SLOT(rowsAboutToBeInserted(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(rowsAboutToBeRemoved(const QModelIndex & , int , int)),
                this, SLOT(rowsAboutToBeRemoved(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(rowsInserted(const QModelIndex & , int , int)),
                this, SLOT(rowsInserted(const QModelIndex & , int , int)));
        connect(state->model, SIGNAL(rowsRemoved(const QModelIndex & , int , int)),
                this, SLOT(rowsRemoved(const QModelIndex & , int , int)));
    }

    reset();
}


void PictureFlowPrivate::clear()
{
    state->reset();
    modelmap.clear();
    triggerRender();
}


void PictureFlowPrivate::triggerRender()
{
    triggerTimer.setSingleShot(true);
    triggerTimer.start(0);
}



void PictureFlowPrivate::insertSlide(int index, const QImage& image)
{
    Q_UNUSED(index);
    Q_UNUSED(image);
//    state->slideImages.insert(index, new QImage(image));
//    triggerRender();
}

void PictureFlowPrivate::replaceSlide(int index, const QImage& image)
{
    Q_UNUSED(index);
    Q_UNUSED(image);
//    Q_ASSERT((index >= 0) && (index < state->slideImages.count()));

//    QImage* i = image.isNull() ? 0 : new QImage(image);
//    delete state->slideImages[index];
//    state->slideImages[index] = i;
//    triggerRender();
}

void PictureFlowPrivate::removeSlide(int index)
{
    Q_UNUSED(index);
//    delete state->slideImages[index];
//    state->slideImages.remove(index);
//    triggerRender();
}


void PictureFlowPrivate::showSlide(int index)
{
    if (index == state->centerSlide.slideIndex)
        return;
    animator->start(index);
}



void PictureFlowPrivate::reset()
{
    clear();
    if (state->model)
    {
        for (int i = 0;i < state->model->rowCount(rootindex);i++)
        {
            QModelIndex idx = state->model->index(i, piccolumn, rootindex);
            insertSlide(i, qvariant_cast<QImage>(state->model->data(idx, picrole)));
            modelmap.insert(i, idx);
        }
        if(modelmap.count())
            currentcenter=modelmap.at(0);
        else
            currentcenter=QModelIndex();
    }
    triggerRender();
}



void PictureFlowPrivate::setCurrentIndex(QModelIndex index)
{
    if (state->model->parent(index) != rootindex)
        return;

    int r = modelmap.indexOf(index);
    if (r < 0)
        return;

    state->centerIndex = r;
    state->reset();
    animator->stop(r);
    triggerRender();
}

