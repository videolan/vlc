/*****************************************************************************
 * snapshot.c : vout internal snapshot
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Gildas Bazin <gbazin _AT_ videolan _DOT_ org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_block.h>
#include <vlc_vout.h>

#include "snapshot.h"
#include "vout_internal.h"

struct vout_snapshot {
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    bool        is_available;
    int         request_count;
    picture_t   *picture;

};

vout_snapshot_t *vout_snapshot_New(void)
{
    vout_snapshot_t *snap = malloc(sizeof (*snap));
    if (unlikely(snap == NULL))
        return NULL;

    vlc_mutex_init(&snap->lock);
    vlc_cond_init(&snap->wait);

    snap->is_available = true;
    snap->request_count = 0;
    snap->picture = NULL;
    return snap;
}

void vout_snapshot_Destroy(vout_snapshot_t *snap)
{
    if (snap == NULL)
        return;

    picture_t *picture = snap->picture;
    while (picture) {
        picture_t *next = picture->p_next;
        picture_Release(picture);
        picture = next;
    }

    free(snap);
}

void vout_snapshot_End(vout_snapshot_t *snap)
{
    if (snap == NULL)
        return;

    vlc_mutex_lock(&snap->lock);

    snap->is_available = false;

    vlc_cond_broadcast(&snap->wait);
    vlc_mutex_unlock(&snap->lock);
}

/* */
picture_t *vout_snapshot_Get(vout_snapshot_t *snap, vlc_tick_t timeout)
{
    if (snap == NULL)
        return NULL;

    const vlc_tick_t deadline = vlc_tick_now() + timeout;

    vlc_mutex_lock(&snap->lock);

    /* */
    snap->request_count++;

    /* */
    while (snap->is_available && !snap->picture &&
        vlc_cond_timedwait(&snap->wait, &snap->lock, deadline) == 0);

    /* */
    picture_t *picture = snap->picture;
    if (picture)
        snap->picture = picture->p_next;
    else if (snap->request_count > 0)
        snap->request_count--;

    vlc_mutex_unlock(&snap->lock);

    return picture;
}

bool vout_snapshot_IsRequested(vout_snapshot_t *snap)
{
    if (snap == NULL)
        return false;

    bool has_request = false;
    if (!vlc_mutex_trylock(&snap->lock)) {
        has_request = snap->request_count > 0;
        vlc_mutex_unlock(&snap->lock);
    }
    return has_request;
}

void vout_snapshot_Set(vout_snapshot_t *snap,
                       const video_format_t *fmt,
                       picture_t *picture)
{
    if (snap == NULL)
        return;

    if (!fmt)
        fmt = &picture->format;

    vlc_mutex_lock(&snap->lock);
    while (snap->request_count > 0) {
        picture_t *dup = picture_Clone(picture);
        if (!dup)
            break;

        video_format_CopyCrop( &dup->format, fmt );

        dup->p_next = snap->picture;
        snap->picture = dup;
        snap->request_count--;
    }
    vlc_cond_broadcast(&snap->wait);
    vlc_mutex_unlock(&snap->lock);
}
/* */
char *vout_snapshot_GetDirectory(void)
{
    return config_GetUserDir(VLC_PICTURES_DIR);
}
/* */
int vout_snapshot_SaveImage(char **name, int *sequential,
                             const block_t *image,
                             vout_thread_t *p_vout,
                             const vout_snapshot_save_cfg_t *cfg)
{
    /* */
    char *filename;

    /* */
    char *prefix = NULL;
    if (cfg->prefix_fmt)
        prefix = str_format(NULL, NULL, cfg->prefix_fmt);
    if (prefix)
        filename_sanitize(prefix);
    else {
        prefix = strdup("vlcsnap-");
        if (prefix == NULL)
            goto error;
    }

    struct stat st;
    bool b_is_folder = false;

    if ( vlc_stat( cfg->path, &st ) == 0 )
        b_is_folder = S_ISDIR( st.st_mode );
    if ( b_is_folder ) {
        if (cfg->is_sequential) {
            for (int num = cfg->sequence; ; num++) {
                if (asprintf(&filename, "%s" DIR_SEP "%s%05d.%s",
                             cfg->path, prefix, num, cfg->format) < 0) {
                    free(prefix);
                    goto error;
                }
                if (vlc_stat(filename, &st)) {
                    *sequential = num;
                    break;
                }
                free(filename);
            }
        } else {
            struct timespec ts;
            struct tm curtime;
            char buffer[128];

            timespec_get(&ts, TIME_UTC);
            if (localtime_r(&ts.tv_sec, &curtime) == NULL)
                gmtime_r(&ts.tv_sec, &curtime);
            if (strftime(buffer, sizeof(buffer), "%Y-%m-%d-%Hh%Mm%Ss",
                         &curtime) == 0)
                strcpy(buffer, "error");

            if (asprintf(&filename, "%s" DIR_SEP "%s%s%03lu.%s",
                         cfg->path, prefix, buffer, ts.tv_nsec / 1000000,
                         cfg->format) < 0)
                filename = NULL;
        }
    } else {
        filename = strdup( cfg->path );
    }
    free(prefix);

    if (!filename)
        goto error;

    /* Save the snapshot */
    FILE *file = vlc_fopen(filename, "wb");
    if (!file) {
        msg_Err(p_vout, "Failed to open '%s'", filename);
        free(filename);
        goto error;
    }
    if (fwrite(image->p_buffer, image->i_buffer, 1, file) != 1) {
        msg_Err(p_vout, "Failed to write to '%s'", filename);
        fclose(file);
        free(filename);
        goto error;
    }
    fclose(file);

    /* */
    if (name)
        *name = filename;
    else
        free(filename);

    return VLC_SUCCESS;

error:
    msg_Err(p_vout, "could not save snapshot");
    return VLC_EGENERIC;
}

