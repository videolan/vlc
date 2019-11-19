/****************************************************************************
 * cdrom.c: cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 VLC authors and VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar
 *          Rémi Duraffort
 *          Derk-Jan Hartman
 *          Samuel Hocevar
 *          Rafaël Carré
 *          Christophe Massiot
 *          Jean-Baptiste Kempf
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef __OS2__
#   define INCL_DOSDEVIOCTL
#endif

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_meta.h>

#if defined( SYS_BSDI )
#   include <dvd.h>
#elif defined ( __APPLE__ )
#   include <CoreFoundation/CFBase.h>
#   include <IOKit/IOKitLib.h>
#   include <IOKit/storage/IOCDTypes.h>
#   include <IOKit/storage/IOCDMedia.h>
#   include <IOKit/storage/IOCDMediaBSDClient.h>
#elif defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
#   include <inttypes.h>
#   include <sys/cdio.h>
#   include <sys/scsiio.h>
#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
#   include <sys/cdio.h>
#   include <sys/cdrio.h>
#elif defined( _WIN32 )
#   include <windows.h>
#   include <winioctl.h>
#elif defined (__linux__)
#   include <sys/ioctl.h>
#   include <linux/cdrom.h>
#elif defined( __OS2__ )
#   include <os2safe.h>
#   include <os2.h>

/*****************************************************************************
 * vlc_DosDevIOCtl: high memory safe wrapper for DosDevIOCtl
 *****************************************************************************
 * Unfortunately, DosDevIOCtl() is not high memory safe API, and is not
 * covered by os2safe.h. So define a wrapper function for it here.
 *****************************************************************************/

static APIRET vlc_DosDevIOCtl( HFILE hdevice, ULONG category, ULONG function,
                               PVOID pParams, ULONG cbParamLenMax,
                               PULONG pcbParamLen, PVOID pData,
                               ULONG cbDataLenMax, PULONG pcbDataLen )
{
    PVOID pParamsLow = NULL;
    PVOID pDataLow = NULL;
    ULONG cbParamLenLow;
    ULONG cbDataLenLow;

    APIRET rc;

    rc = DosAllocMem( &pParamsLow, cbParamLenMax, fALLOC );
    if( rc )
        goto exit_free;

    rc = DosAllocMem( &pDataLow, cbDataLenMax, fALLOC );
    if( rc )
        goto exit_free;

    memcpy( pParamsLow, pParams, cbParamLenMax );
    memcpy( pDataLow, pData, cbDataLenMax );

    cbParamLenLow = *pcbParamLen;
    cbDataLenLow  = *pcbDataLen;

    rc = DosDevIOCtl( hdevice, category, function, pParamsLow,
                      cbParamLenMax, &cbParamLenLow, pDataLow, cbDataLenMax,
                      &cbDataLenLow );

    if( !rc )
    {
        memcpy( pParams, pParamsLow, cbParamLenMax );
        memcpy( pData, pDataLow, cbDataLenMax );

        *pcbParamLen = cbParamLenLow;
        *pcbDataLen  = cbDataLenLow;
    }

exit_free:
    DosFreeMem( pParamsLow);
    DosFreeMem( pDataLow);

    return rc;
}

#   define DosDevIOCtl vlc_DosDevIOCtl
#else
#   error FIXME
#endif

#include "cdrom.h"
#include "cdrom_internals.h"

/*****************************************************************************
 * ioctl_Open: Opens a VCD device or file and returns an opaque handle
 *****************************************************************************/
vcddev_t *ioctl_Open( vlc_object_t *p_this, const char *psz_dev )
{
    int i_ret;
    int b_is_file;
    vcddev_t *p_vcddev;
#if !defined( _WIN32 ) && !defined( __OS2__ )
    struct stat fileinfo;
#endif

    if( !psz_dev ) return NULL;

    /*
     *  Initialize structure with default values
     */
    p_vcddev = malloc( sizeof(*p_vcddev) );
    if( p_vcddev == NULL )
        return NULL;
    p_vcddev->i_vcdimage_handle = -1;
    p_vcddev->psz_dev = NULL;
    memset( &p_vcddev->toc, 0, sizeof(p_vcddev->toc) );
    b_is_file = 1;

    /*
     *  Check if we are dealing with a device or a file (vcd image)
     */
#if defined( _WIN32 ) || defined( __OS2__ )
    if( (strlen( psz_dev ) == 2 && psz_dev[1] == ':') )
    {
        b_is_file = 0;
    }

#else
    if( vlc_stat( psz_dev, &fileinfo ) < 0 )
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

#ifdef _WIN32
        i_ret = win32_vcd_open( p_this, psz_dev, p_vcddev );
#elif defined( __OS2__ )
        i_ret = os2_vcd_open( p_this, psz_dev, p_vcddev );
#else
        p_vcddev->i_device_handle = -1;
        p_vcddev->i_device_handle = vlc_open( psz_dev, O_RDONLY | O_NONBLOCK );
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
    free( p_vcddev->psz_dev );

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

#ifdef _WIN32
    if( p_vcddev->h_device_handle )
        CloseHandle( p_vcddev->h_device_handle );
#elif defined( __OS2__ )
    if( p_vcddev->hcd )
        DosClose( p_vcddev->hcd );
#else
    if( p_vcddev->i_device_handle != -1 )
        vlc_close( p_vcddev->i_device_handle );
#endif
    free( p_vcddev );
}

/*****************************************************************************
 * ioctl_GetTOC: Read the Table of Content, fill in the p_sectors map
 *               if b_fill_sector_info is true.
 *****************************************************************************/
vcddev_toc_t * ioctl_GetTOC( vlc_object_t *p_this, const vcddev_t *p_vcddev,
                             bool b_fill_sectorinfo )
{
    vcddev_toc_t *p_toc = calloc(1, sizeof(*p_toc));
    if(!p_toc)
        return NULL;

    if( p_vcddev->i_vcdimage_handle != -1 )
    {
        /*
         *  vcd image mode
         */

        *p_toc = p_vcddev->toc;
        p_toc->p_sectors = NULL;

        if( b_fill_sectorinfo )
        {
            p_toc->p_sectors = calloc( p_toc->i_tracks + 1, sizeof(*p_toc->p_sectors) );
            if( p_toc->p_sectors == NULL )
            {
                free( p_toc );
                return NULL;
            }
            memcpy( p_toc->p_sectors, p_vcddev->toc.p_sectors,
                    (p_toc->i_tracks + 1) * sizeof(*p_toc->p_sectors) );
        }

        return p_toc;
    }
    else
    {

        /*
         *  vcd device mode
         */

#if defined( __APPLE__ )

        CDTOC *pTOC;
        int i_descriptors;

        if( ( pTOC = darwin_getTOC( p_this, p_vcddev ) ) == NULL )
        {
            msg_Err( p_this, "failed to get the TOC" );
            vcddev_toc_Free( p_toc );
            return NULL;
        }

        i_descriptors = CDTOCGetDescriptorCount( pTOC );
        p_toc->i_tracks = darwin_getNumberOfTracks( pTOC, i_descriptors,
                                                    &p_toc->i_first_track,
                                                    &p_toc->i_last_track );

        if( b_fill_sectorinfo )
        {
            int i, i_leadout = -1;
            CDTOCDescriptor *pTrackDescriptors;
            u_char track;

            p_toc->p_sectors = calloc( p_toc->i_tracks + 1,
                                       sizeof(*p_toc->p_sectors) );
            if( p_toc->p_sectors == NULL )
            {
                vcddev_toc_Free( p_toc );
                darwin_freeTOC( pTOC );
                return NULL;
            }

            pTrackDescriptors = pTOC->descriptors;

            for( p_toc->i_tracks = 0, i = 0; i < i_descriptors; i++ )
            {
                track = pTrackDescriptors[i].point;

                if( track == 0xA2 )
                    i_leadout = i;

                if( track > CD_MAX_TRACK_NO || track < CD_MIN_TRACK_NO )
                    continue;

                p_toc->p_sectors[p_toc->i_tracks].i_control = pTrackDescriptors[i].control;
                p_toc->p_sectors[p_toc->i_tracks++].i_lba =
                    CDConvertMSFToLBA( pTrackDescriptors[i].p );
            }

            if( i_leadout == -1 )
            {
                msg_Err( p_this, "leadout not found" );
                vcddev_toc_Free( p_toc );
                darwin_freeTOC( pTOC );
                return NULL;
            }

            /* set leadout sector */
            p_toc->p_sectors[p_toc->i_tracks].i_lba =
                CDConvertMSFToLBA( pTrackDescriptors[i_leadout].p );
        }

        darwin_freeTOC( pTOC );

#elif defined( _WIN32 )
        DWORD dwBytesReturned;
        CDROM_TOC cdrom_toc;

        if( DeviceIoControl( p_vcddev->h_device_handle, IOCTL_CDROM_READ_TOC,
                             NULL, 0, &cdrom_toc, sizeof(CDROM_TOC),
                             &dwBytesReturned, NULL ) == 0 )
        {
            msg_Err( p_this, "could not read TOCHDR" );
            vcddev_toc_Free( p_toc );
            return NULL;
        }

        p_toc->i_tracks = cdrom_toc.LastTrack - cdrom_toc.FirstTrack + 1;
        p_toc->i_first_track = cdrom_toc.FirstTrack;
        p_toc->i_last_track = cdrom_toc.LastTrack;

        if( b_fill_sectorinfo )
        {
            p_toc->p_sectors = calloc( p_toc->i_tracks + 1, sizeof(p_toc->p_sectors) );
            if( p_toc->p_sectors == NULL )
            {
                vcddev_toc_Free( p_toc );
                return NULL;
            }

            for( int i = 0 ; i <= p_toc->i_tracks ; i++ )
            {
                p_toc->p_sectors[ i ].i_control = cdrom_toc.TrackData[i].Control;
                p_toc->p_sectors[ i ].i_lba = MSF_TO_LBA2(
                                           cdrom_toc.TrackData[i].Address[1],
                                           cdrom_toc.TrackData[i].Address[2],
                                           cdrom_toc.TrackData[i].Address[3] );
                msg_Dbg( p_this, "p_sectors: %i, %i", i, p_toc->p_sectors[i].i_lba);
             }
        }

#elif defined( __OS2__ )
        cdrom_get_tochdr_t get_tochdr = {{'C', 'D', '0', '1'}};
        cdrom_tochdr_t     tochdr;

        ULONG param_len;
        ULONG data_len;
        ULONG rc;

        rc = DosDevIOCtl( p_vcddev->hcd, IOCTL_CDROMAUDIO,
                          CDROMAUDIO_GETAUDIODISK,
                          &get_tochdr, sizeof( get_tochdr ), &param_len,
                          &tochdr, sizeof( tochdr ), &data_len );
        if( rc )
        {
            msg_Err( p_this, "could not read TOCHDR" );
            return 0;
        }

        p_toc->i_tracks = tochdr.last_track - tochdr.first_track + 1;
        p_toc->i_first_track = tochdr.first_track;
        p_toc->i_last_track = tochdr.last_track;

        if( b_fill_sectorinfo )
        {
            cdrom_get_track_t get_track = {{'C', 'D', '0', '1'}, };
            cdrom_track_t track;
            int i;

            p_toc->p_sectors = calloc( p_toc->i_tracks + 1, sizeof(*p_toc->p_sectors) );
            if( p_toc->p_sectors == NULL )
            {
                vcddev_toc_Free( p_toc );
                return NULL;
            }

            for( i = 0 ; i < p_toc->i_tracks ; i++ )
            {
                get_track.track = tochdr.first_track + i;
                rc = DosDevIOCtl( p_vcddev->hcd, IOCTL_CDROMAUDIO,
                                  CDROMAUDIO_GETAUDIOTRACK,
                                  &get_track, sizeof(get_track), &param_len,
                                  &track, sizeof(track), &data_len );
                if (rc)
                {
                    msg_Err( p_this, "could not read %d track",
                             get_track.track );
                    vcddev_toc_Free( p_toc );
                    return NULL;
                }

                p_toc->p_sectors[ i ].i_lba = MSF_TO_LBA2(
                                       track.start.minute,
                                       track.start.second,
                                       track.start.frame );
                msg_Dbg( p_this, "p_sectors: %i, %i", i, p_toc->p_sectors[i].i_lba);
            }

            /* for lead-out track */
            p_toc->p_sectors[ i ].i_lba = MSF_TO_LBA2(
                                   tochdr.lead_out.minute,
                                   tochdr.lead_out.second,
                                   tochdr.lead_out.frame );
            msg_Dbg( p_this, "p_sectors: %i, %i", i, p_toc->p_sectors[i].i_lba);
        }

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H ) \
       || defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
        struct ioc_toc_header tochdr;
        struct ioc_read_toc_entry toc_entries;

        if( ioctl( p_vcddev->i_device_handle, CDIOREADTOCHEADER, &tochdr )
            == -1 )
        {
            msg_Err( p_this, "could not read TOCHDR" );
            vcddev_toc_Free( p_toc );
            return NULL;
        }

        p_toc->i_tracks = tochdr.ending_track - tochdr.starting_track + 1;
        p_toc->i_first_track = tochdr.starting_track;
        p_toc->i_last_track = tochdr.ending_track;

        if( b_fill_sectorinfo )
        {
             p_toc->p_sectors = calloc( p_toc->i_tracks + 1, sizeof(*p_toc->p_sectors) );
             if( p_toc->p_sectors == NULL )
             {
                 vcddev_toc_Free( p_toc );
                 return NULL;
             }

             toc_entries.address_format = CD_LBA_FORMAT;
             toc_entries.starting_track = 0;
             toc_entries.data_len = ( p_toc->i_tracks + 1 ) *
                                        sizeof( struct cd_toc_entry );
             toc_entries.data = (struct cd_toc_entry *)
                                    malloc( toc_entries.data_len );
             if( toc_entries.data == NULL )
             {
                 vcddev_toc_Free( p_toc );
                 return NULL;
             }

             /* Read the TOC */
             if( ioctl( p_vcddev->i_device_handle, CDIOREADTOCENTRYS,
                        &toc_entries ) == -1 )
             {
                 msg_Err( p_this, "could not read the TOC" );
                 free( toc_entries.data );
                 vcddev_toc_Free( p_toc );
                 return NULL;
             }

             /* Fill the p_sectors structure with the track/sector matches */
             for( int i = 0 ; i <= p_toc->i_tracks ; i++ )
             {
#if defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
                 /* FIXME: is this ok? */
                 p_toc->p_sectors[ i ].i_lba = toc_entries.data[i].addr.lba;
#else
                 p_toc->p_sectors[ i ].i_lba = ntohl( toc_entries.data[i].addr.lba );
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
            free( p_toc );
            return NULL;
        }

        p_toc->i_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;
        p_toc->i_first_track = tochdr.cdth_trk0;
        p_toc->i_last_track = tochdr.cdth_trk1;

        if( b_fill_sectorinfo )
        {
            p_toc->p_sectors = calloc( p_toc->i_tracks + 1, sizeof(*p_toc->p_sectors) );
            if( p_toc->p_sectors == NULL )
            {
                free( p_toc );
                return NULL;
            }

            /* Fill the p_sectors structure with the track/sector matches */
            for( int i = 0 ; i <= p_toc->i_tracks ; i++ )
            {
                tocent.cdte_format = CDROM_LBA;
                tocent.cdte_track =
                    ( i == p_toc->i_tracks ) ? CDROM_LEADOUT : tochdr.cdth_trk0 + i;

                if( ioctl( p_vcddev->i_device_handle, CDROMREADTOCENTRY,
                           &tocent ) == -1 )
                {
                    msg_Err( p_this, "could not read TOCENTRY" );
                    free( p_toc->p_sectors );
                    free( p_toc );
                    return NULL;
                }

                p_toc->p_sectors[ i ].i_lba = tocent.cdte_addr.lba;
                p_toc->p_sectors[ i ].i_control = tocent.cdte_ctrl;
            }
        }
#endif

        return p_toc;
    }
}

/****************************************************************************
 * ioctl_ReadSector: Read VCD or CDDA sectors
 ****************************************************************************/
int ioctl_ReadSectors( vlc_object_t *p_this, const vcddev_t *p_vcddev,
                       int i_sector, uint8_t *p_buffer, int i_nb, int i_type )
{
    uint8_t *p_block;

    if( i_type == VCD_TYPE )
        p_block = vlc_alloc( i_nb, VCD_SECTOR_SIZE );
    else
        p_block = p_buffer;

    if( p_vcddev->i_vcdimage_handle != -1 )
    {
        /*
         *  vcd image mode
         */
        if( lseek( p_vcddev->i_vcdimage_handle, i_sector * VCD_SECTOR_SIZE,
                   SEEK_SET ) == -1 )
        {
            msg_Err( p_this, "Could not lseek to sector %d", i_sector );
            goto error;
        }

        if( read( p_vcddev->i_vcdimage_handle, p_block, VCD_SECTOR_SIZE * i_nb)
            == -1 )
        {
            msg_Err( p_this, "Could not read sector %d", i_sector );
            goto error;
        }

    }
    else
    {

        /*
         *  vcd device mode
         */

#if defined( __APPLE__ )
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
            goto error;
        }

#elif defined( _WIN32 )
        DWORD dwBytesReturned;
        RAW_READ_INFO cdrom_raw;

        /* Initialize CDROM_RAW_READ structure */
        cdrom_raw.DiskOffset.QuadPart = CD_SECTOR_SIZE * i_sector;
        cdrom_raw.SectorCount = i_nb;
        cdrom_raw.TrackMode =  i_type == VCD_TYPE ? XAForm2 : CDDA;

        if( DeviceIoControl( p_vcddev->h_device_handle, IOCTL_CDROM_RAW_READ,
                             &cdrom_raw, sizeof(RAW_READ_INFO), p_block,
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
                    goto error;
            }
            else return -1;
        }

#elif defined( __OS2__ )
        cdrom_readlong_t readlong = {{'C', 'D', '0', '1'}, };

        ULONG param_len;
        ULONG data_len;
        ULONG rc;

        readlong.addr_mode = 0;         /* LBA mode */
        readlong.sectors   = i_nb;
        readlong.start     = i_sector;

        rc = DosDevIOCtl( p_vcddev->hcd, IOCTL_CDROMDISK, CDROMDISK_READLONG,
                          &readlong, sizeof( readlong ), &param_len,
                          p_block, VCD_SECTOR_SIZE * i_nb, &data_len );
        if( rc )
        {
            msg_Err( p_this, "could not read block %d", i_sector );
            goto error;
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

        i_ret = ioctl( p_vcddev->i_device_handle, SCIOCCOMMAND, &sc );
        if( i_ret == -1 )
        {
            msg_Err( p_this, "SCIOCCOMMAND failed" );
            goto error;
        }
        if( sc.retsts || sc.error )
        {
            msg_Err( p_this, "SCSI command failed: status %d error %d",
                             sc.retsts, sc.error );
            goto error;
        }

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
        int i_size = VCD_SECTOR_SIZE;

        if( ioctl( p_vcddev->i_device_handle, CDRIOCSETBLOCKSIZE, &i_size )
            == -1 )
        {
            msg_Err( p_this, "Could not set block size" );
            goto error;
        }

        if( lseek( p_vcddev->i_device_handle,
                   i_sector * VCD_SECTOR_SIZE, SEEK_SET ) == -1 )
        {
            msg_Err( p_this, "Could not lseek to sector %d", i_sector );
            goto error;
        }

        if( read( p_vcddev->i_device_handle,
                  p_block, VCD_SECTOR_SIZE * i_nb ) == -1 )
        {
            msg_Err( p_this, "Could not read sector %d", i_sector );
            goto error;
        }

#else
        for( int i = 0; i < i_nb; i++ )
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
                    goto error;
                else
                    break;
            }
        }
#endif
    }

    /* For VCDs, we don't want to keep the header and footer of the
     * sectors read */
    if( i_type == VCD_TYPE )
    {
        for( int i = 0; i < i_nb; i++ )
        {
            memcpy( p_buffer + i * VCD_DATA_SIZE,
                    p_block + i * VCD_SECTOR_SIZE + VCD_DATA_START,
                    VCD_DATA_SIZE );
        }
        free( p_block );
    }

    return( 0 );

error:
    if( i_type == VCD_TYPE )
        free( p_block );
    return( -1 );
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
    FILE *cuefile     = NULL;
    vcddev_toc_t *p_toc = &p_vcddev->toc;
    char line[1024];
    bool b_found      = false;

    /* Check if we are dealing with a .cue file */
    p_pos = strrchr( psz_dev, '.' );
    if( p_pos && !strcasecmp( p_pos, ".cue" ) )
    {
        /* psz_dev must be the cue file. Let's assume there's a .bin
         * file with the same filename */
        if( asprintf( &psz_vcdfile, "%.*s.bin", (int)(p_pos - psz_dev),
                      psz_dev ) < 0 )
            psz_vcdfile = NULL;
        psz_cuefile = strdup( psz_dev );
    }
    else
    if( p_pos )
    {
        /* psz_dev must be the actual vcd file. Let's assume there's a .cue
         * file with the same filename */
        if( asprintf( &psz_cuefile, "%.*s.cue", (int)(p_pos - psz_dev),
                      psz_dev ) < 0 )
            psz_cuefile = NULL;
        psz_vcdfile = strdup( psz_dev );
    }
    else
    {
        if( asprintf( &psz_cuefile, "%s.cue", psz_dev ) == -1 )
            psz_cuefile = NULL;
         /* If we need to look up the .cue file, then we don't have to look
          * for the vcd */
        psz_vcdfile = strdup( psz_dev );
    }

    if( psz_cuefile == NULL || psz_vcdfile == NULL )
        goto error;

    /* Open the cue file and try to parse it */
    msg_Dbg( p_this,"trying .cue file: %s", psz_cuefile );
    cuefile = vlc_fopen( psz_cuefile, "rt" );
    if( cuefile == NULL )
    {
        msg_Dbg( p_this, "could not find .cue file" );
        goto error;
    }

    msg_Dbg( p_this,"guessing vcd image file: %s", psz_vcdfile );
    p_vcddev->i_vcdimage_handle = vlc_open( psz_vcdfile,
                                    O_RDONLY | O_NONBLOCK | O_BINARY );

    while( fgets( line, 1024, cuefile ) && !b_found )
    {
        /* We have a cue file, but no valid vcd file yet */
        char filename[1024];
        char type[16];
        int i_temp = sscanf( line, "FILE \"%1023[^\"]\" %15s", filename, type );
        switch( i_temp )
        {
            case 2:
                msg_Dbg( p_this, "the cue file says the data file is %s", type );
                if( strcasecmp( type, "BINARY" ) )
                    goto error; /* Error if not binary, otherwise treat as case 1 */
                /* fallthrough */
            case 1:
                if( p_vcddev->i_vcdimage_handle == -1 )
                {
                    msg_Dbg( p_this, "we could not find the data file, but we found a new path" );
                    free( psz_vcdfile);
                    if( *filename != '/' && ((p_pos = strrchr( psz_cuefile, '/' ))
                        || (p_pos = strrchr( psz_cuefile, '\\' ) )) )
                    {
                        psz_vcdfile = malloc( strlen(filename) +
                                      (p_pos - psz_cuefile + 1) + 1 );
                        strncpy( psz_vcdfile, psz_cuefile, (p_pos - psz_cuefile + 1) );
                        strcpy( psz_vcdfile + (p_pos - psz_cuefile + 1), filename );
                    } else psz_vcdfile = strdup( filename );
                    msg_Dbg( p_this,"using vcd image file: %s", psz_vcdfile );
                    p_vcddev->i_vcdimage_handle = vlc_open( psz_vcdfile,
                                        O_RDONLY | O_NONBLOCK | O_BINARY );
                }
                b_found = true;
            default:
                break;
        }
    }

    if( p_vcddev->i_vcdimage_handle == -1)
        goto error;

    /* Try to parse the i_tracks and p_sectors info so we can just forget
     * about the cuefile */
    p_toc->i_tracks = 0;

    while( fgets( line, 1024, cuefile ) && p_toc->i_tracks < INT_MAX-1 )
    {
        /* look for a TRACK line */
        char psz_dummy[10];
        if( !sscanf( line, "%9s", psz_dummy ) || strcmp(psz_dummy, "TRACK") )
            continue;

        /* look for an INDEX line */
        while( fgets( line, 1024, cuefile ) )
        {
            int i_num, i_min, i_sec, i_frame;

            if( (sscanf( line, "%*9s %2u %2u:%2u:%2u", &i_num,
                         &i_min, &i_sec, &i_frame ) != 4) || (i_num != 1) )
                continue;

            vcddev_sector_t *buf = realloc (p_toc->p_sectors,
                                            (p_toc->i_tracks + 1) * sizeof (*buf));
            if (buf == NULL)
                goto error;
            p_toc->p_sectors = buf;
            p_toc->p_sectors[p_toc->i_tracks].i_lba = MSF_TO_LBA(i_min, i_sec, i_frame);
            p_toc->p_sectors[p_toc->i_tracks].i_control = 0x00;
            msg_Dbg( p_this, "vcd track %i begins at sector:%i",
                     p_toc->i_tracks, p_toc->p_sectors[p_toc->i_tracks].i_lba );
            p_toc->i_tracks++;
            break;
        }
    }

    /* fill in the last entry */
    vcddev_sector_t *buf = realloc (p_toc->p_sectors,
                                    (p_toc->i_tracks + 1) * sizeof (*buf));
    if (buf == NULL)
        goto error;
    p_toc->p_sectors = buf;
    p_toc->p_sectors[p_toc->i_tracks].i_lba =
            lseek(p_vcddev->i_vcdimage_handle, 0, SEEK_END) / VCD_SECTOR_SIZE;
    p_toc->p_sectors[p_toc->i_tracks].i_control = 0x00;
    msg_Dbg( p_this, "vcd track %i, begins at sector:%i",
             p_toc->i_tracks, p_toc->p_sectors[p_toc->i_tracks].i_lba );
    p_toc->i_tracks++;
    p_toc->i_first_track = 1;
    p_toc->i_last_track = p_toc->i_tracks;
    i_ret = 0;
    goto end;

error:
    free( p_toc->p_sectors );
    memset( p_toc, 0, sizeof(*p_toc) );
end:
    if( cuefile ) fclose( cuefile );
    free( psz_cuefile );
    free( psz_vcdfile );

    return i_ret;
}

/****************************************************************************
 * CloseVCDImage: closes a vcd image opened by OpenVCDImage
 ****************************************************************************/
static void CloseVCDImage( vlc_object_t * p_this, vcddev_t *p_vcddev )
{
    VLC_UNUSED( p_this );
    if( p_vcddev->i_vcdimage_handle != -1 )
        vlc_close( p_vcddev->i_vcdimage_handle );
    else
        return;

    free( p_vcddev->toc.p_sectors );
}

#if defined( __APPLE__ )
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

    if( !service )
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

        if( ( pTOC = malloc( buf_len ) ) != NULL )
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
 * darwin_getNumberOfTracks: get number of tracks in TOC
 *                           and first and last CDDA ones
 ****************************************************************************/
static int darwin_getNumberOfTracks( CDTOC *pTOC, int i_descriptors,
                                     int *pi_first_track,
                                     int *pi_last_track )
{
    u_char track;
    int i, i_tracks = 0;
    int i_min = CD_MAX_TRACK_NO;
    int i_max = CD_MIN_TRACK_NO;
    CDTOCDescriptor *pTrackDescriptors = NULL;

    pTrackDescriptors = (CDTOCDescriptor *)pTOC->descriptors;

    for( i = i_descriptors; i > 0; i-- )
    {
        track = pTrackDescriptors[i].point;

        if( track > CD_MAX_TRACK_NO || track < CD_MIN_TRACK_NO )
            continue;

        if( pTrackDescriptors[i].adr == 0x01 /* kCDSectorTypeCDDA */ )
        {
            i_min = __MIN(i_min, track);
            i_max = __MAX(i_max, track);
        }

        i_tracks++;
    }

    if( i_max < i_min )
        *pi_first_track = *pi_last_track = 0;
    else
    {
        *pi_first_track = i_min;
        *pi_last_track = i_max;
    }

    return( i_tracks );
}
#endif /* __APPLE__ */

#if defined( _WIN32 )
/*****************************************************************************
 * win32_vcd_open: open vcd drive
 *****************************************************************************
 * Use IOCTLs on WinNT/2K/XP.
 *****************************************************************************/
static int win32_vcd_open( vlc_object_t * p_this, const char *psz_dev,
                           vcddev_t *p_vcddev )
{
    /* Initializations */
    p_vcddev->h_device_handle = NULL;

    char psz_win32_drive[7];

    msg_Dbg( p_this, "using winNT/2K/XP ioctl layer" );

    sprintf( psz_win32_drive, "\\\\.\\%c:", psz_dev[0] );

    p_vcddev->h_device_handle = CreateFileA( psz_win32_drive, GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL, OPEN_EXISTING,
                                            FILE_FLAG_NO_BUFFERING |
                                            FILE_FLAG_RANDOM_ACCESS, NULL );
    return (p_vcddev->h_device_handle == NULL) ? -1 : 0;
}

#endif /* _WIN32 */

#ifdef __OS2__
/*****************************************************************************
 * os2_vcd_open: open vcd drive
 *****************************************************************************/
static int os2_vcd_open( vlc_object_t * p_this, const char *psz_dev,
                         vcddev_t *p_vcddev )
{
    char device[] = "X:";
    HFILE hcd;
    ULONG i_action;
    ULONG rc;

    p_vcddev->hcd = 0;

    device[0] = psz_dev[0];
    rc = DosOpen( device, &hcd, &i_action, 0, FILE_NORMAL,
                  OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                  OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_DASD,
                  NULL);
    if( rc )
    {
        msg_Err( p_this, "could not open the device %s", psz_dev );

        return -1;
    }

    p_vcddev->hcd = hcd;
    return 0;
}

#endif

/* */
#define CDTEXT_MAX_BLOCKS 8
#define CDTEXT_MAX_TRACKS 0x7f
#define CDTEXT_PACK_SIZE 18
#define CDTEXT_PACK_HEADER 4
#define CDTEXT_PACK_PAYLOAD 12
#define CDTEXT_TEXT_BUFFER 160 /* arbitrary from the sony docs,
                                  < theorical max 12 * (256 - 4) */
enum cdtext_charset_e
{
    CDTEXT_CHARSET_ISO88591 = 0x00,
    CDTEXT_CHARSET_ASCII7BIT = 0x01,
    CDTEXT_CHARSET_MSJIS = 0x80,
};

static void CdTextAppendPayload( const char *buffer, size_t i_len,
                                 enum cdtext_charset_e e_charset, char **ppsz_text )
{
    size_t i_alloc = *ppsz_text ? strlen( *ppsz_text ) : 0;
    size_t i_extend;
    const char *from_charset;
    switch( e_charset )
    {
        case CDTEXT_CHARSET_ASCII7BIT:
            i_extend = i_len;
            from_charset = NULL;
            break;
        case CDTEXT_CHARSET_ISO88591:
            i_extend = i_len * 2;
            from_charset = "ISO-8859-1";
            break;
        case CDTEXT_CHARSET_MSJIS:
            i_extend = i_len * 4;
            from_charset = "SHIFT-JIS";
            break;
        default: /* no known conversion */
            return;
    }
    size_t i_newsize = i_alloc + i_extend * 2 + 1;

    char *psz_realloc = realloc( *ppsz_text, i_newsize );
    if( !psz_realloc )
        return;
    *ppsz_text = psz_realloc;

    /* copy/convert result */
    if ( from_charset == NULL )
    {
        memcpy( &psz_realloc[i_alloc], buffer, i_len );
        psz_realloc[i_alloc + i_len] = 0;
        EnsureUTF8( psz_realloc );
    }
    else
    {
        vlc_iconv_t ic = vlc_iconv_open( "UTF-8", from_charset );
        if( ic != (vlc_iconv_t) -1 )
        {
            const char *psz_in = buffer;
            size_t i_in = i_len;
            char *psz_out = &psz_realloc[i_alloc];
            size_t i_out = i_extend;
            if( VLC_ICONV_ERR != vlc_iconv( ic, &psz_in, &i_in, &psz_out, &i_out ) )
                psz_realloc[i_alloc + i_extend - i_out] = 0;
            vlc_iconv_close( ic );
        }
    }
}

/* Payload length without terminating 0 */
static size_t CdTextPayloadLength( const char *p_buffer, size_t i_buffer,
                                   bool b_doublebytes )
{
    if( b_doublebytes )
    {
        size_t i_len = 0;
        for( size_t i=0; i<i_buffer/2; i++ )
        {
            if(p_buffer[0] == 0 && p_buffer[1] == 0)
                break;
            i_len += 2;
            p_buffer += 2;
        }
        return i_len;
    }
    else return strnlen( p_buffer, i_buffer );
}

static void CdTextParsePackText( const uint8_t *p_pack,
                                 enum cdtext_charset_e e_charset,
                                 size_t *pi_textbuffer,
                                 size_t *pi_repeatbuffer,
                                 char *textbuffer,
                                 int *pi_last_track,
                                 char *pppsz_info[CDTEXT_MAX_TRACKS + 1][0x10] )
{
    const uint8_t i_pack_type = p_pack[0];
    uint8_t i_track = p_pack[1] & 0x7f;
    const bool b_double_byte = p_pack[3] & 0x80;
    const uint8_t i_char_position = p_pack[3] & 0x0f;

    if( i_char_position == 0 )
        *pi_textbuffer = 0; /* not using remains */

    const uint8_t *p_start = &p_pack[CDTEXT_PACK_HEADER];
    const uint8_t *p_end = p_start + CDTEXT_PACK_PAYLOAD;

    for( const uint8_t *p_readpos = p_start; p_readpos < p_end ; )
    {
        size_t i_payload = CdTextPayloadLength( (char *)p_readpos,
                                                p_end - p_readpos,
                                                b_double_byte );

        /* update max used track # */
        if( i_payload > 0 )
            *pi_last_track = __MAX( *pi_last_track, i_track );

        /* check for repeats */
        if( i_payload == 1 && p_readpos[0] == '\t' &&
            *pi_repeatbuffer && !*pi_textbuffer )
        {
            *pi_textbuffer = *pi_repeatbuffer;
            textbuffer[*pi_textbuffer] = 0;
        }
        else
        {
            /* copy out segment to buffer */
            size_t i_append = i_payload;
            if( *pi_textbuffer + i_payload >= CDTEXT_TEXT_BUFFER )
                i_append = CDTEXT_TEXT_BUFFER - *pi_textbuffer;
            memcpy( &textbuffer[*pi_textbuffer], p_readpos, i_append );
            *pi_textbuffer += i_append;
            *pi_repeatbuffer = 0;
        }

        /* end of pack or just first split ? */
        if( &p_readpos[i_payload] < p_end ) /* not continuing */
        {
            /* commit */
            if(*pi_textbuffer > 0)
            {
                CdTextAppendPayload( textbuffer, *pi_textbuffer, e_charset,
                                     &pppsz_info[i_track][i_pack_type-0x80] );
                *pi_repeatbuffer = *pi_textbuffer;
                *pi_textbuffer = 0;

                if(++i_track > CDTEXT_MAX_TRACKS) /* increment for next part of the split */
                    break;
            }
            /* set read pointer for next track in same pack */
            p_readpos = p_readpos + i_payload + (b_double_byte ? 2 : 1);
        }
        else
        {
            p_readpos = p_end;
        }
    }
}

static int CdTextParse( vlc_meta_t ***ppp_tracks, int *pi_tracks,
                        const uint8_t *p_buffer, int i_buffer )
{
    char *pppsz_info[CDTEXT_MAX_TRACKS + 1][0x10];
    int i_track_last = -1;
    if( i_buffer < 4 )
        return -1;

    p_buffer += 4;
    i_buffer -= 4;

    /* block size information is split in a sequence of 3 */
    const uint8_t *bsznfopayl[3] = { NULL, NULL, NULL };
    for( int i = 0; i < i_buffer/CDTEXT_PACK_SIZE; i++ )
    {
        const uint8_t *p_pack = &p_buffer[CDTEXT_PACK_SIZE*i];
        const uint8_t i_block_number = (p_pack[3] >> 4) & 0x07;
        if( i_block_number > 0 )
            continue;
        if( p_pack[0] == 0x8f )
        {
            const int i_track = p_pack[1] & 0x7f;
            /* can't be higher than 3 blocks */
            if( i_track > 2 )
                return -1;
            /* duplicate should not happen */
            if( bsznfopayl[i_track] != NULL )
                return -1;
            /* point to payload (12) */
            bsznfopayl[i_track] = &p_pack[CDTEXT_PACK_HEADER];
        }
    }
    /* incomplete ? */
    if( (!bsznfopayl[0] ^ !bsznfopayl[1]) ||
        (!bsznfopayl[1] ^ !bsznfopayl[2]) )
        return -1;

    memset( pppsz_info, 0, sizeof(pppsz_info) );

    enum cdtext_charset_e e_textpackcharset;
    if( bsznfopayl[0] )
    {
        e_textpackcharset = bsznfopayl[0][0];
        /* use superset to fix broken decl */
        if( e_textpackcharset == CDTEXT_CHARSET_ASCII7BIT )
            e_textpackcharset = CDTEXT_CHARSET_ISO88591;
    }
    else e_textpackcharset = CDTEXT_CHARSET_ASCII7BIT;

    /* capture buffer */
    char textbuffer[CDTEXT_TEXT_BUFFER];
    size_t i_textbuffer = 0;
    size_t i_repeatbuffer = 0;
    uint8_t i_prev_pack_type = 0x00;

    for( int i = 0; i < i_buffer/CDTEXT_PACK_SIZE; i++ )
    {
        const uint8_t *p_pack = &p_buffer[CDTEXT_PACK_SIZE*i];
        const uint8_t i_pack_type = p_pack[0];
        //const int i_sequence_number = p_block[2];
        const uint8_t i_block_number = (p_pack[3] >> 4) & 0x07;
        //const int i_crc = (p_block[4+12] << 8) | (p_block[4+13] << 0);

        /* non flushed text buffer */
        if(i_textbuffer && i_pack_type != i_prev_pack_type)
        {
            i_textbuffer = 0;
            i_repeatbuffer = 0;
        }
        i_prev_pack_type = i_pack_type;

        uint8_t i_track = p_pack[1] & 0x7f;
        if( i_track > CDTEXT_MAX_TRACKS ||
            (p_pack[1] & 0x80) /* extension flag */ ||
            i_block_number > 0 /* support only first language */
           )
        {
            i_prev_pack_type = 0x00;
            continue;
        }

        /* */
        switch( i_pack_type )
        {
            case 0x80:
            case 0x81:
            case 0x85:
            case 0x87:
            {
                CdTextParsePackText( p_pack, e_textpackcharset,
                                     &i_textbuffer, &i_repeatbuffer, textbuffer,
                                     &i_track_last, pppsz_info );
                break;
            }
            case 0x82:
            case 0x83:
            case 0x84:
            case 0x86:
            case 0x8d:
            case 0x8e:
            default:
                continue;
        }
    }

    if( i_track_last < 0 )
        return -1;

    vlc_meta_t **pp_tracks = calloc( i_track_last+1, sizeof(*pp_tracks) );
    if( !pp_tracks )
        goto exit;

    for( int j = 0; j < 0x10; j++ )
    {
        for( int i = 0; i <= i_track_last; i++ )
        {
            /* */
            const char *psz_default = pppsz_info[0][j];
            const char *psz_value = pppsz_info[i][j];

            if( !psz_value && !psz_default )
                continue;
            vlc_meta_t *p_track = pp_tracks[i];
            if( !p_track )
            {
                p_track = pp_tracks[i] = vlc_meta_New();
                if( !p_track )
                    continue;
            }
            switch( 0x80 + j )
            {
            case 0x80: /* Album/Title */
                if( i == 0 )
                {
                    vlc_meta_SetAlbum( p_track, psz_value );
                }
                else
                {
                    if( psz_value )
                        vlc_meta_SetTitle( p_track, psz_value );
                    if( psz_default )
                        vlc_meta_SetAlbum( p_track, psz_default );
                }
                break;
            case 0x81: /* Performer */
                vlc_meta_SetArtist( p_track,
                                    psz_value ? psz_value : psz_default );
                break;
            case 0x85: /* Messages */
                vlc_meta_SetDescription( p_track,
                                         psz_value ? psz_value : psz_default );
                break;
            case 0x87: /* Genre */
                vlc_meta_SetGenre( p_track,
                                   psz_value ? psz_value : psz_default );
                break;
            /* FIXME unsupported:
             * 0x82: songwriter
             * 0x83: composer
             * 0x84: arrenger
             * 0x86: disc id */
            }
        }
    }
    /* */
exit:
    for( int j = 0; j < 0x10; j++ )
        for( int i = 0; i <= i_track_last; i++ )
            free( pppsz_info[i][j] );

    *ppp_tracks = pp_tracks;
    *pi_tracks = i_track_last+1;
    return pp_tracks ? 0 : -1;
}

#if defined( __APPLE__ ) || \
    defined( __OS2__ ) || \
    defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H ) || \
    defined( HAVE_SCSIREQ_IN_SYS_SCSIIO_H )
static int CdTextRead( vlc_object_t *p_object, const vcddev_t *p_vcddev,
                       uint8_t **pp_buffer, int *pi_buffer )
{
    VLC_UNUSED( p_object );
    VLC_UNUSED( p_vcddev );
    VLC_UNUSED( pp_buffer );
    VLC_UNUSED( pi_buffer );
    return -1;
}
#elif defined( _WIN32 )
static int CdTextRead( vlc_object_t *p_object, const vcddev_t *p_vcddev,
                       uint8_t **pp_buffer, int *pi_buffer )
{
    VLC_UNUSED( p_object );

    CDROM_READ_TOC_EX TOCEx;
    memset(&TOCEx, 0, sizeof(TOCEx));
    TOCEx.Format = CDROM_READ_TOC_EX_FORMAT_CDTEXT;

    const int i_header_size = __MAX( 4, MINIMUM_CDROM_READ_TOC_EX_SIZE );
    uint8_t header[i_header_size];
    DWORD i_read;
    if( !DeviceIoControl( p_vcddev->h_device_handle, IOCTL_CDROM_READ_TOC_EX,
                          &TOCEx, sizeof(TOCEx), header, i_header_size, &i_read, 0 ) )
        return -1;

    const int i_text = 2 + (header[0] << 8) + header[1];
    if( i_text <= 4 )
        return -1;

    /* Read complete CD-TEXT */
    uint8_t *p_text = calloc( 1, i_text );
    if( !p_text )
        return VLC_EGENERIC;

    if( !DeviceIoControl( p_vcddev->h_device_handle, IOCTL_CDROM_READ_TOC_EX,
                          &TOCEx, sizeof(TOCEx), p_text, i_text, &i_read, 0 ) )
    {
        free( p_text );
        return VLC_EGENERIC;
    }

    /* */
    *pp_buffer = p_text;
    *pi_buffer = i_text;
    return VLC_SUCCESS;
}
#else
static int CdTextRead( vlc_object_t *p_object, const vcddev_t *p_vcddev,
                       uint8_t **pp_buffer, int *pi_buffer )
{
    VLC_UNUSED( p_object );

    if( p_vcddev->i_device_handle == -1 )
        return -1;

    struct cdrom_generic_command gc;
    uint8_t header[4];

    /* Read CD-TEXT size */
    memset( header, 0, sizeof(header) );
    memset( &gc, 0, sizeof(gc) );
    gc.cmd[0] = 0x43;   /* Read TOC */
    gc.cmd[1] = 0x02;   /* MSF */
    gc.cmd[2] = 5;      /* CD-Text */
    gc.cmd[7] = ( sizeof(header) >> 8 ) & 0xff;
    gc.cmd[8] = ( sizeof(header) >> 0 ) & 0xff;

    gc.buflen = sizeof(header);
    gc.buffer = header;
    gc.data_direction = CGC_DATA_READ;
    gc.timeout = 1000;

    if( ioctl( p_vcddev->i_device_handle, CDROM_SEND_PACKET, &gc ) == -1 )
        return VLC_EGENERIC;

    /* If the size is less than 4 it is an error, if it 4 then
     * it means no text data */
    const int i_text = 2 + (header[0] << 8) + header[1];
    if( i_text <= 4 )
        return VLC_EGENERIC;

    /* Read complete CD-TEXT */
    uint8_t *p_text = calloc( 1, i_text );
    if( !p_text )
        return VLC_EGENERIC;

    memset( &gc, 0, sizeof(gc) );
    gc.cmd[0] = 0x43;   /* Read TOC */
    gc.cmd[1] = 0x02;   /* MSF */
    gc.cmd[2] = 5;      /* CD-Text */
    gc.cmd[7] = ( i_text >> 8 ) & 0xff;
    gc.cmd[8] = ( i_text >> 0 ) & 0xff;

    gc.buflen = i_text;
    gc.buffer = p_text;
    gc.data_direction = CGC_DATA_READ;
    gc.timeout = 1000;

    if( ioctl( p_vcddev->i_device_handle, CDROM_SEND_PACKET, &gc ) == -1 )
    {
        free( p_text );
        return VLC_EGENERIC;
    }

    /* */
    *pp_buffer = p_text;
    *pi_buffer = i_text;
    return VLC_SUCCESS;
}
#endif

int ioctl_GetCdText( vlc_object_t *p_object, const vcddev_t *p_vcddev,
                     vlc_meta_t ***ppp_tracks, int *pi_tracks )
{
    uint8_t *p_text;
    int i_text;

    if( p_vcddev->i_vcdimage_handle != -1 )
        return -1;

    if( CdTextRead( p_object, p_vcddev, &p_text, &i_text ) )
        return -1;

    CdTextParse( ppp_tracks, pi_tracks, p_text, i_text );
    free( p_text );
    return 0;
}

