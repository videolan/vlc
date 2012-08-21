/*****************************************************************************
 * snapshot.c : vout internal snapshot
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_block.h>

#include "snapshot.h"

/* */
void vout_snapshot_Init(vout_snapshot_t *snap)
{
    vlc_mutex_init(&snap->lock);
    vlc_cond_init(&snap->wait);

    snap->is_available = true;
    snap->request_count = 0;
    snap->picture = NULL;
}
void vout_snapshot_Clean(vout_snapshot_t *snap)
{
    picture_t *picture = snap->picture;
    while (picture) {
        picture_t *next = picture->p_next;
        picture_Release(picture);
        picture = next;
    }

    vlc_cond_destroy(&snap->wait);
    vlc_mutex_destroy(&snap->lock);
}

void vout_snapshot_End(vout_snapshot_t *snap)
{
    vlc_mutex_lock(&snap->lock);

    snap->is_available = false;

    vlc_cond_broadcast(&snap->wait);
    vlc_mutex_unlock(&snap->lock);
}

/* */
picture_t *vout_snapshot_Get(vout_snapshot_t *snap, mtime_t timeout)
{
    vlc_mutex_lock(&snap->lock);

    /* */
    snap->request_count++;

    /* */
    const mtime_t deadline = mdate() + timeout;
    while (snap->is_available && !snap->picture && mdate() < deadline)
        vlc_cond_timedwait(&snap->wait, &snap->lock, deadline);

    /* */
    picture_t *picture = snap->picture;
    if (picture)
        snap->picture = picture->p_next;
    else if (snap->request_count > 0)
        snap->request_count--;

    vlc_mutex_unlock(&snap->lock);

    return picture;
}

/* */
bool vout_snapshot_IsRequested(vout_snapshot_t *snap)
{
    bool has_request = false;
    if (!vlc_mutex_trylock(&snap->lock)) {
        has_request = snap->request_count > 0;
        vlc_mutex_unlock(&snap->lock);
    }
    return has_request;
}
void vout_snapshot_Set(vout_snapshot_t *snap,
                       const video_format_t *fmt,
                       const picture_t *picture)
{
    if (!fmt)
        fmt = &picture->format;

    vlc_mutex_lock(&snap->lock);
    while (snap->request_count > 0) {
        picture_t *dup = picture_NewFromFormat(fmt);
        if (!dup)
            break;

        picture_Copy(dup, picture);

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
                             vlc_object_t *object,
                             const vout_snapshot_save_cfg_t *cfg)
{
    /* */
    char *filename;
    DIR *pathdir = vlc_opendir(cfg->path);
    if (pathdir != NULL) {
        /* The use specified a directory path */
        closedir(pathdir);

        /* */
        char *prefix = NULL;
        if (cfg->prefix_fmt)
            prefix = str_format_time(cfg->prefix_fmt);
        if (prefix)
            filename_sanitize(prefix);
        else {
            prefix = strdup("vlcsnap-");
            if (!prefix)
                goto error;
        }

        if (cfg->is_sequential) {
            for (int num = cfg->sequence; ; num++) {
                struct stat st;

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
            struct tm    curtime;
            time_t       lcurtime = time(NULL) ;

            if (!localtime_r(&lcurtime, &curtime)) {
                const unsigned int id = (image->i_pts / 100000) & 0xFFFFFF;

                msg_Warn(object, "failed to get current time. Falling back to legacy snapshot naming");

                if (asprintf(&filename, "%s" DIR_SEP "%s%u.%s",
                             cfg->path, prefix, id, cfg->format) < 0)
                    filename = NULL;
            } else {
                /* suffix with the last decimal digit in 10s of seconds resolution
                 * FIXME gni ? */
                const int id = (image->i_pts / (100*1000)) & 0xFF;
                char buffer[128];

                if (!strftime(buffer, sizeof(buffer), "%Y-%m-%d-%Hh%Mm%Ss", &curtime))
                    strcpy(buffer, "error");

                if (asprintf(&filename, "%s" DIR_SEP "%s%s%1u.%s",
                             cfg->path, prefix, buffer, id, cfg->format) < 0)
                    filename = NULL;
            }
        }
        free(prefix);
    } else {
        /* The user specified a full path name (including file name) */
        filename = str_format_time(cfg->path);
        path_sanitize(filename);
    }

    if (!filename)
        goto error;

    /* Save the snapshot */
    FILE *file = vlc_fopen(filename, "wb");
    if (!file) {
        msg_Err(object, "Failed to open '%s'", filename);
        free(filename);
        goto error;
    }
    if (fwrite(image->p_buffer, image->i_buffer, 1, file) != 1) {
        msg_Err(object, "Failed to write to '%s'", filename);
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
    msg_Err(object, "could not save snapshot");
    return VLC_EGENERIC;
}

