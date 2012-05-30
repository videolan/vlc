/*
 * BlockBuffer.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include "buffer/BlockBuffer.h"

using namespace dash::buffer;

BlockBuffer::BlockBuffer    (stream_t *stream) :
             sizeMicroSec   (0),
             sizeBytes      (0),
             stream         (stream),
             isEOF          (false)

{
    this->capacityMicroSec  = var_InheritInteger(stream, "dash-buffersize") * 1000000;

    if(this->capacityMicroSec <= 0)
        this->capacityMicroSec = DEFAULTBUFFERLENGTH;

    this->peekBlock = block_Alloc(INTIALPEEKSIZE);

    block_BytestreamInit(&this->buffer);
    vlc_mutex_init(&this->monitorMutex);
    vlc_cond_init(&this->empty);
    vlc_cond_init(&this->full);
}
BlockBuffer::~BlockBuffer   ()
{
    block_Release(this->peekBlock);

    block_BytestreamRelease(&this->buffer);
    vlc_mutex_destroy(&this->monitorMutex);
    vlc_cond_destroy(&this->empty);
    vlc_cond_destroy(&this->full);
}

int     BlockBuffer::peek                 (const uint8_t **pp_peek, unsigned int len)
{
    vlc_mutex_lock(&this->monitorMutex);

    while(this->sizeBytes == 0 && !this->isEOF)
        vlc_cond_wait(&this->full, &this->monitorMutex);

    if(this->sizeBytes == 0)
    {
        vlc_cond_signal(&this->empty);
        vlc_mutex_unlock(&this->monitorMutex);
        return 0;
    }

    size_t ret = len > this->sizeBytes ? this->sizeBytes : len;

    if(ret > this->peekBlock->i_buffer)
        this->peekBlock = block_Realloc(this->peekBlock, 0, ret);

    block_PeekBytes(&this->buffer, this->peekBlock->p_buffer, ret);
    *pp_peek = this->peekBlock->p_buffer;

    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
}

int     BlockBuffer::seekBackwards       (unsigned len)
{
    vlc_mutex_lock(&this->monitorMutex);
    if( this->buffer.i_offset > len )
    {
        this->buffer.i_offset -= len;
        this->sizeBytes += len;
        vlc_mutex_unlock(&this->monitorMutex);
        return VLC_SUCCESS;
    }

    vlc_mutex_unlock(&this->monitorMutex);
    return VLC_EGENERIC;
}

int     BlockBuffer::get                  (void *p_data, unsigned int len)
{
    vlc_mutex_lock(&this->monitorMutex);

    while(this->sizeBytes == 0 && !this->isEOF)
        vlc_cond_wait(&this->full, &this->monitorMutex);

    if(this->sizeBytes == 0)
    {
        vlc_cond_signal(&this->empty);
        vlc_mutex_unlock(&this->monitorMutex);
        return 0;
    }

    int ret = len > this->sizeBytes ? this->sizeBytes : len;

    if(p_data == NULL)
        block_SkipBytes(&this->buffer, ret);
    else
        block_GetBytes(&this->buffer, (uint8_t *)p_data, ret);

    block_BytestreamFlush(&this->buffer);
    this->updateBufferSize(ret);

    this->notify();

    vlc_cond_signal(&this->empty);
    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
}
void    BlockBuffer::put                  (block_t *block)
{
    vlc_mutex_lock(&this->monitorMutex);

    while(this->sizeMicroSec >= this->capacityMicroSec && !this->isEOF)
        vlc_cond_wait(&this->empty, &this->monitorMutex);

    if(this->isEOF)
    {
        vlc_cond_signal(&this->full);
        vlc_mutex_unlock(&this->monitorMutex);
        return;
    }

    this->sizeMicroSec  += block->i_length;
    this->sizeBytes     += block->i_buffer;

    block_BytestreamPush(&this->buffer, block);
    this->notify();

    vlc_cond_signal(&this->full);
    vlc_mutex_unlock(&this->monitorMutex);
}
void    BlockBuffer::setEOF               (bool value)
{
    vlc_mutex_lock(&this->monitorMutex);
    this->isEOF = value;
    vlc_cond_signal(&this->empty);
    vlc_cond_signal(&this->full);
    vlc_mutex_unlock(&this->monitorMutex);
}
bool    BlockBuffer::getEOF               ()
{
    vlc_mutex_locker    lock(&this->monitorMutex);

    return this->isEOF;
}
void    BlockBuffer::attach               (IBufferObserver *observer)
{
    this->bufferObservers.push_back(observer);
}
void    BlockBuffer::notify               ()
{
    for(size_t i = 0; i < this->bufferObservers.size(); i++)
        this->bufferObservers.at(i)->bufferLevelChanged(this->sizeMicroSec, ((float)this->sizeMicroSec / this->capacityMicroSec) * 100);
}
void    BlockBuffer::updateBufferSize     (size_t bytes)
{
    block_t *block = this->buffer.p_block;

    this->sizeMicroSec = 0;

    while(block)
    {
        this->sizeMicroSec += block->i_length;
        block = block->p_next;
    }

    this->sizeBytes -= bytes;
}
mtime_t BlockBuffer::size                 ()
{
    vlc_mutex_lock(&this->monitorMutex);
    mtime_t ret = this->sizeMicroSec;
    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
}
