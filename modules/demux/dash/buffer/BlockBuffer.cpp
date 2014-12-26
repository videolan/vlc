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

BlockBuffer::BlockBuffer    () :
             sizeBytes      (0),
             isEOF          (false)

{
    fifo = block_FifoNew();
    if(!fifo)
        throw VLC_ENOMEM;
}
BlockBuffer::~BlockBuffer   ()
{
    block_FifoRelease(fifo);
}

int BlockBuffer::peek(const uint8_t **pp_peek, unsigned int len)
{
    block_t *p_block = block_FifoShow(fifo);
    if(!p_block)
        return 0;

    *pp_peek = p_block->p_buffer;
    return __MIN(len, p_block->i_buffer);
}

block_t * BlockBuffer::get()
{
    block_t *p_block = block_FifoGet(fifo);
    if(p_block)
        notify();
    return p_block;
}

void BlockBuffer::put(block_t *block)
{
    block_FifoPut(fifo, block);
    notify();
}

void BlockBuffer::setEOF(bool value)
{
    isEOF = value;
    block_FifoWake(fifo);
}

bool BlockBuffer::getEOF()
{
    return isEOF;
}

void    BlockBuffer::attach               (IBufferObserver *observer)
{
    this->bufferObservers.push_back(observer);
}
void    BlockBuffer::notify               ()
{
//    for(size_t i = 0; i < this->bufferObservers.size(); i++)
//        this->bufferObservers.at(i)->bufferLevelChanged(this->sizeMicroSec, ((float)this->sizeMicroSec / this->capacityMicroSec) * 100);
}
