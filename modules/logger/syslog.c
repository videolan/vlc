/*****************************************************************************
 * syslog.c: POSIX syslog logger plugin
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 * Copyright © 2007-2015 Rémi Denis-Courmont
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>

#include <stdarg.h>
#include <syslog.h>

static const int priorities[4] = {
    [VLC_MSG_INFO] = LOG_INFO,
    [VLC_MSG_ERR]  = LOG_ERR,
    [VLC_MSG_WARN] = LOG_WARNING,
    [VLC_MSG_DBG]  = LOG_DEBUG,
};

static void Log(void *opaque, int type, const vlc_log_t *meta,
                const char *format, va_list ap)
{
    static const char default_msg[] = "message lost";
    char *str;
    int priority = priorities[type];

    if (vasprintf(&str, format, ap) == -1)
        str = (char *)default_msg;

    if (meta->psz_header != NULL)
        syslog(priority, "[%s] %s: %s", meta->psz_header, meta->psz_module,
               str);
    else
        syslog(priority, "%s: %s", meta->psz_module, str);

    if (str != default_msg)
        free(str);
    (void) opaque;
}

/* First in list is the default facility used. */
#define DEFINE_SYSLOG_FACILITY \
  DEF("user",   LOG_USER),   \
  DEF("daemon", LOG_DAEMON), \
  DEF("local0", LOG_LOCAL0), \
  DEF("local1", LOG_LOCAL1), \
  DEF("local2", LOG_LOCAL2), \
  DEF("local3", LOG_LOCAL3), \
  DEF("local4", LOG_LOCAL4), \
  DEF("local5", LOG_LOCAL5), \
  DEF("local6", LOG_LOCAL6), \
  DEF("local7", LOG_LOCAL7)

#define DEF(a,b) a
static const char *const fac_names[] = { DEFINE_SYSLOG_FACILITY };
#undef  DEF
#define DEF(a,b) b
static const int         fac_ids[] = { DEFINE_SYSLOG_FACILITY };
#undef  DEF
#undef  DEFINE_SYSLOG_FACILITY

static int var_InheritFacility(vlc_object_t *obj, const char *varname)
{
    char *str = var_InheritString(obj, varname);
    if (unlikely(str == NULL))
        return LOG_USER; /* LOG_USEr is the spec default. */

    for (size_t i = 0; i < sizeof (fac_ids) / sizeof (fac_ids[0]); i++)
    {
        if (!strcmp(str, fac_names[i]))
        {
            free(str);
            return fac_ids[i];
        }
    }

    msg_Warn(obj, "unknown syslog facility \"%s\"", str);
    free(str);
    return LOG_USER;
}

static const char default_ident[] = PACKAGE;

static vlc_log_cb Open(vlc_object_t *obj, void **sysp)
{
    if (!var_InheritBool(obj, "syslog"))
        return NULL;

    char *ident = var_InheritString(obj, "syslog-ident");
    if (ident == NULL)
        ident = (char *)default_ident;
    *sysp = ident;

    /* Open log */
    int facility = var_InheritFacility(obj, "syslog-facility");

    openlog(ident, LOG_PID | LOG_NDELAY, facility);

    /* Set priority filter */
    int mask = LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_INFO);
    if (var_InheritBool(obj, "syslog-debug"))
        mask |= LOG_MASK(LOG_DEBUG);

    setlogmask(mask);

    return Log;
}

static void Close(void *opaque)
{
    char *ident = opaque;

    closelog();
    if (ident != default_ident)
        free(ident);
}

#define SYSLOG_TEXT N_("System log (syslog)")
#define SYSLOG_LONGTEXT N_("Emit log messages through the POSIX system log.")

#define SYSLOG_DEBUG_TEXT N_("Debug messages")
#define SYSLOG_DEBUG_LONGTEXT N_("Include debug messages in system log.")

#define SYSLOG_IDENT_TEXT N_("Identity")
#define SYSLOG_IDENT_LONGTEXT N_("Process identity in system log.")

#define SYSLOG_FACILITY_TEXT N_("Facility")
#define SYSLOG_FACILITY_LONGTEXT N_("System logging facility.")

vlc_module_begin()
    set_shortname(N_( "syslog" ))
    set_description(N_("System logger (syslog)"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("logger", 20)
    set_callbacks(Open, Close)

    add_bool("syslog", false, SYSLOG_TEXT, SYSLOG_LONGTEXT,
             false)
    add_bool("syslog-debug", false, SYSLOG_DEBUG_TEXT, SYSLOG_DEBUG_LONGTEXT,
             false)
    add_string("syslog-ident", default_ident, SYSLOG_IDENT_TEXT,
               SYSLOG_IDENT_LONGTEXT, true)
    add_string("syslog-facility", fac_names[0], SYSLOG_FACILITY_TEXT,
               SYSLOG_FACILITY_LONGTEXT, true)
        change_string_list(fac_names, fac_names)
vlc_module_end()
