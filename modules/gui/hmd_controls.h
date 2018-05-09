#ifndef HMD_CONTROLS_H
#define HMD_CONTROLS_H

#include <string>

#include <vlc_image.h>
#include <vlc_url.h>
#include <vlc_interface.h>
#include <vlc_subpicture.h>
#include <vlc_spu.h>
#include <vlc_filter.h>

#include "hmd.h"


class Control
{
public:

    virtual ~Control()
    {
        if (p_pic != nullptr)
            picture_Release(p_pic);
    }

    virtual void draw(picture_t *p_dstPic) const
    {
        if (!visible || posX < 0 || posY < 0)
            return;

        drawPicture(p_dstPic, p_pic, posX, posY);
    }

    virtual void drawHooverBox(picture_t *p_dstPic) const
    {
        if (!visible || !hasHoover)
            return;

        // Draw the bar
        #define PIXEL_SIZE 4
        unsigned i_dstPitch = p_dstPic->p[0].i_pitch / PIXEL_SIZE;
        uint8_t *p_dst = p_dstPic->p[0].p_pixels
                + (posY * i_dstPitch + posX) * PIXEL_SIZE;


        #define DRAW_PIXEL \
            p_dst[0] = 255; p_dst[1] = 0; \
            p_dst[2] = 0; p_dst[3] = 255; \
            p_dst += PIXEL_SIZE; \

        for (unsigned x = 0; x < width; ++x)
        {
            DRAW_PIXEL;
        }
        p_dst += (i_dstPitch - width) * PIXEL_SIZE;

        for (unsigned y = 0; y < height - 2; ++y)
        {
            DRAW_PIXEL;
            p_dst += (width - 2) * PIXEL_SIZE;
            DRAW_PIXEL;
            p_dst += (i_dstPitch - width) * PIXEL_SIZE;
        }

        for (unsigned x = 0; x < width; ++x)
        {
            DRAW_PIXEL;
        }

        #undef DRAW_PIXEL
        #undef PIXEL_SIZE
    }

    int getX() { return posX; }
    int getY() { return posY; }
    unsigned getWidth() { return width; }
    unsigned getHeight() { return height; }
    video_format_t *getFrameFormat()
    {
        video_format_t *ret = NULL;
        if (likely(p_pic != NULL))
            ret = &p_pic->format;
        return ret;
    }

    void setPressedCallback(void (*cb)(intf_thread_t *p_intf))
    {
        pressedCallback = cb;
    }

    void setVisibility(bool v)
    {
        visible = v;
        if (!visible)
            hasHoover = false;
    }

    virtual bool testAndSetHoover(int x, int y, unsigned pointerSize)
    {
        if (!visible)
            return false;

        bool newHasHoover = false;

        if (canHaveHoover
            && x >= posX - (int)pointerSize && y >= posY - (int)pointerSize
            && x < posX + (int)width && y < posY + (int)height)
        {
            newHasHoover = true;
            if (!hasHoover)
                hooverDate = vlc_tick_now();
        }

        hasHoover = newHasHoover;
        return hasHoover;
    }

    virtual bool testPressed()
    {
        if (!visible)
            return false;

        bool ret = false;
        vlc_tick_t currentDate = vlc_tick_now();

        if (hasHoover && currentDate >= hooverDate + CLOCK_FREQ * 2)
        {
            hooverDate = currentDate;
            ret = true;
            // Call the callback if there is one.
            if (pressedCallback != nullptr)
                pressedCallback(p_intf);
        }

        return ret;
    }

    vlc_tick_t getHooverDate() const
    {
        return hooverDate;
    }

protected:

    Control(intf_thread_t *p_intf, int posX, int posY,
            unsigned width, unsigned height, bool canHaveHoover)
        : p_intf(p_intf), posX(posX), posY(posY),
          width(width), height(height), visible(true),
          canHaveHoover(canHaveHoover), hasHoover(false),
          pressedCallback(nullptr), p_pic(nullptr)
    { }

    Control(intf_thread_t *p_intf, int posX, int posY)
        : Control(p_intf, posX, posY, 0, 0, false)
    { }

    void drawPicture(picture_t *p_dstPic, picture_t *p_srcPic,
                     int picPosX, int picPosY) const
    {
        if (!filter_ConfigureBlend(p_intf->p_sys->p_blendFilter, p_dstPic->format.i_visible_width,
                                  p_dstPic->format.i_visible_height, &p_srcPic->format))
            filter_Blend(p_intf->p_sys->p_blendFilter, p_dstPic, picPosX, picPosY, p_srcPic, 255);
    }

    picture_t *loadPicture(std::string picPath)
    {
        picture_t *p_ret;

        image_handler_t *p_imgHandler = image_HandlerCreate(p_intf);
        video_format_t fmt_in, fmt_out;
        video_format_Init(&fmt_in, 0);
        video_format_Init(&fmt_out, VLC_CODEC_RGBA);

        char *psz_url = vlc_path2uri(picPath.c_str(), NULL);
        p_ret = image_ReadUrl(p_imgHandler, psz_url, &fmt_in, &fmt_out);
        free(psz_url);

        video_format_Clean(&fmt_in);
        video_format_Clean(&fmt_out);
        image_HandlerDelete(p_imgHandler);

        return p_ret;
    }

    intf_thread_t *p_intf;

    int posX, posY;
    unsigned width, height;

    bool visible;

    bool canHaveHoover;
    bool hasHoover;
    vlc_tick_t hooverDate;

    void (*pressedCallback)(intf_thread_t *p_intf);

    picture_t *p_pic;
};


class Background : public Control
{
public:
    Background(intf_thread_t *p_intf, std::string picPath)
        : Control(p_intf, 0, 0)
    {
        p_pic = loadPicture(picPath);
        if (likely(p_pic != NULL))
        {
            width = p_pic->format.i_width;
            height = p_pic->format.i_height;
        }
    }
};


class Button : public Control
{
public:
    Button(intf_thread_t *p_intf, int posX, int posY, std::string picPath)
        : Control(p_intf, posX, posY, 0, 0, true)
    {
        p_pic = loadPicture(picPath);
        if (likely(p_pic != NULL))
        {
            width = p_pic->format.i_width;
            height = p_pic->format.i_height;
        }
    }
};


class Image : public Control
{
public:
    Image(intf_thread_t *p_intf, int posX, int posY, std::string picPath)
        : Control(p_intf, posX, posY)
    {
        p_pic = loadPicture(picPath);
        if (likely(p_pic != NULL))
        {
            width = p_pic->format.i_width;
            height = p_pic->format.i_height;
        }
    }
};


/**
 * @brief The Pointer class
 * Beware, it uses a spetial mask image to compute the activation animation.
 * The principle is that each pixel that has a first non null colorcomponent
 * sees its color changing with time.
 * The alpha channel of the mask however stays untouched.
 */
class Pointer : public Control
{
public:
    Pointer(intf_thread_t *p_intf, std::string maskPath)
        : Control(p_intf, 0, 0),
          p_mask(nullptr), validationProgress(0)
    {
        p_mask = loadPicture(maskPath);
        if (likely(p_mask != NULL))
        {
            p_pic = picture_NewFromFormat(&p_mask->format);
            if (likely(p_pic != NULL))
            {
                width = p_pic->format.i_width;
                height = p_pic->format.i_height;
            }
        }
    }

    virtual void draw(picture_t *p_dstPic) const
    {
        if (!visible || posX < 0 || posY < 0)
            return;

        drawPicture(p_pic, p_mask, 0, 0);

        // Calculate the color.
        uint8_t color[3] = {0, 0, 255};
        if (validationProgress >= 0.5f)
        {
            color[1] = (uint8_t)(-255 * 2 + 255 * 2 * validationProgress);
            color[2] = (uint8_t)(255 * 2 - 255 * 2 * validationProgress);
        }

        // Apply the color on the mask.
        #define PIXEL_SIZE 4
        uint8_t *p_pixMask = p_mask->p[0].p_pixels;
        uint8_t *p_pixDst = p_pic->p[0].p_pixels;
        unsigned delta = p_mask->p[0].i_pitch - p_mask->p[0].i_visible_pitch;

        for (unsigned y = 0; y < p_pic->format.i_visible_height; ++y)
        {
            for (unsigned x = 0; x < p_pic->format.i_visible_width;
                 ++x, p_pixMask += PIXEL_SIZE, p_pixDst += PIXEL_SIZE)
            {
                if (p_pixMask[0] > 0)
                {
                    p_pixDst[0] = color[0];
                    p_pixDst[1] = color[1];
                    p_pixDst[2] = color[2];
                }
            }
            p_pixMask += delta; p_pixDst += delta;
        }
        #undef PIXEL_SIZE

        drawPicture(p_dstPic, p_pic, posX, posY);
    }

    /**
     * @brief Set the central position of the pointer.
     */
    virtual void setPosition(int x, int y)
    {
        posX = x - p_pic->format.i_width / 2;
        posY = y - p_pic->format.i_height / 2;
    }

    virtual void resetValidationProgress()
    {
        validationProgress = 0;
    }

    virtual void updateValidationProgress(const Control *hooveredControl)
    {
        vlc_tick_t diff = vlc_tick_now() -
                          hooveredControl->getHooverDate();
        float p = (float) secf_from_vlc_tick(diff) / 2 /* duration */;
        // Clip to the [0, 1] range.
        validationProgress = std::min(1.f, std::max(0.f, p));
    }

protected:
    picture_t *p_mask;
    float validationProgress;
};


class Slider : public Control
{
public:
    Slider(intf_thread_t *p_intf, int posX, int posY,
           unsigned width, std::string picPath)
        : Control(p_intf, posX, posY, width, 0, true),
          progress(0), progressSetCallback(nullptr)
    {
        p_pic = loadPicture(picPath);
        if (likely(p_pic != NULL))
            height = p_pic->format.i_height;
    }

    virtual void draw(picture_t *p_dstPic) const
    {
        if (!visible)
            return;

        unsigned picPosX = posX + width * progress;

        // Draw the bar
        #define BAR_HEIGHT 2
        #define PIXEL_SIZE 4
        unsigned i_dstPitch = p_dstPic->p[0].i_pitch / PIXEL_SIZE;
        unsigned deltaY = (p_pic->format.i_height - BAR_HEIGHT) / 2;
        uint8_t *p_dst = p_dstPic->p[0].p_pixels
                + ((posY + deltaY) * i_dstPitch + posX) * PIXEL_SIZE;
        for (unsigned y = 0; y < BAR_HEIGHT; ++y)
        {
            for (unsigned x = 0; x < width; ++x)
            {
                p_dst[0] = 255;
                p_dst[1] = 0;
                p_dst[2] = 0;
                p_dst[3] = 255;

                p_dst += PIXEL_SIZE;
            }
            p_dst += (i_dstPitch - width) * PIXEL_SIZE;
        }
        #undef BAR_HEIGHT
        #undef PIXEL_SIZE

        // Draw the slider
        drawPicture(p_dstPic, p_pic, picPosX, posY);
    }

    virtual bool testAndSetHoover(int x, int y, unsigned pointerSize)
    {
        bool ret = Control::testAndSetHoover(x, y, pointerSize);

        if (ret)
        {
            userProgress = (x - posX) / (float)width;
            // Restrict to the [0, 1] range.
            userProgress = std::min(std::max(0.f, userProgress), 1.f);
        }

        return ret;
    }

    virtual bool testPressed()
    {
        if (!visible)
            return false;

        bool ret = false;
        vlc_tick_t currentDate = vlc_tick_now();

        if (hasHoover && currentDate >= hooverDate + CLOCK_FREQ * 2)
        {
            hooverDate = currentDate;
            ret = true;
            // Call the callback if there is one.
            if (pressedCallback != nullptr)
                pressedCallback(p_intf);
            if (progressSetCallback != nullptr)
                progressSetCallback(p_intf, userProgress);
        }

        return ret;
    }

    void setProgress(float p)
    {
        // Clip to the [0, 1] range.
        progress = std::min(std::max(p, 0.f), 1.f);
    }

    void setProgressSetCallback(void (*cb)(intf_thread_t *p_intf, float progress))
    {
        progressSetCallback = cb;
    }

protected:
    float progress; // 0: start, 1: end
    float userProgress; // The current progress set by the user

    void (*progressSetCallback)(intf_thread_t *p_intf, float progress);
};


struct subpicture_updater_sys_t {
    int  position;
    char *text;
};


class Text : public Control
{
public:
    Text(intf_thread_t *p_intf, int posX, int posY,
         std::string text)
        : Control(p_intf, posX, posY), subPicRegion(NULL)
    {
        // Create the subpicture
        video_format_t fmt;
        video_format_Init( &fmt, VLC_CODEC_TEXT);
        fmt.i_sar_num = 1;
        fmt.i_sar_den = 1;

        subPicRegion = subpicture_region_New(&fmt);
        if (!subPicRegion)
            return;

        subPicRegion->p_text = text_segment_New(NULL);
        if (!subPicRegion->p_text)
            return;
        subPicRegion->p_text->style = text_style_New();
        subPicRegion->p_text->style->i_font_size = 14;

        // Set the text
        setText(text);
    }

    ~Text()
    {
        subpicture_region_Delete(subPicRegion);
    }

    virtual void draw(picture_t *p_dstPic) const
    {
        if (!visible)
            return;

        if (subPicRegion && subPicRegion->p_picture)
            drawPicture(p_dstPic, subPicRegion->p_picture, posX, posY);
    }

    void setText(std::string t)
    {
        // Replace the text
        if (!subPicRegion || !subPicRegion->p_text)
            return;
        free(subPicRegion->p_text->psz_text);

        subPicRegion->p_text->psz_text = strdup(t.c_str());

        if (!subPicRegion->p_text->psz_text)
            return;

        // Render the text.
        intf_sys_t *p_sys = p_intf->p_sys;
        vlc_fourcc_t p_chroma_list[] = {VLC_CODEC_RGBA, 0};
        p_sys->p_textFilter->pf_render(p_sys->p_textFilter, subPicRegion, subPicRegion, p_chroma_list);
    }

protected:
    subpicture_region_t *subPicRegion;
};


#endif // HMD_CONTROLS_H
