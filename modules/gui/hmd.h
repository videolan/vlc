#ifndef HMD_H
#define HMD_H

#include <vector>

#include <vlc_common.h>

class Control;
class Button;
class Slider;
class Text;
class Pointer;


struct intf_sys_t
{
    input_thread_t *p_input;
    unsigned i_inputTime;
    unsigned i_inputLength;

    vout_thread_t *p_vout;

    std::vector<Control *> controls;
    Button *play;
    Button *pause;
    Button *next;
    Button *previous;
    Button *quitHMDMode;
    Slider *timeSlider;
    Slider *volumeSlider;
    Text *startText;
    Text *endText;
    Text *mediaNameText;
    Pointer *pointer;

    unsigned i_ctlWidth;
    unsigned i_ctlHeight;
    bool b_ctlVisible;
    vlc_tick_t ctlSetVisibleDate;

    filter_t *p_textFilter;
    filter_t *p_blendFilter;

    vlc_mutex_t renderMutex;
};


#endif // HMD_H
