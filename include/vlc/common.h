/*****************************************************************************
 * vlc.h: global header for libvlc (old-style)
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
 * \defgroup libvlc_old Libvlc Old
 * This is libvlc, the base library of the VLC program.
 * This is the legacy API. Please consider using the new libvlc API
 *
 * @{
 */


#ifndef _VLC_COMMON_H
#define _VLC_COMMON_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/*****************************************************************************
 * Our custom types
 *****************************************************************************/
typedef struct vlc_list_t vlc_list_t; /* (shouldn't be exposed) */
typedef struct vlc_object_t vlc_object_t; /* (shouldn't be exposed) */

#if (defined( WIN32 ) || defined( UNDER_CE )) && !defined( __MINGW32__ )
typedef signed __int64 vlc_int64_t;
# else
typedef signed long long vlc_int64_t;
#endif

/**
 * VLC value structure (shouldn't be exposed)
 */
typedef union
{
    int             i_int;
    bool      b_bool;
    float           f_float;
    char *          psz_string;
    void *          p_address;
    vlc_object_t *  p_object;
    vlc_list_t *    p_list;
    vlc_int64_t     i_time;

    struct { char *psz_name; int i_object_id; } var;

   /* Make sure the structure is at least 64bits */
    struct { char a, b, c, d, e, f, g, h; } padding;

} vlc_value_t;

/**
 * VLC list structure  (shouldn't be exposed)
 */
struct vlc_list_t
{
    int             i_count;
    vlc_value_t *   p_values;
    int *           pi_types;

};

/*****************************************************************************
 * Error values (shouldn't be exposed)
 *****************************************************************************/
#define VLC_SUCCESS         -0                                   /* No error */
#define VLC_ENOMEM          -1                          /* Not enough memory */
#define VLC_ETHREAD         -2                               /* Thread error */
#define VLC_ETIMEOUT        -3                                    /* Timeout */

#define VLC_ENOMOD         -10                           /* Module not found */

#define VLC_ENOOBJ         -20                           /* Object not found */
#define VLC_EBADOBJ        -21                            /* Bad object type */

#define VLC_ENOVAR         -30                         /* Variable not found */
#define VLC_EBADVAR        -31                         /* Bad variable value */

#define VLC_ENOITEM        -40                           /**< Item not found */

#define VLC_EEXIT         -255                             /* Program exited */
#define VLC_EEXITSUCCESS  -999                /* Program exited successfully */
#define VLC_EGENERIC      -666                              /* Generic error */

/**
 * \defgroup var_type Variable types  (shouldn't be exposed)
 * These are the different types a vlc variable can have.
 * @{
 */
#define VLC_VAR_VOID      0x0010
#define VLC_VAR_BOOL      0x0020
#define VLC_VAR_INTEGER   0x0030
#define VLC_VAR_HOTKEY    0x0031
#define VLC_VAR_STRING    0x0040
#define VLC_VAR_MODULE    0x0041
#define VLC_VAR_FILE      0x0042
#define VLC_VAR_DIRECTORY 0x0043
#define VLC_VAR_VARIABLE  0x0044
#define VLC_VAR_FLOAT     0x0050
#define VLC_VAR_TIME      0x0060
#define VLC_VAR_ADDRESS   0x0070
#define VLC_VAR_MUTEX     0x0080
#define VLC_VAR_LIST      0x0090
/**@}*/

/*****************************************************************************
 * Required internal headers
 *****************************************************************************/
#if defined( __LIBVLC__ )
#   include "vlc_common.h"
#endif


/*****************************************************************************
 * Shared library Export macros
 *****************************************************************************/
#ifndef VLC_PUBLIC_API
#  define VLC_PUBLIC_API extern
#endif

/*****************************************************************************
 * Compiler specific
 *****************************************************************************/

#ifndef VLC_DEPRECATED_API
# ifdef __LIBVLC__
/* Avoid unuseful warnings from libvlc with our deprecated APIs */
#    define VLC_DEPRECATED_API VLC_PUBLIC_API
# else /* __LIBVLC__ */
#  if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#    define VLC_DEPRECATED_API VLC_PUBLIC_API __attribute__((deprecated))
#  else
#    define VLC_DEPRECATED_API VLC_PUBLIC_API
#  endif
# endif /* __LIBVLC__ */
#endif


# ifdef __cplusplus
}
# endif


#endif /* _VLC_COMMON_H */
