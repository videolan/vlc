/*
 * StreamOutput.cpp
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
#include "StreamOutput.hpp"
#include <vlc_stream.h>
#include <vlc_demux.h>

using namespace adaptative;

AbstractStreamOutput::AbstractStreamOutput(demux_t *demux, const StreamFormat &format_)
{
    realdemux = demux;
    pcr = VLC_TS_INVALID;
    format = format_;
}

void AbstractStreamOutput::setLanguage(const std::string &lang)
{
    language = lang;
}

void AbstractStreamOutput::setDescription(const std::string &desc)
{
    description = desc;
}

const StreamFormat & AbstractStreamOutput::getStreamFormat() const
{
    return format;
}

AbstractStreamOutput::~AbstractStreamOutput()
{
}

mtime_t AbstractStreamOutput::getPCR() const
{
    return pcr;
}

BaseStreamOutput::BaseStreamOutput(demux_t *demux, const StreamFormat &format, const std::string &name) :
    AbstractStreamOutput(demux, format)
{
    this->name = name;
    seekable = true;
    restarting = false;
    demuxstream = NULL;
    b_drop = false;
    timestamps_offset = VLC_TS_INVALID;

    fakeesout = new es_out_t;
    if (!fakeesout)
        throw VLC_ENOMEM;

    vlc_mutex_init(&lock);

    fakeesout->pf_add = esOutAdd_Callback;
    fakeesout->pf_control = esOutControl_Callback;
    fakeesout->pf_del = esOutDel_Callback;
    fakeesout->pf_destroy = esOutDestroy_Callback;
    fakeesout->pf_send = esOutSend_Callback;
    fakeesout->p_sys = (es_out_sys_t*) this;

    demuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout);
    if(!demuxstream)
        throw VLC_EGENERIC;
}

BaseStreamOutput::~BaseStreamOutput()
{
    if (demuxstream)
        stream_Delete(demuxstream);

    /* shouldn't be any */
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
        delete *it;

    delete fakeesout;
    vlc_mutex_destroy(&lock);
}

mtime_t BaseStreamOutput::getFirstDTS() const
{
    mtime_t ret = VLC_TS_INVALID;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        const Demuxed *pair = *it;
        const block_t *p_block = pair->p_queue;
        while( p_block && p_block->i_dts == VLC_TS_INVALID )
        {
            p_block = p_block->p_next;
        }

        if(p_block)
        {
            ret = p_block->i_dts;
            break;
        }
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return ret;
}

int BaseStreamOutput::esCount() const
{
    return queues.size();
}

void BaseStreamOutput::pushBlock(block_t *block, bool)
{
    stream_DemuxSend(demuxstream, block);
}

bool BaseStreamOutput::seekAble() const
{
    bool b_canswitch = switchAllowed();
    return (demuxstream && seekable && b_canswitch);
}

void BaseStreamOutput::setPosition(mtime_t nztime)
{
    vlc_mutex_lock(&lock);
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        (*it)->drop();
    }

    if(reinitsOnSeek())
    {
        /* disable appending until restarted */
        b_drop = true;
        vlc_mutex_unlock(&lock);
        restart();
        vlc_mutex_lock(&lock);
        b_drop = false;
    }

    pcr = VLC_TS_INVALID;
    vlc_mutex_unlock(&lock);

    es_out_Control(realdemux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                   VLC_TS_0 + nztime);
}

bool BaseStreamOutput::restart()
{
    stream_t *newdemuxstream = stream_DemuxNew(realdemux, name.c_str(), fakeesout);
    if(!newdemuxstream)
        return false;

    vlc_mutex_lock(&lock);
    restarting = true;
    stream_t *olddemuxstream = demuxstream;
    demuxstream = newdemuxstream;
    vlc_mutex_unlock(&lock);

    if(olddemuxstream)
        stream_Delete(olddemuxstream);

    return true;
}

bool BaseStreamOutput::reinitsOnSeek() const
{
    return true;
}

bool BaseStreamOutput::switchAllowed() const
{
    bool b_allowed;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    b_allowed = !restarting;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_allowed;
}

bool BaseStreamOutput::isSelected() const
{
    bool b_selected = false;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end() && !b_selected; ++it)
    {
        const Demuxed *pair = *it;
        if(pair->es_id)
            es_out_Control(realdemux->out, ES_OUT_GET_ES_STATE, pair->es_id, &b_selected);
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_selected;
}

bool BaseStreamOutput::isEmpty() const
{
    bool b_empty = true;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end() && b_empty; ++it)
    {
        b_empty = !(*it)->p_queue;
    }
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_empty;
}

void BaseStreamOutput::sendToDecoder(mtime_t nzdeadline)
{
    vlc_mutex_lock(&lock);
    sendToDecoderUnlocked(nzdeadline);
    vlc_mutex_unlock(&lock);
}

void BaseStreamOutput::sendToDecoderUnlocked(mtime_t nzdeadline)
{
    std::list<Demuxed *>::const_iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        Demuxed *pair = *it;
        while(pair->p_queue && pair->p_queue->i_dts <= VLC_TS_0 + nzdeadline)
        {
            block_t *p_block = pair->p_queue;
            pair->p_queue = pair->p_queue->p_next;
            p_block->p_next = NULL;

            if(pair->pp_queue_last == &p_block->p_next)
                pair->pp_queue_last = &pair->p_queue;

            realdemux->out->pf_send(realdemux->out, pair->es_id, p_block);
        }
    }
}

void BaseStreamOutput::setTimestampOffset(mtime_t offset)
{
    vlc_mutex_lock(&lock);
    timestamps_offset = VLC_TS_0 + offset;
    vlc_mutex_unlock(&lock);
}

BaseStreamOutput::Demuxed::Demuxed(es_out_id_t *id, const es_format_t *fmt)
{
    p_queue = NULL;
    pp_queue_last = &p_queue;
    recycle = false;
    es_id = id;
    es_format_Init(&fmtcpy, UNKNOWN_ES, 0);
    es_format_Copy(&fmtcpy, fmt);
}

BaseStreamOutput::Demuxed::~Demuxed()
{
    es_format_Clean(&fmtcpy);
    drop();
}

void BaseStreamOutput::Demuxed::drop()
{
    if(p_queue)
    {
        block_ChainRelease(p_queue);
        p_queue = NULL;
        pp_queue_last = &p_queue;
    }
}

/* Static callbacks */
es_out_id_t * BaseStreamOutput::esOutAdd_Callback(es_out_t *fakees, const es_format_t *p_fmt)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    return me->esOutAdd(p_fmt);
}

int BaseStreamOutput::esOutSend_Callback(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    return me->esOutSend(p_es, p_block);
}

void BaseStreamOutput::esOutDel_Callback(es_out_t *fakees, es_out_id_t *p_es)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    me->esOutDel(p_es);
}

int BaseStreamOutput::esOutControl_Callback(es_out_t *fakees, int i_query, va_list ap)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    return me->esOutControl(i_query, ap);
}

void BaseStreamOutput::esOutDestroy_Callback(es_out_t *fakees)
{
    BaseStreamOutput *me = (BaseStreamOutput *) fakees->p_sys;
    me->esOutDestroy();
}
/* !Static callbacks */

es_out_id_t * BaseStreamOutput::esOutAdd(const es_format_t *p_fmt)
{
    es_out_id_t *p_es = NULL;

    vlc_mutex_lock(&lock);

    std::list<Demuxed *>::iterator it;
    bool b_hasestorecyle = false;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        Demuxed *pair = *it;
        b_hasestorecyle |= pair->recycle;

        if( p_es )
            continue;

        if( restarting )
        {
            /* If we're recycling from same format */
            if( es_format_IsSimilar(p_fmt, &pair->fmtcpy) &&
                    p_fmt->i_extra == pair->fmtcpy.i_extra &&
                    !memcmp(p_fmt->p_extra, pair->fmtcpy.p_extra, p_fmt->i_extra) )
            {
                msg_Dbg(realdemux, "reusing output for codec %4.4s", (char*)&p_fmt->i_codec);
                pair->recycle = false;
                p_es = pair->es_id;
            }
        }
    }

    if(!b_hasestorecyle)
    {
        restarting = false;
    }

    if(!p_es)
    {
        es_format_t fmtcpy;
        es_format_Init(&fmtcpy, p_fmt->i_cat, p_fmt->i_codec);
        es_format_Copy(&fmtcpy, p_fmt);
        fmtcpy.i_group = 0;
        if(!fmtcpy.psz_language && !language.empty())
            fmtcpy.psz_language = strdup(language.c_str());
        if(!fmtcpy.psz_description && !description.empty())
            fmtcpy.psz_description = strdup(description.c_str());
        p_es = realdemux->out->pf_add(realdemux->out, &fmtcpy);
        if(p_es)
        {
            Demuxed *pair = new (std::nothrow) Demuxed(p_es, &fmtcpy);
            if(pair)
                queues.push_back(pair);
        }
        es_format_Clean(&fmtcpy);
    }
    vlc_mutex_unlock(&lock);

    return p_es;
}

int BaseStreamOutput::esOutSend(es_out_id_t *p_es, block_t *p_block)
{
    vlc_mutex_lock(&lock);
    if(b_drop)
    {
        block_ChainRelease( p_block );
    }
    else
    {
        if( timestamps_offset > VLC_TS_INVALID )
        {
            if(p_block->i_dts > VLC_TS_INVALID)
                p_block->i_dts += (timestamps_offset - VLC_TS_0);

            if(p_block->i_pts > VLC_TS_INVALID)
                p_block->i_pts += (timestamps_offset - VLC_TS_0);
        }

        std::list<Demuxed *>::const_iterator it;
        for(it=queues.begin(); it!=queues.end();++it)
        {
            Demuxed *pair = *it;
            if(pair->es_id == p_es)
            {
                block_ChainLastAppend(&pair->pp_queue_last, p_block);
                break;
            }
        }
    }
    vlc_mutex_unlock(&lock);
    return VLC_SUCCESS;
}

void BaseStreamOutput::esOutDel(es_out_id_t *p_es)
{
    vlc_mutex_lock(&lock);
    std::list<Demuxed *>::iterator it;
    for(it=queues.begin(); it!=queues.end();++it)
    {
        if((*it)->es_id == p_es)
        {
            sendToDecoderUnlocked(INT64_MAX - VLC_TS_0);
            break;
        }
    }

    if(it != queues.end())
    {
        if(restarting)
        {
            (*it)->recycle = true;
        }
        else
        {
            delete *it;
            queues.erase(it);
        }
    }

    if(!restarting)
        realdemux->out->pf_del(realdemux->out, p_es);

    vlc_mutex_unlock(&lock);
}

int BaseStreamOutput::esOutControl(int i_query, va_list args)
{
    if (i_query == ES_OUT_SET_PCR )
    {
        vlc_mutex_lock(&lock);
        pcr = (int64_t)va_arg( args, int64_t );
        if(pcr > VLC_TS_INVALID && timestamps_offset > VLC_TS_INVALID)
            pcr += (timestamps_offset - VLC_TS_0);
        vlc_mutex_unlock(&lock);
        return VLC_SUCCESS;
    }
    else if( i_query == ES_OUT_SET_GROUP_PCR )
    {
        vlc_mutex_lock(&lock);
        static_cast<void>(va_arg( args, int ));
        pcr = (int64_t)va_arg( args, int64_t );
        if(pcr > VLC_TS_INVALID && timestamps_offset > VLC_TS_INVALID)
            pcr += (timestamps_offset - VLC_TS_0);
        vlc_mutex_unlock(&lock);
        return VLC_SUCCESS;
    }
    else if( i_query == ES_OUT_GET_ES_STATE )
    {
        va_arg( args, es_out_id_t * );
        bool *pb = va_arg( args, bool * );
        *pb = true;
        return VLC_SUCCESS;
    }

    vlc_mutex_lock(&lock);
    bool b_restarting = restarting;
    vlc_mutex_unlock(&lock);

    if( b_restarting )
    {
        return VLC_EGENERIC;
    }

    return realdemux->out->pf_control(realdemux->out, i_query, args);
}

void BaseStreamOutput::esOutDestroy()
{
    realdemux->out->pf_destroy(realdemux->out);
}

