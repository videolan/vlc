/*****************************************************************************
 * DBMSDIOutput.hpp: Decklink SDI Output
 *****************************************************************************
 * Copyright © 2014-2016 VideoLAN and VideoLAN Authors
 *                  2018 VideoLabs
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
#ifndef DBMSDIOUTPUT_HPP
#define DBMSDIOUTPUT_HPP

#include "SDIOutput.hpp"

#include <vlc_es.h>
#include "../../access/vlc_decklink.h"

namespace sdi_sout
{
    class DBMSDIOutput : public SDIOutput
    {
        public:
            DBMSDIOutput(sout_stream_t *);
            ~DBMSDIOutput();
            virtual AbstractStream *Add(const es_format_t *); /* reimpl */
            virtual int Open(); /* impl */
            virtual int Process(); /* impl */
            virtual int Send(AbstractStream *, block_t *); /* reimpl */

        protected:
            int ProcessVideo(picture_t *, block_t *);
            int ProcessAudio(block_t *);
            virtual int ConfigureVideo(const video_format_t *); /* impl */
            virtual int ConfigureAudio(const audio_format_t *); /* impl */

        private:
            IDeckLink *p_card;
            IDeckLinkOutput *p_output;

            BMDTimeScale timescale;
            BMDTimeValue frameduration;
            vlc_tick_t lasttimestamp;
            /* XXX: workaround card clock drift */
            struct
            {
                vlc_tick_t offset; /* > 0 if clock card is slower */
                vlc_tick_t system_reference;
                BMDTimeValue hardware_reference;
            } clock;
            bool b_running;
            int Start(vlc_tick_t);
            int doProcessVideo(picture_t *, block_t *);
            picture_t * CreateNoSignalPicture(const char*, const video_format_t *);
            void checkClockDrift();
    };
}

#endif // DBMSDIOUTPUT_HPP
