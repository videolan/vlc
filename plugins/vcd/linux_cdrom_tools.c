/****************************************************************************
 * linux_cdrom_tools.c: linux cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
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
 * Functions declared in this file use lots of ioctl calls, thus they are 
 * Linux-specific.                                           
 ****************************************************************************/


#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#include <sys/ioctl.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "intf_msg.h"

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"


#include "input_vcd.h"
#include "linux_cdrom_tools.h"
/*****************************************************************************
* read_toc : Reads the Table of Content of a CD-ROM and fills p_vcd with     *
*            the read information                                            *
*****************************************************************************/
int read_toc ( thread_vcd_data_t * p_vcd )
{ 
    int i ;
    struct cdrom_tochdr tochdr ;
    struct cdrom_tocentry tocent ;
    int fd = p_vcd->vcdhandle ;
 
    /* first we read the TOC header */
    if (ioctl(fd, CDROMREADTOCHDR, &tochdr) == -1)
    {
        intf_ErrMsg("problem occured when reading CD's TOCHDR\n") ;
        return -1 ;
    }
  
    p_vcd->nb_tracks = tochdr.cdth_trk1;
    /* nb_tracks + 1 because we put the lead_out tracks for computing last
     track's size */
    p_vcd->tracks_sector = malloc( (p_vcd->nb_tracks + 1) 
                                            * sizeof( int ) );
    if ( p_vcd->tracks_sector == NULL ) 
    {
        intf_ErrMsg("could not malloc tracks_sector");
        return -1;
    }

    /* then for each track we read its TOC entry */

    for(i=tochdr.cdth_trk0 ;i<=tochdr.cdth_trk1;i++)
    {
        tocent.cdte_track = i ;
        tocent.cdte_format = CDROM_LBA ;
        if (ioctl(fd, CDROMREADTOCENTRY, &tocent) == -1)
        {
          intf_ErrMsg("problem occured when reading CD's TOCENTRY\n") ;
          free (p_vcd->tracks_sector) ;
          return -1 ;
        }
    
        p_vcd->tracks_sector[i] = tocent.cdte_addr.lba ;

    }
  
    /* finally we read the lead-out track toc entry */

    tocent.cdte_track = CDROM_LEADOUT ;
    tocent.cdte_format = CDROM_LBA ;
    if (ioctl(fd, CDROMREADTOCENTRY, &tocent) == -1)
    {
        intf_ErrMsg("problem occured when readind CD's 
                   lead-out track TOC entry") ;
        free (p_vcd->tracks_sector) ;
        return -1 ;
    }

    p_vcd->tracks_sector[p_vcd->nb_tracks + 1] = tocent.cdte_addr.lba ;
  
    return 1 ;

}
   
/****************************************************************************
 * VCD_sector_read : Function that reads a sector (2324 bytes) from VCD   
 ****************************************************************************/
int VCD_sector_read ( struct thread_vcd_data_s * p_vcd, byte_t * p_buffer )
{
    byte_t                        p_read_block[VCD_SECTOR_SIZE] ;
    struct cdrom_msf0             msf_cursor ;
    
    msf_cursor = lba2msf( p_vcd->current_sector ) ;
   
#ifdef DEBUG
    intf_DbgMsg("Playing frame %d:%d-%d\n", msf_cursor.minute, 
                msf_cursor.second, msf_cursor.frame) ;
#endif
   
    memcpy(p_read_block, &msf_cursor, sizeof(struct cdrom_msf0)) ;
        
    if (ioctl(p_vcd->vcdhandle, CDROMREADRAW, p_read_block) == -1)
    {
        intf_ErrMsg("problem occured when reading CD") ;
        free (p_read_block) ;
        return -1 ;
    }
        
    /* we don't want to keep the header of the read sector */
    memcpy( p_buffer, &p_read_block[VCD_DATA_START], 
            VCD_DATA_SIZE );
    
    
    p_vcd->current_sector ++;
    
    if ( p_vcd->current_sector == 
            p_vcd->tracks_sector[p_vcd->current_track + 1] )
    {
        p_vcd->b_end_of_track = 1;
    }
    
    return 1;
}


/*****************************************************************************
 * lba2msf : converts a logical block address into a minute/second/frame
 *           address.
 *****************************************************************************/

struct cdrom_msf0 lba2msf( int lba)
{
    struct cdrom_msf0                      msf_result ;

    /* we add 2*CD_FRAMES since the 2 first seconds are not played*/
    
    msf_result.minute = (lba+2*CD_FRAMES) / ( CD_FRAMES * CD_SECS ) ;
    msf_result.second = ( (lba+2*CD_FRAMES) % ( CD_FRAMES * CD_SECS ) ) 
        / CD_FRAMES ;
    msf_result.frame = ( (lba+2*CD_FRAMES) % ( CD_FRAMES * CD_SECS ) ) 
        % CD_FRAMES ;
    return msf_result ;
}
