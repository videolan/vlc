#ifndef VIDEORENDERER_HPP
#define VIDEORENDERER_HPP

#include <QObject>
#include <QMutex>
#include "qt.hpp"
#include "vlc_vout_window.h"
#include "videosurface.hpp"

class QVoutWindow : public QObject
{
    Q_OBJECT
public:
    QVoutWindow(QObject* parent = nullptr);
    virtual ~QVoutWindow();

    virtual bool setupVoutWindow(vout_window_t* window);
    virtual void enableVideo(unsigned width, unsigned height, bool fullscreen);
    virtual void disableVideo();
    virtual void windowClosed();

    virtual VideoSurfaceProvider* getVideoSurfaceProvider() = 0;

public slots:
    void onMousePressed( int vlcButton );
    void onMouseReleased( int vlcButton );
    void onMouseDoubleClick( int vlcButton );
    void onMouseMoved( float x, float y );
    void onMouseWheeled(const QPointF &pos, int delta, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Qt::Orientation orient);
    void onKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void onSurfaceSizeChanged(QSizeF size);

protected:
    QMutex m_voutlock;
    vout_window_t* m_voutWindow = nullptr;
    bool m_hasVideo = false;

    QSizeF m_surfaceSize;

};

#endif // VIDEORENDERER_HPP
