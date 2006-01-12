/*****************************************************************************
 * macros.h : Macros mapping for the HTTP interface
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include "http.h"

enum macroType
{
    MVLC_UNKNOWN = 0,
    MVLC_CONTROL,
        MVLC_PLAY,
        MVLC_STOP,
        MVLC_PAUSE,
        MVLC_NEXT,
        MVLC_PREVIOUS,
        MVLC_ADD,
        MVLC_DEL,
        MVLC_EMPTY,
        MVLC_SEEK,
        MVLC_KEEP,
        MVLC_SORT,
        MVLC_MOVE,
        MVLC_VOLUME,
        MVLC_FULLSCREEN,

        MVLC_CLOSE,
        MVLC_SHUTDOWN,

        MVLC_VLM_NEW,
        MVLC_VLM_SETUP,
        MVLC_VLM_DEL,
        MVLC_VLM_PLAY,
        MVLC_VLM_PAUSE,
        MVLC_VLM_STOP,
        MVLC_VLM_SEEK,
        MVLC_VLM_LOAD,
        MVLC_VLM_SAVE,

    MVLC_INCLUDE,
    MVLC_FOREACH,
    MVLC_IF,
    MVLC_RPN,
    MVLC_STACK,
    MVLC_ELSE,
    MVLC_END,
    MVLC_GET,
    MVLC_SET,
        MVLC_INT,
        MVLC_FLOAT,
        MVLC_STRING,

    MVLC_VALUE
};

/* Static mapping of macros type to macro strings */
static struct
{
    char *psz_name;
    int  i_type;
}
StrToMacroTypeTab [] =
{
    { "control",    MVLC_CONTROL },
        /* player control */
        { "play",           MVLC_PLAY },
        { "stop",           MVLC_STOP },
        { "pause",          MVLC_PAUSE },
        { "next",           MVLC_NEXT },
        { "previous",       MVLC_PREVIOUS },
        { "seek",           MVLC_SEEK },
        { "keep",           MVLC_KEEP },
        { "fullscreen",     MVLC_FULLSCREEN },
        { "volume",         MVLC_VOLUME },

        /* playlist management */
        { "add",            MVLC_ADD },
        { "delete",         MVLC_DEL },
        { "empty",          MVLC_EMPTY },
        { "sort",           MVLC_SORT },
        { "move",           MVLC_MOVE },

        /* admin control */
        { "close",          MVLC_CLOSE },
        { "shutdown",       MVLC_SHUTDOWN },

        /* vlm control */
        { "vlm_new",        MVLC_VLM_NEW },
        { "vlm_setup",      MVLC_VLM_SETUP },
        { "vlm_del",        MVLC_VLM_DEL },
        { "vlm_play",       MVLC_VLM_PLAY },
        { "vlm_pause",      MVLC_VLM_PAUSE },
        { "vlm_stop",       MVLC_VLM_STOP },
        { "vlm_seek",       MVLC_VLM_SEEK },
        { "vlm_load",       MVLC_VLM_LOAD },
        { "vlm_save",       MVLC_VLM_SAVE },

    { "rpn",        MVLC_RPN },
    { "stack",      MVLC_STACK },

    { "include",    MVLC_INCLUDE },
    { "foreach",    MVLC_FOREACH },
    { "value",      MVLC_VALUE },

    { "if",         MVLC_IF },
    { "else",       MVLC_ELSE },
    { "end",        MVLC_END },
    { "get",        MVLC_GET },
    { "set",        MVLC_SET },
        { "int",            MVLC_INT },
        { "float",          MVLC_FLOAT },
        { "string",         MVLC_STRING },

    /* end */
    { NULL,         MVLC_UNKNOWN }
};
