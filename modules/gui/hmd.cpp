/*****************************************************************************
 * gui/hmd.cpp: HMD controller interface for vout
 *****************************************************************************
 * Copyright © 2019 the VideoLAN team
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <functional>

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_input.h>
#include <vlc_es.h>
#include <vlc_playlist_legacy.h>
#include <vlc_meta.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_image.h>
#include <vlc_modules.h>
#include <vlc_filter.h>
#include <vlc_hmd_controller.h>
#include <vlc_tick.h>

#include "hmd.h"
#include "hmd_controls.h"

#define CONTROL_PICTURE_DIR "hmd"


static int  Open(vlc_object_t *p_this);
static void Close(vlc_object_t *p_this);

static int initTextRenderer(intf_thread_t *p_intf);
static void releaseTextRenderer(intf_thread_t *p_intf);

static void drawHMDController(intf_thread_t *p_intf, vlc_hmd_controller_t *p_ctl);
static void testControlsHoover(intf_thread_t *p_intf, int x, int y,
                               unsigned pointerSize);

static std::string getPicPath(std::string fileName);

static int InputEvent(vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t val, void *p_data);
static int PlaylistEvent(vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t val, void *p_data);
static int ViewpointEvent(vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data);

static void StateChanged(intf_thread_t *p_intf);
static void PositionChanged(intf_thread_t *p_intf, input_thread_t *p_input);
static void LengthChanged(intf_thread_t *p_intf, input_thread_t *p_input);
static int VolumeChanged(vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data);

void playPressed(intf_thread_t *p_intf);
void pausePressed(intf_thread_t *p_intf);
void previousPressed(intf_thread_t *p_intf);
void nextPressed(intf_thread_t *p_intf);
void quitHMDModePressed(intf_thread_t *p_intf);
void timelinePositionChanged(intf_thread_t *p_intf, float progress);
void volumePositionChanged(intf_thread_t *p_intf, float progress);


vlc_module_begin ()
    set_shortname("HMDcontroller")
    set_description("HMD controller")
    set_capability("interface", 0)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_callbacks(Open, Close)
    add_shortcut("hmdcontroller")
vlc_module_end ()


#define MAX_VOLUME_RATIO 1.25f


static int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    intf_sys_t *p_sys = p_intf->p_sys = (intf_sys_t *)calloc(1, sizeof(intf_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    vlc_mutex_init(&p_sys->renderMutex);

    playlist_t *p_playlist = pl_Get(p_intf);
    var_AddCallback(p_playlist, "input-current", PlaylistEvent, p_intf);
    var_AddCallback(p_playlist, "volume", VolumeChanged, p_intf);

    // Init the text renderer.
    if (unlikely(initTextRenderer(p_intf) != VLC_SUCCESS))
    {
        vlc_mutex_destroy(&p_sys->renderMutex);
        free(p_sys);
        return VLC_EGENERIC;
    }

    // Add the controls
    Control *b = new Background(p_intf, getPicPath("hmd.png"));
    if (unlikely(b == NULL))
    {
        releaseTextRenderer(p_intf);
        vlc_mutex_destroy(&p_sys->renderMutex);
        free(p_sys);
    }

    p_sys->i_ctlWidth = b->getWidth();
    p_sys->i_ctlHeight = b->getHeight();

    // Init the blend filter
    p_sys->p_blendFilter = filter_NewBlend(p_this, b->getFrameFormat());
    if (unlikely(p_sys->p_blendFilter == NULL))
    {
        delete b;
        releaseTextRenderer(p_intf);
        vlc_mutex_destroy(&p_sys->renderMutex);
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_sys->controls.push_back(b);

    #define TEST_CTL(CTL)                                            \
        if (unlikely(CTL == NULL))                                   \
        {                                                            \
            for (std::vector<Control *>::const_iterator it =         \
                 p_sys->controls.begin();                            \
                 it != p_sys->controls.end(); ++it)                  \
                delete *it;                                          \
            releaseTextRenderer(p_intf);                             \
            vlc_mutex_destroy(&p_sys->renderMutex);                  \
            free(p_sys);                                             \
            return VLC_ENOMEM;                                       \
        }

    p_sys->play = new Button(p_intf, 250, 30, getPicPath("play.png"));
    TEST_CTL(p_sys->play);
    p_sys->play->setPressedCallback(playPressed);
    p_sys->controls.push_back(p_sys->play);

    p_sys->pause = new Button(p_intf, 250, 30, getPicPath("pause.png"));
    TEST_CTL(p_sys->pause);
    p_sys->pause->setPressedCallback(pausePressed);
    p_sys->pause->setVisibility(false);
    p_sys->controls.push_back(p_sys->pause);

    p_sys->previous = new Button(p_intf, 205, 30, getPicPath("previous.png"));
    TEST_CTL(p_sys->previous);
    p_sys->previous->setPressedCallback(previousPressed);
    p_sys->controls.push_back(p_sys->previous);

    p_sys->next = new Button(p_intf, 295, 30, getPicPath("next.png"));
    TEST_CTL(p_sys->next);
    p_sys->next->setPressedCallback(nextPressed);
    p_sys->controls.push_back(p_sys->next);

    p_sys->quitHMDMode = new Button(p_intf, 10, 40, getPicPath("quit.png"));
    TEST_CTL(p_sys->quitHMDMode);
    p_sys->quitHMDMode->setPressedCallback(quitHMDModePressed);
    p_sys->controls.push_back(p_sys->quitHMDMode);

    p_sys->startText = new Text(p_intf, 5, 8, std::string());
    TEST_CTL(p_sys->startText);
    p_sys->controls.push_back(p_sys->startText);

    p_sys->endText = new Text(p_intf, 495, 8, std::string());
    TEST_CTL(p_sys->endText);
    p_sys->controls.push_back(p_sys->endText);

    p_sys->mediaNameText = new Text(p_intf, 5, 113, std::string());
    TEST_CTL(p_sys->mediaNameText);
    p_sys->controls.push_back(p_sys->mediaNameText);

    p_sys->timeSlider = new Slider(p_intf, 50, 10, 440, getPicPath("slider_disc.png"));
    TEST_CTL(p_sys->timeSlider);
    p_sys->timeSlider->setProgressSetCallback(timelinePositionChanged);
    p_sys->controls.push_back(p_sys->timeSlider);

    p_sys->volumeSlider = new Slider(p_intf, 415, 115, 100, getPicPath("slider_disc.png"));
    TEST_CTL(p_sys->volumeSlider);
    p_sys->volumeSlider->setProgressSetCallback(volumePositionChanged);
    p_sys->controls.push_back(p_sys->volumeSlider);

    Control *p_img = new Image(p_intf, 390, 110, getPicPath("speaker.png"));
    TEST_CTL(p_img);
    p_sys->controls.push_back(p_img);

    p_sys->pointer = new Pointer(p_intf, getPicPath("pointer.png"));
    TEST_CTL(p_sys->pointer);
    p_sys->controls.push_back(p_sys->pointer);

    return VLC_SUCCESS;
}


static void Close(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);
    for (std::vector<Control *>::const_iterator it = p_sys->controls.begin();                            \
         it != p_sys->controls.end(); ++it)
        delete *it;

    releaseTextRenderer(p_intf);
    filter_DeleteBlend(p_sys->p_blendFilter);

    if (p_sys->p_vout)
    {
        var_DelCallback(p_sys->p_vout, "viewpoint", ViewpointEvent, p_intf);
        vlc_object_release(p_sys->p_vout);
    }

    if (p_sys->p_input)
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );

    playlist_t *p_playlist = pl_Get(p_intf);
    var_DelCallback(p_playlist, "volume", VolumeChanged, p_intf);
    var_DelCallback(p_playlist, "input-current", PlaylistEvent, p_intf);

    vlc_mutex_unlock(&p_sys->renderMutex);

    vlc_mutex_destroy(&p_sys->renderMutex);

    free(p_sys);
}


/*****************************************************************************
 * UI framework functions
 *****************************************************************************/


static int initTextRenderer(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    filter_t *text = p_sys->p_textFilter = (filter_t *)vlc_object_create(p_intf, sizeof(*text));
    if (!text)
        return VLC_EGENERIC;

    es_format_Init(&text->fmt_in, VIDEO_ES, 0);
    es_format_Init(&text->fmt_out, VIDEO_ES, 0);
    text->fmt_out.video.i_width          =
    text->fmt_out.video.i_visible_width  = 500;
    text->fmt_out.video.i_height         =
    text->fmt_out.video.i_visible_height = 200;

    text->p_module = module_need(text, "text renderer", "$text-renderer", false);
    if (unlikely(text->p_module == NULL))
    {
        vlc_object_release(text);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


static void releaseTextRenderer(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    module_unneed(p_sys->p_textFilter, p_sys->p_textFilter->p_module);
    vlc_object_release(p_sys->p_textFilter);
}


static void drawHMDController(intf_thread_t *p_intf, vlc_hmd_controller_t *p_ctl)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);

    p_ctl->p_pic = picture_New(VLC_CODEC_RGBA, p_sys->i_ctlWidth, p_sys->i_ctlHeight, 1, 1);
    memset(p_ctl->p_pic->p[0].p_pixels, 0,
           p_ctl->p_pic->p[0].i_pitch * p_ctl->p_pic->p[0].i_lines);

    for (std::vector<Control *>::const_iterator it = p_sys->controls.begin();
         it != p_sys->controls.end(); ++it)
        (*it)->draw(p_ctl->p_pic);

    for (std::vector<Control *>::const_iterator it = p_sys->controls.begin();
         it != p_sys->controls.end(); ++it)
        (*it)->drawHooverBox(p_ctl->p_pic);

    vlc_mutex_unlock(&p_sys->renderMutex);
}


static void testControlsHoover(intf_thread_t *p_intf, int x, int y,
                               unsigned pointerSize)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);
    p_sys->pointer->resetValidationProgress();
    for (std::vector<Control *>::const_iterator it = p_sys->controls.begin();
         it != p_sys->controls.end(); ++it)
    {
        if ((*it)->testAndSetHoover(x, y, pointerSize))
            p_sys->pointer->updateValidationProgress(*it);
    }
    vlc_mutex_unlock(&p_sys->renderMutex);
}


static void testControlsPressed(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);
    for (std::vector<Control *>::const_iterator it = p_sys->controls.begin();
         it != p_sys->controls.end(); ++it)
        (*it)->testPressed();
    vlc_mutex_unlock(&p_sys->renderMutex);
}


static std::string getPicPath(std::string fileName)
{
    std::string HRTFPath;

    char *dataDir = config_GetSysPath(VLC_PKG_DATA_DIR, CONTROL_PICTURE_DIR);
    if (dataDir != NULL)
    {
        std::stringstream ss;
        ss << std::string(dataDir) << DIR_SEP << fileName;
        HRTFPath = ss.str();
        free(dataDir);
    }

    return HRTFPath;
}


/*****************************************************************************
 * VLC variable callbacks.
 *****************************************************************************/


static int PlaylistEvent(vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t val, void *p_data)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = (input_thread_t *)val.p_address;

    (void) p_this; (void) psz_var;

    if (p_sys->p_input != NULL)
    {
        assert( p_sys->p_input == oldval.p_address );
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
    }

    p_sys->p_input = p_input;

    if (p_input != NULL)
        var_AddCallback(p_input, "intf-event", InputEvent, p_intf);

    return VLC_SUCCESS;
}


static int InputEvent(vlc_object_t *p_this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t val, void *p_data)
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var; (void) oldval;

    switch (val.i_int)
    {
    case INPUT_EVENT_VOUT:
        /* intf-event is serialized against itself and is the sole user of
         * p_sys->p_vout. So there is no need to acquire the lock currently. */
        if (p_sys->p_vout != NULL)
        {
            /* /!\ Beware of lock inversion with var_DelCallback() /!\ */
            var_DelCallback(p_sys->p_vout, "viewpoint", ViewpointEvent,
                            p_intf);
            vlc_object_release(p_sys->p_vout);
        }

        p_sys->p_vout = input_GetVout(p_input);
        if (p_sys->p_vout != NULL)
        {
            var_AddCallback(p_sys->p_vout, "viewpoint", ViewpointEvent,
                            p_intf);
        }

        break;
    case INPUT_EVENT_STATE:
        StateChanged(p_intf);
        break;
    case INPUT_EVENT_POSITION:
        PositionChanged(p_intf, p_input);
        break;
    case INPUT_EVENT_LENGTH:
        LengthChanged(p_intf, p_input);
        break;
    }

    return VLC_SUCCESS;
}


static int ViewpointEvent(vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) p_this; (void) psz_var; (void) oldval;

    vlc_viewpoint_t *p_vp = (vlc_viewpoint_t *)newval.p_address;

    vlc_hmd_controller_t *p_ctl = vlc_hmd_controller_New();

    p_ctl->pos.f_depth = -0.7f;

    float u = fabs(p_ctl->pos.f_depth) * tanf(p_vp->yaw * M_PI / 180.f);
    float v = fabs(p_ctl->pos.f_depth) * tanf(-p_vp->pitch * M_PI / 180.f);

    // Show / hide the controller:
    p_ctl->pos.f_left = -0.5f;
    p_ctl->pos.f_right = 0.5f;
    float f_hmdContWorldHeight = (p_ctl->pos.f_right - p_ctl->pos.f_left)
            * p_sys->i_ctlHeight / p_sys->i_ctlWidth;
    p_ctl->pos.f_bottom = -0.6f;
    p_ctl->pos.f_top = p_ctl->pos.f_bottom + f_hmdContWorldHeight;

    // Determine the visibility of the controller.
    #define CONTROLLER_VISIBILITY_TIMEOUT 5
    if (p_sys->b_ctlVisible)
    {
        vlc_tick_t  curDate = vlc_tick_now();
        if (u >= p_ctl->pos.f_left && u <= p_ctl->pos.f_right
            && v >= p_ctl->pos.f_bottom && v <= p_ctl->pos.f_top)
            p_sys->ctlSetVisibleDate = curDate;

        if (vlc_tick_now() > p_sys->ctlSetVisibleDate
            + CONTROLLER_VISIBILITY_TIMEOUT * CLOCK_FREQ)
            p_sys->b_ctlVisible = false;
    }
    else if (fabs(p_vp->pitch) > 45.f)
    {
        vlc_tick_t curDate = vlc_tick_now();
        p_sys->b_ctlVisible = true;
        p_sys->ctlSetVisibleDate = curDate;
    }
    p_ctl->b_visible = p_sys->b_ctlVisible;

    int x = (u - p_ctl->pos.f_left) * p_sys->i_ctlWidth / (p_ctl->pos.f_right - p_ctl->pos.f_left);
    int y = p_sys->i_ctlHeight - (v - p_ctl->pos.f_bottom) * p_sys->i_ctlHeight / f_hmdContWorldHeight;

    vlc_mutex_lock(&p_sys->renderMutex);
    p_sys->pointer->setPosition(x, y);
    vlc_mutex_unlock(&p_sys->renderMutex);

    testControlsHoover(p_intf, x, y, p_sys->pointer->getHeight());
    testControlsPressed(p_intf);

    drawHMDController(p_intf, p_ctl);
    input_UpdateHMDController(p_sys->p_input, p_ctl);

    vlc_hmd_controller_Release(p_ctl);

    return VLC_SUCCESS;
}


std::string getTimeString(int64_t t)
{
    std::stringstream ss;
    int64_t min = t / 60;
    int64_t sec = t % 60;
    ss << std::setfill('0') << std::setw(2) << min
       << ":"
       << std::setfill('0') << std::setw(2) << sec;
    return ss.str();
}


static void StateChanged(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);
    switch (var_GetInteger(p_sys->p_input, "state"))
    {
        case OPENING_S:
        case PLAYING_S:
            p_sys->play->setVisibility(false);
            p_sys->pause->setVisibility(true);
            p_sys->mediaNameText->setText(
                input_GetItem(p_sys->p_input)->psz_name);
            break;
        case PAUSE_S:
            p_sys->play->setVisibility(true);
            p_sys->pause->setVisibility(false);
            break;
        default:
            p_sys->play->setVisibility(false);
            p_sys->pause->setVisibility(true);
            break;
    }
    vlc_mutex_unlock(&p_sys->renderMutex);
}


static void PositionChanged(intf_thread_t *p_intf,
                            input_thread_t *p_input)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    p_sys->i_inputTime = var_GetInteger(p_input, "time") / CLOCK_FREQ;

    vlc_mutex_lock(&p_sys->renderMutex);
    if (p_sys->i_inputLength > 0 && p_sys->i_inputTime > 0)
        p_sys->timeSlider->setProgress((float)p_sys->i_inputTime / p_sys->i_inputLength);
    p_sys->startText->setText(getTimeString(p_sys->i_inputTime));
    vlc_mutex_unlock(&p_sys->renderMutex);
}


static void LengthChanged(intf_thread_t *p_intf,
                          input_thread_t *p_input)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    p_sys->i_inputLength = var_GetInteger(p_input, "length") / CLOCK_FREQ;

    vlc_mutex_lock(&p_sys->renderMutex);
    p_sys->endText->setText(getTimeString(p_sys->i_inputLength));
    vlc_mutex_unlock(&p_sys->renderMutex);
}


static int VolumeChanged(vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    VLC_UNUSED(newval);

    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->renderMutex);
    p_sys->volumeSlider->setProgress(newval.f_float / MAX_VOLUME_RATIO);
    vlc_mutex_unlock(&p_sys->renderMutex);

    return VLC_SUCCESS;
}


/*****************************************************************************
 * UI control callbacks.
 *****************************************************************************/

void playPressed(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    var_SetInteger(p_sys->p_input, "state", PLAYING_S);
}


void pausePressed(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    var_SetInteger(p_sys->p_input, "state", PAUSE_S);
}


void previousPressed(intf_thread_t *p_intf)
{
    playlist_Prev(pl_Get(p_intf));
}


void nextPressed(intf_thread_t *p_intf)
{
    playlist_Next(pl_Get(p_intf));
}


void quitHMDModePressed(intf_thread_t *p_intf)
{
    msg_Err(p_intf, "quitHMDModePressed");
    intf_sys_t *p_sys = p_intf->p_sys;
    var_SetBool(pl_Get(p_intf), "hmd", false);
    var_SetBool(p_sys->p_vout, "hmd", false);
}


void timelinePositionChanged(intf_thread_t *p_intf, float progress)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    // Release the mutex as the Positionchanged method is a callback
    vlc_mutex_unlock(&p_sys->renderMutex);
    var_SetInteger(p_sys->p_input, "time",
                   progress * p_sys->i_inputLength * CLOCK_FREQ);
    vlc_mutex_lock(&p_sys->renderMutex);
}


void volumePositionChanged(intf_thread_t *p_intf, float progress)
{
    playlist_VolumeSet(pl_Get(p_intf), progress * MAX_VOLUME_RATIO);
}
