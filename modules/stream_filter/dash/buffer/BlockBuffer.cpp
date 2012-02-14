/*
 * BlockBuffer.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    this->capacityMicroSec  = var_InheritInteger(stream, "dash-bufferSize") * 1000000;

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
    std::cout << "Delete buffer" << std::endl;

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

    std::cout << "Peek Bytes: " << ret << " from buffer length: " << this->sizeBytes << std::endl;

    block_PeekBytes(&this->buffer, this->peekBlock->p_buffer, ret);
    *pp_peek = this->peekBlock->p_buffer;

    std::cout << "Buffer length Sec: " << this->sizeMicroSec << " Bytes: " << this->sizeBytes<< std::endl;
    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
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

    this->reduceBufferMilliSec(ret);

    std::cout << "Get Bytes: " << ret << " from buffer length: " << this->sizeBytes << std::endl;

    block_GetBytes(&this->buffer, (uint8_t *)p_data, ret);
    block_BytestreamFlush(&this->buffer);

    std::cout << "Buffer length: " << this->sizeMicroSec << " Bytes: " << this->sizeBytes << std::endl;

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

    std::cout << "Put MilliSec: " << block->i_length << " Bytes: " << block->i_buffer << " into buffer" << std::endl;
    this->sizeMicroSec   += block->i_length;
    this->sizeBytes += block->i_buffer;

    block_BytestreamPush(&this->buffer, block);

    std::cout << "Buffer length: " << this->sizeMicroSec << " Bytes: " << this->sizeBytes << std::endl;
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
    vlc_mutex_lock(&this->monitorMutex);
    bool ret = this->isEOF;
    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
}
void    BlockBuffer::attach               (IBufferObserver *observer)
{
    this->bufferObservers.push_back(observer);
}
void    BlockBuffer::notify               ()
{
    for(size_t i = 0; i < this->bufferObservers.size(); i++)
        this->bufferObservers.at(i)->bufferLevelChanged(this->sizeMicroSec, this->sizeMicroSec / this->capacityMicroSec);
}
void    BlockBuffer::reduceBufferMilliSec (size_t bytes)
{
    size_t  pos      = 0;
    float   microsec = 0;

    block_t *block = this->buffer.p_block;

    if(bytes < (block->i_buffer - this->buffer.i_offset))
    {
        pos = bytes;
        microsec = ((float)block->i_length / block->i_buffer) * bytes;
    }
    else
    {
        pos = block->i_buffer - this->buffer.i_offset;
        microsec = ((float)block->i_length / block->i_buffer) * (block->i_buffer - this->buffer.i_offset);
    }

    while(pos < bytes)
    {
        block = block->p_next;
        if((bytes - pos) < (block->i_buffer - this->buffer.i_offset))
        {
            pos = bytes;
            microsec += ((float)block->i_length / block->i_buffer) * (bytes - pos);
        }
        else
        {
            pos += block->i_buffer;
            microsec += block->i_length;
        }
    }

    std::cout << "Reduce: " << microsec << std::endl;
    this->sizeMicroSec  -= microsec;
    this->sizeBytes     -= bytes;

    if(this->sizeMicroSec < 0)
        this->sizeMicroSec = 0;

    if(this->sizeBytes == 0)
        this->sizeMicroSec = 0;
}
mtime_t BlockBuffer::size                 ()
{
    vlc_mutex_lock(&this->monitorMutex);
    mtime_t ret = this->sizeMicroSec;
    vlc_mutex_unlock(&this->monitorMutex);
    return ret;
}
