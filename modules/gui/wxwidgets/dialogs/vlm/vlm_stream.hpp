/*****************************************************************************
 * vlm_stream.hpp: Representation of a VLM Stream
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id: vlm_stream.hpp 12502 2005-09-09 19:38:01Z gbazin $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* FIXME : This is not WX-specific and should be moved to core */
#ifndef _VLM_STREAM_H_
#define _VLM_STREAM_H_

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_vlm.h>

#include <string>
using namespace std;

class VLMWrapper;

/**
 * This class encapsulates a VLM Stream and provides additional services
 */
class VLMStream
{
public:
    VLMStream( intf_thread_t *, vlm_media_t * , VLMWrapper *);
    virtual ~VLMStream();

    vlm_media_t   *p_media;


    void Delete();
    virtual void Disable();
    virtual void Enable();

    /* FIXME: provide accessor */
    VLMWrapper    *p_vlm;
protected:
    intf_thread_t *p_intf;
    friend class VLMWrapper;
private:
};

/**
 * This class encapsulates a VLM Broadcast stream
 */
class VLMBroadcastStream : public VLMStream
{
public:
    VLMBroadcastStream( intf_thread_t *, vlm_media_t *, VLMWrapper *);
    virtual ~VLMBroadcastStream();

    void Play();
    void Pause();
    void Stop();

};

/**
 * This class encapsulates a VLM VOD Stream
 */
class VLMVODStream : public VLMStream
{
public:
    VLMVODStream( intf_thread_t *, vlm_media_t *, VLMWrapper *);
    virtual ~VLMVODStream();
};

#endif
