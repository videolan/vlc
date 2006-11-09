/*****************************************************************************
 * dump.c
 *****************************************************************************
 * Copyright © 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Author: Rémi Denis-Courmont <rem # videolan,org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "vlc_access.h"
#include "vlc_block.h"
#include "charset.h"
#include "vlc_keys.h"

#define DEFAULT_MARGIN 32 // megabytes

#define FORCE_TEXT N_("Force use of dump module")
#define FORCE_LONGTEXT N_("Activate the dump module " \
                          "even for media with fast seeking.")

#define MARGIN_TEXT N_("Maximum size of temporary file (Mb)")
#define MARGIN_LONGTEXT N_("The dump module will abort dumping of the media " \
                           "if more than this much megabyte were performed.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ();
    set_shortname (_("Dump"));
    set_description (_("Dump"));
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_ACCESS_FILTER);
    set_capability ("access_filter", 0);
    add_shortcut ("dump");
    set_callbacks (Open, Close);

    add_bool ("dump-force", VLC_FALSE, NULL, FORCE_TEXT,
              FORCE_LONGTEXT, VLC_FALSE);
    add_integer ("dump-margin", DEFAULT_MARGIN, NULL, MARGIN_TEXT,
                 MARGIN_LONGTEXT, VLC_FALSE);
vlc_module_end();

static int Read (access_t *access, uint8_t *buffer, int len);
static block_t *Block (access_t *access);
static int Seek (access_t *access, int64_t offset);
static int Control (access_t *access, int cmd, va_list ap);

static void Trigger (access_t *access);
static int KeyHandler (vlc_object_t *obj, char const *varname,
                       vlc_value_t oldval, vlc_value_t newval, void *data);

struct access_sys_t
{
    FILE *stream;
    int64_t tmp_max;
    int64_t dumpsize;
};

/**
 * Open()
 */
static int Open (vlc_object_t *obj)
{
    access_t *access = (access_t*)obj;
    access_t *src = access->p_source;

    if (!var_CreateGetBool (access, "dump-force"))
    {
        vlc_bool_t b;
        if ((access2_Control (src, ACCESS_CAN_FASTSEEK, &b) == 0) && b)
        {
            msg_Dbg (obj, "dump filter useless");
            return VLC_EGENERIC;
        }
    }

    if (src->pf_read != NULL)
        access->pf_read = Read;
    else
        access->pf_block = Block;
    if (src->pf_seek != NULL)
        access->pf_seek = Seek;

    access->pf_control = Control;
    access->info = src->info;

    access_sys_t *p_sys = access->p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;
    memset (p_sys, 0, sizeof (*p_sys));

    if ((p_sys->stream = tmpfile ()) == NULL)
    {
        msg_Err (access, "cannot create temporary file: %s", strerror (errno));
        free (p_sys);
        return VLC_EGENERIC;
    }
    p_sys->tmp_max = ((int64_t)var_CreateGetInteger (access, "dump-margin")) << 20;

    var_AddCallback (access->p_libvlc, "key-pressed", KeyHandler, access);

    return VLC_SUCCESS;
}


/**
 * Close()
 */
static void Close (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *p_sys = access->p_sys;

    var_DelCallback (access->p_libvlc, "key-pressed", KeyHandler, access);

    if (p_sys->stream != NULL)
        fclose (p_sys->stream);
    free (p_sys);
}


static void Dump (access_t *access, const uint8_t *buffer, size_t len)
{
    access_sys_t *p_sys = access->p_sys;
    FILE *stream = p_sys->stream;

    if ((stream == NULL) /* not dumping */
     || (access->info.i_pos < p_sys->dumpsize) /* already known data */)
        return;

    size_t needed = access->info.i_pos - p_sys->dumpsize;
    if (len < needed)
        return; /* gap between data and dump offset (seek too far ahead?) */

    buffer += len - needed;
    len = needed;

    if (len == 0)
        return; /* no useful data */

    if ((p_sys->tmp_max != -1) && (access->info.i_pos > p_sys->tmp_max))
    {
        msg_Dbg (access, "too much data - dump will not work");
        goto error;
    }

    assert (len > 0);
    if (fwrite (buffer, len, 1, stream) != 1)
    {
        msg_Err (access, "cannot write to file: %s", strerror (errno));
        goto error;
    }

    p_sys->dumpsize += len;
    return;

error:
    fclose (stream);
    p_sys->stream = NULL;
}


static int Read (access_t *access, uint8_t *buffer, int len)
{
    access_t *src = access->p_source;

    src->info.i_update = access->info.i_update;
    len = src->pf_read (src, buffer, len);
    access->info = src->info;

    Dump (access, buffer, len);

    return len;
}


static block_t *Block (access_t *access)
{
    access_t *src = access->p_source;
    block_t *block;

    src->info.i_update = access->info.i_update;
    block = src->pf_block (src);
    access->info = src->info;

    if ((block == NULL) || (block->i_buffer <= 0))
        return block;

    Dump (access, block->p_buffer, block->i_buffer);

    return block;
}


static int Control (access_t *access, int cmd, va_list ap)
{
    access_t *src = access->p_source;

    return src->pf_control (src, cmd, ap);
}


static int Seek (access_t *access, int64_t offset)
{
    access_t *src = access->p_source;
    access_sys_t *p_sys = access->p_sys;

    if (p_sys->tmp_max == -1)
    {
        msg_Err (access, "cannot seek while dumping!");
        return VLC_EGENERIC;
    }

    if (p_sys->stream != NULL)
        msg_Dbg (access, "seeking - dump might not work");

    src->info.i_update = access->info.i_update;
    int ret = src->pf_seek (src, offset);
    access->info = src->info;
    return ret;
}


#ifndef HAVE_LOCALTIME_R
static inline struct tm *localtime_r (const time_t *now, struct tm *res)
{
    struct tm *unsafe = localtime (now);
    /*
     * This is not thread-safe. Blame your C library.
     * On Win32 there SHOULD be _localtime_s instead, but of course
     * Cygwin and Mingw32 don't know about it. You're on your own if you
     * this platform.
     */
    if (unsafe == NULL)
        return NULL;

    memcpy (res, unsafe, sizeof (*res));
    return res;
}
#endif


static void Trigger (access_t *access)
{
    access_sys_t *p_sys = access->p_sys;

    if (p_sys->stream == NULL)
        return; // too late

    if (p_sys->tmp_max == -1)
        return; // already triggered - should we stop? FIXME

    time_t now;
    time (&now);

    struct tm t;
    if (localtime_r (&now, &t) == NULL)
        return; // No time, eh? Well, I'd rather not run on your computer.

    if (t.tm_year > 999999999)
        // Humanity is about 300 times older than when this was written,
        // and there is an off-by-one in the following sprintf().
        return;

    const char *home = access->p_libvlc->psz_homedir;

    /* Hmm what about the extension?? */
    char filename[strlen (home) + sizeof ("/vlcdump-YYYYYYYYY-MM-DD-HH-MM-SS.ts")];
    sprintf (filename, "%s/vlcdump-%04u-%02u-%02u-%02u-%02u-%02u.ts", home,
             t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    msg_Info (access, "dumping media to \"%s\"...", filename);

    FILE *newstream = fopen (filename, "wb");
    if (newstream == NULL)
    {
        msg_Err (access, "cannot create dump file \"%s\": %s", filename,
                 strerror (errno));
        return;
    }

    /* This might cause excessive hard drive work :( */
    FILE *oldstream = p_sys->stream;
    rewind (oldstream);

    for (;;)
    {
        char buf[16384];
        size_t len = fread (buf, 1, sizeof (buf), oldstream);
        if (len == 0)
        {
            if (ferror (oldstream))
            {
                msg_Err (access, "cannot read temporary file: %s",
                         strerror (errno));
                break;
            }

            /* Done with temporary file */
            fclose (oldstream);
            p_sys->stream = newstream;
            p_sys->tmp_max = -1;
            return;
        }

        if (fwrite (buf, len, 1, newstream) != 1)
        {
            msg_Err (access, "cannot write dump file: %s", strerror (errno));
            break;
        }
    }

    /* Failed to copy temporary file */
    fseek (oldstream, 0, SEEK_END);
    fclose (newstream);
    return;
}


static int KeyHandler (vlc_object_t *obj, char const *varname,
                       vlc_value_t oldval, vlc_value_t newval, void *data)
{
    access_t *access = data;

    (void)oldval;
    (void)obj;

    for (struct hotkey *key = access->p_libvlc->p_hotkeys;
         key->psz_action != NULL; key++)
    {
        if (key->i_key == newval.i_int)
        {
            if (key->i_action == ACTIONID_DUMP)
                Trigger ((access_t *)data);
            break;
        }
    }

    return VLC_SUCCESS;
}
