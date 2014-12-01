/*
 * BlockBuffer.h
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

#ifndef BLOCKBUFFER_H_
#define BLOCKBUFFER_H_

#include "buffer/IBufferObserver.h"

#include <vlc_stream.h>
#include <vector>

#define DEFAULTBUFFERLENGTH 30000000
#define INTIALPEEKSIZE      32768

namespace dash
{
    namespace buffer
    {
        class BlockBuffer
        {
            public:
                BlockBuffer           ();
                virtual ~BlockBuffer  ();

                void    put           (block_t *block);
                block_t *get          ();
                int     peek          (const uint8_t **pp_peek, unsigned int i_peek);
                void    setEOF        (bool value);
                bool    getEOF        ();
                void    attach        (IBufferObserver *observer);
                void    notify        ();

            private:
                size_t              sizeBytes;
                bool                isEOF;
                block_fifo_t        *fifo;

                std::vector<IBufferObserver *> bufferObservers;
        };
    }
}

#endif /* BLOCKBUFFER_H_ */
