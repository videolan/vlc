/*****************************************************************************
 * debug.h: vlc debug macros
 * Stand alone file
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Benoît Steiner <benny@via.ecp.fr>
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
 * - <string.h>
 * - intf_msg.h
 *****************************************************************************/


/*****************************************************************************
 * ASSERT
 *****************************************************************************
 * This macro is used to test that a pointer is not nul. It insert the needed
 * code when the program is compiled with the debug option, but does nothing
 * in release program.
 *****************************************************************************/
#ifdef DEBUG
#define ASSERT(p_Mem)                                                         \
if (!(p_Mem))                                                                 \
    intf_ErrMsg("Void pointer error: "                                        \
                "%s line %d (variable %s at address %p)\n",                   \
                 __FILE__, __LINE__, #p_Mem, &p_Mem);

#else
#define ASSERT(p_Mem)

#endif


/*****************************************************************************
 * RZERO
 *****************************************************************************
 * This macro is used to initialise a variable to 0. It is very useful when
 * used with the ASSERT macro. It also only insert the needed code when the
 * program is compiled with the debug option.
 *****************************************************************************/
#ifdef DEBUG
#define RZERO(r_Var)                                                          \
bzero(&(r_Var), sizeof((r_Var)));

#else
#define RZERO(r_Var)

#endif


/*****************************************************************************
 * PZERO
 *****************************************************************************
 * This macro is used to initiase the memory pointed out by a pointer to 0.
 * It has the same purpose than RZERO, but for pointers.
 *****************************************************************************/
/* It is already defined on BSD */
#ifndef PZERO
#ifdef DEBUG
#define PZERO(p_Mem)                                                          \
bzero((p_Mem), sizeof(*(p_Mem)));

#else
#define PZERO(p_Mem)

#endif
#endif


/*****************************************************************************
 * AZERO
 *****************************************************************************
 * This macro is used to initiase an array of variables to 0.
 * It has the same purpose than RZERO or PZERO, but for array
 *****************************************************************************/
#ifdef DEBUG
#define AZERO(p_Array, i_Size)                                                \
bzero((p_Array), (i_Size)*sizeof(*(p_Array)));

#else
#define ZERO(p_Array, i_Size)

#endif
