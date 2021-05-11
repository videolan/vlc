/*****************************************************************************
 * http.c: HTTP PUT logger plugin
 *****************************************************************************
 * Copyright © 2021 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>

#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#include "../access/http/outfile.h"
#include "../access/http/connmgr.h"

#define CFG_PREFIX "http-logger-"

typedef struct
{
    /* State lock for the logger */
    vlc_mutex_t http_lock;

    /* HTTP stack variables */
    char *endpoint, *ua, *username, *password;
    struct vlc_http_mgr *http_mgr;
    struct vlc_http_outfile *http_stream;

    /* Logger state */
    int verbosity;
} vlc_logger_sys_t;


/* The blocks are dummy blocks and need not be released */
static void dummy_block_release(block_t *block)
    { (void)block; }

static const struct vlc_block_callbacks dummy_block_cbs =
    { dummy_block_release };

/* Logger callback for sending log */
static void LogHttp(void *opaque, int type, const vlc_log_t *meta,
                    const char *format, va_list ap)
{
    vlc_logger_sys_t *sys = opaque;

    if (sys->verbosity < type)
        return;

    static const char *msg_types[4] = {
        "", " error", " warning", " debug" };
    const char *msg_type = "";
    if (type >= 0 && (size_t)type < ARRAY_SIZE(msg_types))
        msg_type = msg_types[type];

    /* The logs don't have endlines, so add one here.
     * TODO: should the chunk be exposed at the vlc_http level
     * to add the new line separately? */
    char *http_format;
    if (asprintf(&http_format, "%s %s %s: %s\n", meta->psz_object_type,
                 meta->psz_module, msg_type, format) < 0)
        return;

    /* The logs are not formatted and needs to be to write into the
     * http stream, so allocate memory for that. */
    char *output = NULL;
    int size = vasprintf(&output, http_format, ap);
    free(http_format);

    if (size < 0)
        return;

    /* Initialize a dummy block to send the formatted log to the HTTP
     * stack. The formatted log won't be released by the HTTP stack. */
    block_t block;
    block_Init(&block, &dummy_block_cbs, output, size);

    /* Lock between logs so that the HTTP stack is not cluttered. */
    vlc_mutex_lock(&sys->http_lock);
    /* TODO: http write failure case is not handled. */
    /* int ret = */ vlc_http_outfile_write(sys->http_stream, &block);
    vlc_mutex_unlock(&sys->http_lock);
    free(output);
}

static void Close(void *opaque)
{
    vlc_logger_sys_t *sys = opaque;

    vlc_http_outfile_close(sys->http_stream);
    vlc_http_mgr_destroy(sys->http_mgr);

    free(sys->ua);
    free(sys->username);
    free(sys->password);
    free(sys->endpoint);
    free(sys);
}

static const struct vlc_logger_operations *Open(vlc_object_t *obj,
                                                void **restrict sysp)
{
    if (!var_InheritBool(obj, "http-logging"))
        return NULL;

    int verbosity = var_InheritInteger(obj, "verbose");
    if (verbosity < 0)
        return NULL; /* nothing to log */
    verbosity += VLC_MSG_ERR;

    char *endpoint = var_InheritString(obj, CFG_PREFIX "endpoint");
    if (endpoint == NULL)
        return NULL;

    vlc_logger_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
    {
        free(endpoint);
        return NULL;
    }

    vlc_mutex_init(&sys->http_lock);
    sys->http_stream = NULL;
    sys->verbosity = verbosity;
    sys->ua = var_InheritString(obj, "http-user-agent");
    sys->username = var_InheritString(obj, CFG_PREFIX "username");
    sys->password = var_InheritString(obj, CFG_PREFIX "password");
    sys->endpoint = endpoint;

    sys->http_mgr = vlc_http_mgr_create(obj, NULL);
    if (sys->http_mgr == NULL)
        goto error;

    /* Open the log file and remove any buffering for the stream */
    sys->http_stream = vlc_http_outfile_create(sys->http_mgr,
            endpoint, sys->ua, sys->username, sys->password);

    if (sys->http_stream == NULL)
    {
        /* TODO: can we log here? */
        msg_Err(obj, "error opening endpoint `%s': %s", endpoint,
                vlc_strerror_c(errno) );
        goto error;
    }

    *sysp = sys;

    static const struct vlc_logger_operations http_logger_ops =
        { LogHttp, Close };
    return &http_logger_ops;

error:
    if (sys->http_mgr)
        vlc_http_mgr_destroy(sys->http_mgr);

    free(sys->ua);
    free(sys->username);
    free(sys->password);

    free(sys);
    free(endpoint);
    return NULL;
}

#define HTTP_LOGGING_TEXT "Enable HTTP logging"
#define HTTP_LOGGING_LONGTEXT \
    "Enable HTTP logging to the HTTP endpoint from " \
    CFG_PREFIX "endpoint."

#define HTTP_ENDPOINT_TEXT "HTTP logging endpoint"
#define HTTP_ENDPOINT_LONGTEXT \
    "HTTP endpoint where the HTTP logger will send PUT request"

#define HTTP_USERNAME_TEXT "HTTP logging username"
#define HTTP_PASSWORD_TEXT "HTTP logging password"

vlc_module_begin()
    set_shortname(N_("HTTP Logger"))
    set_description(N_("HTTP logger"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("logger", 20)
    set_callback(Open)

    add_bool("http-logging", false,
             HTTP_LOGGING_TEXT, HTTP_LOGGING_LONGTEXT, false)
    add_string(CFG_PREFIX "endpoint", "",
               HTTP_ENDPOINT_TEXT, HTTP_ENDPOINT_LONGTEXT, false)
    add_string(CFG_PREFIX "username", "",
               HTTP_USERNAME_TEXT, "", false)
    add_string(CFG_PREFIX "password", "",
               HTTP_PASSWORD_TEXT, "", false)
vlc_module_end ()
