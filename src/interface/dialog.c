/*****************************************************************************
 * dialog.c: User dialog functions
 *****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

/**
 * \file dialog.c
 * User dialogs core
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_dialog.h>
#include <assert.h>
#include "libvlc.h"

static vlc_mutex_t provider_lock = VLC_STATIC_MUTEX;

#undef dialog_Register
/**
 * Registers an object as the dialog provider.
 * It is assumed that the appropriate variable callbacks are already
 * registered.
 */
int dialog_Register (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    int ret = VLC_EGENERIC;

    vlc_mutex_lock (&provider_lock);
    if (priv->p_dialog_provider == NULL)
    {   /* Since the object is responsible for unregistering itself before
         * it terminates, at reference is not needed. */
        priv->p_dialog_provider = obj;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock (&provider_lock);
    return ret;
}

#undef dialog_Unregister
/**
 * Unregisters the dialog provider.
 * Note that unless you have unregistered the callbacks already, the provider
 * might still be in use by other threads. Also, you need to cancel all
 * pending dialogs yourself.
 */
int dialog_Unregister (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    int ret = VLC_EGENERIC;

    vlc_mutex_lock (&provider_lock);
    if (priv->p_dialog_provider == obj)
    {
        priv->p_dialog_provider = NULL;
        ret = VLC_SUCCESS;
    }
    vlc_mutex_unlock (&provider_lock);
    return ret;
}

static vlc_object_t *dialog_GetProvider (vlc_object_t *obj)
{
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);
    vlc_object_t *provider;

    vlc_mutex_lock (&provider_lock);
    if ((provider = priv->p_dialog_provider) != NULL)
        vlc_object_hold (provider);
    vlc_mutex_unlock (&provider_lock);
    return provider;
}

/**
 * Sends an error message through the user interface (if any).
 * @param obj the VLC object emitting the error
 * @param modal whether to wait for user to acknowledge the error
 *              before returning control to the caller
 * @param title title of the error dialog
 * @param fmt format string for the error message
 * @param ap parameters list for the formatted error message
 */
void dialog_VFatal (vlc_object_t *obj, bool modal, const char *title,
                    const char *fmt, va_list ap)
{
    char *text;

    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
    {
        msg_Err (obj, "%s", title);
        msg_GenericVa (obj, VLC_MSG_ERR, MODULE_STRING, fmt, ap);
        return;
    }

    if (vasprintf (&text, fmt, ap) != -1)
    {
        dialog_fatal_t dialog = { title, text, modal, };
        var_SetAddress (provider, "dialog-fatal", &dialog);
        free (text);
    }
    vlc_object_release (provider);
}

#undef dialog_Login
/**
 * Requests a username and password through the user interface.
 * @param obj the VLC object requesting credential informations
 * @param username a pointer to the specified username [OUT]
 * @param password a pointer to the specified password [OUT]
 * @param title title for the dialog
 * @param text format string for the message in the dialog
 * @return Nothing. If a user name resp. a password was specified,
 * it will be returned as a heap-allocated character array
 * into the username resp password pointer. Those must be freed with free().
 * Otherwise *username resp *password will be NULL.
 */
void dialog_Login (vlc_object_t *obj, char **username, char **password,
                   const char *title, const char *fmt, ...)
{
    assert ((username != NULL) && (password != NULL));

    *username = *password = NULL;
    if (obj->i_flags & OBJECT_FLAGS_NOINTERACT)
        return;

    vlc_object_t *provider = dialog_GetProvider (obj);
    if (provider == NULL)
        return;

    char *text;
    va_list ap;

    va_start (ap, fmt);
    if (vasprintf (&text, fmt, ap) != -1)
    {
        dialog_login_t dialog = { title, text, username, password, };
        var_SetAddress (provider, "dialog-login", &dialog);
        free (text);
    }
    va_end (ap);
    vlc_object_release (provider);
}
