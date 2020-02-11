#include "qtvlcwidget.h"
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QCoreApplication>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QThread>
#include <cmath>

#include <mutex>

#include <vlc/vlc.h>

class VLCVideo
{
public:
    VLCVideo(QtVLCWidget *widget)
        :mWidget(widget)
    {
        mBuffers[0] = NULL;
        mBuffers[1] = NULL;
        mBuffers[2] = NULL;
    }

    ~VLCVideo()
    {
        cleanup(this);
    }

    /// return the texture to be displayed
    QOpenGLFramebufferObject *getVideoFrame()
    {
        std::lock_guard<std::mutex> lock(m_text_lock);
        if (m_updated) {
            std::swap(m_idx_swap, m_idx_display);
            m_updated = false;
        }
        return mBuffers[m_idx_display];
    }

    /// this callback will create the surfaces and FBO used by VLC to perform its rendering
    static bool resizeRenderTextures(void* data, const libvlc_video_render_cfg_t *cfg,
                                     libvlc_video_output_cfg_t *render_cfg)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        if (cfg->width != that->m_width || cfg->height != that->m_height)
            cleanup(data);

        that->mBuffers[0] = new QOpenGLFramebufferObject(cfg->width, cfg->height);
        that->mBuffers[1] = new QOpenGLFramebufferObject(cfg->width, cfg->height);
        that->mBuffers[2] = new QOpenGLFramebufferObject(cfg->width, cfg->height);

        that->m_width = cfg->width;
        that->m_height = cfg->height;

        that->mBuffers[that->m_idx_render]->bind();

        render_cfg->opengl_format = GL_RGBA;
        render_cfg->full_range = true;
        render_cfg->colorspace = libvlc_video_colorspace_BT709;
        render_cfg->primaries  = libvlc_video_primaries_BT709;
        render_cfg->transfer   = libvlc_video_transfer_func_SRGB;

        return true;
    }

    // This callback is called during initialisation.
    static bool setup(void** data, const libvlc_video_setup_device_cfg_t *cfg,
                      libvlc_video_setup_device_info_t *out)
    {
        if (!QOpenGLContext::supportsThreadedOpenGL())
            return false;

        VLCVideo* that = static_cast<VLCVideo*>(*data);
        that->m_width = 0;
        that->m_height = 0;
        return true;
    }


    // This callback is called to release the texture and FBO created in resize
    static void cleanup(void* data)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        if (that->m_width == 0 && that->m_height == 0)
            return;
        delete that->mBuffers[0];
        that->mBuffers[0] = NULL;
        delete that->mBuffers[1];
        that->mBuffers[1] = NULL;
        delete that->mBuffers[2];
        that->mBuffers[2] = NULL;
    }

    //This callback is called after VLC performs drawing calls
    static void swap(void* data)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        std::lock_guard<std::mutex> lock(that->m_text_lock);
        that->m_updated = true;
        that->mWidget->update();
        std::swap(that->m_idx_swap, that->m_idx_render);
        that->mBuffers[that->m_idx_render]->bind();
    }

    // This callback is called to set the OpenGL context
    static bool make_current(void* data, bool current)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        if (current)
            that->mWidget->makeCurrent();
        else
            that->mWidget->doneCurrent();
        return true;
    }

    // This callback is called by VLC to get OpenGL functions.
    static void* get_proc_address(void* data, const char* current)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        QOpenGLContext *ctx = that->mWidget->context();
        return reinterpret_cast<void*>(ctx->getProcAddress(current));
    }

private:
    QtVLCWidget *mWidget;

    //FBO data
    unsigned m_width = 0;
    unsigned m_height = 0;
    std::mutex m_text_lock;
    QOpenGLFramebufferObject *mBuffers[3];
    GLuint m_tex[3];
    GLuint m_fbo[3];
    size_t m_idx_render = 0;
    size_t m_idx_swap = 1;
    size_t m_idx_display = 2;
    bool m_updated = false;
};


QtVLCWidget::QtVLCWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_program(nullptr),
      vertexBuffer(QOpenGLBuffer::VertexBuffer),
      vertexIndexBuffer(QOpenGLBuffer::IndexBuffer)
{
    // --transparent causes the clear color to be transparent. Therefore, on systems that
    // support it, the widget will become transparent apart from the logo.

    const char *args[] = {
        "--verbose=4"
    };
    m_vlc = libvlc_new(sizeof(args) / sizeof(*args), args);

    mVLC = new VLCVideo(this);
}

bool QtVLCWidget::playMedia(const char* url)
{
    m_media = libvlc_media_new_location (m_vlc, url);
    if (m_media == nullptr) {
        fprintf(stderr, "unable to create media %s", url);
        return false;
    }
    m_mp = libvlc_media_player_new_from_media (m_media);
    if (m_mp == nullptr) {
        fprintf(stderr, "unable to create media player");
        libvlc_media_release(m_media);
        return false;
    }

    // Define the opengl rendering callbacks
    libvlc_video_set_output_callbacks(m_mp, libvlc_video_engine_opengl,
        VLCVideo::setup, VLCVideo::cleanup, nullptr, VLCVideo::resizeRenderTextures, VLCVideo::swap,
        VLCVideo::make_current, VLCVideo::get_proc_address, nullptr, nullptr,
        mVLC);

    // Play the video
    libvlc_media_player_play (m_mp);

    return true;
}

QtVLCWidget::~QtVLCWidget()
{
    cleanup();
}

QSize QtVLCWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize QtVLCWidget::sizeHint() const
{
    return QSize(400, 400);
}

void QtVLCWidget::cleanup()
{
    stop();
    if (m_vlc)
        libvlc_release(m_vlc);
    if (m_program == nullptr)
        return;
    makeCurrent();
    vertexBuffer.destroy();
    vertexIndexBuffer.destroy();
    delete m_program;
    m_program = 0;
    doneCurrent();
}

void QtVLCWidget::stop()
{
    if (m_mp) {
        libvlc_media_player_release(m_mp);
        m_mp = nullptr;
    }
    if (m_media) {
        libvlc_media_release(m_media);
        m_media = nullptr;
    }
}

static const char *vertexShaderSource =
    "attribute vec2 position;\n"
    "varying vec2 texcoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    texcoord = position * vec2(0.5) + vec2(0.5);\n"
    "}\n";

static const char *fragmentShaderSource =
    "uniform sampler2D texture;\n"
    "\n"
    "varying vec2 texcoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = texture2D(texture, texcoord);\n"
    "};\n";

/*
 * Data used to seed our vertex array and element array buffers:
 */
static const GLfloat g_vertex_buffer_data[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
};
static const GLushort g_element_buffer_data[] = { 0, 1, 2, 3 };

void QtVLCWidget::initializeGL()
{
    // In this example the widget's corresponding top-level window can change
    // several times during the widget's lifetime. Whenever this happens, the
    // QOpenGLWidget's associated context is destroyed and a new one is created.
    // Therefore we have to be prepared to clean up the resources on the
    // aboutToBeDestroyed() signal, instead of the destructor. The emission of
    // the signal will be followed by an invocation of initializeGL() where we
    // can recreate all resources.
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &QtVLCWidget::cleanup);

    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(g_vertex_buffer_data, sizeof(g_vertex_buffer_data));

    vertexIndexBuffer.create();
    vertexIndexBuffer.bind();
    vertexIndexBuffer.allocate(g_element_buffer_data, sizeof(g_element_buffer_data));

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->link();

    m_program->setUniformValue("texture", 0);

    m_program->bindAttributeLocation("position", 0);
}

void QtVLCWidget::paintGL()
{
    QOpenGLFramebufferObject *fbo = mVLC->getVideoFrame();
    if (fbo != NULL)
    {
        m_program->bind();

        glClearColor(1.0, 0.5, 0.0, 1.0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo->takeTexture());

        vertexBuffer.bind();
        m_program->setAttributeArray("position", (const QVector2D *)nullptr, sizeof(GLfloat)*2);
        //vertexBuffer.release();

        m_program->enableAttributeArray("position");

        vertexIndexBuffer.bind();
        glDrawElements(
            GL_TRIANGLE_STRIP,  /* mode */
            4,                  /* count */
            GL_UNSIGNED_SHORT,  /* type */
            (void*)0            /* element array buffer offset */
        );
        //vertexIndexBuffer.release();

        m_program->disableAttributeArray("position");

        //m_program->release();
    }
}

void QtVLCWidget::resizeGL(int w, int h)
{
    /* TODO */
}
