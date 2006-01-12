/****************************************************************************
 * cdrom.c: cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#endif

#if defined( SYS_BSDI )
#   include <dvd.h>
#elif defined ( SYS_DARWIN )
#   include <CoreFoundation/CFBase.h>
#   include <IOKit/IOKitLib.h>
#   include <IOKit/storage/IOCDTypes.h>
#   include <IOKit/storage/IOCDMedia.h>
#   include <IOKit/storage/IOCDMediaBSDClient.h>
#elif defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
#   include <sys/inttypes.h>
#   include <sys/cdio.h>
#   include <sys/scsiio.h>
#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
#   include <sys/cdio.h>
#   include <sys/cdrio.h>
#elif defined( WIN32 )
#   include <windows.h>
#   include <winioctl.h>
#else
#   include <linux/cdrom.h>
#endif

#include "cdrom_internals.h"
#include "cdrom.h"

/*****************************************************************************
 * ioctl_Open: Opens a VCD device or file and returns an opaque handle
 *****************************************************************************/
vcddev_t *ioctl_Open( vlc_object_t *p_this, const char *psz_dev )
{
    int i_ret;
    int b_is_file;
    vcddev_t *p_vcddev;
#ifndef WIN32
    struct stat fileinfo;
#endif

    if( !psz_dev ) return NULL;

    /*
     *  Initialize structure with default values
     */
    p_vcddev = (vcddev_t *)malloc( sizeof(vcddev_t) );
    if( p_vcddev == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return NULL;
    }
    p_vcddev->i_vcdimage_handle = -1;
    p_vcddev->psz_dev = NULL;
    b_is_file = 1;

    /*
     *  Check if we are dealing with a device or a file (vcd image)
     */
#ifdef WIN32
    if( (strlen( psz_dev ) == 2 && psz_dev[1] == ':') )
    {
        b_is_file = 0;
    }

#else
    if( stat( psz_dev, &fileinfo ) < 0 )
    {
        free( p_vcddev );
        return NULL;
    }

    /* Check if this is a block/char device */
    if( S_ISBLK( fileinfo.st_mode ) || S_ISCHR( fileinfo.st_mode ) )
        b_is_file = 0;
#endif

    if( b_is_file )
    {
        i_ret = OpenVCDImage( p_this, psz_dev, p_vcddev );
    }
    else
    {
        /*
         *  open the vcd device
         */

#ifdef WIN32
        i_ret = win32_vcd_open( p_this, psz_dev, p_vcddev );
#else
        p_vcddev->i_device_handle = -1;
        p_vcddev->i_device_handle = open( psz_dev, O_RDONLY | O_NONBLOCK );
        i_ret = (p_vcddev->i_device_handle == -1) ? -1 : 0;
#endif
    }

    if( i_ret == 0 )
    {
        p_vcddev->psz_dev = (char *)strdup( psz_dev );
    }
    else
    {
        free( p_vcddev );
        p_vcddev = NULL;
    }

    return p_vcddev;
}

/*****************************************************************************
 * ioctl_Close: Closes an already opened VCD device or file.
 *****************************************************************************/
void ioctl_Close( vlc_object_t * p_this, vcddev_t *p_vcddev )
{
    if( p_vcddev->psz_dev ) free( p_vcddev->psz_dev );

    if( p_vcddev->i_vcdimage_handle != -1 )
    {
        /*
         *  vcd image mode
         */

        CloseVCDImage( p_this, p_vcddev );
        return;
    }

    /*
     *  vcd device mode
     */

#ifdef WIN32
    if( p_vcddev->h_device_handle )
        CloseHandle( p_vcddev->h_device_handle );
    if( p_vcddev->hASPI )
        FreeLibrary( (HMODULE)p_vcddev->hASPI );
#else
    if( p_vcddev->i_device_handle != -1 )
        close( p_vcddev->i_device_handle );
#endif
}

/*****************************************************************************
 * ioctl_GetTracksMap: Read the Table of Content, fill in the pp_sectors map
 *                     if pp_sectors is not null and return the number of
 *                     tracks available.
 *****************************************************************************/
int ioctl_GetTracksMap( vlc_object_t *p_this, const vcddev_t *p_vcddev,
                        int **pp_sectors )
{
    int i_tracks = 0;

    if( p_vcddev->i_vcdimage_handle != -1 )
    {
        /*
         *  vcd image mode
         */

        i_tracks = p_vcddev->i_tracks;

        if( pp_sectors )
        {
            *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );
            if( *pp_sectors == NULL )
            {
                msg_Err( p_this, "out of memory" );
                return 0;
            }
            memcpy( *pp_sectors, p_vcddev->p_sectors,
                    (i_tracks + 1) * sizeof(int) );
        }

        return i_tracks;
    }
    else
    {

        /*
         *  vcd device mode
         */

#if defined( SYS_DARWIN )

        CDTOC *pTOC;
        int i_descriptors;

        if( ( pTOC = darwin_getTOC( p_this, p_vcddev ) ) == NULL )
        {
            msg_Err( p_this, "failed to get the TOC" );
            return 0;
        }

        i_descriptors = darwin_getNumberOfDescriptors( pTOC );
        i_tracks = darwin_getNumberOfTracks( pTOC, i_descriptors );

        if( pp_sectors )
        {
            int i, i_leadout = -1;
            CDTOCDescriptor *pTrackDescriptors;
            u_char track;

            *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );
            if( *pp_sectors == NULL )
            {
                msg_Err( p_this, "out of memory" );
                darwin_freeTOC( pTOC );
                return 0;
            }

            pTrackDescriptors = pTOC->descriptors;

            for( i_tracks = 0, i = 0; i <= i_descriptors; i++ )
            {
                track = pTrackDescriptors[i].point;

                if( track == 0xA2 )
                    i_leadout = i;

                if( track > CD_MAX_TRACK_NO || track < CD_MIN_TRACK_NO )
                    continue;

                (*pp_sectors)[i_tracks++] =
                    CDConvertMSFToLBA( pTrackDescriptors[i].p );
            }

            if( i_leadout == -1 )
            {
                msg_Err( p_this, "leadout not found" );
                free( *pp_sectors );
                darwin_freeTOC( pTOC );
                return 0;
            }

            /* set leadout sector */
            (*pp_sectors)[i_tracks] =
                CDConvertMSFToLBA( pTrackDescriptors[i_leadout].p );
        }

        darwin_freeTOC( pTOC );

#elif defined( WIN32 )
        if( p_vcddev->hASPI )
        {
            HANDLE hEvent;
            struct SRB_ExecSCSICmd ssc;
            byte_t p_tocheader[ 4 ];

            /* Create the transfer completion event */
            hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
            if( hEvent == NULL )
            {
                return -1;
            }

            memset( &ssc, 0, sizeof( ssc ) );

            ssc.SRB_Cmd         = SC_EXEC_SCSI_CMD;
            ssc.SRB_Flags       = SRB_DIR_IN | SRB_EVENT_NOTIFY;
            ssc.SRB_HaId        = LOBYTE( p_vcddev->i_sid );
            ssc.SRB_Target      = HIBYTE( p_vcddev->i_sid );
            ssc.SRB_SenseLen    = SENSE_LEN;

            ssc.SRB_PostProc = (LPVOID) hEvent;
            ssc.SRB_CDBLen      = 10;

            /* Operation code */
            ssc.CDBByte[ 0 ] = READ_TOC;

            /* Format */
            ssc.CDBByte[ 2 ] = READ_TOC_FORMAT_TOC;

            /* Starting track */
            ssc.CDBByte[ 6 ] = 0;

            /* Allocation length and buffer */
            ssc.SRB_BufLen = sizeof( p_tocheader );
            ssc.SRB_BufPointer  = p_tocheader;
            ssc.CDBByte[ 7 ] = ( ssc.SRB_BufLen >>  8 ) & 0xff;
            ssc.CDBByte[ 8 ] = ( ssc.SRB_BufLen       ) & 0xff;

            /* Initiate transfer */
            ResetEvent( hEvent );
            p_vcddev->lpSendCommand( (void*) &ssc );

            /* If the command has still not been processed, wait until it's
             * finished */
            if( ssc.SRB_Status == SS_PENDING )
                WaitForSingleObject( hEvent, INFINITE );

            /* check that the transfer went as planned */
            if( ssc.SRB_Status != SS_COMP )
            {
                CloseHandle( hEvent );
                return 0;
            }

            i_tracks = p_tocheader[3] - p_tocheader[2] + 1;

            if( pp_sectors )
            {
                int i, i_toclength;
                byte_t *p_fulltoc;

                i_toclength = 4 /* header */ + p_tocheader[0] +
                              ((unsigned int)p_tocheader[1] << 8);

                p_fulltoc = malloc( i_toclength );
                *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );

                if( *pp_sectors == NULL || p_fulltoc == NULL )
                {
                    if( *pp_sectors ) free( *pp_sectors );
                    if( p_fulltoc ) free( p_fulltoc );
                    msg_Err( p_this, "out of memory" );
                    CloseHandle( hEvent );
                    return 0;
                }

                /* Allocation length and buffer */
                ssc.SRB_BufLen = i_toclength;
                ssc.SRB_BufPointer  = p_fulltoc;
                ssc.CDBByte[ 7 ] = ( ssc.SRB_BufLen >>  8 ) & 0xff;
                ssc.CDBByte[ 8 ] = ( ssc.SRB_BufLen       ) & 0xff;

                /* Initiate transfer */
                ResetEvent( hEvent );
                p_vcddev->lpSendCommand( (void*) &ssc );

                /* If the command has still not been processed, wait until it's
                 * finished */
                if( ssc.SRB_Status == SS_PENDING )
                    WaitForSingleObject( hEvent, INFINITE );

                /* check that the transfer went as planned */
                if( ssc.SRB_Status != SS_COMP )
                    i_tracks = 0;

                for( i = 0 ; i <= i_tracks ; i++ )
                {
                    int i_index = 8 + 8 * i;
                    (*pp_sectors)[ i ] = ((int)p_fulltoc[ i_index ] << 24) +
                                         ((int)p_fulltoc[ i_index+1 ] << 16) +
                                         ((int)p_fulltoc[ i_index+2 ] << 8) +
                                         (int)p_fulltoc[ i_index+3 ];

                    msg_Dbg( p_this, "p_sectors: %i, %i", i, (*pp_sectors)[i]);
                }

                free( p_fulltoc );
            }

            CloseHandle( hEvent );

        }
        else
        {
            DWORD dwBytesReturned;
            CDROM_TOC cdrom_toc;

            if( DeviceIoControl( p_vcddev->h_device_handle,
                                 IOCTL_CDROM_READ_TOC,
                                 NULL, 0, &cdrom_toc, sizeof(CDROM_TOC),
                                 &dwBytesReturned, NULL ) == 0 )
            {
                msg_Err( p_this, "could not read TOCHDR" );
                return 0;
            }

            i_tracks = cdrom_toc.LastTrack - cdrom_toc.FirstTrack + 1;

            if( pp_sectors )
            {
                int i;

                *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );
                if( *pp_sectors == NULL )
                {
                    msg_Err( p_this, "out of memory" );
                    return 0;
                }

                for( i = 0 ; i <= i_tracks ; i++ )
                {
                    (*pp_sectors)[ i ] = MSF_TO_LBA2(
                                           cdrom_toc.TrackData[i].Address[1],
                                           cdrom_toc.TrackData[i].Address[2],
                                           cdrom_toc.TrackData[i].Address[3] );
                    msg_Dbg( p_this, "p_sectors: %i, %i", i, (*pp_sectors)[i]);
                }
            }
        }

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H ) \
       || defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
        struct ioc_toc_header tochdr;
        struct ioc_read_toc_entry toc_entries;

        if( ioctl( p_vcddev->i_device_handle, CDIOREADTOCHEADER, &tochdr )
            == -1 )
        {
            msg_Err( p_this, "could not read TOCHDR" );
            return 0;
        }

        i_tracks = tochdr.ending_track - tochdr.starting_track + 1;

        if( pp_sectors )
        {
             int i;

             *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );
             if( *pp_sectors == NULL )
             {
                 msg_Err( p_this, "out of memory" );
                 return NULL;
             }

             toc_entries.address_format = CD_LBA_FORMAT;
             toc_entries.starting_track = 0;
             toc_entries.data_len = ( i_tracks + 1 ) *
                                        sizeof( struct cd_toc_entry );
             toc_entries.data = (struct cd_toc_entry *)
                                    malloc( toc_entries.data_len );
             if( toc_entries.data == NULL )
             {
                 msg_Err( p_this, "out of memory" );
                 free( *pp_sectors );
                 return 0;
             }

             /* Read the TOC */
             if( ioctl( p_vcddev->i_device_handle, CDIOREADTOCENTRYS,
                        &toc_entries ) == -1 )
             {
                 msg_Err( p_this, "could not read the TOC" );
                 free( *pp_sectors );
                 free( toc_entries.data );
                 return 0;
             }

             /* Fill the p_sectors structure with the track/sector matches */
             for( i = 0 ; i <= i_tracks ; i++ )
             {
#if defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
                 /* FIXME: is this ok? */
                 (*pp_sectors)[ i ] = toc_entries.data[i].addr.lba;
#else
                 (*pp_sectors)[ i ] = ntohl( toc_entries.data[i].addr.lba );
#endif
             }
        }
#else
        struct cdrom_tochdr   tochdr;
        struct cdrom_tocentry tocent;

        /* First we read the TOC header */
        if( ioctl( p_vcddev->i_device_handle, CDROMREADTOCHDR, &tochdr )
            == -1 )
        {
            msg_Err( p_this, "could not read TOCHDR" );
            return 0;
        }

        i_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;

        if( pp_sectors )
        {
            int i;

            *pp_sectors = malloc( (i_tracks + 1) * sizeof(int) );
            if( *pp_sectors == NULL )
            {
                msg_Err( p_this, "out of memory" );
                return 0;
            }

            /* Fill the p_sectors structure with the track/sector matches */
            for( i = 0 ; i <= i_tracks ; i++ )
            {
                tocent.cdte_format = CDROM_LBA;
                tocent.cdte_track =
                    ( i == i_tracks ) ? CDROM_LEADOUT : tochdr.cdth_trk0 + i;

                if( ioctl( p_vcddev->i_device_handle, CDROMREADTOCENTRY,
                           &tocent ) == -1 )
                {
                    msg_Err( p_this, "could not read TOCENTRY" );
                    free( *pp_sectors );
                    return 0;
                }

                (*pp_sectors)[ i ] = tocent.cdte_addr.lba;
            }
        }
#endif

        return i_tracks;
    }
}

/****************************************************************************
 * ioctl_ReadSector: Read VCD or CDDA sectors
 ****************************************************************************/
int ioctl_ReadSectors( vlc_object_t *p_this, const vcddev_t *p_vcddev,
                       int i_sector, byte_t * p_buffer, int i_nb, int i_type )
{
    byte_t *p_block;
    int i;

    if( i_type == VCD_TYPE ) p_block = malloc( VCD_SECTOR_SIZE * i_nb );
    else p_block = p_buffer;

    if( p_vcddev->i_vcdimage_handle != -1 )
    {
        /*
         *  vcd image mode
         */
        if( lseek( p_vcddev->i_vcdimage_handle, i_sector * VCD_SECTOR_SIZE,
                   SEEK_SET ) == -1 )
        {
            msg_Err( p_this, "Could not lseek to sector %d", i_sector );
            if( i_type == VCD_TYPE ) free( p_block );
            return -1;
        }

        if( read( p_vcddev->i_vcdimage_handle, p_block, VCD_SECTOR_SIZE * i_nb)
            == -1 )
        {
            msg_Err( p_this, "Could not read sector %d", i_sector );
            if( i_type == VCD_TYPE ) free( p_block );
            return -1;
        }

    }
    else
    {

        /*
         *  vcd device mode
         */

#if defined( SYS_DARWIN )
        dk_cd_read_t cd_read;

        memset( &cd_read, 0, sizeof(cd_read) );

        cd_read.offset = i_sector * VCD_SECTOR_SIZE;
        cd_read.sectorArea = kCDSectorAreaSync | kCDSectorAreaHeader |
                             kCDSectorAreaSubHeader | kCDSectorAreaUser |
                             kCDSectorAreaAuxiliary;
        cd_read.sectorType = kCDSectorTypeUnknown;

        cd_read.buffer = p_block;
        cd_read.bufferLength = VCD_SECTOR_SIZE * i_nb;

        if( ioctl( p_vcddev->i_device_handle, DKIOCCDREAD, &cd_read ) == -1 )
        {
            msg_Err( p_this, "could not read block %d", i_sector );
            if( i_type == VCD_TYPE ) free( p_block );
            return -1;
        }

#elif defined( WIN32 )
        if( p_vcddev->hASPI )
        {
            HANDLE hEvent;
            struct SRB_ExecSCSICmd ssc;

            /* Create the transfer completion event */
            hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
            if( hEvent == NULL )
            {
                if( i_type == VCD_TYPE ) free( p_block );
                return -1;
            }

            memset( &ssc, 0, sizeof( ssc ) );

            ssc.SRB_Cmd         = SC_EXEC_SCSI_CMD;
            ssc.SRB_Flags       = SRB_DIR_IN | SRB_EVENT_NOTIFY;
            ssc.SRB_HaId        = LOBYTE( p_vcddev->i_sid );
            ssc.SRB_Target      = HIBYTE( p_vcddev->i_sid );
            ssc.SRB_SenseLen    = SENSE_LEN;

            ssc.SRB_PostProc = (LPVOID) hEvent;
            ssc.SRB_CDBLen      = 12;

            /* Operation code */
            ssc.CDBByte[ 0 ] = READ_CD;

            /* Sector type */
            ssc.CDBByte[ 1 ] = i_type == VCD_TYPE ? SECTOR_TYPE_MODE2_FORM2 :
                                                    SECTOR_TYPE_CDDA;

            /* Start of LBA */
            ssc.CDBByte[ 2 ] = ( i_sector >> 24 ) & 0xff;
            ssc.CDBByte[ 3 ] = ( i_sector >> 16 ) & 0xff;
            ssc.CDBByte[ 4 ] = ( i_sector >>  8 ) & 0xff;
            ssc.CDBByte[ 5 ] = ( i_sector       ) & 0xff;

            /* Transfer length */
            ssc.CDBByte[ 6 ] = ( i_nb >> 16 ) & 0xff;
            ssc.CDBByte[ 7 ] = ( i_nb >> 8  ) & 0xff;
            ssc.CDBByte[ 8 ] = ( i_nb       ) & 0xff;

            /* Data selection */
            ssc.CDBByte[ 9 ] = i_type == VCD_TYPE ? READ_CD_RAW_MODE2 :
                                                    READ_CD_USERDATA;

            /* Result buffer */
            ssc.SRB_BufPointer  = p_block;
            ssc.SRB_BufLen = VCD_SECTOR_SIZE * i_nb;

            /* Initiate transfer */
            ResetEvent( hEvent );
            p_vcddev->lpSendCommand( (void*) &ssc );

            /* If the command has still not been processed, wait until it's
             * finished */
            if( ssc.SRB_Status == SS_PENDING )
            {
                WaitForSingleObject( hEvent, INFINITE );
            }
            CloseHandle( hEvent );

            /* check that the transfer went as planned */
            if( ssc.SRB_Status != SS_COMP )
            {
                if( i_type == VCD_TYPE ) free( p_block );
                return -1;
            }
        }
        else
        {
            DWORD dwBytesReturned;
            RAW_READ_INFO cdrom_raw;

            /* Initialize CDROM_RAW_READ structure */
            cdrom_raw.DiskOffset.QuadPart = CD_SECTOR_SIZE * i_sector;
            cdrom_raw.SectorCount = i_nb;
            cdrom_raw.TrackMode =  i_type == VCD_TYPE ? XAForm2 : CDDA;

            if( DeviceIoControl( p_vcddev->h_device_handle,
                                 IOCTL_CDROM_RAW_READ, &cdrom_raw,
                                 sizeof(RAW_READ_INFO), p_block,
                                 VCD_SECTOR_SIZE * i_nb, &dwBytesReturned,
                                 NULL ) == 0 )
            {
                if( i_type == VCD_TYPE )
                {
                    /* Retry in YellowMode2 */
                    cdrom_raw.TrackMode = YellowMode2;
                    if( DeviceIoControl( p_vcddev->h_device_handle,
                                 IOCTL_CDROM_RAW_READ, &cdrom_raw,
                                 sizeof(RAW_READ_INFO), p_block,
                                 VCD_SECTOR_SIZE * i_nb, &dwBytesReturned,
                                 NULL ) == 0 )
                    {
                        free( p_block );
                        return -1;
                    }
                }
                else return -1;
            }
        }

#elif defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
        struct scsireq  sc;
        int i_ret;

        memset( &sc, 0, sizeof(sc) );
        sc.cmd[0] = 0xBE;
        sc.cmd[1] = i_type == VCD_TYPE ? SECTOR_TYPE_MODE2_FORM2:
                                         SECTOR_TYPE_CDDA;
        sc.cmd[2] = (i_sector >> 24) & 0xff;
        sc.cmd[3] = (i_sector >> 16) & 0xff;
        sc.cmd[4] = (i_sector >>  8) & 0xff;
        sc.cmd[5] = (i_sector >>  0) & 0xff;
        sc.cmd[6] = (i_nb >> 16) & 0xff;
        sc.cmd[7] = (i_nb >>  8) & 0xff;
        sc.cmd[8] = (i_nb      ) & 0xff;
        sc.cmd[9] = i_type == VCD_TYPE ? READ_CD_RAW_MODE2 : READ_CD_USERDATA;
        sc.cmd[10] = 0; /* sub channel */
        sc.cmdlen = 12;
        sc.databuf = (caddr_t)p_block;
        sc.datalen = VCD_SECTOR_SIZE * i_nb;
        sc.senselen = sizeof( sc.sense );
        sc.flags = SCCMD_READ;
        sc.timeout = 10000;

        i_ret = ioctl( i_fd, SCIOCCOMMAND, &sc );
        if( i_ret == -1 )
        {
            msg_Err( p_this, "SCIOCCOMMAND failed" );
            if( i_type == VCD_TYPE ) free( p_block );
            return -1;
        }
        if( sc.retsts || sc.error )
        {
            msg_Err( p_this, "SCSI command failed: status %d error %d\n",
                             sc.retsts, sc.error );
            if( i_type == VCD_TYPE ) free( p_block );
           return -1;
        }

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
        int i_size = VCD_SECTOR_SIZE;

        if( ioctl( p_vcddev->i_device_handle, CDRIOCSETBLOCKSIZE, &i_size )
            == -1 )
        {
            msg_Err( p_this, "Could not set block size" );
            if( i_type == VCD_TYPE ) free( p_block );
            return( -1 );
        }

        if( lseek( p_vcddev->i_device_handle,
                   i_sector * VCD_SECTOR_SIZE, SEEK_SET ) == -1 )
        {
            msg_Err( p_this, "Could not lseek to sector %d", i_sector );
            if( i_type == VCD_TYPE ) free( p_block );
            return( -1 );
        }

        if( read( p_vcddev->i_device_handle,
                  p_block, VCD_SECTOR_SIZE * i_nb ) == -1 )
        {
            msg_Err( p_this, "Could not read sector %d", i_sector );
            if( i_type == VCD_TYPE ) free( p_block );
            return( -1 );
        }

#else
        for( i = 0; i < i_nb; i++ )
        {
            int i_dummy = i_sector + i + 2 * CD_FRAMES;

#define p_msf ((struct cdrom_msf0 *)(p_block + i * VCD_SECTOR_SIZE))
            p_msf->minute =   i_dummy / (CD_FRAMES * CD_SECS);
            p_msf->second = ( i_dummy % (CD_FRAMES * CD_SECS) ) / CD_FRAMES;
            p_msf->frame =  ( i_dummy % (CD_FRAMES * CD_SECS) ) % CD_FRAMES;
#undef p_msf

            if( ioctl( p_vcddev->i_device_handle, CDROMREADRAW,
                       p_block + i * VCD_SECTOR_SIZE ) == -1 )
            {
                msg_Err( p_this, "could not read block %i from disc",
                         i_sector );

                if( i == 0 )
                {
                    if( i_type == VCD_TYPE ) free( p_block );
                    return( -1 );
                }
                else break;
            }
        }
#endif
    }

    /* For VCDs, we don't want to keep the header and footer of the
     * sectors read */
    if( i_type == VCD_TYPE )
    {
        for( i = 0; i < i_nb; i++ )
        {
            memcpy( p_buffer + i * VCD_DATA_SIZE,
                    p_block + i * VCD_SECTOR_SIZE + VCD_DATA_START,
                    VCD_DATA_SIZE );
        }
        free( p_block );
    }

    return( 0 );
}

/****************************************************************************
 * Private functions
 ****************************************************************************/

/****************************************************************************
 * OpenVCDImage: try to open a vcd image from a .cue file
 ****************************************************************************/
static int OpenVCDImage( vlc_object_t * p_this, const char *psz_dev,
                         vcddev_t *p_vcddev )
{
    int i_ret = -1;
    char *p_pos;
    char *psz_vcdfile = NULL;
    char *psz_cuefile = NULL;
    FILE *cuefile;
    char line[1024];

    /* Check if we are dealing with a .cue file */
    p_pos = strrchr( psz_dev, '.' );
    if( p_pos && !strcmp( p_pos, ".cue" ) )
    {
        psz_cuefile = strdup( psz_dev );
    }
    else
    {
        /* psz_dev must be the actual vcd file. Let's assume there's a .cue
         * file with the same filename */
        if( p_pos )
        {
            psz_cuefile = malloc( p_pos - psz_dev + 5 /* ".cue" */ );
            strncpy( psz_cuefile, psz_dev, p_pos - psz_dev );
            strcpy( psz_cuefile + (p_pos - psz_dev), ".cue");
        }
        else
        {
            psz_cuefile = malloc( strlen(psz_dev) + 5 /* ".cue" */ );
            sprintf( psz_cuefile, "%s.cue", psz_dev );
        }
    }

    /* Open the cue file and try to parse it */
    msg_Dbg( p_this,"trying .cue file: %s", psz_cuefile );
    cuefile = fopen( psz_cuefile, "rt" );
    if( cuefile && fscanf( cuefile, "FILE %c", line ) &&
        fgets( line, 1024, cuefile ) )
    {
        p_pos = strchr( line, '"' );
        if( p_pos )
        {
            *p_pos = 0;

            /* Take care of path standardization */
            if( *line != '/' && ((p_pos = strrchr( psz_cuefile, '/' ))
                || (p_pos = strrchr( psz_cuefile, '\\' ) )) )
            {
                psz_vcdfile = malloc( strlen(line) +
                                      (p_pos - psz_cuefile + 1) + 1 );
                strncpy( psz_vcdfile, psz_cuefile, (p_pos - psz_cuefile + 1) );
                strcpy( psz_vcdfile + (p_pos - psz_cuefile + 1), line );
            }
            else psz_vcdfile = strdup( line );
        }
    }

    if( psz_vcdfile )
    {
        msg_Dbg( p_this,"using vcd image file: %s", psz_vcdfile );
        p_vcddev->i_vcdimage_handle = open( psz_vcdfile,
                                        O_RDONLY | O_NONBLOCK | O_BINARY );
        i_ret = (p_vcddev->i_vcdimage_handle == -1) ? -1 : 0;
    }

    /* Try to parse the i_tracks and p_sectors info so we can just forget
     * about the cuefile */
    if( i_ret == 0 )
    {
        int p_sectors[100];
        int i_tracks = 0;
        int i_num;
        char psz_dummy[10];

        while( fgets( line, 1024, cuefile ) )
        {
            /* look for a TRACK line */
            if( !sscanf( line, "%9s", psz_dummy ) ||
                strcmp(psz_dummy, "TRACK") )
                continue;

            /* look for an INDEX line */
            while( fgets( line, 1024, cuefile ) )
            {
                int i_min, i_sec, i_frame;

                if( (sscanf( line, "%9s %2u %2u:%2u:%2u", psz_dummy, &i_num,
                            &i_min, &i_sec, &i_frame ) != 5) || (i_num != 1) )
                    continue;

                i_tracks++;
                p_sectors[i_tracks - 1] = MSF_TO_LBA(i_min, i_sec, i_frame);
                msg_Dbg( p_this, "vcd track %i begins at sector:%i",
                         i_tracks - 1, p_sectors[i_tracks - 1] );
                break;
            }
        }

        /* fill in the last entry */
        p_sectors[i_tracks] = lseek(p_vcddev->i_vcdimage_handle, 0, SEEK_END)
                                / VCD_SECTOR_SIZE;
        msg_Dbg( p_this, "vcd track %i, begins at sector:%i",
                 i_tracks, p_sectors[i_tracks] );
        p_vcddev->i_tracks = i_tracks;
        p_vcddev->p_sectors = malloc( (i_tracks + 1) * sizeof(int) );
        memcpy( p_vcddev->p_sectors, p_sectors, (i_tracks + 1) * sizeof(int) );

    }

    if( cuefile ) fclose( cuefile );
    if( psz_cuefile ) free( psz_cuefile );
    if( psz_vcdfile ) free( psz_vcdfile );

    return i_ret;
}

/****************************************************************************
 * CloseVCDImage: closes a vcd image opened by OpenVCDImage
 ****************************************************************************/
static void CloseVCDImage( vlc_object_t * p_this, vcddev_t *p_vcddev )
{
    if( p_vcddev->i_vcdimage_handle != -1 )
        close( p_vcddev->i_vcdimage_handle );
    else
        return;

    if( p_vcddev->p_sectors )
        free( p_vcddev->p_sectors );
}

#if defined( SYS_DARWIN )
/****************************************************************************
 * darwin_getTOC: get the TOC
 ****************************************************************************/
static CDTOC *darwin_getTOC( vlc_object_t * p_this, const vcddev_t *p_vcddev )
{
    mach_port_t port;
    char *psz_devname;
    kern_return_t ret;
    CDTOC *pTOC = NULL;
    io_iterator_t iterator;
    io_registry_entry_t service;
    CFMutableDictionaryRef properties;
    CFDataRef data;

    /* get the device name */
    if( ( psz_devname = strrchr( p_vcddev->psz_dev, '/') ) != NULL )
        ++psz_devname;
    else
        psz_devname = p_vcddev->psz_dev;

    /* unraw the device name */
    if( *psz_devname == 'r' )
        ++psz_devname;

    /* get port for IOKit communication */
    if( ( ret = IOMasterPort( MACH_PORT_NULL, &port ) ) != KERN_SUCCESS )
    {
        msg_Err( p_this, "IOMasterPort: 0x%08x", ret );
        return( NULL );
    }

    /* get service iterator for the device */
    if( ( ret = IOServiceGetMatchingServices( 
                    port, IOBSDNameMatching( port, 0, psz_devname ),
                    &iterator ) ) != KERN_SUCCESS )
    {
        msg_Err( p_this, "IOServiceGetMatchingServices: 0x%08x", ret );
        return( NULL );
    }

    /* first service */
    service = IOIteratorNext( iterator );
    IOObjectRelease( iterator );

    /* search for kIOCDMediaClass */ 
    while( service && !IOObjectConformsTo( service, kIOCDMediaClass ) )
    {
        if( ( ret = IORegistryEntryGetParentIterator( service, 
                        kIOServicePlane, &iterator ) ) != KERN_SUCCESS )
        {
            msg_Err( p_this, "IORegistryEntryGetParentIterator: 0x%08x", ret );
            IOObjectRelease( service );
            return( NULL );
        }

        IOObjectRelease( service );
        service = IOIteratorNext( iterator );
        IOObjectRelease( iterator );
    }

    if( service == NULL )
    {
        msg_Err( p_this, "search for kIOCDMediaClass came up empty" );
        return( NULL );
    }

    /* create a CF dictionary containing the TOC */
    if( ( ret = IORegistryEntryCreateCFProperties( service, &properties,
                    kCFAllocatorDefault, kNilOptions ) ) != KERN_SUCCESS )
    {
        msg_Err( p_this, "IORegistryEntryCreateCFProperties: 0x%08x", ret );
        IOObjectRelease( service );
        return( NULL );
    }

    /* get the TOC from the dictionary */
    if( ( data = (CFDataRef) CFDictionaryGetValue( properties,
                                    CFSTR(kIOCDMediaTOCKey) ) ) != NULL )
    {
        CFRange range;
        CFIndex buf_len;

        buf_len = CFDataGetLength( data ) + 1;
        range = CFRangeMake( 0, buf_len );

        if( ( pTOC = (CDTOC *)malloc( buf_len ) ) != NULL )
        {
            CFDataGetBytes( data, range, (u_char *)pTOC );
        }
    }
    else
    {
        msg_Err( p_this, "CFDictionaryGetValue failed" );
    }

    CFRelease( properties );
    IOObjectRelease( service ); 

    return( pTOC ); 
}

/****************************************************************************
 * darwin_getNumberOfDescriptors: get number of descriptors in TOC 
 ****************************************************************************/
static int darwin_getNumberOfDescriptors( CDTOC *pTOC )
{
    int i_descriptors;

    /* get TOC length */
    i_descriptors = pTOC->length;

    /* remove the first and last session */
    i_descriptors -= ( sizeof(pTOC->sessionFirst) +
                       sizeof(pTOC->sessionLast) );

    /* divide the length by the size of a single descriptor */
    i_descriptors /= sizeof(CDTOCDescriptor);

    return( i_descriptors );
}

/****************************************************************************
 * darwin_getNumberOfTracks: get number of tracks in TOC 
 ****************************************************************************/
static int darwin_getNumberOfTracks( CDTOC *pTOC, int i_descriptors )
{
    u_char track;
    int i, i_tracks = 0; 
    CDTOCDescriptor *pTrackDescriptors;

    pTrackDescriptors = pTOC->descriptors;

    for( i = i_descriptors; i >= 0; i-- )
    {
        track = pTrackDescriptors[i].point;

        if( track > CD_MAX_TRACK_NO || track < CD_MIN_TRACK_NO )
            continue;

        i_tracks++; 
    }

    return( i_tracks );
}
#endif /* SYS_DARWIN */

#if defined( WIN32 )
/*****************************************************************************
 * win32_vcd_open: open vcd drive
 *****************************************************************************
 * Load and use aspi if it is available, otherwise use IOCTLs on WinNT/2K/XP.
 *****************************************************************************/
static int win32_vcd_open( vlc_object_t * p_this, const char *psz_dev,
                           vcddev_t *p_vcddev )
{
    /* Initializations */
    p_vcddev->h_device_handle = NULL;
    p_vcddev->i_sid = 0;
    p_vcddev->hASPI = 0;
    p_vcddev->lpSendCommand = 0;

    if( WIN_NT )
    {
        char psz_win32_drive[7];

        msg_Dbg( p_this, "using winNT/2K/XP ioctl layer" );

        sprintf( psz_win32_drive, "\\\\.\\%c:", psz_dev[0] );

        p_vcddev->h_device_handle = CreateFile( psz_win32_drive, GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL, OPEN_EXISTING,
                                            FILE_FLAG_NO_BUFFERING |
                                            FILE_FLAG_RANDOM_ACCESS, NULL );
        return (p_vcddev->h_device_handle == NULL) ? -1 : 0;
    }
    else
    {
        HMODULE hASPI = NULL;
        long (*lpGetSupport)( void ) = NULL;
        long (*lpSendCommand)( void* ) = NULL;
        DWORD dwSupportInfo;
        int i, j, i_hostadapters;
        char c_drive = psz_dev[0];

        hASPI = LoadLibrary( "wnaspi32.dll" );
        if( hASPI != NULL )
        {
            (FARPROC) lpGetSupport = GetProcAddress( hASPI,
                                                     "GetASPI32SupportInfo" );
            (FARPROC) lpSendCommand = GetProcAddress( hASPI,
                                                      "SendASPI32Command" );
        }

        if( hASPI == NULL || lpGetSupport == NULL || lpSendCommand == NULL )
        {
            msg_Dbg( p_this,
                     "unable to load aspi or get aspi function pointers" );
            if( hASPI ) FreeLibrary( hASPI );
            return -1;
        }

        /* ASPI support seems to be there */

        dwSupportInfo = lpGetSupport();

        if( HIBYTE( LOWORD ( dwSupportInfo ) ) == SS_NO_ADAPTERS )
        {
            msg_Dbg( p_this, "no host adapters found (aspi)" );
            FreeLibrary( hASPI );
            return -1;
        }

        if( HIBYTE( LOWORD ( dwSupportInfo ) ) != SS_COMP )
        {
            msg_Dbg( p_this, "unable to initalize aspi layer" );
            FreeLibrary( hASPI );
            return -1;
        }

        i_hostadapters = LOBYTE( LOWORD( dwSupportInfo ) );
        if( i_hostadapters == 0 )
        {
            FreeLibrary( hASPI );
            return -1;
        }

        c_drive = c_drive > 'Z' ? c_drive - 'a' : c_drive - 'A';

        for( i = 0; i < i_hostadapters; i++ )
        {
          for( j = 0; j < 15; j++ )
          {
              struct SRB_GetDiskInfo srbDiskInfo;

              srbDiskInfo.SRB_Cmd         = SC_GET_DISK_INFO;
              srbDiskInfo.SRB_HaId        = i;
              srbDiskInfo.SRB_Flags       = 0;
              srbDiskInfo.SRB_Hdr_Rsvd    = 0;
              srbDiskInfo.SRB_Target      = j;
              srbDiskInfo.SRB_Lun         = 0;

              lpSendCommand( (void*) &srbDiskInfo );

              if( (srbDiskInfo.SRB_Status == SS_COMP) &&
                  (srbDiskInfo.SRB_Int13HDriveInfo == c_drive) )
              {
                  /* Make sure this is a cdrom device */
                  struct SRB_GDEVBlock   srbGDEVBlock;

                  memset( &srbGDEVBlock, 0, sizeof(struct SRB_GDEVBlock) );
                  srbGDEVBlock.SRB_Cmd    = SC_GET_DEV_TYPE;
                  srbGDEVBlock.SRB_HaId   = i;
                  srbGDEVBlock.SRB_Target = j;

                  lpSendCommand( (void*) &srbGDEVBlock );

                  if( ( srbGDEVBlock.SRB_Status == SS_COMP ) &&
                      ( srbGDEVBlock.SRB_DeviceType == DTYPE_CDROM ) )
                  {
                      p_vcddev->i_sid = MAKEWORD( i, j );
                      p_vcddev->hASPI = (long)hASPI;
                      p_vcddev->lpSendCommand = lpSendCommand;
                      msg_Dbg( p_this, "using aspi layer" );

                      return 0;
                  }
                  else
                  {
                      FreeLibrary( hASPI );
                      msg_Dbg( p_this, "%c: is not a cdrom drive",
                               psz_dev[0] );
                      return -1;
                  }
              }
          }
        }

        FreeLibrary( hASPI );
        msg_Dbg( p_this, "unable to get haid and target (aspi)" );

    }

    return -1;
}

#endif /* WIN32 */
