/*****************************************************************************
 * mmdevice.h : Windows Multimedia Device API audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#ifndef VLC_AOUT_MMDEVICE_H
# define VLC_AOUT_MMDEVICE_H 1

typedef struct aout_api aout_api_t;

/**
 * Audio output simplified API for Windows
 */
struct aout_api
{
    VLC_COMMON_MEMBERS
    void *sys;

    HRESULT (*time_get)(aout_api_t *, mtime_t *);
    HRESULT (*play)(aout_api_t *, block_t *);
    HRESULT (*pause)(aout_api_t *, bool);
    HRESULT (*flush)(aout_api_t *);
};

/**
 * Creates an audio output stream on a given Windows multimedia device.
 * \param parent parent VLC object
 * \param fmt audio output sample format [IN/OUT]
 * \param dev audio output device
 * \param sid audio output session GUID [IN]
 */
aout_api_t *aout_api_Start(vlc_object_t *parent, audio_sample_format_t *fmt,
                           IMMDevice *dev, const GUID *sid);
#define aout_api_Start(o,f,d,s) aout_api_Start(VLC_OBJECT(o),f,d,s)

/**
 * Destroys an audio output stream.
 */
void aout_api_Stop(aout_api_t *);

static inline HRESULT aout_api_TimeGet(aout_api_t *api, mtime_t *delay)
{
    return (api->time_get)(api, delay);
}

static inline HRESULT aout_api_Play(aout_api_t *api, block_t *block)
{
    return (api->play)(api, block);
}

static inline HRESULT aout_api_Pause(aout_api_t *api, bool paused)
{
    return (api->pause)(api, paused);
}

static inline HRESULT aout_api_Flush(aout_api_t *api)
{
    return (api->flush)(api);
}
#endif
