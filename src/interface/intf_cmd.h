/*****************************************************************************
 * intf_cmd.h: interface commands parsing and executions functions
 * This file implements the interface commands execution functions. It is used
 * by command-line oriented interfaces and scripts. The commands themselves are
 * implemented in intf_ctrl.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_cmd.h,v 1.4 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Required headers:
 *  none
 *****************************************************************************/

/*****************************************************************************
 * intf_arg_t: control fonction argument descriptor
 *****************************************************************************
 * This structure is used to control an argument type and to transmit
 * arguments to control functions. It is also used to parse format string and
 * build an easier to use array of arguments.
 *****************************************************************************/
typedef struct
{
    /* Argument type */
    int         i_flags;                          /* argument type and flags */
    int         i_index;                   /* index of mask in format string */

    /* Converted arguments value */
    char *      psz_str;                                     /* string value */
    char *      ps_name;              /* name, can be '\0' or '=' terminated */
    long        i_num;                                      /* integer value */
    float       f_num;                                        /* float value */
} intf_arg_t;

/* Arguments flags */
#define INTF_STR_ARG            1                         /* string argument */
#define INTF_INT_ARG            2                        /* integer argument */
#define INTF_FLOAT_ARG          4                          /* float argument */
#define INTF_NAMED_ARG          16                         /* named argument */
#define INTF_OPT_ARG            256                    /* optionnal argument */
#define INTF_REP_ARG            512              /* argument can be repeated */
#define INTF_PRESENT_ARG        1024        /* argument has been encountered */

/*****************************************************************************
 * intf_command_t: control command descriptor
 *****************************************************************************
 * This structure describes a control commands. It stores informations needed
 * for argument type checking, command execution but also a short inline help.
 * See control.c for more informations about fields.
 *****************************************************************************/
typedef struct
{
    /* Function control */
    char *          psz_name;                                /* command name */
    int (* function)( int i_argc, intf_arg_t *p_argv );          /* function */
    char *          psz_format;                          /* arguments format */

    /* Function informations */
    char *          psz_summary;                                /* info text */
    char *          psz_usage;                                 /* usage text */
    char *          psz_help;                                   /* help text */
} intf_command_t;

/*****************************************************************************
 * Error constants
 *****************************************************************************
 * These errors should be used as return values for control functions (see
 * control.c). The intf_ExecCommand function as different behaviour depending
 * of the error it received. Other errors numbers can be used, but their valued
 * should be positive to avoid conflict with future error codes.
 *****************************************************************************/

#define INTF_NO_ERROR       0                                     /* success */
#define INTF_FATAL_ERROR    -1          /* fatal error: the program will end */
#define INTF_CRITICAL_ERROR -2      /* critical error: the program will exit */
#define INTF_USAGE_ERROR    -3 /* usage error: command usage will be displayed */
#define INTF_OTHER_ERROR    -4/* other error: command prints its own message */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int intf_ExecCommand    ( char *psz_cmd );
int intf_ExecScript     ( char *psz_filename );
