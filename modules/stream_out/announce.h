/*****************************************************************************
 * announce.h : Session announcement
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/

#ifdef HAVE_SLP_H
#   include <slp.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

/*****************************************************************************
 * slp_session_t: SLP Session descriptor
 *****************************************************************************/
struct slp_session_t
{
        char *psz_url;
        char *psz_name;
};

typedef struct slp_session_t slp_session_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int              sout_SLPReg        (sout_instance_t *,char *,char *);
int              sout_SLPDereg      (sout_instance_t *,char *,char *);

