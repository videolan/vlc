/*****************************************************************************
 * libvlc_media_player.h:  libvlc_media_player external API
 *****************************************************************************
 * Copyright (C) 1998-2015 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef VLC_LIBVLC_MEDIA_PLAYER_H
#define VLC_LIBVLC_MEDIA_PLAYER_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/** \defgroup libvlc_media_player LibVLC media player
 * \ingroup libvlc
 * A LibVLC media player plays one media (usually in a custom drawable).
 * @{
 * \file
 * LibVLC simple media player external API
 */

typedef struct libvlc_media_player_t libvlc_media_player_t;

/**
 * Description for video, audio tracks and subtitles. It contains
 * id, name (description string) and pointer to next record.
 */
typedef struct libvlc_track_description_t
{
    int   i_id;
    char *psz_name;
    struct libvlc_track_description_t *p_next;

} libvlc_track_description_t;

/**
 * Description for titles
 */
enum
{
    libvlc_title_menu          = 0x01,
    libvlc_title_interactive   = 0x02
};

typedef struct libvlc_title_description_t
{
    int64_t i_duration; /**< duration in milliseconds */
    char *psz_name; /**< title name */
    unsigned i_flags; /**< info if item was recognized as a menu, interactive or plain content by the demuxer */
} libvlc_title_description_t;

/**
 * Description for chapters
 */
typedef struct libvlc_chapter_description_t
{
    int64_t i_time_offset; /**< time-offset of the chapter in milliseconds */
    int64_t i_duration; /**< duration of the chapter in milliseconds */
    char *psz_name; /**< chapter name */
} libvlc_chapter_description_t;

/**
 * Description for audio output. It contains
 * name, description and pointer to next record.
 */
typedef struct libvlc_audio_output_t
{
    char *psz_name;
    char *psz_description;
    struct libvlc_audio_output_t *p_next;

} libvlc_audio_output_t;

/**
 * Description for audio output device.
 */
typedef struct libvlc_audio_output_device_t
{
    struct libvlc_audio_output_device_t *p_next; /**< Next entry in list */
    char *psz_device; /**< Device identifier string */
    char *psz_description; /**< User-friendly device description */
    /* More fields may be added here in later versions */
} libvlc_audio_output_device_t;

/**
 * Marq options definition
 */
typedef enum libvlc_video_marquee_option_t {
    libvlc_marquee_Enable = 0,
    libvlc_marquee_Text,                  /** string argument */
    libvlc_marquee_Color,
    libvlc_marquee_Opacity,
    libvlc_marquee_Position,
    libvlc_marquee_Refresh,
    libvlc_marquee_Size,
    libvlc_marquee_Timeout,
    libvlc_marquee_X,
    libvlc_marquee_Y
} libvlc_video_marquee_option_t;

/**
 * Navigation mode
 */
typedef enum libvlc_navigate_mode_t
{
    libvlc_navigate_activate = 0,
    libvlc_navigate_up,
    libvlc_navigate_down,
    libvlc_navigate_left,
    libvlc_navigate_right,
    libvlc_navigate_popup
} libvlc_navigate_mode_t;

/**
 * Enumeration of values used to set position (e.g. of video title).
 */
typedef enum libvlc_position_t {
    libvlc_position_disable=-1,
    libvlc_position_center,
    libvlc_position_left,
    libvlc_position_right,
    libvlc_position_top,
    libvlc_position_top_left,
    libvlc_position_top_right,
    libvlc_position_bottom,
    libvlc_position_bottom_left,
    libvlc_position_bottom_right
} libvlc_position_t;

/**
 * Enumeration of teletext keys than can be passed via
 * libvlc_video_set_teletext()
 */
typedef enum libvlc_teletext_key_t {
    libvlc_teletext_key_red = 'r' << 16,
    libvlc_teletext_key_green = 'g' << 16,
    libvlc_teletext_key_yellow = 'y' << 16,
    libvlc_teletext_key_blue = 'b' << 16,
    libvlc_teletext_key_index = 'i' << 16,
} libvlc_teletext_key_t;

/**
 * Opaque equalizer handle.
 *
 * Equalizer settings can be applied to a media player.
 */
typedef struct libvlc_equalizer_t libvlc_equalizer_t;

/**
 * Create an empty Media Player object
 *
 * \param p_libvlc_instance the libvlc instance in which the Media Player
 *        should be created.
 * \return a new media player object, or NULL on error.
 * It must be released by libvlc_media_player_release().
 */
LIBVLC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t *p_libvlc_instance );

/**
 * Create a Media Player object from a Media
 *
 * \param p_md the media. Afterwards the p_md can be safely
 *        destroyed.
 * \return a new media player object, or NULL on error.
 * It must be released by libvlc_media_player_release().
 */
LIBVLC_API libvlc_media_player_t * libvlc_media_player_new_from_media( libvlc_media_t *p_md );

/**
 * Release a media_player after use
 * Decrement the reference count of a media player object. If the
 * reference count is 0, then libvlc_media_player_release() will
 * release the media player object. If the media player object
 * has been released, then it should not be used again.
 *
 * \param p_mi the Media Player to free
 */
LIBVLC_API void libvlc_media_player_release( libvlc_media_player_t *p_mi );

/**
 * Retain a reference to a media player object. Use
 * libvlc_media_player_release() to decrement reference count.
 *
 * \param p_mi media player object
 */
LIBVLC_API void libvlc_media_player_retain( libvlc_media_player_t *p_mi );

/**
 * Set the media that will be used by the media_player. If any,
 * previous md will be released.
 *
 * \param p_mi the Media Player
 * \param p_md the Media. Afterwards the p_md can be safely
 *        destroyed.
 */
LIBVLC_API void libvlc_media_player_set_media( libvlc_media_player_t *p_mi,
                                                   libvlc_media_t *p_md );

/**
 * Get the media used by the media_player.
 *
 * \param p_mi the Media Player
 * \return the media associated with p_mi, or NULL if no
 *         media is associated
 */
LIBVLC_API libvlc_media_t * libvlc_media_player_get_media( libvlc_media_player_t *p_mi );

/**
 * Get the Event Manager from which the media player send event.
 *
 * \param p_mi the Media Player
 * \return the event manager associated with p_mi
 */
LIBVLC_API libvlc_event_manager_t * libvlc_media_player_event_manager ( libvlc_media_player_t *p_mi );

/**
 * is_playing
 *
 * \param p_mi the Media Player
 * \retval true media player is playing
 * \retval false media player is not playing
 */
LIBVLC_API bool libvlc_media_player_is_playing(libvlc_media_player_t *p_mi);

/**
 * Play
 *
 * \param p_mi the Media Player
 * \return 0 if playback started (and was already started), or -1 on error.
 */
LIBVLC_API int libvlc_media_player_play ( libvlc_media_player_t *p_mi );

/**
 * Pause or resume (no effect if there is no media)
 *
 * \param mp the Media Player
 * \param do_pause play/resume if zero, pause if non-zero
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API void libvlc_media_player_set_pause ( libvlc_media_player_t *mp,
                                                    int do_pause );

/**
 * Toggle pause (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_pause ( libvlc_media_player_t *p_mi );

/**
 * Stop asynchronously
 *
 * \note This function is asynchronous. In case of success, the user should
 * wait for the libvlc_MediaPlayerStopped event to know when the stop is
 * finished.
 *
 * \param p_mi the Media Player
 * \return 0 if the player is being stopped, -1 otherwise (no-op)
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API int libvlc_media_player_stop_async ( libvlc_media_player_t *p_mi );

/**
 * Set a renderer to the media player
 *
 * \note must be called before the first call of libvlc_media_player_play() to
 * take effect.
 *
 * \see libvlc_renderer_discoverer_new
 *
 * \param p_mi the Media Player
 * \param p_item an item discovered by libvlc_renderer_discoverer_start()
 * \return 0 on success, -1 on error.
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API int libvlc_media_player_set_renderer( libvlc_media_player_t *p_mi,
                                                 libvlc_renderer_item_t *p_item );

/**
 * Enumeration of the Video color primaries.
 */
typedef enum libvlc_video_color_primaries_t {
    libvlc_video_primaries_BT601_525 = 1,
    libvlc_video_primaries_BT601_625 = 2,
    libvlc_video_primaries_BT709     = 3,
    libvlc_video_primaries_BT2020    = 4,
    libvlc_video_primaries_DCI_P3    = 5,
    libvlc_video_primaries_BT470_M   = 6,
} libvlc_video_color_primaries_t;

/**
 * Enumeration of the Video color spaces.
 */
typedef enum libvlc_video_color_space_t {
    libvlc_video_colorspace_BT601  = 1,
    libvlc_video_colorspace_BT709  = 2,
    libvlc_video_colorspace_BT2020 = 3,
} libvlc_video_color_space_t;

/**
 * Enumeration of the Video transfer functions.
 */
typedef enum libvlc_video_transfer_func_t {
    libvlc_video_transfer_func_LINEAR     = 1,
    libvlc_video_transfer_func_SRGB       = 2,
    libvlc_video_transfer_func_BT470_BG   = 3,
    libvlc_video_transfer_func_BT470_M    = 4,
    libvlc_video_transfer_func_BT709      = 5,
    libvlc_video_transfer_func_PQ         = 6,
    libvlc_video_transfer_func_SMPTE_240  = 7,
    libvlc_video_transfer_func_HLG        = 8,
} libvlc_video_transfer_func_t;


/**
 * Callback prototype to allocate and lock a picture buffer.
 *
 * Whenever a new video frame needs to be decoded, the lock callback is
 * invoked. Depending on the video chroma, one or three pixel planes of
 * adequate dimensions must be returned via the second parameter. Those
 * planes must be aligned on 32-bytes boundaries.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param planes start address of the pixel planes (LibVLC allocates the array
 *             of void pointers, this callback must initialize the array) [OUT]
 * \return a private pointer for the display and unlock callbacks to identify
 *         the picture buffers
 */
typedef void *(*libvlc_video_lock_cb)(void *opaque, void **planes);

/**
 * Callback prototype to unlock a picture buffer.
 *
 * When the video frame decoding is complete, the unlock callback is invoked.
 * This callback might not be needed at all. It is only an indication that the
 * application can now read the pixel values if it needs to.
 *
 * \note A picture buffer is unlocked after the picture is decoded,
 * but before the picture is displayed.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param picture private pointer returned from the @ref libvlc_video_lock_cb
 *                callback [IN]
 * \param planes pixel planes as defined by the @ref libvlc_video_lock_cb
 *               callback (this parameter is only for convenience) [IN]
 */
typedef void (*libvlc_video_unlock_cb)(void *opaque, void *picture,
                                       void *const *planes);

/**
 * Callback prototype to display a picture.
 *
 * When the video frame needs to be shown, as determined by the media playback
 * clock, the display callback is invoked.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param picture private pointer returned from the @ref libvlc_video_lock_cb
 *                callback [IN]
 */
typedef void (*libvlc_video_display_cb)(void *opaque, void *picture);

/**
 * Callback prototype to configure picture buffers format.
 * This callback gets the format of the video as output by the video decoder
 * and the chain of video filters (if any). It can opt to change any parameter
 * as it needs. In that case, LibVLC will attempt to convert the video format
 * (rescaling and chroma conversion) but these operations can be CPU intensive.
 *
 * \param opaque pointer to the private pointer passed to
 *               libvlc_video_set_callbacks() [IN/OUT]
 * \param chroma pointer to the 4 bytes video format identifier [IN/OUT]
 * \param width pointer to the buffer width in pixels[IN/OUT]
 * \param height pointer to the buffer height in pixels[IN/OUT]
 * \param pitches table of scanline pitches in bytes for each pixel plane
 *                (the table is allocated by LibVLC) [OUT]
 * \param lines table of scanlines count for each plane [OUT]
 * \return the number of picture buffers allocated, 0 indicates failure
 *
 * \version LibVLC 4.0.0 and later.
 * \param (width+1) - pointer to display width in pixels[IN]
 * \param (height+1) - pointer to display height in pixels[IN]
 *
 * \note
 * For each pixels plane, the scanline pitch must be bigger than or equal to
 * the number of bytes per pixel multiplied by the pixel width.
 * Similarly, the number of scanlines must be bigger than of equal to
 * the pixel height.
 * Furthermore, we recommend that pitches and lines be multiple of 32
 * to not break assumptions that might be held by optimized code
 * in the video decoders, video filters and/or video converters.
 */
typedef unsigned (*libvlc_video_format_cb)(void **opaque, char *chroma,
                                           unsigned *width, unsigned *height,
                                           unsigned *pitches,
                                           unsigned *lines);

/**
 * Callback prototype to configure picture buffers format.
 *
 * \param opaque private pointer as passed to libvlc_video_set_format_callbacks()
 *               (and possibly modified by @ref libvlc_video_format_cb) [IN]
 */
typedef void (*libvlc_video_cleanup_cb)(void *opaque);


/**
 * Set callbacks and private data to render decoded video to a custom area
 * in memory.
 * Use libvlc_video_set_format() or libvlc_video_set_format_callbacks()
 * to configure the decoded format.
 *
 * \warning Rendering video into custom memory buffers is considerably less
 * efficient than rendering in a custom window as normal.
 *
 * For optimal perfomances, VLC media player renders into a custom window, and
 * does not use this function and associated callbacks. It is <b>highly
 * recommended</b> that other LibVLC-based application do likewise.
 * To embed video in a window, use libvlc_media_player_set_xid() or equivalent
 * depending on the operating system.
 *
 * If window embedding does not fit the application use case, then a custom
 * LibVLC video output display plugin is required to maintain optimal video
 * rendering performances.
 *
 * The following limitations affect performance:
 * - Hardware video decoding acceleration will either be disabled completely,
 *   or require (relatively slow) copy from video/DSP memory to main memory.
 * - Sub-pictures (subtitles, on-screen display, etc.) must be blent into the
 *   main picture by the CPU instead of the GPU.
 * - Depending on the video format, pixel format conversion, picture scaling,
 *   cropping and/or picture re-orientation, must be performed by the CPU
 *   instead of the GPU.
 * - Memory copying is required between LibVLC reference picture buffers and
 *   application buffers (between lock and unlock callbacks).
 *
 * \param mp the media player
 * \param lock callback to lock video memory (must not be NULL)
 * \param unlock callback to unlock video memory (or NULL if not needed)
 * \param display callback to display video (or NULL if not needed)
 * \param opaque private pointer for the three callbacks (as first parameter)
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API
void libvlc_video_set_callbacks( libvlc_media_player_t *mp,
                                 libvlc_video_lock_cb lock,
                                 libvlc_video_unlock_cb unlock,
                                 libvlc_video_display_cb display,
                                 void *opaque );

/**
 * Set decoded video chroma and dimensions.
 * This only works in combination with libvlc_video_set_callbacks(),
 * and is mutually exclusive with libvlc_video_set_format_callbacks().
 *
 * \param mp the media player
 * \param chroma a four-characters string identifying the chroma
 *               (e.g. "RV32" or "YUYV")
 * \param width pixel width
 * \param height pixel height
 * \param pitch line pitch (in bytes)
 * \version LibVLC 1.1.1 or later
 * \bug All pixel planes are expected to have the same pitch.
 * To use the YCbCr color space with chrominance subsampling,
 * consider using libvlc_video_set_format_callbacks() instead.
 */
LIBVLC_API
void libvlc_video_set_format( libvlc_media_player_t *mp, const char *chroma,
                              unsigned width, unsigned height,
                              unsigned pitch );

/**
 * Set decoded video chroma and dimensions. This only works in combination with
 * libvlc_video_set_callbacks().
 *
 * \param mp the media player
 * \param setup callback to select the video format (cannot be NULL)
 * \param cleanup callback to release any allocated resources (or NULL)
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API
void libvlc_video_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_video_format_cb setup,
                                        libvlc_video_cleanup_cb cleanup );


typedef struct
{
    bool hardware_decoding; /** set if D3D11_CREATE_DEVICE_VIDEO_SUPPORT is needed for D3D11 */
} libvlc_video_setup_device_cfg_t;

typedef struct
{
    union {
        struct {
            void *device_context; /** ID3D11DeviceContext* */
        } d3d11;
        struct {
            void *device;         /** IDirect3D9* */
            int  adapter;         /** Adapter to use with the IDirect3D9* */
        } d3d9;
    };
} libvlc_video_setup_device_info_t;

/**
 * Callback prototype called to initialize user data.
 * Setup the rendering environment.
 *
 * \param opaque private pointer passed to the @a libvlc_video_set_output_callbacks()
 *               on input. The callback can change this value on output to be
 *               passed to all the other callbacks set on @a libvlc_video_set_output_callbacks().
 *               [IN/OUT]
 * \param cfg requested configuration of the video device [IN]
 * \param out libvlc_video_setup_device_info_t* to fill [OUT]
 * \return true on success
 * \version LibVLC 4.0.0 or later
 *
 * For \ref libvlc_video_engine_d3d9 the output must be a IDirect3D9*.
 * A reference to this object is held until the \ref LIBVLC_VIDEO_DEVICE_CLEANUP is called.
 * the device must be created with D3DPRESENT_PARAMETERS.hDeviceWindow set to 0.
 *
 * For \ref libvlc_video_engine_d3d11 the output must be a ID3D11DeviceContext*.
 * A reference to this object is held until the \ref LIBVLC_VIDEO_DEVICE_CLEANUP is called.
 * The ID3D11Device used to create ID3D11DeviceContext must have multithreading enabled.
 */
typedef bool (*libvlc_video_output_setup_cb)(void **opaque,
                                      const libvlc_video_setup_device_cfg_t *cfg,
                                      libvlc_video_setup_device_info_t *out);


/**
 * Callback prototype called to release user data
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \version LibVLC 4.0.0 or later
 */
typedef void (*libvlc_video_output_cleanup_cb)(void* opaque);

typedef struct
{
    unsigned width;                        /** rendering video width in pixel */
    unsigned height;                      /** rendering video height in pixel */
    unsigned bitdepth;      /** rendering video bit depth in bits per channel */
    bool full_range;          /** video is full range or studio/limited range */
    libvlc_video_color_space_t colorspace;              /** video color space */
    libvlc_video_color_primaries_t primaries;       /** video color primaries */
    libvlc_video_transfer_func_t transfer;        /** video transfer function */
    void *device;   /** device used for rendering, IDirect3DDevice9* for D3D9 */
} libvlc_video_render_cfg_t;

typedef struct
{
    union {
        int dxgi_format;  /** the rendering DXGI_FORMAT for \ref libvlc_video_engine_d3d11*/
        uint32_t d3d9_format;  /** the rendering D3DFORMAT for \ref libvlc_video_engine_d3d9 */
        int opengl_format;  /** the rendering GLint GL_RGBA or GL_RGB for \ref libvlc_video_engine_opengl and
                            for \ref libvlc_video_engine_gles2 */
        void *p_surface; /** currently unused */
    };
    bool full_range;          /** video is full range or studio/limited range */
    libvlc_video_color_space_t colorspace;              /** video color space */
    libvlc_video_color_primaries_t primaries;       /** video color primaries */
    libvlc_video_transfer_func_t transfer;        /** video transfer function */
} libvlc_video_output_cfg_t;

/**
 * Callback prototype called on video size changes.
 * Update the rendering output setup.
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \param cfg configuration of the video that will be rendered [IN]
 * \param output configuration describing with how the rendering is setup [OUT]
 * \version LibVLC 4.0.0 or later
 *
 * \note the configuration device for Direct3D9 is the IDirect3DDevice9 that VLC
 *       uses to render. The host must set a Render target and call Present()
 *       when it needs the drawing from VLC to be done. This object is not valid
 *       anymore after Cleanup is called.
 *
 * Tone mapping, range and color conversion will be done depending on the values
 * set in the output structure.
 */
typedef bool (*libvlc_video_update_output_cb)(void* opaque, const libvlc_video_render_cfg_t *cfg,
                                              libvlc_video_output_cfg_t *output );


/**
 * Callback prototype called after performing drawing calls.
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \version LibVLC 4.0.0 or later
 */
typedef void (*libvlc_video_swap_cb)(void* opaque);

/**
 * Callback prototype to set up the OpenGL context for rendering.
 * Tell the host the rendering is about to start/has finished.
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \param enter true to set the context as current, false to unset it [IN]
 * \return true on success
 * \version LibVLC 4.0.0 or later
 *
 * On Direct3D11 the following may change on the provided ID3D11DeviceContext*
 * between \ref enter being true and \ref enter being false:
 * - IASetPrimitiveTopology()
 * - IASetInputLayout()
 * - IASetVertexBuffers()
 * - IASetIndexBuffer()
 * - VSSetConstantBuffers()
 * - VSSetShader()
 * - PSSetSamplers()
 * - PSSetConstantBuffers()
 * - PSSetShaderResources()
 * - PSSetShader()
 * - RSSetViewports()
 * - DrawIndexed()
 */
typedef bool (*libvlc_video_makeCurrent_cb)(void* opaque, bool enter);

/**
 * Callback prototype to load opengl functions
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \param fct_name name of the opengl function to load
 * \return a pointer to the named OpenGL function the NULL otherwise
 * \version LibVLC 4.0.0 or later
 */
typedef void* (*libvlc_video_getProcAddress_cb)(void* opaque, const char* fct_name);

typedef struct
{
    /* similar to SMPTE ST 2086 mastering display color volume */
    uint16_t RedPrimary[2];
    uint16_t GreenPrimary[2];
    uint16_t BluePrimary[2];
    uint16_t WhitePoint[2];
    unsigned int MaxMasteringLuminance;
    unsigned int MinMasteringLuminance;
    uint16_t MaxContentLightLevel;
    uint16_t MaxFrameAverageLightLevel;
} libvlc_video_frame_hdr10_metadata_t;

typedef enum libvlc_video_metadata_type_t {
    libvlc_video_metadata_frame_hdr10, /**< libvlc_video_frame_hdr10_metadata_t */
} libvlc_video_metadata_type_t;

/**
 * Callback prototype to receive metadata before rendering.
 *
 * \param opaque private pointer passed to the @a libvlc_video_set_output_callbacks() [IN]
 * \param type type of data passed in metadata [IN]
 * \param metadata the type of metadata [IN]
 * \version LibVLC 4.0.0 or later
 */
typedef void (*libvlc_video_frameMetadata_cb)(void* opaque, libvlc_video_metadata_type_t type, const void *metadata);

/**
 * Enumeration of the Video engine to be used on output.
 * can be passed to @a libvlc_video_set_output_callbacks
 */
typedef enum libvlc_video_engine_t {
    /** Disable rendering engine */
    libvlc_video_engine_disable,
    libvlc_video_engine_opengl,
    libvlc_video_engine_gles2,
    /** Direct3D11 rendering engine */
    libvlc_video_engine_d3d11,
    /** Direct3D9 rendering engine */
    libvlc_video_engine_d3d9,
} libvlc_video_engine_t;

/** Set the callback to call when the host app resizes the rendering area.
 *
 * This allows text rendering and aspect ratio to be handled properly when the host
 * rendering size changes.
 *
 * It may be called before the \ref libvlc_video_output_setup_cb callback.
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \param report_size_change callback which must be called when the host size changes. [IN]
 *        The callback is valid until another call to \ref libvlc_video_output_set_resize_cb
 *        is done. This may be called from any thread.
 * \param report_opaque private pointer to pass to the \ref report_size_change callback. [IN]
 */
typedef void( *libvlc_video_output_set_resize_cb )( void *opaque,
                                                    void (*report_size_change)(void *report_opaque, unsigned width, unsigned height),
                                                    void *report_opaque );

/** Tell the host the rendering for the given plane is about to start
 *
 * \param opaque private pointer set on the opaque parameter of @a libvlc_video_output_setup_cb() [IN]
 * \param plane number of the rendering plane to select
 * \return true on success
 * \version LibVLC 4.0.0 or later
 *
 * \note This is only used with \ref libvlc_video_engine_d3d11.
 *
 * The host should call OMSetRenderTargets for Direct3D11. If this callback is
 * not used (set to NULL in @a libvlc_video_set_output_callbacks()) OMSetRenderTargets
 * has to be set during the @a libvlc_video_makeCurrent_cb()
 * entering call.
 *
 * The number of planes depend on the DXGI_FORMAT returned during the
 * \ref LIBVLC_VIDEO_UPDATE_OUTPUT call. It's usually one plane except for
 * semi-planar formats like DXGI_FORMAT_NV12 or DXGI_FORMAT_P010.
 */
typedef bool( *libvlc_video_output_select_plane_cb )( void *opaque, size_t plane );

/**
 * Set callbacks and data to render decoded video to a custom texture
 *
 * \warning VLC will perform video rendering in its own thread and at its own rate,
 * You need to provide your own synchronisation mechanism.
 *
 * \param mp the media player
 * \param engine the GPU engine to use
 * \param setup_cb callback called to initialize user data
 * \param cleanup_cb callback called to clean up user data
 * \param resize_cb callback to set the resize callback
 * \param update_output_cb callback to get the rendering format of the host (cannot be NULL)
 * \param swap_cb callback called after rendering a video frame (cannot be NULL)
 * \param makeCurrent_cb callback called to enter/leave the rendering context (cannot be NULL)
 * \param getProcAddress_cb opengl function loading callback (cannot be NULL for \ref libvlc_video_engine_opengl and for \ref libvlc_video_engine_gles2)
 * \param metadata_cb callback to provide frame metadata (D3D11 only)
 * \param select_plane_cb callback to select different D3D11 rendering targets
 * \param opaque private pointer passed to callbacks
 *
 * \retval true engine selected and callbacks set
 * \retval false engine type unknown, callbacks not set
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API
bool libvlc_video_set_output_callbacks( libvlc_media_player_t *mp,
                                        libvlc_video_engine_t engine,
                                        libvlc_video_output_setup_cb setup_cb,
                                        libvlc_video_output_cleanup_cb cleanup_cb,
                                        libvlc_video_output_set_resize_cb resize_cb,
                                        libvlc_video_update_output_cb update_output_cb,
                                        libvlc_video_swap_cb swap_cb,
                                        libvlc_video_makeCurrent_cb makeCurrent_cb,
                                        libvlc_video_getProcAddress_cb getProcAddress_cb,
                                        libvlc_video_frameMetadata_cb metadata_cb,
                                        libvlc_video_output_select_plane_cb select_plane_cb,
                                        void* opaque );

/**
 * Set the NSView handler where the media player should render its video output.
 *
 * Use the vout called "macosx".
 *
 * The drawable is an NSObject that follow the VLCVideoViewEmbedding
 * protocol:
 *
 * @code{.m}
 * @protocol VLCVideoViewEmbedding <NSObject>
 * - (void)addVoutSubview:(NSView *)view;
 * - (void)removeVoutSubview:(NSView *)view;
 * @end
 * @endcode
 *
 * Or it can be an NSView object.
 *
 * If you want to use it along with Qt see the QMacCocoaViewContainer. Then
 * the following code should work:
 * @code{.mm}
 * {
 *     NSView *video = [[NSView alloc] init];
 *     QMacCocoaViewContainer *container = new QMacCocoaViewContainer(video, parent);
 *     libvlc_media_player_set_nsobject(mp, video);
 *     [video release];
 * }
 * @endcode
 *
 * You can find a live example in VLCVideoView in VLCKit.framework.
 *
 * \param p_mi the Media Player
 * \param drawable the drawable that is either an NSView or an object following
 * the VLCVideoViewEmbedding protocol.
 */
LIBVLC_API void libvlc_media_player_set_nsobject ( libvlc_media_player_t *p_mi, void * drawable );

/**
 * Get the NSView handler previously set with libvlc_media_player_set_nsobject().
 *
 * \param p_mi the Media Player
 * \return the NSView handler or 0 if none where set
 */
LIBVLC_API void * libvlc_media_player_get_nsobject ( libvlc_media_player_t *p_mi );

/**
 * Set an X Window System drawable where the media player should render its
 * video output. The call takes effect when the playback starts. If it is
 * already started, it might need to be stopped before changes apply.
 * If LibVLC was built without X11 output support, then this function has no
 * effects.
 *
 * By default, LibVLC will capture input events on the video rendering area.
 * Use libvlc_video_set_mouse_input() and libvlc_video_set_key_input() to
 * disable that and deliver events to the parent window / to the application
 * instead. By design, the X11 protocol delivers input events to only one
 * recipient.
 *
 * \warning
 * The application must call the XInitThreads() function from Xlib before
 * libvlc_new(), and before any call to XOpenDisplay() directly or via any
 * other library. Failure to call XInitThreads() will seriously impede LibVLC
 * performance. Calling XOpenDisplay() before XInitThreads() will eventually
 * crash the process. That is a limitation of Xlib.
 *
 * \param p_mi media player
 * \param drawable X11 window ID
 *
 * \note
 * The specified identifier must correspond to an existing Input/Output class
 * X11 window. Pixmaps are <b>not</b> currently supported. The default X11
 * server is assumed, i.e. that specified in the DISPLAY environment variable.
 *
 * \warning
 * LibVLC can deal with invalid X11 handle errors, however some display drivers
 * (EGL, GLX, VA and/or VDPAU) can unfortunately not. Thus the window handle
 * must remain valid until playback is stopped, otherwise the process may
 * abort or crash.
 *
 * \bug
 * No more than one window handle per media player instance can be specified.
 * If the media has multiple simultaneously active video tracks, extra tracks
 * will be rendered into external windows beyond the control of the
 * application.
 */
LIBVLC_API void libvlc_media_player_set_xwindow(libvlc_media_player_t *p_mi,
                                                uint32_t drawable);

/**
 * Get the X Window System window identifier previously set with
 * libvlc_media_player_set_xwindow(). Note that this will return the identifier
 * even if VLC is not currently using it (for instance if it is playing an
 * audio-only input).
 *
 * \param p_mi the Media Player
 * \return an X window ID, or 0 if none where set.
 */
LIBVLC_API uint32_t libvlc_media_player_get_xwindow ( libvlc_media_player_t *p_mi );

/**
 * Set a Win32/Win64 API window handle (HWND) where the media player should
 * render its video output. If LibVLC was built without Win32/Win64 API output
 * support, then this has no effects.
 *
 * \param p_mi the Media Player
 * \param drawable windows handle of the drawable
 */
LIBVLC_API void libvlc_media_player_set_hwnd ( libvlc_media_player_t *p_mi, void *drawable );

/**
 * Get the Windows API window handle (HWND) previously set with
 * libvlc_media_player_set_hwnd(). The handle will be returned even if LibVLC
 * is not currently outputting any video to it.
 *
 * \param p_mi the Media Player
 * \return a window handle or NULL if there are none.
 */
LIBVLC_API void *libvlc_media_player_get_hwnd ( libvlc_media_player_t *p_mi );

/**
 * Set the android context.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_mi the media player
 * \param p_awindow_handler org.videolan.libvlc.AWindow jobject owned by the
 *        org.videolan.libvlc.MediaPlayer class from the libvlc-android project.
 */
LIBVLC_API void libvlc_media_player_set_android_context( libvlc_media_player_t *p_mi,
                                                         void *p_awindow_handler );

/**
 * Callback prototype for audio playback.
 *
 * The LibVLC media player decodes and post-processes the audio signal
 * asynchronously (in an internal thread). Whenever audio samples are ready
 * to be queued to the output, this callback is invoked.
 *
 * The number of samples provided per invocation may depend on the file format,
 * the audio coding algorithm, the decoder plug-in, the post-processing
 * filters and timing. Application must not assume a certain number of samples.
 *
 * The exact format of audio samples is determined by libvlc_audio_set_format()
 * or libvlc_audio_set_format_callbacks() as is the channels layout.
 *
 * Note that the number of samples is per channel. For instance, if the audio
 * track sampling rate is 48000 Hz, then 1200 samples represent 25 milliseconds
 * of audio signal - regardless of the number of audio channels.
 *
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param samples pointer to a table of audio samples to play back [IN]
 * \param count number of audio samples to play back
 * \param pts expected play time stamp (see libvlc_delay())
 */
typedef void (*libvlc_audio_play_cb)(void *data, const void *samples,
                                     unsigned count, int64_t pts);

/**
 * Callback prototype for audio pause.
 *
 * LibVLC invokes this callback to pause audio playback.
 *
 * \note The pause callback is never called if the audio is already paused.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param pts time stamp of the pause request (should be elapsed already)
 */
typedef void (*libvlc_audio_pause_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio resumption.
 *
 * LibVLC invokes this callback to resume audio playback after it was
 * previously paused.
 *
 * \note The resume callback is never called if the audio is not paused.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param pts time stamp of the resumption request (should be elapsed already)
 */
typedef void (*libvlc_audio_resume_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio buffer flush.
 *
 * LibVLC invokes this callback if it needs to discard all pending buffers and
 * stop playback as soon as possible. This typically occurs when the media is
 * stopped.
 *
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_flush_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio buffer drain.
 *
 * LibVLC may invoke this callback when the decoded audio track is ending.
 * There will be no further decoded samples for the track, but playback should
 * nevertheless continue until all already pending buffers are rendered.
 *
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_drain_cb)(void *data);

/**
 * Callback prototype for audio volume change.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param volume software volume (1. = nominal, 0. = mute)
 * \param mute muted flag
 */
typedef void (*libvlc_audio_set_volume_cb)(void *data,
                                           float volume, bool mute);

/**
 * Sets callbacks and private data for decoded audio.
 *
 * Use libvlc_audio_set_format() or libvlc_audio_set_format_callbacks()
 * to configure the decoded audio format.
 *
 * \note The audio callbacks override any other audio output mechanism.
 * If the callbacks are set, LibVLC will <b>not</b> output audio in any way.
 *
 * \param mp the media player
 * \param play callback to play audio samples (must not be NULL)
 * \param pause callback to pause playback (or NULL to ignore)
 * \param resume callback to resume playback (or NULL to ignore)
 * \param flush callback to flush audio buffers (or NULL to ignore)
 * \param drain callback to drain audio buffers (or NULL to ignore)
 * \param opaque private pointer for the audio callbacks (as first parameter)
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API
void libvlc_audio_set_callbacks( libvlc_media_player_t *mp,
                                 libvlc_audio_play_cb play,
                                 libvlc_audio_pause_cb pause,
                                 libvlc_audio_resume_cb resume,
                                 libvlc_audio_flush_cb flush,
                                 libvlc_audio_drain_cb drain,
                                 void *opaque );

/**
 * Set callbacks and private data for decoded audio. This only works in
 * combination with libvlc_audio_set_callbacks().
 * Use libvlc_audio_set_format() or libvlc_audio_set_format_callbacks()
 * to configure the decoded audio format.
 *
 * \param mp the media player
 * \param set_volume callback to apply audio volume,
 *                   or NULL to apply volume in software
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API
void libvlc_audio_set_volume_callback( libvlc_media_player_t *mp,
                                       libvlc_audio_set_volume_cb set_volume );

/**
 * Callback prototype to setup the audio playback.
 *
 * This is called when the media player needs to create a new audio output.
 * \param opaque pointer to the data pointer passed to
 *               libvlc_audio_set_callbacks() [IN/OUT]
 * \param format 4 bytes sample format [IN/OUT]
 * \param rate sample rate [IN/OUT]
 * \param channels channels count [IN/OUT]
 * \return 0 on success, anything else to skip audio playback
 */
typedef int (*libvlc_audio_setup_cb)(void **opaque, char *format, unsigned *rate,
                                     unsigned *channels);

/**
 * Callback prototype for audio playback cleanup.
 *
 * This is called when the media player no longer needs an audio output.
 * \param opaque data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_cleanup_cb)(void *data);

/**
 * Sets decoded audio format via callbacks.
 *
 * This only works in combination with libvlc_audio_set_callbacks().
 *
 * \param mp the media player
 * \param setup callback to select the audio format (cannot be NULL)
 * \param cleanup callback to release any allocated resources (or NULL)
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API
void libvlc_audio_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_audio_setup_cb setup,
                                        libvlc_audio_cleanup_cb cleanup );

/**
 * Sets a fixed decoded audio format.
 *
 * This only works in combination with libvlc_audio_set_callbacks(),
 * and is mutually exclusive with libvlc_audio_set_format_callbacks().
 *
 * \param mp the media player
 * \param format a four-characters string identifying the sample format
 *               (e.g. "S16N" or "f32l")
 * \param rate sample rate (expressed in Hz)
 * \param channels channels count
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API
void libvlc_audio_set_format( libvlc_media_player_t *mp, const char *format,
                              unsigned rate, unsigned channels );

/** \bug This might go away ... to be replaced by a broader system */

/**
 * Get the current movie length (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie length (in ms), or -1 if there is no media.
 */
LIBVLC_API libvlc_time_t libvlc_media_player_get_length( libvlc_media_player_t *p_mi );

/**
 * Get the current movie time (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie time (in ms), or -1 if there is no media.
 */
LIBVLC_API libvlc_time_t libvlc_media_player_get_time( libvlc_media_player_t *p_mi );

/**
 * Set the movie time (in ms). This has no effect if no media is being played.
 * Not all formats and protocols support this.
 *
 * \param p_mi the Media Player
 * \param b_fast prefer fast seeking or precise seeking
 * \param i_time the movie time (in ms).
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_media_player_set_time( libvlc_media_player_t *p_mi,
                                             libvlc_time_t i_time, bool b_fast );

/**
 * Get movie position as percentage between 0.0 and 1.0.
 *
 * \param p_mi the Media Player
 * \return movie position, or -1. in case of error
 */
LIBVLC_API float libvlc_media_player_get_position( libvlc_media_player_t *p_mi );

/**
 * Set movie position as percentage between 0.0 and 1.0.
 * This has no effect if playback is not enabled.
 * This might not work depending on the underlying input format and protocol.
 *
 * \param p_mi the Media Player
 * \param b_fast prefer fast seeking or precise seeking
 * \param f_pos the position
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_media_player_set_position( libvlc_media_player_t *p_mi,
                                                 float f_pos, bool b_fast );

/**
 * Set movie chapter (if applicable).
 *
 * \param p_mi the Media Player
 * \param i_chapter chapter number to play
 */
LIBVLC_API void libvlc_media_player_set_chapter( libvlc_media_player_t *p_mi, int i_chapter );

/**
 * Get movie chapter.
 *
 * \param p_mi the Media Player
 * \return chapter number currently playing, or -1 if there is no media.
 */
LIBVLC_API int libvlc_media_player_get_chapter( libvlc_media_player_t *p_mi );

/**
 * Get movie chapter count
 *
 * \param p_mi the Media Player
 * \return number of chapters in movie, or -1.
 */
LIBVLC_API int libvlc_media_player_get_chapter_count( libvlc_media_player_t *p_mi );

/**
 * Get title chapter count
 *
 * \param p_mi the Media Player
 * \param i_title title
 * \return number of chapters in title, or -1
 */
LIBVLC_API int libvlc_media_player_get_chapter_count_for_title(
                       libvlc_media_player_t *p_mi, int i_title );

/**
 * Set movie title
 *
 * \param p_mi the Media Player
 * \param i_title title number to play
 */
LIBVLC_API void libvlc_media_player_set_title( libvlc_media_player_t *p_mi, int i_title );

/**
 * Get movie title
 *
 * \param p_mi the Media Player
 * \return title number currently playing, or -1
 */
LIBVLC_API int libvlc_media_player_get_title( libvlc_media_player_t *p_mi );

/**
 * Get movie title count
 *
 * \param p_mi the Media Player
 * \return title number count, or -1
 */
LIBVLC_API int libvlc_media_player_get_title_count( libvlc_media_player_t *p_mi );

/**
 * Set previous chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_previous_chapter( libvlc_media_player_t *p_mi );

/**
 * Set next chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_next_chapter( libvlc_media_player_t *p_mi );

/**
 * Get the requested movie play rate.
 * @warning Depending on the underlying media, the requested rate may be
 * different from the real playback rate.
 *
 * \param p_mi the Media Player
 * \return movie play rate
 */
LIBVLC_API float libvlc_media_player_get_rate( libvlc_media_player_t *p_mi );

/**
 * Set movie play rate
 *
 * \param p_mi the Media Player
 * \param rate movie play rate to set
 * \return -1 if an error was detected, 0 otherwise (but even then, it might
 * not actually work depending on the underlying media protocol)
 */
LIBVLC_API int libvlc_media_player_set_rate( libvlc_media_player_t *p_mi, float rate );

/**
 * Get current movie state
 *
 * \param p_mi the Media Player
 * \return the current state of the media player (playing, paused, ...) \see libvlc_state_t
 */
LIBVLC_API libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *p_mi );

/**
 * How many video outputs does this media player have?
 *
 * \param p_mi the media player
 * \return the number of video outputs
 */
LIBVLC_API unsigned libvlc_media_player_has_vout( libvlc_media_player_t *p_mi );

/**
 * Is this media player seekable?
 *
 * \param p_mi the media player
 * \retval true media player can seek
 * \retval false media player cannot seek
 */
LIBVLC_API bool libvlc_media_player_is_seekable(libvlc_media_player_t *p_mi);

/**
 * Can this media player be paused?
 *
 * \param p_mi the media player
 * \retval true media player can be paused
 * \retval false media player cannot be paused
 */
LIBVLC_API bool libvlc_media_player_can_pause(libvlc_media_player_t *p_mi);

/**
 * Check if the current program is scrambled
 *
 * \param p_mi the media player
 * \retval true current program is scrambled
 * \retval false current program is not scrambled
 *
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API bool libvlc_media_player_program_scrambled( libvlc_media_player_t *p_mi );

/**
 * Display the next frame (if supported)
 *
 * \param p_mi the media player
 */
LIBVLC_API void libvlc_media_player_next_frame( libvlc_media_player_t *p_mi );

/**
 * Navigate through DVD Menu
 *
 * \param p_mi the Media Player
 * \param navigate the Navigation mode
 * \version libVLC 2.0.0 or later
 */
LIBVLC_API void libvlc_media_player_navigate( libvlc_media_player_t* p_mi,
                                              unsigned navigate );

/**
 * Set if, and how, the video title will be shown when media is played.
 *
 * \param p_mi the media player
 * \param position position at which to display the title, or libvlc_position_disable to prevent the title from being displayed
 * \param timeout title display timeout in milliseconds (ignored if libvlc_position_disable)
 * \version libVLC 2.1.0 or later
 */
LIBVLC_API void libvlc_media_player_set_video_title_display( libvlc_media_player_t *p_mi, libvlc_position_t position, unsigned int timeout );

/**
 * Get the track list for one type
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \note You need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.  Not doing this will result in
 * an empty list.
 *
 * \note This track list is a snapshot of the current tracks when this function
 * is called. If a track is updated after this call, the user will need to call
 * this function again to get the updated track.
 *
 *
 * The track list can be used to get track informations and to select specific
 * tracks.
 *
 * \param p_mi the media player
 * \param type type of the track list to request
 *
 * \return a valid libvlc_media_tracklist_t or NULL in case of error, if there
 * is no track for a category, the returned list will have a size of 0, delete
 * with libvlc_media_tracklist_delete()
 */
LIBVLC_API libvlc_media_tracklist_t *
libvlc_media_player_get_tracklist( libvlc_media_player_t *p_mi,
                                   libvlc_track_type_t type );


/**
 * Get the selected track for one type
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \warning More than one tracks can be selected for one type. In that case,
 * libvlc_media_player_get_tracklist() should be used.
 *
 * \param p_mi the media player
 * \param type type of the selected track
 *
 * \return a valid track or NULL if there is no selected tracks for this type,
 * release it with libvlc_media_track_release().
 */
LIBVLC_API libvlc_media_track_t *
libvlc_media_player_get_selected_track( libvlc_media_player_t *p_mi,
                                        libvlc_track_type_t type );

/*
 * Get a track from a track id
 *
 * \version LibVLC 4.0.0 and later.
 *
 * This function can be used to get the last updated informations of a track.
 *
 * \param p_mi the media player
 * \param psz_id valid string representing a track id (cf. psz_id from \ref
 * libvlc_media_track_t)
 *
 * \return a valid track or NULL if there is currently no tracks identified by
 * the string id, release it with libvlc_media_track_release().
 */
LIBVLC_API libvlc_media_track_t *
libvlc_media_player_get_track_from_id( libvlc_media_player_t *p_mi,
                                       const char *psz_id );


/**
 * Select a track or unselect all tracks for one type
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \note Use libvlc_media_player_select_tracks() for multiple selection
 *
 * \param p_mi the media player
 * \param type type of the selected track
 * \param track track to select or NULL to unselect all tracks of for this type
 */
LIBVLC_API void
libvlc_media_player_select_track( libvlc_media_player_t *p_mi,
                                  libvlc_track_type_t type,
                                  const libvlc_media_track_t *track );

/**
 * Select multiple tracks for one type
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \note The internal track list can change between the calls of
 * libvlc_media_player_get_tracklist() and
 * libvlc_media_player_set_tracks(). If a track selection change but the
 * track is not present anymore, the player will just ignore it.
 *
 * \note selecting multiple audio tracks is currently not supported.
 *
 * \param p_mi the media player
 * \param type type of the selected track
 * \param tracks pointer to the track array
 * \param track_count number of tracks in the track array
 */
LIBVLC_API void
libvlc_media_player_select_tracks( libvlc_media_player_t *p_mi,
                                   libvlc_track_type_t type,
                                   const libvlc_media_track_t **tracks,
                                   size_t track_count );

/**
 * Select tracks by their string identifier
 *
 * \version LibVLC 4.0.0 and later.
 *
 * This function can be used pre-select a list of tracks before starting the
 * player. It has only effect for the current media. It can also be used when
 * the player is already started.
 *
 * 'str_ids' can contain more than one track id, delimited with ','. "" or any
 * invalid track id will cause the player to unselect all tracks of that
 * category. NULL will disable the preference for newer tracks without
 * unselecting any current tracks.
 *
 * Example:
 * - (libvlc_track_video, "video/1,video/2") will select these 2 video tracks.
 * If there is only one video track with the id "video/0", no tracks will be
 * selected.
 * - (libvlc_track_type_t, "${slave_url_md5sum}/spu/0) will select one spu
 * added by an input slave with the corresponding url.
 *
 * \note The string identifier of a track can be found via psz_id from \ref
 * libvlc_media_track_t
 *
 * \note selecting multiple audio tracks is currently not supported.
 *
 * \param p_mi the media player
 * \param type type to select
 * \param psz_ids list of string identifier or NULL
 */
LIBVLC_API void
libvlc_media_player_select_tracks_by_ids( libvlc_media_player_t *p_mi,
                                          libvlc_track_type_t type,
                                          const char *psz_ids );

/**
 * Add a slave to the current media player.
 *
 * \note If the player is playing, the slave will be added directly. This call
 * will also update the slave list of the attached libvlc_media_t.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \see libvlc_media_slaves_add
 *
 * \param p_mi the media player
 * \param i_type subtitle or audio
 * \param psz_uri Uri of the slave (should contain a valid scheme).
 * \param b_select True if this slave should be selected when it's loaded
 *
 * \return 0 on success, -1 on error.
 */
LIBVLC_API
int libvlc_media_player_add_slave( libvlc_media_player_t *p_mi,
                                   libvlc_media_slave_type_t i_type,
                                   const char *psz_uri, bool b_select );

/**
 * Release (free) libvlc_track_description_t
 *
 * \param p_track_description the structure to release
 */
LIBVLC_API void libvlc_track_description_list_release( libvlc_track_description_t *p_track_description );

/** \defgroup libvlc_video LibVLC video controls
 * @{
 */

/**
 * Toggle fullscreen status on non-embedded video outputs.
 *
 * @warning The same limitations applies to this function
 * as to libvlc_set_fullscreen().
 *
 * \param p_mi the media player
 */
LIBVLC_API void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi );

/**
 * Enable or disable fullscreen.
 *
 * @warning With most window managers, only a top-level windows can be in
 * full-screen mode. Hence, this function will not operate properly if
 * libvlc_media_player_set_xwindow() was used to embed the video in a
 * non-top-level window. In that case, the embedding window must be reparented
 * to the root window <b>before</b> fullscreen mode is enabled. You will want
 * to reparent it back to its normal parent when disabling fullscreen.
 *
 * \note This setting applies to any and all current or future active video
 * tracks and windows for the given media player. The choice of fullscreen
 * output for each window is left to the operating system.
 *
 * \param p_mi the media player
 * \param b_fullscreen boolean for fullscreen status
 */
LIBVLC_API void libvlc_set_fullscreen(libvlc_media_player_t *p_mi, bool b_fullscreen);

/**
 * Get current fullscreen status.
 *
 * \param p_mi the media player
 * \return the fullscreen status (boolean)
 *
 * \retval false media player is windowed
 * \retval true media player is in fullscreen mode
 */
LIBVLC_API bool libvlc_get_fullscreen( libvlc_media_player_t *p_mi );

/**
 * Enable or disable key press events handling, according to the LibVLC hotkeys
 * configuration. By default and for historical reasons, keyboard events are
 * handled by the LibVLC video widget.
 *
 * \note On X11, there can be only one subscriber for key press and mouse
 * click events per window. If your application has subscribed to those events
 * for the X window ID of the video widget, then LibVLC will not be able to
 * handle key presses and mouse clicks in any case.
 *
 * \warning This function is only implemented for X11 and Win32 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle key press events, false to ignore them.
 */
LIBVLC_API
void libvlc_video_set_key_input( libvlc_media_player_t *p_mi, unsigned on );

/**
 * Enable or disable mouse click events handling. By default, those events are
 * handled. This is needed for DVD menus to work, as well as a few video
 * filters such as "puzzle".
 *
 * \see libvlc_video_set_key_input().
 *
 * \warning This function is only implemented for X11 and Win32 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle mouse click events, false to ignore them.
 */
LIBVLC_API
void libvlc_video_set_mouse_input( libvlc_media_player_t *p_mi, unsigned on );

/**
 * Get the pixel dimensions of a video.
 *
 * \param p_mi media player
 * \param num number of the video (starting from, and most commonly 0)
 * \param px pointer to get the pixel width [OUT]
 * \param py pointer to get the pixel height [OUT]
 * \return 0 on success, -1 if the specified video does not exist
 */
LIBVLC_API
int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned num,
                           unsigned *px, unsigned *py );

/**
 * Get the mouse pointer coordinates over a video.
 * Coordinates are expressed in terms of the decoded video resolution,
 * <b>not</b> in terms of pixels on the screen/viewport (to get the latter,
 * you can query your windowing system directly).
 *
 * Either of the coordinates may be negative or larger than the corresponding
 * dimension of the video, if the cursor is outside the rendering area.
 *
 * @warning The coordinates may be out-of-date if the pointer is not located
 * on the video rendering area. LibVLC does not track the pointer if it is
 * outside of the video widget.
 *
 * @note LibVLC does not support multiple pointers (it does of course support
 * multiple input devices sharing the same pointer) at the moment.
 *
 * \param p_mi media player
 * \param num number of the video (starting from, and most commonly 0)
 * \param px pointer to get the abscissa [OUT]
 * \param py pointer to get the ordinate [OUT]
 * \return 0 on success, -1 if the specified video does not exist
 */
LIBVLC_API
int libvlc_video_get_cursor( libvlc_media_player_t *p_mi, unsigned num,
                             int *px, int *py );

/**
 * Get the current video scaling factor.
 * See also libvlc_video_set_scale().
 *
 * \param p_mi the media player
 * \return the currently configured zoom factor, or 0. if the video is set
 * to fit to the output window/drawable automatically.
 */
LIBVLC_API float libvlc_video_get_scale( libvlc_media_player_t *p_mi );

/**
 * Set the video scaling factor. That is the ratio of the number of pixels on
 * screen to the number of pixels in the original decoded video in each
 * dimension. Zero is a special value; it will adjust the video to the output
 * window/drawable (in windowed mode) or the entire screen.
 *
 * Note that not all video outputs support scaling.
 *
 * \param p_mi the media player
 * \param f_factor the scaling factor, or zero
 */
LIBVLC_API void libvlc_video_set_scale( libvlc_media_player_t *p_mi, float f_factor );

/**
 * Get current video aspect ratio.
 *
 * \param p_mi the media player
 * \return the video aspect ratio or NULL if unspecified
 * (the result must be released with free() or libvlc_free()).
 */
LIBVLC_API char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi );

/**
 * Set new video aspect ratio.
 *
 * \param p_mi the media player
 * \param psz_aspect new video aspect-ratio or NULL to reset to default
 * \note Invalid aspect ratios are ignored.
 */
LIBVLC_API void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi, const char *psz_aspect );

/**
 * Create a video viewpoint structure.
 *
 * \version LibVLC 3.0.0 and later
 *
 * \return video viewpoint or NULL
 *         (the result must be released with free()).
 */
LIBVLC_API libvlc_video_viewpoint_t *libvlc_video_new_viewpoint(void);

/**
 * Update the video viewpoint information.
 *
 * \note It is safe to call this function before the media player is started.
 *
 * \version LibVLC 3.0.0 and later
 *
 * \param p_mi the media player
 * \param p_viewpoint video viewpoint allocated via libvlc_video_new_viewpoint()
 * \param b_absolute if true replace the old viewpoint with the new one. If
 * false, increase/decrease it.
 * \return -1 in case of error, 0 otherwise
 *
 * \note the values are set asynchronously, it will be used by the next frame displayed.
 */
LIBVLC_API int libvlc_video_update_viewpoint( libvlc_media_player_t *p_mi,
                                              const libvlc_video_viewpoint_t *p_viewpoint,
                                              bool b_absolute);

/**
 * Get current video subtitle.
 *
 * \param p_mi the media player
 * \return the video subtitle selected, or -1 if none
 */
LIBVLC_API int libvlc_video_get_spu( libvlc_media_player_t *p_mi );

/**
 * Get the number of available video subtitles.
 *
 * \param p_mi the media player
 * \return the number of available video subtitles
 */
LIBVLC_API int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video subtitles.
 *
 * \param p_mi the media player
 * \return list containing description of available video subtitles.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi );

/**
 * Set new video subtitle.
 *
 * \param p_mi the media player
 * \param i_spu video subtitle track to select (i_id from track description)
 * \return 0 on success, -1 if out of range
 */
LIBVLC_API int libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu );

/**
 * Get the current subtitle delay. Positive values means subtitles are being
 * displayed later, negative values earlier.
 *
 * \param p_mi media player
 * \return time (in microseconds) the display of subtitles is being delayed
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API int64_t libvlc_video_get_spu_delay( libvlc_media_player_t *p_mi );

/**
 * Set the subtitle text scale.
 *
 * The scale factor is expressed as a percentage of the default size, where
 * 1.0 represents 100 percent.
 *
 * A value of 0.5 would result in text half the normal size, and a value of 2.0
 * would result in text twice the normal size.
 *
 * The minimum acceptable value for the scale factor is 0.1.
 *
 * The maximum is 5.0 (five times normal size).
 *
 * \param p_mi media player
 * \param f_scale scale factor in the range [0.1;5.0] (default: 1.0)
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API void libvlc_video_set_spu_text_scale( libvlc_media_player_t *p_mi, float f_scale );

/**
 * Set the subtitle delay. This affects the timing of when the subtitle will
 * be displayed. Positive values result in subtitles being displayed later,
 * while negative values will result in subtitles being displayed earlier.
 *
 * The subtitle delay will be reset to zero each time the media changes.
 *
 * \param p_mi media player
 * \param i_delay time (in microseconds) the display of subtitles should be delayed
 * \return 0 on success, -1 on error
 * \version LibVLC 2.0.0 or later
 */
LIBVLC_API int libvlc_video_set_spu_delay( libvlc_media_player_t *p_mi, int64_t i_delay );

/**
 * Get the full description of available titles
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_mi the media player
 * \param titles address to store an allocated array of title descriptions
 *        descriptions (must be freed with libvlc_title_descriptions_release()
 *        by the caller) [OUT]
 *
 * \return the number of titles (-1 on error)
 */
LIBVLC_API int libvlc_media_player_get_full_title_descriptions( libvlc_media_player_t *p_mi,
                                                                libvlc_title_description_t ***titles );

/**
 * Release a title description
 *
 * \version LibVLC 3.0.0 and later
 *
 * \param p_titles title description array to release
 * \param i_count number of title descriptions to release
 */
LIBVLC_API
    void libvlc_title_descriptions_release( libvlc_title_description_t **p_titles,
                                            unsigned i_count );

/**
 * Get the full description of available chapters
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_mi the media player
 * \param i_chapters_of_title index of the title to query for chapters (uses current title if set to -1)
 * \param pp_chapters address to store an allocated array of chapter descriptions
 *        descriptions (must be freed with libvlc_chapter_descriptions_release()
 *        by the caller) [OUT]
 *
 * \return the number of chapters (-1 on error)
 */
LIBVLC_API int libvlc_media_player_get_full_chapter_descriptions( libvlc_media_player_t *p_mi,
                                                                  int i_chapters_of_title,
                                                                  libvlc_chapter_description_t *** pp_chapters );

/**
 * Release a chapter description
 *
 * \version LibVLC 3.0.0 and later
 *
 * \param p_chapters chapter description array to release
 * \param i_count number of chapter descriptions to release
 */
LIBVLC_API
void libvlc_chapter_descriptions_release( libvlc_chapter_description_t **p_chapters,
                                          unsigned i_count );

/**
 * Set/unset the video crop ratio.
 *
 * This function forces a crop ratio on any and all video tracks rendered by
 * the media player. If the display aspect ratio of a video does not match the
 * crop ratio, either the top and bottom, or the left and right of the video
 * will be cut out to fit the crop ratio.
 *
 * For instance, a ratio of 1:1 will force the video to a square shape.
 *
 * To disable video crop, set a crop ratio with zero as denominator.
 *
 * A call to this function overrides any previous call to any of
 * libvlc_video_set_crop_ratio(), libvlc_video_set_crop_border() and/or
 * libvlc_video_set_crop_window().
 *
 * \see libvlc_video_set_aspect_ratio()
 *
 * \param mp the media player
 * \param num crop ratio numerator (ignored if denominator is 0)
 * \param den crop ratio denominator (or 0 to unset the crop ratio)
 *
 * \version LibVLC 4.0.0 and later
 */
LIBVLC_API
void libvlc_video_set_crop_ratio(libvlc_media_player_t *mp,
                                 unsigned num, unsigned den);

/**
 * Set the video crop window.
 *
 * This function selects a sub-rectangle of video to show. Any pixels outside
 * the rectangle will not be shown.
 *
 * To unset the video crop window, use libvlc_video_set_crop_ratio() or
 * libvlc_video_set_crop_border().
 *
 * A call to this function overrides any previous call to any of
 * libvlc_video_set_crop_ratio(), libvlc_video_set_crop_border() and/or
 * libvlc_video_set_crop_window().
 *
 * \param mp the media player
 * \param x abscissa (i.e. leftmost sample column offset) of the crop window
 * \param y ordinate (i.e. topmost sample row offset) of the crop window
 * \param width sample width of the crop window (cannot be zero)
 * \param height sample height of the crop window (cannot be zero)
 *
 * \version LibVLC 4.0.0 and later
 */
LIBVLC_API
void libvlc_video_set_crop_window(libvlc_media_player_t *mp,
                                  unsigned x, unsigned y,
                                  unsigned width, unsigned height);

/**
 * Set the video crop borders.
 *
 * This function selects the size of video edges to be cropped out.
 *
 * To unset the video crop borders, set all borders to zero.
 *
 * A call to this function overrides any previous call to any of
 * libvlc_video_set_crop_ratio(), libvlc_video_set_crop_border() and/or
 * libvlc_video_set_crop_window().
 *
 * \param mp the media player
 * \param left number of sample columns to crop on the left
 * \param right number of sample columns to crop on the right
 * \param top number of sample rows to crop on the top
 * \param bottom number of sample rows to corp on the bottom
 *
 * \version LibVLC 4.0.0 and later
 */
LIBVLC_API
void libvlc_video_set_crop_border(libvlc_media_player_t *mp,
                                  unsigned left, unsigned right,
                                  unsigned top, unsigned bottom);

/**
 * Get current teletext page requested or 0 if it's disabled.
 *
 * Teletext is disabled by default, call libvlc_video_set_teletext() to enable
 * it.
 *
 * \param p_mi the media player
 * \return the current teletext page requested.
 */
LIBVLC_API int libvlc_video_get_teletext( libvlc_media_player_t *p_mi );

/**
 * Set new teletext page to retrieve.
 *
 * This function can also be used to send a teletext key.
 *
 * \param p_mi the media player
 * \param i_page teletex page number requested. This value can be 0 to disable
 * teletext, a number in the range ]0;1000[ to show the requested page, or a
 * \ref libvlc_teletext_key_t. 100 is the default teletext page.
 */
LIBVLC_API void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page );

/**
 * Get number of available video tracks.
 *
 * \param p_mi media player
 * \return the number of available video tracks (int)
 */
LIBVLC_API int libvlc_video_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video tracks.
 *
 * \param p_mi media player
 * \return list with description of available video tracks, or NULL on error.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current video track.
 *
 * \param p_mi media player
 * \return the video track ID (int) or -1 if no active input
 */
LIBVLC_API int libvlc_video_get_track( libvlc_media_player_t *p_mi );

/**
 * Set video track.
 *
 * \param p_mi media player
 * \param i_track the track ID (i_id field from track description)
 * \return 0 on success, -1 if out of range
 */
LIBVLC_API
int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Take a snapshot of the current video window.
 *
 * If i_width AND i_height is 0, original size is used.
 * If i_width XOR i_height is 0, original aspect-ratio is preserved.
 *
 * \param p_mi media player instance
 * \param num number of video output (typically 0 for the first/only one)
 * \param psz_filepath the path of a file or a folder to save the screenshot into
 * \param i_width the snapshot's width
 * \param i_height the snapshot's height
 * \return 0 on success, -1 if the video was not found
 */
LIBVLC_API
int libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, unsigned num,
                                const char *psz_filepath, unsigned int i_width,
                                unsigned int i_height );

/**
 * Enable or disable deinterlace filter
 *
 * \param p_mi libvlc media player
 * \param deinterlace state -1: auto (default), 0: disabled, 1: enabled
 * \param psz_mode type of deinterlace filter, NULL for current/default filter
 * \version LibVLC 4.0.0 and later
 */
LIBVLC_API void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi,
                                              int deinterlace,
                                              const char *psz_mode );

/**
 * Get an integer marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_option_t
 */
LIBVLC_API int libvlc_video_get_marquee_int( libvlc_media_player_t *p_mi,
                                                 unsigned option );

/**
 * Enable, disable or set an integer marquee option
 *
 * Setting libvlc_marquee_Enable has the side effect of enabling (arg !0)
 * or disabling (arg 0) the marq filter.
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_option_t
 * \param i_val marq option value
 */
LIBVLC_API void libvlc_video_set_marquee_int( libvlc_media_player_t *p_mi,
                                                  unsigned option, int i_val );

/**
 * Set a marquee string option
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_option_t
 * \param psz_text marq option value
 */
LIBVLC_API void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
                                                     unsigned option, const char *psz_text );

/** option values for libvlc_video_{get,set}_logo_{int,string} */
enum libvlc_video_logo_option_t {
    libvlc_logo_enable,
    libvlc_logo_file,           /**< string argument, "file,d,t;file,d,t;..." */
    libvlc_logo_x,
    libvlc_logo_y,
    libvlc_logo_delay,
    libvlc_logo_repeat,
    libvlc_logo_opacity,
    libvlc_logo_position
};

/**
 * Get integer logo option.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to get, values of libvlc_video_logo_option_t
 */
LIBVLC_API int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
                                              unsigned option );

/**
 * Set logo option as integer. Options that take a different type value
 * are ignored.
 * Passing libvlc_logo_enable as option value has the side effect of
 * starting (arg !0) or stopping (arg 0) the logo filter.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param value logo option value
 */
LIBVLC_API void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                               unsigned option, int value );

/**
 * Set logo option as string. Options that take a different type value
 * are ignored.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param psz_value logo option value
 */
LIBVLC_API void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
                                      unsigned option, const char *psz_value );


/** option values for libvlc_video_{get,set}_adjust_{int,float,bool} */
enum libvlc_video_adjust_option_t {
    libvlc_adjust_Enable = 0,
    libvlc_adjust_Contrast,
    libvlc_adjust_Brightness,
    libvlc_adjust_Hue,
    libvlc_adjust_Saturation,
    libvlc_adjust_Gamma
};

/**
 * Get integer adjust option.
 *
 * \param p_mi libvlc media player instance
 * \param option adjust option to get, values of libvlc_video_adjust_option_t
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API int libvlc_video_get_adjust_int( libvlc_media_player_t *p_mi,
                                                unsigned option );

/**
 * Set adjust option as integer. Options that take a different type value
 * are ignored.
 * Passing libvlc_adjust_enable as option value has the side effect of
 * starting (arg !0) or stopping (arg 0) the adjust filter.
 *
 * \param p_mi libvlc media player instance
 * \param option adust option to set, values of libvlc_video_adjust_option_t
 * \param value adjust option value
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API void libvlc_video_set_adjust_int( libvlc_media_player_t *p_mi,
                                                 unsigned option, int value );

/**
 * Get float adjust option.
 *
 * \param p_mi libvlc media player instance
 * \param option adjust option to get, values of libvlc_video_adjust_option_t
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API float libvlc_video_get_adjust_float( libvlc_media_player_t *p_mi,
                                                    unsigned option );

/**
 * Set adjust option as float. Options that take a different type value
 * are ignored.
 *
 * \param p_mi libvlc media player instance
 * \param option adust option to set, values of libvlc_video_adjust_option_t
 * \param value adjust option value
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API void libvlc_video_set_adjust_float( libvlc_media_player_t *p_mi,
                                                   unsigned option, float value );

/** @} video */

/** \defgroup libvlc_audio LibVLC audio controls
 * @{
 */

/**
 * Audio channels
 */
typedef enum libvlc_audio_output_channel_t {
    libvlc_AudioChannel_Error   = -1,
    libvlc_AudioChannel_Stereo  =  1,
    libvlc_AudioChannel_RStereo =  2,
    libvlc_AudioChannel_Left    =  3,
    libvlc_AudioChannel_Right   =  4,
    libvlc_AudioChannel_Dolbys  =  5
} libvlc_audio_output_channel_t;


/**
 * Gets the list of available audio output modules.
 *
 * \param p_instance libvlc instance
 * \return list of available audio outputs. It must be freed with
*          \see libvlc_audio_output_list_release \see libvlc_audio_output_t .
 *         In case of error, NULL is returned.
 */
LIBVLC_API libvlc_audio_output_t *
libvlc_audio_output_list_get( libvlc_instance_t *p_instance );

/**
 * Frees the list of available audio output modules.
 *
 * \param p_list list with audio outputs for release
 */
LIBVLC_API
void libvlc_audio_output_list_release( libvlc_audio_output_t *p_list );

/**
 * Selects an audio output module.
 * \note Any change will take be effect only after playback is stopped and
 * restarted. Audio output cannot be changed while playing.
 *
 * \param p_mi media player
 * \param psz_name name of audio output,
 *               use psz_name of \see libvlc_audio_output_t
 * \return 0 if function succeeded, -1 on error
 */
LIBVLC_API int libvlc_audio_output_set( libvlc_media_player_t *p_mi,
                                        const char *psz_name );

/**
 * Gets a list of potential audio output devices,
 * \see libvlc_audio_output_device_set().
 *
 * \note Not all audio outputs support enumerating devices.
 * The audio output may be functional even if the list is empty (NULL).
 *
 * \note The list may not be exhaustive.
 *
 * \warning Some audio output devices in the list might not actually work in
 * some circumstances. By default, it is recommended to not specify any
 * explicit audio device.
 *
 * \param mp media player
 * \return A NULL-terminated linked list of potential audio output devices.
 * It must be freed with libvlc_audio_output_device_list_release()
 * \version LibVLC 2.2.0 or later.
 */
LIBVLC_API libvlc_audio_output_device_t *
libvlc_audio_output_device_enum( libvlc_media_player_t *mp );

/**
 * Gets a list of audio output devices for a given audio output module,
 * \see libvlc_audio_output_device_set().
 *
 * \note Not all audio outputs support this. In particular, an empty (NULL)
 * list of devices does <b>not</b> imply that the specified audio output does
 * not work.
 *
 * \note The list might not be exhaustive.
 *
 * \warning Some audio output devices in the list might not actually work in
 * some circumstances. By default, it is recommended to not specify any
 * explicit audio device.
 *
 * \param p_instance libvlc instance
 * \param aout audio output name
 *                 (as returned by libvlc_audio_output_list_get())
 * \return A NULL-terminated linked list of potential audio output devices.
 * It must be freed with libvlc_audio_output_device_list_release()
 * \version LibVLC 2.1.0 or later.
 */
LIBVLC_API libvlc_audio_output_device_t *
libvlc_audio_output_device_list_get( libvlc_instance_t *p_instance,
                                     const char *aout );

/**
 * Frees a list of available audio output devices.
 *
 * \param p_list list with audio outputs for release
 * \version LibVLC 2.1.0 or later.
 */
LIBVLC_API void libvlc_audio_output_device_list_release(
                                        libvlc_audio_output_device_t *p_list );

/**
 * Configures an explicit audio output device.
 *
 * If the module paramater is NULL, audio output will be moved to the device
 * specified by the device identifier string immediately. This is the
 * recommended usage.
 *
 * A list of adequate potential device strings can be obtained with
 * libvlc_audio_output_device_enum().
 *
 * However passing NULL is supported in LibVLC version 2.2.0 and later only;
 * in earlier versions, this function would have no effects when the module
 * parameter was NULL.
 *
 * If the module parameter is not NULL, the device parameter of the
 * corresponding audio output, if it exists, will be set to the specified
 * string. Note that some audio output modules do not have such a parameter
 * (notably MMDevice and PulseAudio).
 *
 * A list of adequate potential device strings can be obtained with
 * libvlc_audio_output_device_list_get().
 *
 * \note This function does not select the specified audio output plugin.
 * libvlc_audio_output_set() is used for that purpose.
 *
 * \warning The syntax for the device parameter depends on the audio output.
 *
 * Some audio output modules require further parameters (e.g. a channels map
 * in the case of ALSA).
 *
 * \param mp media player
 * \param module If NULL, current audio output module.
 *               if non-NULL, name of audio output module
                 (\see libvlc_audio_output_t)
 * \param device_id device identifier string
 * \return Nothing. Errors are ignored (this is a design bug).
 */
LIBVLC_API void libvlc_audio_output_device_set( libvlc_media_player_t *mp,
                                                const char *module,
                                                const char *device_id );

/**
 * Get the current audio output device identifier.
 *
 * This complements libvlc_audio_output_device_set().
 *
 * \warning The initial value for the current audio output device identifier
 * may not be set or may be some unknown value. A LibVLC application should
 * compare this value against the known device identifiers (e.g. those that
 * were previously retrieved by a call to libvlc_audio_output_device_enum or
 * libvlc_audio_output_device_list_get) to find the current audio output device.
 *
 * It is possible that the selected audio output device changes (an external
 * change) without a call to libvlc_audio_output_device_set. That may make this
 * method unsuitable to use if a LibVLC application is attempting to track
 * dynamic audio device changes as they happen.
 *
 * \param mp media player
 * \return the current audio output device identifier
 *         NULL if no device is selected or in case of error
 *         (the result must be released with free()).
 * \version LibVLC 3.0.0 or later.
 */
LIBVLC_API char *libvlc_audio_output_device_get( libvlc_media_player_t *mp );

/**
 * Toggle mute status.
 *
 * \param p_mi media player
 * \warning Toggling mute atomically is not always possible: On some platforms,
 * other processes can mute the VLC audio playback stream asynchronously. Thus,
 * there is a small race condition where toggling will not work.
 * See also the limitations of libvlc_audio_set_mute().
 */
LIBVLC_API void libvlc_audio_toggle_mute( libvlc_media_player_t *p_mi );

/**
 * Get current mute status.
 *
 * \param p_mi media player
 * \return the mute status (boolean) if defined, -1 if undefined/unapplicable
 */
LIBVLC_API int libvlc_audio_get_mute( libvlc_media_player_t *p_mi );

/**
 * Set mute status.
 *
 * \param p_mi media player
 * \param status If status is true then mute, otherwise unmute
 * \warning This function does not always work. If there are no active audio
 * playback stream, the mute status might not be available. If digital
 * pass-through (S/PDIF, HDMI...) is in use, muting may be unapplicable. Also
 * some audio output plugins do not support muting at all.
 * \note To force silent playback, disable all audio tracks. This is more
 * efficient and reliable than mute.
 */
LIBVLC_API void libvlc_audio_set_mute( libvlc_media_player_t *p_mi, int status );

/**
 * Get current software audio volume.
 *
 * \param p_mi media player
 * \return the software volume in percents
 * (0 = mute, 100 = nominal / 0dB)
 */
LIBVLC_API int libvlc_audio_get_volume( libvlc_media_player_t *p_mi );

/**
 * Set current software audio volume.
 *
 * \param p_mi media player
 * \param i_volume the volume in percents (0 = mute, 100 = 0dB)
 * \return 0 if the volume was set, -1 if it was out of range
 */
LIBVLC_API int libvlc_audio_set_volume( libvlc_media_player_t *p_mi, int i_volume );

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media player
 * \return the number of available audio tracks (int), or -1 if unavailable
 */
LIBVLC_API int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available audio tracks.
 *
 * \param p_mi media player
 * \return list with description of available audio tracks, or NULL.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current audio track.
 *
 * \param p_mi media player
 * \return the audio track ID or -1 if no active input.
 */
LIBVLC_API int libvlc_audio_get_track( libvlc_media_player_t *p_mi );

/**
 * Set current audio track.
 *
 * \param p_mi media player
 * \param i_track the track ID (i_id field from track description)
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Get current audio channel.
 *
 * \param p_mi media player
 * \return the audio channel \see libvlc_audio_output_channel_t
 */
LIBVLC_API int libvlc_audio_get_channel( libvlc_media_player_t *p_mi );

/**
 * Set current audio channel.
 *
 * \param p_mi media player
 * \param channel the audio channel, \see libvlc_audio_output_channel_t
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_audio_set_channel( libvlc_media_player_t *p_mi, int channel );

/**
 * Get current audio delay.
 *
 * \param p_mi media player
 * \return the audio delay (microseconds)
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API int64_t libvlc_audio_get_delay( libvlc_media_player_t *p_mi );

/**
 * Set current audio delay. The audio delay will be reset to zero each time the media changes.
 *
 * \param p_mi media player
 * \param i_delay the audio delay (microseconds)
 * \return 0 on success, -1 on error
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API int libvlc_audio_set_delay( libvlc_media_player_t *p_mi, int64_t i_delay );

/**
 * Get the number of equalizer presets.
 *
 * \return number of presets
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API unsigned libvlc_audio_equalizer_get_preset_count( void );

/**
 * Get the name of a particular equalizer preset.
 *
 * This name can be used, for example, to prepare a preset label or menu in a user
 * interface.
 *
 * \param u_index index of the preset, counting from zero
 * \return preset name, or NULL if there is no such preset
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API const char *libvlc_audio_equalizer_get_preset_name( unsigned u_index );

/**
 * Get the number of distinct frequency bands for an equalizer.
 *
 * \return number of frequency bands
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API unsigned libvlc_audio_equalizer_get_band_count( void );

/**
 * Get a particular equalizer band frequency.
 *
 * This value can be used, for example, to create a label for an equalizer band control
 * in a user interface.
 *
 * \param u_index index of the band, counting from zero
 * \return equalizer band frequency (Hz), or -1 if there is no such band
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API float libvlc_audio_equalizer_get_band_frequency( unsigned u_index );

/**
 * Create a new default equalizer, with all frequency values zeroed.
 *
 * The new equalizer can subsequently be applied to a media player by invoking
 * libvlc_media_player_set_equalizer().
 *
 * The returned handle should be freed via libvlc_audio_equalizer_release() when
 * it is no longer needed.
 *
 * \return opaque equalizer handle, or NULL on error
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API libvlc_equalizer_t *libvlc_audio_equalizer_new( void );

/**
 * Create a new equalizer, with initial frequency values copied from an existing
 * preset.
 *
 * The new equalizer can subsequently be applied to a media player by invoking
 * libvlc_media_player_set_equalizer().
 *
 * The returned handle should be freed via libvlc_audio_equalizer_release() when
 * it is no longer needed.
 *
 * \param u_index index of the preset, counting from zero
 * \return opaque equalizer handle, or NULL on error
 *         (it must be released with libvlc_audio_equalizer_release())
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API libvlc_equalizer_t *libvlc_audio_equalizer_new_from_preset( unsigned u_index );

/**
 * Release a previously created equalizer instance.
 *
 * The equalizer was previously created by using libvlc_audio_equalizer_new() or
 * libvlc_audio_equalizer_new_from_preset().
 *
 * It is safe to invoke this method with a NULL p_equalizer parameter for no effect.
 *
 * \param p_equalizer opaque equalizer handle, or NULL
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API void libvlc_audio_equalizer_release( libvlc_equalizer_t *p_equalizer );

/**
 * Set a new pre-amplification value for an equalizer.
 *
 * The new equalizer settings are subsequently applied to a media player by invoking
 * libvlc_media_player_set_equalizer().
 *
 * The supplied amplification value will be clamped to the -20.0 to +20.0 range.
 *
 * \param p_equalizer valid equalizer handle, must not be NULL
 * \param f_preamp preamp value (-20.0 to 20.0 Hz)
 * \return zero on success, -1 on error
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API int libvlc_audio_equalizer_set_preamp( libvlc_equalizer_t *p_equalizer, float f_preamp );

/**
 * Get the current pre-amplification value from an equalizer.
 *
 * \param p_equalizer valid equalizer handle, must not be NULL
 * \return preamp value (Hz)
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API float libvlc_audio_equalizer_get_preamp( libvlc_equalizer_t *p_equalizer );

/**
 * Set a new amplification value for a particular equalizer frequency band.
 *
 * The new equalizer settings are subsequently applied to a media player by invoking
 * libvlc_media_player_set_equalizer().
 *
 * The supplied amplification value will be clamped to the -20.0 to +20.0 range.
 *
 * \param p_equalizer valid equalizer handle, must not be NULL
 * \param f_amp amplification value (-20.0 to 20.0 Hz)
 * \param u_band index, counting from zero, of the frequency band to set
 * \return zero on success, -1 on error
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API int libvlc_audio_equalizer_set_amp_at_index( libvlc_equalizer_t *p_equalizer, float f_amp, unsigned u_band );

/**
 * Get the amplification value for a particular equalizer frequency band.
 *
 * \param p_equalizer valid equalizer handle, must not be NULL
 * \param u_band index, counting from zero, of the frequency band to get
 * \return amplification value (Hz); NaN if there is no such frequency band
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API float libvlc_audio_equalizer_get_amp_at_index( libvlc_equalizer_t *p_equalizer, unsigned u_band );

/**
 * Apply new equalizer settings to a media player.
 *
 * The equalizer is first created by invoking libvlc_audio_equalizer_new() or
 * libvlc_audio_equalizer_new_from_preset().
 *
 * It is possible to apply new equalizer settings to a media player whether the media
 * player is currently playing media or not.
 *
 * Invoking this method will immediately apply the new equalizer settings to the audio
 * output of the currently playing media if there is any.
 *
 * If there is no currently playing media, the new equalizer settings will be applied
 * later if and when new media is played.
 *
 * Equalizer settings will automatically be applied to subsequently played media.
 *
 * To disable the equalizer for a media player invoke this method passing NULL for the
 * p_equalizer parameter.
 *
 * The media player does not keep a reference to the supplied equalizer so it is safe
 * for an application to release the equalizer reference any time after this method
 * returns.
 *
 * \param p_mi opaque media player handle
 * \param p_equalizer opaque equalizer handle, or NULL to disable the equalizer for this media player
 * \return zero on success, -1 on error
 * \version LibVLC 2.2.0 or later
 */
LIBVLC_API int libvlc_media_player_set_equalizer( libvlc_media_player_t *p_mi, libvlc_equalizer_t *p_equalizer );

/**
 * Media player roles.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * See \ref libvlc_media_player_set_role()
 */
typedef enum libvlc_media_player_role {
    libvlc_role_None = 0, /**< Don't use a media player role */
    libvlc_role_Music,   /**< Music (or radio) playback */
    libvlc_role_Video, /**< Video playback */
    libvlc_role_Communication, /**< Speech, real-time communication */
    libvlc_role_Game, /**< Video game */
    libvlc_role_Notification, /**< User interaction feedback */
    libvlc_role_Animation, /**< Embedded animation (e.g. in web page) */
    libvlc_role_Production, /**< Audio editting/production */
    libvlc_role_Accessibility, /**< Accessibility */
    libvlc_role_Test /** Testing */
#define libvlc_role_Last libvlc_role_Test
} libvlc_media_player_role_t;

/**
 * Gets the media role.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_mi media player
 * \return the media player role (\ref libvlc_media_player_role_t)
 */
LIBVLC_API int libvlc_media_player_get_role(libvlc_media_player_t *p_mi);

/**
 * Sets the media role.
 *
 * \param p_mi media player
 * \param role the media player role (\ref libvlc_media_player_role_t)
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_media_player_set_role(libvlc_media_player_t *p_mi,
                                            unsigned role);

/** @} audio */

/** @} media_player */

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_PLAYER_H */
