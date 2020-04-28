//g++ sdl_opengl_player.cpp $(pkg-config --cflags --libs libvlc sdl2 gl)

/* Licence WTFPL  */
/* Written by Pierre Lamot */

#include <stdio.h>
#include <stdlib.h>

#include <exception>
#include <mutex>
#include <vector>

#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <vlc/vlc.h>

/*
 * This program show how to use libvlc_video_set_output_callbacks API.
 *
 * The main idea is to set up libvlc to render into FBO, and to use a
 * triple buffer mechanism to share textures between VLC and the rendering
 * thread of our application
 */

// Shader sources
const GLchar* vertexSource =
    "attribute vec4 a_position;    \n"
    "attribute vec2 a_uv;          \n"
    "varying vec2 v_TexCoordinate; \n"
    "void main()                   \n"
    "{                             \n"
    "    v_TexCoordinate = a_uv;   \n"
    "    gl_Position = vec4(a_position.xyz, 1.0);  \n"
    "}                             \n";

const GLchar* fragmentSource =
    "uniform sampler2D u_videotex; \n"
    "varying vec2 v_TexCoordinate; \n"
    "void main()                   \n"
    "{                             \n"
    "    gl_FragColor = texture2D(u_videotex, v_TexCoordinate); \n"
    "}                             \n";

class VLCVideo
{
public:
    VLCVideo(SDL_Window *window):
        m_win(window)
    {
        const char *args[] = {
            "--verbose=4"
        };
        m_vlc = libvlc_new(sizeof(args) / sizeof(*args), args);

        //VLC opengl context needs to be shared with SDL context
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        m_ctx = SDL_GL_CreateContext(window);
    }

    ~VLCVideo()
    {
        stop();
        if (m_vlc)
            libvlc_release(m_vlc);
    }

    bool playMedia(const char* url)
    {
        m_media = libvlc_media_new_location (m_vlc, url);
        if (m_media == NULL) {
            fprintf(stderr, "unable to create media %s", url);
            return false;
        }
        m_mp = libvlc_media_player_new_from_media (m_media);
        if (m_mp == NULL) {
            fprintf(stderr, "unable to create media player");
            libvlc_media_release(m_media);
            return false;
        }
        // Define the opengl rendering callbacks
        libvlc_video_set_output_callbacks(m_mp, libvlc_video_engine_opengl,
            setup, cleanup, nullptr, resize, swap,
            make_current, get_proc_address, nullptr, nullptr,
            this);

        // Play the video
        libvlc_media_player_play (m_mp);

        return true;
    }

    void stop()
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

    /// return the texture to be displayed
    GLuint getVideoFrame(bool* out_updated)
    {
        std::lock_guard<std::mutex> lock(m_text_lock);
        if (out_updated)
            *out_updated = m_updated;
        if (m_updated) {
            std::swap(m_idx_swap, m_idx_display);
            m_updated = false;
        }
        return m_tex[m_idx_display];
    }

    /// this callback will create the surfaces and FBO used by VLC to perform its rendering
    static bool resize(void* data, const libvlc_video_render_cfg_t *cfg,
                       libvlc_video_output_cfg_t *render_cfg)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        if (cfg->width != that->m_width || cfg->height != that->m_height)
            cleanup(data);

        glGenTextures(3, that->m_tex);
        glGenFramebuffers(3, that->m_fbo);

        for (int i = 0; i < 3; i++) {
            glBindTexture(GL_TEXTURE_2D, that->m_tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cfg->width, cfg->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindFramebuffer(GL_FRAMEBUFFER, that->m_fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, that->m_tex[i], 0);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }

        that->m_width = cfg->width;
        that->m_height = cfg->height;

        glBindFramebuffer(GL_FRAMEBUFFER, that->m_fbo[that->m_idx_render]);

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

        glDeleteTextures(3, that->m_tex);
        glDeleteFramebuffers(3, that->m_fbo);
    }

    //This callback is called after VLC performs drawing calls
    static void swap(void* data)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        std::lock_guard<std::mutex> lock(that->m_text_lock);
        that->m_updated = true;
        std::swap(that->m_idx_swap, that->m_idx_render);
        glBindFramebuffer(GL_FRAMEBUFFER, that->m_fbo[that->m_idx_render]);
    }

    // This callback is called to set the OpenGL context
    static bool make_current(void* data, bool current)
    {
        VLCVideo* that = static_cast<VLCVideo*>(data);
        if (current)
            return SDL_GL_MakeCurrent(that->m_win, that->m_ctx) == 0;
        else
            return SDL_GL_MakeCurrent(that->m_win, 0) == 0;
    }

    // This callback is called by VLC to get OpenGL functions.
    static void* get_proc_address(void* /*data*/, const char* current)
    {
        return SDL_GL_GetProcAddress(current);
    }

private:
    //VLC objects
    libvlc_instance_t*  m_vlc = nullptr;
    libvlc_media_player_t* m_mp = nullptr;
    libvlc_media_t* m_media = nullptr;

    //FBO data
    unsigned m_width = 0;
    unsigned m_height = 0;
    std::mutex m_text_lock;
    GLuint m_tex[3];
    GLuint m_fbo[3];
    size_t m_idx_render = 0;
    size_t m_idx_swap = 1;
    size_t m_idx_display = 2;
    bool m_updated = false;

    //SDL context
    SDL_Window* m_win;
    SDL_GLContext m_ctx;
};

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <uri>\n", argv[0]);
        return 1;
    }

    SDL_Window* wnd = SDL_CreateWindow("test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetSwapInterval(0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_GLContext glc = SDL_GL_CreateContext(wnd);

    VLCVideo video(wnd);

    SDL_Renderer* rdr = SDL_CreateRenderer(
        wnd, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    // Create Vertex Array Object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Create a Vertex Buffer Object and copy the vertex data to it
    GLuint vbo;
    glGenBuffers(1, &vbo);

    //vertex X, vertex Y, UV X, UV Y
    GLfloat vertices[] = {
        -0.5f,  0.5f, 0.f, 1.f,
        -0.5f, -0.5f, 0.f, 0.f,
         0.5f,  0.5f, 1.f, 1.f,
         0.5f, -0.5f, 1.f, 0.f
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Create and compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    // Create and compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    {
        GLuint shader[] = {vertexShader, fragmentShader};
        const char *shaderName[] = {"vertex", "fragment"};
        for (int i = 0; i < 2; i++) {
            int len;
            glGetShaderiv(shader[i], GL_INFO_LOG_LENGTH, &len);
            if (len <= 1)
                continue;
            std::vector<char> infoLog(len);
            int charsWritten;
            glGetShaderInfoLog(shader[i], len, &charsWritten, infoLog.data());
            fprintf(stderr, "%s shader info log: %s\n", shaderName[i], infoLog.data());

            GLint status = GL_TRUE;
            glGetShaderiv(shader[i], GL_COMPILE_STATUS, &status);
            if (status == GL_FALSE) {
                fprintf(stderr, "compile %s shader failed\n", shaderName[i]);
                SDL_DestroyWindow(wnd);
                SDL_Quit();
                return 1;
            }
        }
    }

    // Link the vertex and fragment shader into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    {
        int len;
        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &len);
        if (len > 1) {
            std::vector<char> infoLog(len);
            int charsWritten;
            glGetProgramInfoLog(shaderProgram, len, &charsWritten, infoLog.data());
            fprintf(stderr, "shader program: %s\n", infoLog.data());
        }

        GLint status = GL_TRUE;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            fprintf(stderr, "unable to use program\n");
            SDL_DestroyWindow(wnd);
            SDL_Quit();
            return 1;
        }
    }

    glUseProgram(shaderProgram);

    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(shaderProgram, "a_position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);

    // Specify the layout of the vertex data
    GLint uvAttrib = glGetAttribLocation(shaderProgram, "a_uv");
    glEnableVertexAttribArray(uvAttrib);
    glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (GLvoid*)(2*sizeof(float)));

    // Specify the texture of the video
    GLint textUniform = glGetUniformLocation(shaderProgram, "u_videotex");
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(textUniform, /*GL_TEXTURE*/0);

    //start playing the video
    if (!video.playMedia(argv[1])) {
        SDL_DestroyWindow(wnd);
        SDL_Quit();
        return 1;
    }

    bool updated = false;
    bool quit = false;
    while(!quit) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT)
                quit = true;
        }

        // Clear the screen to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Get the current video texture and bind it
        GLuint tex = video.getVideoFrame(&updated);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Draw the video rectangle
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);

        SDL_GL_SwapWindow(wnd);
    };

    video.stop();

    SDL_DestroyWindow(wnd);
    SDL_Quit();

    return 0;
}
