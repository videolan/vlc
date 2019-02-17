/*****************************************************************************
 * dvb.c: LinuxTV channels list
 *****************************************************************************
 * Copyright (C) 2005-2012 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_charset.h>

#include "playlist.h"

static int ReadDir(stream_t *, input_item_node_t *);
static input_item_t *ParseLine(char *line);

/** Detect dvb-utils zap channels.conf format */
int Import_DVB(vlc_object_t *p_this)
{
    stream_t *demux = (stream_t *)p_this;

    CHECK_FILE(demux);
    if (!stream_HasExtension(demux, ".conf" ) && !demux->obj.force )
        return VLC_EGENERIC;

    /* Check if this really is a channels file */
    const uint8_t *peek;
    int len = vlc_stream_Peek(demux->s, &peek, 1023);
    if (len <= 0)
        return VLC_EGENERIC;

    const uint8_t *eol = memchr(peek, '\n', len);
    if (eol == NULL)
        return VLC_EGENERIC;
    len = eol - peek;

    char line[len + 1];
    memcpy(line, peek, len);
    line[len] = '\0';

    input_item_t *item = ParseLine(line);
    if (item == NULL)
        return VLC_EGENERIC;
    input_item_Release(item);

    msg_Dbg(demux, "found valid channels.conf file");
    demux->pf_control = access_vaDirectoryControlHelper;
    demux->pf_readdir = ReadDir;

    return VLC_SUCCESS;
}

/** Parses the whole channels.conf file */
static int ReadDir(stream_t *s, input_item_node_t *subitems)
{
    char *line;

    while ((line = vlc_stream_ReadLine(s->s)) != NULL)
    {
        input_item_t *item = ParseLine(line);
        free(line);
        if (item == NULL)
            continue;

        input_item_node_AppendItem(subitems, item);
        input_item_Release(item);
    }

    return VLC_SUCCESS;
}

static int cmp(const void *k, const void *e)
{
    return strcmp(k, e);
}

static const char *ParseFEC(const char *str)
{
     static const struct fec
     {
         char dvb[5];
         char vlc[5];
     } tab[] = {
         { "1_2", "1/2" }, { "2_3", "2/3" }, { "3_4", "3/4" },
         { "4_5", "4/5" }, { "5_6", "5/6" }, { "6_7", "6/7" },
         { "7_8", "7/8" }, { "8_9", "8/9" }, { "9_10", "9/10" },
         { "AUTO", "" },   { "NONE", "0" }
     };

     if (str == NULL || strncmp(str, "FEC_", 4))
         return NULL;
     str += 4;

     const struct fec *f = bsearch(str, tab, sizeof (tab) / sizeof(tab[0]),
                                   sizeof (tab[0]), cmp);
     return (f != NULL) ? f->vlc : NULL;
}

static const char *ParseModulation(const char *str)
{
     static const struct mod
     {
         char dvb[9];
         char vlc[7];
     } tab[] = {
         { "8VSB", "8VSB" }, { "APSK_16", "16APSK" }, { "APSK_32", "32APSK" },
         { "DQPSK", "DQPSK" }, { "PSK_8", "8PSK" }, { "QPSK", "QPSK" },
         { "QAM_128", "128QAM" }, { "QAM_16", "16QAM" },
         { "QAM_256", "256QAM" }, { "QAM_32", "32QAM" },
         { "QAM_64", "64QAM" }, { "QAM_AUTO", "QAM" },
         { "VSB_16", "16VSB" }, { "VSB_8", "8VSB" }
     };

     if( str == NULL )
         return NULL;

     const struct mod *m = bsearch(str, tab, sizeof (tab) / sizeof(tab[0]),
                                   sizeof (tab[0]), cmp);
     return (m != NULL) ? m->vlc : NULL;
}

static const char *ParseGuard(const char *str)
{
     static const struct guard
     {
         char dvb[7];
         char vlc[7];
     } tab[] = {
         { "19_128", "19/128" }, { "19_256", "19/256" }, { "1_128", "1/128" },
         { "1_16", "1/16" }, { "1_32", "1/32" }, { "1_4", "1/4" },
         { "1_8", "1/8" }, { "AUTO", "" },
     };

     if (str == NULL || strncmp(str, "GUARD_INTERVAL_", 15))
         return NULL;
     str += 15;

     const struct guard *g = bsearch(str, tab, sizeof (tab) / sizeof(tab[0]),
                                     sizeof (tab[0]), cmp);
     return (g != NULL) ? g->vlc : NULL;
}

/* http://www.linuxtv.org/vdrwiki/index.php/Syntax_of_channels.conf or not...
 * Read the dvb-apps source code for reference. */
static input_item_t *ParseLine(char *line)
{
    char *str, *end;

    line += strspn(line, " \t\r"); /* skip leading white spaces */
    if (*line == '#')
        return NULL; /* skip comments */

    /* Extract channel cute name */
    char *name = strsep(&line, ":");
    assert(name != NULL);
    EnsureUTF8(name);

    /* Extract central frequency */
    str = strsep(&line, ":");
    if (str == NULL)
        return NULL;
    unsigned long freq = strtoul(str, &end, 10);
    if (*end)
        return NULL;

    /* Extract tuning parameters */
    str = strsep(&line, ":");
    if (str == NULL)
        return NULL;

    char *mrl;

    if (!strcmp(str, "h") || !strcmp(str, "v"))
    {   /* DVB-S */
        char polarization = toupper(*str);

        /* TODO: sat no. */
        str = strsep(&line, ":");
        if (str == NULL)
            return NULL;

        /* baud rate */
        str = strsep(&line, ":");
        if (str == NULL)
            return NULL;

        unsigned long rate = strtoul(str, &end, 10);
        if (*end || rate > (ULONG_MAX / 1000u))
            return NULL;
        rate *= 1000;

        if (asprintf(&mrl,
                     "dvb-s://frequency=%"PRIu64":polarization=%c:srate=%lu",
                     freq * UINT64_C(1000000), polarization, rate) == -1)
            mrl = NULL;
    }
    else
    if (!strncmp(str, "INVERSION_", 10))
    {   /* DVB-C or DVB-T */
        int inversion;

        str += 10;
        if (strcmp(str, "AUTO"))
            inversion = -1;
        else if (strcmp(str, "OFF"))
            inversion = 0;
        else if (strcmp(str, "ON"))
            inversion = 1;
        else
            return NULL;

        str = strsep(&line, ":");
        if (str == NULL)
            return NULL;

        if (strncmp(str, "BANDWIDTH_", 10))
        {   /* DVB-C */
            unsigned long rate = strtoul(str, &end, 10);
            if (*end)
                return NULL;

            const char *fec = ParseFEC(strsep(&line, ":"));
            const char *mod = ParseModulation(strsep(&line,":"));
            if (fec == NULL || mod == NULL)
                return NULL;

            if (asprintf(&mrl, "dvb-c://frequency=%lu:inversion:%d:srate=%lu:"
                         "fec=%s:modulation=%s", freq, inversion, rate, fec,
                         mod) == -1)
                mrl = NULL;
        }
        else
        {   /* DVB-T */
            unsigned bandwidth = atoi(str + 10);

            const char *hp = ParseFEC(strsep(&line, ":"));
            const char *lp = ParseFEC(strsep(&line, ":"));
            const char *mod = ParseModulation(strsep(&line, ":"));

            if (hp == NULL || lp == NULL || mod == NULL)
                return NULL;

            str = strsep(&line, ":");
            if (str == NULL || strncmp(str, "TRANSMISSION_MODE_", 18))
                return NULL;
            int xmit = atoi(str);
            if (xmit == 0)
                xmit = -1; /* AUTO */

            const char *guard = ParseGuard(strsep(&line,":"));
            if (guard == NULL)
                return NULL;

            str = strsep(&line, ":");
            if (str == NULL || strncmp(str, "HIERARCHY_", 10))
                return NULL;
            str += 10;
            int hierarchy = atoi(str);
            if (!strcmp(str, "AUTO"))
                hierarchy = -1;

            if (asprintf(&mrl, "dvb-t://frequency=%lu:inversion=%d:"
                         "bandwidth=%u:code-rate-hp=%s:code-rate-lp=%s:"
                         "modulation=%s:transmission=%d:guard=%s:"
                         "hierarchy=%d", freq, inversion, bandwidth, hp, lp,
                         mod, xmit, guard, hierarchy) == -1)
                mrl = NULL;
        }
    }
    else
    {   /* ATSC */
        const char *mod = ParseModulation(str);
        if (mod == NULL)
            return NULL;

        if (asprintf(&mrl, "atsc://frequency=%lu:modulation=%s", freq,
                     mod) == -1)
            mrl = NULL;
    }

    if (unlikely(mrl == NULL))
        return NULL;

    /* Video PID (TODO? set video track) */
    strsep(&line, ":");
    /* Audio PID (TODO? set audio track) */
    strsep(&line, ":");
    /* Extract SID */
    str = strsep(&line, ":");
    if (str == NULL)
    {
        free(mrl);
        return NULL;
    }
    unsigned long sid = strtoul(str, &end, 10);
    if (*end || sid > 65535)
    {
        free(mrl);
        return NULL;
    }

    char sid_opt[sizeof("program=65535")];
    snprintf(sid_opt, sizeof(sid_opt), "program=%lu", sid);

    input_item_t *item = input_item_NewCard(mrl, name);
    free(mrl);
    if (item != NULL)
        input_item_AddOption(item, sid_opt, 0);
    return item;
}
