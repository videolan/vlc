/*****************************************************************************
 * glspectrum.c: spectrum visualization module based on OpenGL
 *****************************************************************************
 * Copyright © 2009-2013 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_vout_wrapper.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>
#include <vlc_rand.h>

#include <GL/gl.h>

#include <math.h>

#include "visual/fft.h"
#include "visual/window.h"


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_("The width of the visualization window, in pixels.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_("The height of the visualization window, in pixels.")

vlc_module_begin()
    set_shortname(N_("glSpectrum"))
    set_description(N_("3D OpenGL spectrum visualization"))
    set_capability("visualization", 0)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_VISUAL)

    add_integer("glspectrum-width", 400, WIDTH_TEXT, WIDTH_LONGTEXT, false)
    add_integer("glspectrum-height", 300, HEIGHT_TEXT, HEIGHT_LONGTEXT, false)

    add_shortcut("glspectrum")
    set_callbacks(Open, Close)
vlc_module_end()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct filter_sys_t
{
    vlc_thread_t thread;
    vlc_sem_t    ready;
    bool         b_error;

    /* Audio data */
    unsigned i_channels;
    block_fifo_t    *fifo;
    unsigned i_prev_nb_samples;
    int16_t *p_prev_s16_buff;

    /* Opengl */
    vout_thread_t  *p_vout;
    vout_display_t *p_vd;

    float f_rotationAngle;
    float f_rotationIncrement;

    /* Window size */
    int i_width;
    int i_height;

    /* FFT window parameters */
    window_param wind_param;
};


static block_t *DoWork(filter_t *, block_t *);
static void *Thread(void *);

#define SPECTRUM_WIDTH 4.0
#define NB_BANDS 20
#define ROTATION_INCREMENT 0.1
#define BAR_DECREMENT 0.075
#define ROTATION_MAX 20

const GLfloat lightZeroColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
const GLfloat lightZeroPosition[] = {0.0f, 3.0f, 10.0f, 0.0f};

/**
 * Open the module.
 * @param p_this: the filter object
 * @return VLC_SUCCESS or vlc error codes
 */
static int Open(vlc_object_t * p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    p_sys = p_filter->p_sys = (filter_sys_t*)malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    /* Create the object for the thread */
    vlc_sem_init(&p_sys->ready, 0);
    p_sys->b_error = false;
    p_sys->i_width = var_InheritInteger(p_filter, "glspectrum-width");
    p_sys->i_height = var_InheritInteger(p_filter, "glspectrum-height");
    p_sys->i_channels = aout_FormatNbChannels(&p_filter->fmt_in.audio);
    p_sys->i_prev_nb_samples = 0;
    p_sys->p_prev_s16_buff = NULL;

    p_sys->f_rotationAngle = 0;
    p_sys->f_rotationIncrement = ROTATION_INCREMENT;

    /* Fetch the FFT window parameters */
    window_get_param( VLC_OBJECT( p_filter ), &p_sys->wind_param );

    /* Create the FIFO for the audio data. */
    p_sys->fifo = block_FifoNew();
    if (p_sys->fifo == NULL)
        goto error;

    /* Create the thread */
    if (vlc_clone(&p_sys->thread, Thread, p_filter,
                  VLC_THREAD_PRIORITY_VIDEO))
        goto error;

    /* Wait for the displaying thread to be ready. */
    vlc_sem_wait(&p_sys->ready);
    if (p_sys->b_error)
    {
        vlc_join(p_sys->thread, NULL);
        goto error;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;

error:
    vlc_sem_destroy(&p_sys->ready);
    free(p_sys);
    return VLC_EGENERIC;
}


/**
 * Close the module.
 * @param p_this: the filter object
 */
static void Close(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Terminate the thread. */
    vlc_cancel(p_sys->thread);
    vlc_join(p_sys->thread, NULL);

    /* Free the ressources */
    vout_DeleteDisplay(p_sys->p_vd, NULL);
    vlc_object_release(p_sys->p_vout);

    block_FifoRelease(p_sys->fifo);
    free(p_sys->p_prev_s16_buff);

    vlc_sem_destroy(&p_sys->ready);
    free(p_sys);
}


/**
 * Do the actual work with the new sample.
 * @param p_filter: filter object
 * @param p_in_buf: input buffer
 */
static block_t *DoWork(filter_t *p_filter, block_t *p_in_buf)
{
    block_t *block = block_Duplicate(p_in_buf);
    if (likely(block != NULL))
        block_FifoPut(p_filter->p_sys->fifo, block);
    return p_in_buf;
}


/**
  * Init the OpenGL scene.
  **/
static void initOpenGLScene(void)
{
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glMatrixMode(GL_PROJECTION);
    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, 0.5f, 10.0f);

    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.0, -2.0, -2.0);

    // Init the light.
    glEnable(GL_LIGHTING);

    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
    glLightfv(GL_LIGHT0, GL_POSITION, lightZeroPosition);

    glShadeModel(GL_SMOOTH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


/**
 * Draw one bar of the Spectrum.
 */
static void drawBar(void)
{
    const float w = SPECTRUM_WIDTH / NB_BANDS - 0.05;

    const GLfloat vertexCoords[] = {
        0.0, 0.0, 0.0,   w, 0.0, 0.0,   0.0, 1.0, 0.0,
        0.0, 1.0, 0.0,   w, 0.0, 0.0,   w  , 1.0, 0.0,

        0.0, 0.0, -w,    0.0, 0.0, 0.0,   0.0, 1.0, -w,
        0.0, 1.0, -w,    0.0, 0.0, 0.0,   0.0, 1.0, 0.0,

        w, 0.0, 0.0,     w, 0.0, -w,   w, 1.0, 0.0,
        w, 1.0, 0.0,     w, 0.0, -w,   w, 1.0, -w,

        w, 0.0, -w,      0.0, 0.0, -w,  0.0, 1.0, -w,
        0.0, 1.0, -w,    w, 1.0, -w,    w, 0.0, -w,

        0.0, 1.0, 0.0,   w, 1.0, 0.0,   w, 1.0, -w,
        0.0, 1.0, 0.0,   w, 1.0, -w,    0.0, 1.0, -w,
    };

    const GLfloat normals[] = {
        0.0, 0.0, 1.0,   0.0, 0.0, 1.0,   0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,   0.0, 0.0, 1.0,   0.0, 0.0, 1.0,

        -1.0, 0.0, 0.0,   -1.0, 0.0, 0.0,   -1.0, 0.0, 0.0,
        -1.0, 0.0, 0.0,   -1.0, 0.0, 0.0,   -1.0, 0.0, 0.0,

        1.0, 0.0, 0.0,   1.0, 0.0, 0.0,   1.0, 0.0, 0.0,
        1.0, 0.0, 0.0,   1.0, 0.0, 0.0,   1.0, 0.0, 0.0,

        0.0, 0.0, -1.0,   0.0, 0.0, -1.0,   0.0, 0.0, -1.0,
        0.0, 0.0, -1.0,   0.0, 0.0, -1.0,   0.0, 0.0, -1.0,

        0.0, 1.0, 0.0,   0.0, 1.0, 0.0,   0.0, 1.0, 0.0,
        0.0, 1.0, 0.0,   0.0, 1.0, 0.0,   0.0, 1.0, 0.0,
    };

    glVertexPointer(3, GL_FLOAT, 0, vertexCoords);
    glNormalPointer(GL_FLOAT, 0, normals);
    glDrawArrays(GL_TRIANGLES, 0, 6 * 5);
}


/**
 * Set the color of one bar of the spectrum.
 * @param f_height the height of the bar.
 */
static void setBarColor(float f_height)
{
    float r, b;

#define BAR_MAX_HEIGHT 4.2
    r = -1.0 + 2 / BAR_MAX_HEIGHT * f_height;
    b = 2.0 - 2 / BAR_MAX_HEIGHT * f_height;
#undef BAR_MAX_HEIGHT

    /* Test the ranges. */
    r = r > 1.0 ? 1.0 : r;
    b = b > 1.0 ? 1.0 : b;

    r = r < 0.0 ? 0.0 : r;
    b = b < 0.0 ? 0.0 : b;

    /* Set the bar color. */
    glColor4f(r, 0.0, b, 1.0);
}


/**
 * Draw all the bars of the spectrum.
 * @param heights the heights of all the bars.
 */
static void drawBars(float heights[])
{
    glPushMatrix();
    glTranslatef(-2.0, 0.0, 0.0);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    float w = SPECTRUM_WIDTH / NB_BANDS;
    for (unsigned i = 0; i < NB_BANDS; ++i)
    {
        glPushMatrix();
        glScalef(1.0, heights[i], 1.0);
        setBarColor(heights[i]);
        drawBar();
        glPopMatrix();

        glTranslatef(w, 0.0, 0.0);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    glPopMatrix();
}


/**
 * Update thread which do the rendering
 * @param p_this: the p_thread object
 */
static void *Thread( void *p_data )
{
    filter_t  *p_filter = (filter_t*)p_data;
    filter_sys_t *p_sys = p_filter->p_sys;

    video_format_t fmt;
    vlc_gl_t *gl;
    unsigned int i_last_width = 0;
    unsigned int i_last_height = 0;

    /* Create the openGL provider */
    p_sys->p_vout =
        (vout_thread_t *)vlc_object_create(p_filter, sizeof(vout_thread_t));
    if (!p_sys->p_vout)
        goto error;

    /* Configure the video format for the opengl provider. */
    video_format_Init(&fmt, 0);
    video_format_Setup(&fmt, VLC_CODEC_RGB32, p_sys->i_width, p_sys->i_height,
                       p_sys->i_width, p_sys->i_height, 0, 1 );
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    /* Init vout state. */
    vout_display_state_t state;
    memset(&state, 0, sizeof(state));
    state.cfg.display.sar.num = 1;
    state.cfg.display.sar.den = 1;
    state.cfg.is_display_filled = true;
    state.cfg.zoom.num = 1;
    state.cfg.zoom.den = 1;
    state.sar.num = 1;
    state.sar.den = 1;

    p_sys->p_vd = vout_NewDisplay(p_sys->p_vout, &fmt, &state,
                                  "opengl", 1000000, 1000000);
    if (!p_sys->p_vd)
    {
        vlc_object_release(p_sys->p_vout);
        goto error;
    }

    gl = vout_GetDisplayOpengl(p_sys->p_vd);
    if (!gl)
    {
        vout_DeleteDisplay(p_sys->p_vd, NULL);
        vlc_object_release(p_sys->p_vout);
        goto error;
    }

    vlc_sem_post(&p_sys->ready);

    vlc_gl_MakeCurrent(gl);
    initOpenGLScene();
    vlc_gl_ReleaseCurrent(gl);

    float height[NB_BANDS] = {0};

    while (1)
    {
        block_t *block = block_FifoGet(p_sys->fifo);

        int canc = vlc_savecancel();

        vlc_gl_MakeCurrent(gl);
        /* Manage the events */
        vout_ManageDisplay(p_sys->p_vd, true);
        if (p_sys->p_vd->cfg->display.width != i_last_width ||
            p_sys->p_vd->cfg->display.height != i_last_height)
        {
            /* FIXME it is not perfect as we will have black bands */
            vout_display_place_t place;
            vout_display_PlacePicture(&place, &p_sys->p_vd->source,
                                      p_sys->p_vd->cfg, false);

            i_last_width  = p_sys->p_vd->cfg->display.width;
            i_last_height = p_sys->p_vd->cfg->display.height;
        }

        /* Horizontal scale for 20-band equalizer */
        const unsigned xscale[] = {0,1,2,3,4,5,6,7,8,11,15,20,27,
                                   36,47,62,82,107,141,184,255};

        fft_state *p_state = NULL; /* internal FFT data */
        DEFINE_WIND_CONTEXT(wind_ctx); /* internal window data */

        unsigned i, j;
        float p_output[FFT_BUFFER_SIZE];           /* Raw FFT Result  */
        int16_t p_buffer1[FFT_BUFFER_SIZE];        /* Buffer on which we perform
                                                      the FFT (first channel) */
        int16_t p_dest[FFT_BUFFER_SIZE];           /* Adapted FFT result */
        float *p_buffl = (float*)block->p_buffer;  /* Original buffer */

        int16_t  *p_buffs;                         /* int16_t converted buffer */
        int16_t  *p_s16_buff;                      /* int16_t converted buffer */

        if (!block->i_nb_samples) {
            msg_Err(p_filter, "no samples yet");
            goto release;
        }

        /* Allocate the buffer only if the number of samples change */
        if (block->i_nb_samples != p_sys->i_prev_nb_samples)
        {
            free(p_sys->p_prev_s16_buff);
            p_sys->p_prev_s16_buff = malloc(block->i_nb_samples *
                                            p_sys->i_channels *
                                            sizeof(int16_t));
            if (!p_sys->p_prev_s16_buff)
                goto release;
            p_sys->i_prev_nb_samples = block->i_nb_samples;
        }
        p_buffs = p_s16_buff = p_sys->p_prev_s16_buff;

        /* Convert the buffer to int16_t
           Pasted from float32tos16.c */
        for (i = block->i_nb_samples * p_sys->i_channels; i--;)
        {
            union {float f; int32_t i;} u;

            u.f = *p_buffl + 384.0;
            if (u.i > 0x43c07fff)
                *p_buffs = 32767;
            else if (u.i < 0x43bf8000)
                *p_buffs = -32768;
            else
                *p_buffs = u.i - 0x43c00000;

            p_buffl++; p_buffs++;
        }
        p_state = visual_fft_init();
        if (!p_state)
        {
            msg_Err(p_filter,"unable to initialize FFT transform");
            goto release;
        }
        if (!window_init(FFT_BUFFER_SIZE, &p_sys->wind_param, &wind_ctx))
        {
            msg_Err(p_filter,"unable to initialize FFT window");
            goto release;
        }
        p_buffs = p_s16_buff;
        for (i = 0 ; i < FFT_BUFFER_SIZE; i++)
        {
            p_output[i] = 0;
            p_buffer1[i] = *p_buffs;

            p_buffs += p_sys->i_channels;
            if (p_buffs >= &p_s16_buff[block->i_nb_samples * p_sys->i_channels])
                p_buffs = p_s16_buff;
        }
        window_scale_in_place (p_buffer1, &wind_ctx);
        fft_perform (p_buffer1, p_output, p_state);

        for (i = 0; i< FFT_BUFFER_SIZE; ++i)
            p_dest[i] = p_output[i] *  (2 ^ 16)
                        / ((FFT_BUFFER_SIZE / 2 * 32768) ^ 2);

        for (i = 0 ; i < NB_BANDS; i++)
        {
            /* Decrease the previous size of the bar. */
            height[i] -= BAR_DECREMENT;
            if (height[i] < 0)
                height[i] = 0;

            int y = 0;
            /* We search the maximum on one scale
               to determine the current size of the bar. */
            for (j = xscale[i]; j < xscale[i + 1]; j++)
            {
                if (p_dest[j] > y)
                     y = p_dest[j];
            }
            /* Calculate the height of the bar */
            float new_height = y != 0 ? log(y) * 0.4 : 0;
            height[i] = new_height > height[i]
                        ? new_height : height[i];
        }

        /* Determine the camera rotation angle. */
        p_sys->f_rotationAngle += p_sys->f_rotationIncrement;
        if (p_sys->f_rotationAngle <= -ROTATION_MAX)
            p_sys->f_rotationIncrement = ROTATION_INCREMENT;
        else if (p_sys->f_rotationAngle >= ROTATION_MAX)
            p_sys->f_rotationIncrement = -ROTATION_INCREMENT;

        /* Render the frame. */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushMatrix();
            glRotatef(p_sys->f_rotationAngle, 0, 1, 0);
            drawBars(height);
        glPopMatrix();

        /* Wait to swapp the frame on time. */
        mwait(block->i_pts + (block->i_length / 2));
        if (!vlc_gl_Lock(gl))
        {
            vlc_gl_Swap(gl);
            vlc_gl_Unlock(gl);
        }

release:
        window_close(&wind_ctx);
        fft_close(p_state);
        vlc_gl_ReleaseCurrent(gl);
        block_Release(block);
        vlc_restorecancel(canc);
    }

    assert(0);

error:
    p_sys->b_error = true;
    vlc_sem_post(&p_sys->ready);
    return NULL;
}
