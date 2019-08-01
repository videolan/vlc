/*
 * SourceStream.hpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#ifndef SOURCESTREAM_HPP
#define SOURCESTREAM_HPP

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <string>

namespace adaptive
{
    class AbstractSource;

    class AbstractSourceStream
    {
        public:
            virtual ~AbstractSourceStream() {}
            virtual stream_t *makeStream() = 0;
            virtual void Reset() = 0;
            virtual size_t Peek(const uint8_t **, size_t) = 0;
    };

    class AbstractChunksSourceStream : public AbstractSourceStream
    {
        public:
            AbstractChunksSourceStream(vlc_object_t *, AbstractSource *);
            virtual ~AbstractChunksSourceStream();
            virtual void Reset(); /* impl */
            virtual stream_t *makeStream(); /* impl */

        protected:
            virtual ssize_t Read(uint8_t *, size_t) = 0;
            virtual int     Seek(uint64_t) = 0;
            virtual std::string getContentType() = 0;
            bool b_eof;
            vlc_object_t *p_obj;
            AbstractSource *source;

        private:
            static ssize_t read_Callback(stream_t *, void *, size_t);
            static int seek_Callback(stream_t *, uint64_t);
            static int control_Callback( stream_t *, int i_query, va_list );
            static void delete_Callback( stream_t * );
    };

    class ChunksSourceStream : public AbstractChunksSourceStream
    {
        public:
            ChunksSourceStream(vlc_object_t *, AbstractSource *);
            virtual ~ChunksSourceStream();
            virtual void Reset(); /* reimpl */

        protected:
            virtual ssize_t Read(uint8_t *, size_t); /* impl */
            virtual int     Seek(uint64_t); /* impl */
            virtual size_t  Peek(const uint8_t **, size_t); /* impl */
            virtual std::string getContentType(); /* impl */

        private:
            block_t *p_block;
    };

    class BufferedChunksSourceStream : public AbstractChunksSourceStream
    {
        public:
            BufferedChunksSourceStream(vlc_object_t *, AbstractSource *);
            virtual ~BufferedChunksSourceStream();
            virtual void Reset(); /* reimpl */

        protected:
            virtual ssize_t Read(uint8_t *, size_t); /* impl */
            virtual int     Seek(uint64_t); /* impl */
            virtual size_t  Peek(const uint8_t **, size_t); /* impl */
            virtual std::string getContentType(); /* impl */

        private:
            void fillByteStream();
            static const int MAX_BACKEND = 5 * 1024 * 1024;
            static const int MIN_BACKEND_CLEANUP = 50 * 1024;
            uint64_t i_global_offset;
            size_t i_bytestream_offset;
            block_bytestream_t bs;
    };
}
#endif // SOURCESTREAM_HPP
