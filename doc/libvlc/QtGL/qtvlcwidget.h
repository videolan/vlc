#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

class QtVLCWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    QtVLCWidget(QWidget *parent = 0);
    ~QtVLCWidget();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    bool playMedia(const char* url);

public slots:
    void cleanup();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    QOpenGLVertexArrayObject m_vao;
    QOpenGLShaderProgram *m_program;

    class VLCVideo  *mVLC;

    void stop();
    struct libvlc_instance_t*  m_vlc = nullptr;
    struct libvlc_media_player_t* m_mp = nullptr;
    struct libvlc_media_t* m_media = nullptr;

    QOpenGLBuffer vertexBuffer, vertexIndexBuffer;
};

#endif /* GLWIDGET_H */
