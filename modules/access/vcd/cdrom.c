/****************************************************************************
 * cdrom.c: cdrom tools
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: cdrom.c,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
 *         Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>

#if defined( SYS_BSDI )
#   include <dvd.h>
#elif defined ( SYS_DARWIN )
#   include <CoreFoundation/CFBase.h>
#   include <IOKit/IOKitLib.h>
#   include <IOKit/storage/IOCDTypes.h>
#   include <IOKit/storage/IOCDMedia.h>
#   include <IOKit/storage/IOCDMediaBSDClient.h>
#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
#   include <sys/cdio.h>
#   include <sys/cdrio.h>
#else
#   include <linux/cdrom.h>
#endif

#include "cdrom.h"

/*****************************************************************************
 * Platform specific 
 *****************************************************************************/
#if defined( SYS_DARWIN )
CDTOC *getTOC( const char * );
#define freeTOC( p ) free( (void*)p )
int getNumberOfDescriptors( CDTOC * );
int getNumberOfTracks( CDTOC *, int );
#define CD_MIN_TRACK_NO 01
#define CD_MAX_TRACK_NO 99
#endif

/*****************************************************************************
 * ioctl_ReadTocHeader: Read the TOC header and return the track number.
 *****************************************************************************/
int ioctl_GetTrackCount( int i_fd, const char *psz_dev )
{
    int i_count = -1;

#if defined( SYS_DARWIN )
    CDTOC *pTOC;
    int i_descriptors;

    if( ( pTOC = getTOC( psz_dev ) ) == NULL )
    {
//X        intf_ErrMsg( "vcd error: failed to get the TOC" );
        return( -1 );
    }

    i_descriptors = getNumberOfDescriptors( pTOC );
    i_count = getNumberOfTracks( pTOC, i_descriptors );

    freeTOC( pTOC );

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
    struct ioc_toc_header tochdr;
    
    if( ioctl( i_fd, CDIOREADTOCHEADER, &tochdr ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return -1;
    }

    i_count = tochdr.ending_track - tochdr.starting_track + 1;
    
#else
    struct cdrom_tochdr   tochdr;

    /* First we read the TOC header */
    if( ioctl( i_fd, CDROMREADTOCHDR, &tochdr ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return -1;
    }

    i_count = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;
#endif

    return( i_count );
}

/*****************************************************************************
 * ioctl_GetSectors: Read the Table of Contents and fill p_vcd.
 *****************************************************************************/
int * ioctl_GetSectors( int i_fd, const char *psz_dev )
{
    int i, i_tracks;
    int *p_sectors = NULL;

#if defined( SYS_DARWIN )
    CDTOC *pTOC;
    u_char track;
    int i_descriptors;
    int i_leadout = -1;
    CDTOCDescriptor *pTrackDescriptors;

    if( ( pTOC = getTOC( psz_dev ) ) == NULL )
    {
//X        intf_ErrMsg( "vcd error: failed to get the TOC" );
        return( NULL );
    }

    i_descriptors = getNumberOfDescriptors( pTOC );
    i_tracks = getNumberOfTracks( pTOC, i_descriptors );

    p_sectors = malloc( (i_tracks + 1) * sizeof(int) );
    if( p_sectors == NULL )
    {
//X        intf_ErrMsg( "vcd error: could not allocate p_sectors" );
        freeTOC( pTOC );
        return NULL;
    }
    
    pTrackDescriptors = pTOC->descriptors;

    for( i_tracks = 0, i = 0; i <= i_descriptors; i++ )
    {
        track = pTrackDescriptors[i].point;

        if( track == 0xA2 )
            i_leadout = i;

        if( track > CD_MAX_TRACK_NO || track < CD_MIN_TRACK_NO )
            continue;

        p_sectors[i_tracks++] = 
            CDConvertMSFToLBA( pTrackDescriptors[i].p );
    }

    if( i_leadout == -1 )
    {
//X        intf_ErrMsg( "vcd error: leadout not found" );
        free( p_sectors );
        freeTOC( pTOC );
        return( NULL );
    } 

    /* set leadout sector */
    p_sectors[i_tracks] = 
        CDConvertMSFToLBA( pTrackDescriptors[i_leadout].p ); 

    freeTOC( pTOC );

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
    struct ioc_read_toc_entry toc_entries;

    i_tracks = ioctl_GetTrackCount( i_fd, psz_dev );
    p_sectors = malloc( (i_tracks + 1) * sizeof(int) );
    if( p_sectors == NULL )
    {
//X        intf_ErrMsg( "vcd error: could not allocate p_sectors" );
        return NULL;
    }

    toc_entries.address_format = CD_LBA_FORMAT;
    toc_entries.starting_track = 0;
    toc_entries.data_len = ( i_tracks + 1 ) * sizeof( struct cd_toc_entry ); 
    toc_entries.data = (struct cd_toc_entry *) malloc( toc_entries.data_len );
    if( toc_entries.data == NULL )
    {
//X        intf_ErrMsg( "vcd error: not enoug memory" );
        free( p_sectors );
        return NULL;
    }
 
    /* Read the TOC */
    if( ioctl( i_fd, CDIOREADTOCENTRYS, &toc_entries ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read the TOC" );
        free( p_sectors );
        free( toc_entries.data );
        return NULL;
    }
    
    /* Fill the p_sectors structure with the track/sector matches */
    for( i = 0 ; i <= i_tracks ; i++ )
    {
        p_sectors[ i ] = ntohl( toc_entries.data[i].addr.lba );
    }
#else
    struct cdrom_tochdr   tochdr;
    struct cdrom_tocentry tocent;

    /* First we read the TOC header */
    if( ioctl( i_fd, CDROMREADTOCHDR, &tochdr ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read TOCHDR" );
        return NULL;
    }

    i_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;

    p_sectors = malloc( (i_tracks + 1) * sizeof(int) );
    if( p_sectors == NULL )
    {
//X        intf_ErrMsg( "vcd error: could not allocate p_sectors" );
        return NULL;
    }

    /* Fill the p_sectors structure with the track/sector matches */
    for( i = 0 ; i <= i_tracks ; i++ )
    {
        tocent.cdte_format = CDROM_LBA;
        tocent.cdte_track =
            ( i == i_tracks ) ? CDROM_LEADOUT : tochdr.cdth_trk0 + i;

        if( ioctl( i_fd, CDROMREADTOCENTRY, &tocent ) == -1 )
        {
//X            intf_ErrMsg( "vcd error: could not read TOCENTRY" );
            free( p_sectors );
            return NULL;
        }

        p_sectors[ i ] = tocent.cdte_addr.lba;
    }
#endif

    return p_sectors;
}

/****************************************************************************
 * ioctl_ReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
int ioctl_ReadSector( int i_fd, int i_sector, byte_t * p_buffer )
{
    byte_t p_block[ VCD_SECTOR_SIZE ];

#if defined( SYS_DARWIN )
    dk_cd_read_t cd_read;

    memset( &cd_read, 0, sizeof(cd_read) );

    cd_read.offset = i_sector * VCD_SECTOR_SIZE;
    cd_read.sectorArea = kCDSectorAreaSync | kCDSectorAreaHeader |
                         kCDSectorAreaSubHeader | kCDSectorAreaUser |
                         kCDSectorAreaAuxiliary;
    cd_read.sectorType = kCDSectorTypeUnknown;

    cd_read.buffer = p_block;
    cd_read.bufferLength = sizeof(p_block);

    if( ioctl( i_fd, DKIOCCDREAD, &cd_read ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read block %d", i_sector );
        return( -1 );
    }

#elif defined( HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H )
    
    int i_size = VCD_SECTOR_SIZE;

    if( ioctl( i_fd, CDRIOCSETBLOCKSIZE, &i_size ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: Could not set block size" );
        return( -1 );
    }

    if( lseek( i_fd, i_sector * VCD_SECTOR_SIZE, SEEK_SET ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: Could not lseek to sector %d", i_sector );
        return( -1 );
    }

    if( read( i_fd, p_block, VCD_SECTOR_SIZE ) == -1 )
    {
//X        intf_ErrMsg( "vcd error: Could not read sector %d", i_sector );
        return( -1 );
    }

#else
    int i_dummy = i_sector + 2 * CD_FRAMES;

#define p_msf ((struct cdrom_msf0 *)p_block)
    p_msf->minute =   i_dummy / (CD_FRAMES * CD_SECS);
    p_msf->second = ( i_dummy % (CD_FRAMES * CD_SECS) ) / CD_FRAMES;
    p_msf->frame =  ( i_dummy % (CD_FRAMES * CD_SECS) ) % CD_FRAMES;
#undef p_msf

    if( ioctl(i_fd, CDROMREADRAW, p_block) == -1 )
    {
//X        intf_ErrMsg( "vcd error: could not read block %i from disc",
//X                     i_sector );
        return( -1 );
    }
#endif

    /* We don't want to keep the header of the read sector */
    memcpy( p_buffer, p_block + VCD_DATA_START, VCD_DATA_SIZE );

    return( 0 );
}

#if defined( SYS_DARWIN )
/****************************************************************************
 * getTOC: get the TOC
 ****************************************************************************/
CDTOC *getTOC( const char *psz_dev )
{
    mach_port_t port;
    char *psz_devname;
    kern_return_t ret;
    CDTOC *pTOC = NULL;
    io_iterator_t iterator;
    io_registry_entry_t service;
    CFDictionaryRef properties;
    CFDataRef data;

    if( psz_dev == NULL )
    {
//X        intf_ErrMsg( "vcd error: invalid device path" );
        return( NULL );
    }

    /* get the device name */
    if( ( psz_devname = strrchr( psz_dev, '/') ) != NULL )
        ++psz_devname;
    else
        psz_devname = (char *)psz_dev;

    /* unraw the device name */
    if( *psz_devname == 'r' )
        ++psz_devname;

    /* get port for IOKit communication */
    if( ( ret = IOMasterPort( MACH_PORT_NULL, &port ) ) != KERN_SUCCESS )
    {
//X        intf_ErrMsg( "vcd error: IOMasterPort: 0x%08x", ret );
        return( NULL );
    }

    /* get service iterator for the device */
    if( ( ret = IOServiceGetMatchingServices( 
                    port, IOBSDNameMatching( port, 0, psz_devname ),
                    &iterator ) ) != KERN_SUCCESS )
    {
//X        intf_ErrMsg( "vcd error: IOServiceGetMatchingServices: 0x%08x", ret );
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
//X            intf_ErrMsg( "vcd error: "
//X                         "IORegistryEntryGetParentIterator: 0x%08x", ret );
            IOObjectRelease( service );
            return( NULL );
        }

        IOObjectRelease( service );
        service = IOIteratorNext( iterator );
        IOObjectRelease( iterator );
    }

    if( service == NULL )
    {
//X        intf_ErrMsg( "vcd error: search for kIOCDMediaClass came up empty" );
        return( NULL );
    }

    /* create a CF dictionary containing the TOC */
    if( ( ret = IORegistryEntryCreateCFProperties( service, &properties,
                    kCFAllocatorDefault, kNilOptions ) ) != KERN_SUCCESS )
    {
//X        intf_ErrMsg( "vcd error: "
//X                     " IORegistryEntryCreateCFProperties: 0x%08x", ret );
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
//X        intf_ErrMsg( "vcd error: CFDictionaryGetValue failed" );
    }

    CFRelease( properties );
    IOObjectRelease( service ); 

    return( pTOC ); 
}

/****************************************************************************
 * getNumberOfDescriptors: get number of descriptors in TOC 
 ****************************************************************************/
int getNumberOfDescriptors( CDTOC *pTOC )
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
 * getNumberOfTracks: get number of tracks in TOC 
 ****************************************************************************/
int getNumberOfTracks( CDTOC *pTOC, int i_descriptors )
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
#endif
