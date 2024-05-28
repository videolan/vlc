/*****************************************************************************
 * extensions.c
 *****************************************************************************
 * Copyright (C) 2024 VideoLabs
 *
 * Authors: Vikram Kangotra <vikramkangotra8055@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_extensions.h>

int vlc_extension_VaControl( extensions_manager_t *p_mgr,
                         int i_control,
                         extension_t *ext,
                         va_list args )
{
    if (p_mgr->ops == NULL)
        return p_mgr->pf_control(p_mgr, i_control, ext, args);

    switch (i_control) {
        case EXTENSION_ACTIVATE:
            if (p_mgr->ops->activate != NULL)
                return p_mgr->ops->activate(p_mgr, ext);
            return VLC_EGENERIC;
        case EXTENSION_DEACTIVATE:
            if (p_mgr->ops->deactivate != NULL)
                return p_mgr->ops->deactivate(p_mgr, ext);
            return VLC_EGENERIC;
        case EXTENSION_IS_ACTIVATED:
        {
            bool *is_activated = va_arg(args, bool *);
            if (p_mgr->ops->is_activated != NULL)
                *is_activated = p_mgr->ops->is_activated(p_mgr, ext);
            else
                *is_activated = false;
            return VLC_SUCCESS;
        }
        case EXTENSION_HAS_MENU:
        {
            bool *has_menu = va_arg(args, bool *);
            if (p_mgr->ops->has_menu != NULL)
                *has_menu = p_mgr->ops->has_menu(p_mgr, ext);
            else
                *has_menu = false;
            return VLC_SUCCESS;
        }
        case EXTENSION_GET_MENU:
            if (p_mgr->ops->get_menu != NULL) {
                char ***pppsz_titles = va_arg(args, char ***);
                uint16_t **ppi_ids = va_arg(args, uint16_t **);
                return p_mgr->ops->get_menu(p_mgr, ext, pppsz_titles, ppi_ids);
            }
            return VLC_EGENERIC;
        case EXTENSION_TRIGGER_ONLY: 
        {
            bool *p_trigger_only = va_arg(args, bool *);
            if (p_mgr->ops->trigger_only != NULL)
                *p_trigger_only = p_mgr->ops->trigger_only(p_mgr, ext);
            else
                *p_trigger_only = false;
            return VLC_SUCCESS;
        }
        case EXTENSION_TRIGGER:
            if (p_mgr->ops->trigger != NULL)
                return p_mgr->ops->trigger(p_mgr, ext);
            return VLC_EGENERIC;
        case EXTENSION_TRIGGER_MENU:
            if (p_mgr->ops->trigger_menu != NULL) {
                int i = va_arg(args, int);
                return p_mgr->ops->trigger_menu(p_mgr, ext, i);
            }
            return VLC_EGENERIC;
        case EXTENSION_SET_INPUT:
            if (p_mgr->ops->set_input != NULL) {
                input_item_t *p_item = va_arg(args, input_item_t *);
                return p_mgr->ops->set_input(p_mgr, ext, p_item);
            }
            return VLC_EGENERIC;
        case EXTENSION_PLAYING_CHANGED:
            if (p_mgr->ops->playing_changed != NULL) {
                int i = va_arg(args, int);
                return p_mgr->ops->playing_changed(p_mgr, ext, i);
            }
            return VLC_EGENERIC;
        case EXTENSION_META_CHANGED:
            if (p_mgr->ops->meta_changed != NULL)
                return p_mgr->ops->meta_changed(p_mgr, ext);
            return VLC_EGENERIC;
        default:
            vlc_assert_unreachable();
    }
}

