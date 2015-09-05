/*
 * StreamOutput.hpp
 *****************************************************************************
 * Copyright (C) 2014-2015 VideoLAN and VLC authors
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
#ifndef STREAMOUTPUT_HPP
#define STREAMOUTPUT_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string>
#include <list>
#include <vlc_common.h>
#include <vlc_es.h>
#include "../StreamFormat.hpp"
#include "FakeESOut.hpp"

namespace adaptative
{
    class AbstractStreamOutput
    {
    public:
        AbstractStreamOutput(demux_t *, const StreamFormat &);
        virtual ~AbstractStreamOutput();

        void setLanguage(const std::string &);
        void setDescription(const std::string &);
        const StreamFormat & getStreamFormat() const;
        virtual void pushBlock(block_t *, bool) = 0;
        virtual mtime_t getPCR() const = 0;
        virtual mtime_t getFirstDTS() const = 0;
        virtual int esCount() const = 0;
        virtual bool seekAble() const = 0;
        virtual void setPosition(mtime_t) = 0;
        virtual void sendToDecoder(mtime_t) = 0;
        virtual bool isEmpty() const = 0;
        virtual bool reinitsOnSeek() const = 0;
        virtual bool switchAllowed() const = 0;
        virtual bool isSelected() const = 0;

    protected:
        demux_t  *realdemux;
        std::string language;
        std::string description;

    private:
        StreamFormat format;
    };

    class AbstractStreamOutputFactory
    {
        public:
            virtual ~AbstractStreamOutputFactory() {}
            virtual AbstractStreamOutput *create(demux_t*, const StreamFormat &) const = 0;
    };

    class BaseStreamOutput : public AbstractStreamOutput,
                             public ExtraFMTInfoInterface
    {
        friend class BaseStreamOutputEsOutControlPCRCommand;

    public:
        BaseStreamOutput(demux_t *, const StreamFormat &, const std::string &);
        virtual ~BaseStreamOutput();
        virtual void pushBlock(block_t *, bool); /* reimpl */
        virtual mtime_t getPCR() const; /* reimpl */
        virtual mtime_t getFirstDTS() const; /* reimpl */
        virtual int esCount() const; /* reimpl */
        virtual bool seekAble() const; /* reimpl */
        virtual void setPosition(mtime_t); /* reimpl */
        virtual void sendToDecoder(mtime_t); /* reimpl */
        virtual bool isEmpty() const; /* reimpl */
        virtual bool reinitsOnSeek() const; /* reimpl */
        virtual bool switchAllowed() const; /* reimpl */
        virtual bool isSelected() const; /* reimpl */
        void setTimestampOffset(mtime_t);

        virtual void fillExtraFMTInfo( es_format_t * ) const;

    protected:
        FakeESOut *fakeesout; /* to intercept/proxy what is sent from demuxstream */
        stream_t *demuxstream;
        bool      seekable;
        std::string name;

    private:
        bool restart();
    };

}
#endif // STREAMOUTPUT_HPP
