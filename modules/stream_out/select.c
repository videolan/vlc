/*****************************************************************************
 * select.c: Select individual es to enable or disable from stream
 *****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
 * Based upon autodel.c written by: Christophe Massiot <massiot@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_network.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define PORT_TEXT N_("Command UDP port")
#define PORT_LONGTEXT N_( \
    "UDP port to listen to for commands (show | enable <pid> | disable <pid>)." )

#define DISABLE_TEXT N_("Disable ES id")
#define DISABLE_LONGTEXT N_( \
    "Disable ES id at startup." )

#define SOUT_CFG_PREFIX "sout-select-"

vlc_module_begin ()
    set_shortname(N_("Select"))
    set_description(N_("Select individual es to enable or disable from stream"))
    set_capability("sout stream", 50 )
    add_integer(SOUT_CFG_PREFIX "port", 5001, PORT_TEXT, PORT_LONGTEXT, true)
    add_integer(SOUT_CFG_PREFIX "disable", -1, DISABLE_TEXT, DISABLE_LONGTEXT, false)
    add_shortcut("select")
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static sout_stream_id_t *Add   (sout_stream_t *, es_format_t *);
static int               Del   (sout_stream_t *, sout_stream_id_t *);
static int               Send  (sout_stream_t *, sout_stream_id_t *, block_t *);

static void* Command(void *);

struct sout_stream_id_t
{
    sout_stream_id_t *id;
    es_format_t fmt;
    bool b_error;
    bool b_enabled;
};

struct sout_stream_sys_t
{
    sout_stream_id_t **pp_es;
    int i_es_num;

    vlc_mutex_t es_lock;
    vlc_thread_t thread;

    int i_fd;
    int i_id_disable;
};

static const char *const ppsz_sout_options[] = {
    "disable", "port", NULL
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys = malloc(sizeof(sout_stream_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    if (!p_stream->p_next)
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                      p_stream->p_cfg);

    int port = var_GetInteger(p_stream, SOUT_CFG_PREFIX "port");
    p_sys->i_fd = net_ListenUDP1(VLC_OBJECT(p_stream), NULL, port);
    if (p_sys->i_fd < 0)
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->i_id_disable = var_GetInteger(p_stream, SOUT_CFG_PREFIX "disable");

    p_sys->pp_es = NULL;
    p_sys->i_es_num = 0;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    vlc_mutex_init(&p_sys->es_lock);

    if(vlc_clone(&p_sys->thread, Command, p_stream, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_mutex_destroy(&p_sys->es_lock);
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close (vlc_object_t * p_this)
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    /* Stop the thread */
    vlc_cancel(p_sys->thread);
    vlc_join(p_sys->thread, NULL);

    /* Free the ressources */
    net_Close( p_sys->i_fd );
    vlc_mutex_destroy(&p_sys->es_lock);

    p_stream->p_sout->i_out_pace_nocontrol--;

    free(p_sys);
}

/****************************************************************************
 * Command Thread:
 ****************************************************************************/
static void* Command(void *p_this)
{
    sout_stream_t *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    while (vlc_object_alive(p_stream))
    {
        char psz_buffer[20];

        int i_len = recv(p_sys->i_fd, psz_buffer, sizeof(psz_buffer)-1, 0);
        if (i_len < 4)
            continue;

        psz_buffer[i_len] = '\0';
        msg_Info( p_stream, "command: %s", psz_buffer );

        if (strncmp(psz_buffer, "show", 4) == 0)
        {
            vlc_mutex_lock(&p_sys->es_lock);
            mutex_cleanup_push(&p_sys->es_lock);
            for (int i = 0; i < p_sys->i_es_num; i++)
            {
                i_len = snprintf(psz_buffer, sizeof(psz_buffer), "%.4s : %d",
                                 (char *)&p_sys->pp_es[i]->fmt.i_codec,
                                 p_sys->pp_es[i]->fmt.i_id);
                psz_buffer[i_len] = '\0';
                msg_Info(p_stream, psz_buffer);
            }
            vlc_cleanup_pop();
        }
        else
        {
            bool b_apply = false;
            bool b_select = false;
            int i_pid = 0x1FFF;

            if (strncmp(psz_buffer, "enable", 6) == 0)
            {
                i_pid = atol(psz_buffer+7);
                b_select = true;
                b_apply = true;
            }
            else if (strncmp(psz_buffer, "disable", 7) == 0)
            {
                i_pid = atol(psz_buffer+8);
                b_apply = true;
            }

            if (b_apply)
            {
                vlc_mutex_lock(&p_sys->es_lock);
                mutex_cleanup_push(&p_sys->es_lock);
                for (int i = 0; i < p_sys->i_es_num; i++)
                {
                    msg_Info(p_stream, "elementary stream pid %d",
                             p_sys->pp_es[i]->fmt.i_id);
                    if (p_sys->pp_es[i]->fmt.i_id == i_pid)
                    {
                        p_sys->pp_es[i]->b_enabled = b_select;
                        msg_Info(p_stream, "%s: %d", b_select ? "enable" : "disable", i_pid);
                    }
                }
                vlc_cleanup_pop();
            }
        }
    }

    return NULL;
}

static sout_stream_id_t *Add(sout_stream_t *p_stream, es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_t *p_es = malloc(sizeof(sout_stream_id_t));
    if( !p_es )
        return NULL;

    p_es->fmt = *p_fmt;
    p_es->id = NULL;
    p_es->b_error = false;
    p_es->b_enabled = (p_es->fmt.i_id == p_sys->i_id_disable) ? false : true;

    vlc_mutex_lock(&p_sys->es_lock);
    TAB_APPEND(p_sys->i_es_num, p_sys->pp_es, p_es);
    vlc_mutex_unlock(&p_sys->es_lock);

    return p_es;
}

static int Del(sout_stream_t *p_stream, sout_stream_id_t *p_es)
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_t *id = p_es->id;

    vlc_mutex_lock(&p_sys->es_lock);
    TAB_REMOVE(p_sys->i_es_num, p_sys->pp_es, p_es);
    vlc_mutex_unlock(&p_sys->es_lock);

    free(p_es);

    if (id != NULL)
        return p_stream->p_next->pf_del(p_stream->p_next, id);
    else
        return VLC_SUCCESS;
}

static int Send(sout_stream_t *p_stream, sout_stream_id_t *p_es,
                block_t *p_buffer)
{
    if (p_es->id == NULL && p_es->b_error != true)
    {
        p_es->id = p_stream->p_next->pf_add(p_stream->p_next, &p_es->fmt);
        if (p_es->id == NULL)
        {
            p_es->b_error = true;
            msg_Err(p_stream, "couldn't create chain for id %d",
                    p_es->fmt.i_id);
        }
    }

    if ((p_es->b_error != true) && p_es->b_enabled)
        p_stream->p_next->pf_send(p_stream->p_next, p_es->id, p_buffer);
    else
        block_ChainRelease(p_buffer);

    return VLC_SUCCESS;
}
