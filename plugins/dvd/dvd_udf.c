/*****************************************************************************
 * dvd_udf.c: udf filesystem tools.
 *****************************************************************************
 * Mainly used to find asolute logical block adress of *.ifo files. It only
 * contains the basic udf handling functions
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_udf.c,v 1.11 2001/06/12 22:14:44 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - dvdudf by Christian Wolff <scarabaeus@convergence.de>
 *  - fixes by Billy Biggs <vektor@dumbterm.net>
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

/*
 * Preamble
 */
#include "defs.h"

#ifdef HAVE_CSS
#   define MODULE_NAME dvd
#else /* HAVE_CSS */
#   define MODULE_NAME dvdnocss
#endif /* HAVE_CSS */

#include "modules_inner.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include <videolan/dvdcss.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "input_dvd.h"
#include "dvd_ifo.h"

#include "modules.h"
#include "modules_export.h"

#define UDFADshort      1
#define UDFADlong       2
#define UDFADext        4

typedef struct partition_s
{
    boolean_t     b_valid;
    u8            pi_volume_desc[128];
    u16           i_flags;
    u16           i_number;
    u8            pi_contents[32];
    u32           i_access_type;
    u32           i_start;
    u32           i_length;
    dvdcss_handle dvdhandle;
} partition_t;

typedef struct ad_s
{
    u32         i_location;
    u32         i_length;
    u8          i_flags;
    u16         i_partition;
} ad_t;

/* for direct data access, LSB first */
#define GETN1(p) ((u8)pi_data[p])
#define GETN2(p) ((u16)pi_data[p]|((u16)pi_data[(p)+1]<<8))
#define GETN4(p) ((u32)pi_data[p]|((u32)pi_data[(p)+1]<<8)|((u32)pi_data[(p)+2]<<16)|((u32)pi_data[(p)+3]<<24))
#define GETN(p,n,target) memcpy(target,&pi_data[p],n)


/*****************************************************************************
 * UDFReadLB: reads absolute Logical Block of the disc
 *****************************************************************************
 * Returns number of read bytes on success, 0 on error
 *****************************************************************************/
static int UDFReadLB( dvdcss_handle dvdhandle, off_t i_lba,
                      size_t i_block_count, u8 *pi_data )
{
    if( dvdcss_seek( dvdhandle, i_lba ) < 0 )
    {
        intf_ErrMsg( "UDF: Postion not found" );
        return 0;
    }

    return dvdcss_read( dvdhandle, pi_data, i_block_count, DVDCSS_NOFLAGS );
}


/*****************************************************************************
 * UDFDecode: decode unicode encoded udf data
 *****************************************************************************/
static int UDFDecode( u8 * pi_data, int i_len, char * psz_target )
{
    int     p = 1;
    int     i = 0;

    if( !( pi_data[0] & 0x18 ) )
    {
        psz_target[0] = '\0';
        return 0;
    }

    if( pi_data[0] & 0x10 )
    {
        /* ignore MSB of unicode16 */
        p++;

        while( p < i_len )
        {
            psz_target[i++] = pi_data[p+=2];
        }
    }
    else
    {
        while( p < i_len )
        {
            psz_target[i++] = pi_data[p++];
        }
    }
    
    psz_target[i]='\0';

    return 0;
}

#if 0
/**
 *
 **/

int UDFEntity (u8 *data, u8 *Flags, char *Identifier)
{
    Flags[0] = data[0];
    strncpy (Identifier, &data[1], 5);

    return 0;
}
#endif


/*****************************************************************************
 * UDFDescriptor: gives a tag ID from your data to find out what it refers to
 *****************************************************************************/
static int UDFDescriptor( u8 * pi_data, u16 * pi_tag_id )
{
    pi_tag_id[0] = GETN2( 0 );
    /* TODO: check CRC 'n stuff */

    return 0;
}


/*****************************************************************************
 * UDFExtendAD: main volume information
 *****************************************************************************/
static int UDFExtentAD (u8 * pi_data, u32 * pi_length, u32 * pi_location)
{
    pi_length[0] = GETN4( 0 );
    pi_location[0] = GETN4( 4 );

    return 0;
}


/*****************************************************************************
 * UDFAD: file set information
 *****************************************************************************/
static int UDFAD( u8 * pi_data, struct ad_s * p_ad, u8 i_type,
                  struct partition_s partition )
{
    p_ad->i_length = GETN4( 0 );
    p_ad->i_flags = p_ad->i_length >> 30;
    p_ad->i_length &= 0x3FFFFFFF;

    switch( i_type )
    {
        case UDFADshort:
            p_ad->i_location = GETN4( 4 );
            /* use number of current partition */
            p_ad->i_partition = partition.i_number;
             break;

        case UDFADlong:
            p_ad->i_location = GETN4( 4 );
            p_ad->i_partition = GETN2( 8 );
            break;

        case UDFADext:
            p_ad->i_location = GETN4( 12 );
            p_ad->i_partition = GETN2( 16 );
            break;
    }

    return 0;
}


/*****************************************************************************
 * UDFICB: takes Information Control Block from pi_data
 *****************************************************************************/
static int UDFICB( u8 * pi_data, u8 * pi_file_type, u16 * pi_flags)
{
    pi_file_type[0] = GETN1( 11 );
    pi_flags[0] = GETN2( 18 );

    return 0;
}


/*****************************************************************************
 * UDFPartition: gets partition descriptor
 *****************************************************************************/
static int UDFPartition( u8 * pi_data, u16 * pi_flags, u16 * pi_nb,
                         char * ps_contents, u32 * pi_start, u32 * pi_length )
{
    pi_flags[0] = GETN2( 20 );
    pi_nb[0] = GETN2( 22 );
    GETN( 24, 32, ps_contents );
    pi_start[0] = GETN4( 188 );
    pi_length[0] = GETN4( 192 );

    return 0;
}


/*****************************************************************************
 * UDFLogVolume: reads the volume descriptor and checks the parameters
 *****************************************************************************
 * Returns 0 on OK, 1 on error
 *****************************************************************************/
static int UDFLogVolume(u8 * pi_data, char * p_volume_descriptor )
{
    u32 i_lb_size;
    u32 i_MT_L;
    u32 i_N_PM;

    UDFDecode( &pi_data[84], 128, p_volume_descriptor );

    i_lb_size = GETN4( 212 );        // should be 2048
    i_MT_L = GETN4( 264 );        // should be 6
    i_N_PM = GETN4( 268 );        // should be 1

    if( i_lb_size != DVD_LB_SIZE )
    {
        intf_ErrMsg( "UDF: Non valid sector size (%d)", i_lb_size );
        return 1;
    }

    return 0;
}


/*****************************************************************************
 * UDFFileEntry: fills a ad_t struct with information at pi_data
 *****************************************************************************/
static int UDFFileEntry( u8 * pi_data, u8 * pi_file_type, struct ad_s * p_ad,
                         struct partition_s partition )
{
    u8      i_file_type;
    u16     i_flags;
    u32     i_L_EA;
    u32     i_L_AD;
    int     p;

    UDFICB( &pi_data[16], &i_file_type, &i_flags );

    pi_file_type[0] = i_file_type;
    i_L_EA = GETN4( 168 );
    i_L_AD = GETN4( 172 );
    p = 176 + i_L_EA;

    while( p < 176 + i_L_EA + i_L_AD )
    {
        switch( i_flags & 0x07 )
        {
        case 0:
            UDFAD( &pi_data[p], p_ad, UDFADshort, partition );
            p += 0x08;
            break;
        case 1:
            UDFAD( &pi_data[p], p_ad, UDFADlong, partition );
            p += 0x10;
            break;
        case 2:
            UDFAD( &pi_data[p], p_ad, UDFADext, partition );
            p += 0x14;
            break;
        case 3:
            switch( i_L_AD )
            {
            case 0x08:
                UDFAD( &pi_data[p], p_ad, UDFADshort, partition );
                break;
            case 0x10:
                UDFAD( &pi_data[p], p_ad, UDFADlong, partition );
                break;
            case 0x14:
                UDFAD( &pi_data[p], p_ad, UDFADext, partition );
                break;
            }
        default:
            p += i_L_AD;
            break;
        }
    }

    return 0;
}


/*****************************************************************************
 * UDFFileIdentifier: gives filename and characteristics of pi_data
 *****************************************************************************/
static int UDFFileIdentifier( u8 * pi_data, u8 * pi_file_characteristics,
                              char * psz_filename, struct ad_s * p_file_icb,
                              struct partition_s partition )
{
    u8      i_L_FI;
    u16     i_L_IU;
  
    pi_file_characteristics[0] = GETN1( 18 );
    i_L_FI = GETN1( 19 );
    UDFAD( &pi_data[20], p_file_icb, UDFADlong, partition );
    i_L_IU = GETN2( 36 );

    if( i_L_FI )
    {
        UDFDecode( &pi_data[38+i_L_IU], i_L_FI, psz_filename );
    }
    else
    {
        psz_filename[0]='\0';
    }

    return  4 * ( ( 38 + i_L_FI + i_L_IU + 3 ) / 4 );
}


/*****************************************************************************
 * UDFMapICB: Maps ICB to FileAD
 *****************************************************************************
 * ICB: Location of ICB of directory to scan
 * FileType: Type of the file
 * File: Location of file the ICB is pointing to
 * return 1 on success, 0 on error;
 *****************************************************************************/
static int UDFMapICB( struct ad_s icb, u8 * pi_file_type, struct ad_s * p_file,
                      struct partition_s partition )
{
    u8      pi_lb[DVD_LB_SIZE];
    u32     i_lba;
    u16     i_tag_id;

    i_lba = partition.i_start + icb.i_location;

    do
    {
        if( !UDFReadLB( partition.dvdhandle, i_lba++, 1, pi_lb ) )
        {
            i_tag_id = 0;
        }
        else
        {
            UDFDescriptor( pi_lb , &i_tag_id );
        }

        if( i_tag_id == 261 )
        {
            UDFFileEntry( pi_lb, pi_file_type, p_file, partition );
            return 1;
        }

    } while( ( i_lba <= partition.i_start + icb.i_location +
               ( icb.i_length - 1 ) / DVD_LB_SIZE ) && ( i_tag_id != 261 ) );

    return 0;
}

/*****************************************************************************
 * UDFScanDir: serach filename in dir
 *****************************************************************************
 * Dir: Location of directory to scan
 * FileName: Name of file to look for
 * FileICB: Location of ICB of the found file
 * return 1 on success, 0 on error;
 *****************************************************************************/
static int UDFScanDir( struct ad_s dir, char * psz_filename,
                       struct ad_s * p_file_icb, struct partition_s partition )
{
    u8      pi_lb[2*DVD_LB_SIZE];
    u32     i_lba;
    u16     i_tag_id;
    u8      i_file_char;
    char    psz_temp[DVD_LB_SIZE];
    int     p;
  
    /* Scan dir for ICB of file */
    i_lba = partition.i_start + dir.i_location;
#if 0
    do
    {
        if( !UDFReadLB( partition.dvdhandle, i_lba++, 1, pi_lb ) )
        {
            i_tag_id = 0;
        }
        else
        {
            p=0;
            while( p < DVD_LB_SIZE )
            {
                UDFDescriptor( &pi_lb[p], &i_tag_id );

                if( i_tag_id == 257 )
                {
                    p += UDFFileIdentifier( &pi_lb[p], &i_file_char,
                                            psz_temp, p_file_icb, partition );
                    if( !strcasecmp( psz_filename, psz_temp ) )
                    {
                        return 1;
                    }
                }
                else
                {
                    p = DVD_LB_SIZE;
                }
            }
        }

    } while( i_lba <=
      partition.i_start + dir.i_location + ( dir.i_length - 1 ) / DVD_LB_SIZE );

#else

    if( UDFReadLB( partition.dvdhandle, i_lba, 2, pi_lb ) <= 0 ) {
        return 0;
    }

    p = 0;
    while( p < dir.i_length )
    {
        if( p > DVD_LB_SIZE )
        {
            ++i_lba;
            p -= DVD_LB_SIZE;
            dir.i_length -= DVD_LB_SIZE;
            if( UDFReadLB( partition.dvdhandle, i_lba, 2, pi_lb ) <= 0 )
            {
                return 0;
            }
        }

        UDFDescriptor( &pi_lb[p], &i_tag_id );

        if( i_tag_id == 257 )
        {
            p += UDFFileIdentifier( &pi_lb[p], &i_file_char,
                                    psz_temp, p_file_icb, partition );
            if( !strcasecmp( psz_filename, psz_temp ) )
            {
                return 1;
            }
        }
        else
        {
            return 0;
        }
    }

#endif
    return 0;
}

/*****************************************************************************
 * UDFFindPartition: looks for a partition on the disc
 *****************************************************************************
 *   partnum: number of the partition, starting at 0
 *   part: structure to fill with the partition information
 *   return 1 if partition found, 0 on error;
 *****************************************************************************/
static int UDFFindPartition( int i_part_nb, struct partition_s *p_partition )
{
    u8          pi_lb[DVD_LB_SIZE];
    u8          pi_anchor[DVD_LB_SIZE];
    u16         i_tag_id;
    u32         i_lba;
    u32         i_MVDS_location;
    u32         i_MVDS_length;
    u32         i_last_sector;
    boolean_t   b_term;
    boolean_t   b_vol_valid;
    int         i;

    /* Find Anchor */
    i_last_sector = 0;
 
    /* try #1, prime anchor */
    i_lba = 256;    
    b_term = 0;

    /* Search anchor loop */
    while( 1 )
    {
        if( UDFReadLB( p_partition->dvdhandle, i_lba, 1, pi_anchor ) )
        {
            UDFDescriptor( pi_anchor, &i_tag_id );
        }
        else
        {
            i_tag_id = 0;
        }

        if( i_tag_id != 2 )
        {                
            /* not an anchor? */
            if( b_term )
            {
                /* final try failed */
                return 0;
            }

            if( i_last_sector )
            {
                /* we already found the last sector
                 * try #3, alternative backup anchor */
                i_lba = i_last_sector;    
            
                /* but that's just about enough, then! */
                b_term = 1;            
            }
            else
            {
                /* TODO: find last sector of the disc (this is optional) */
                if( i_last_sector )
                {
                    /* try #2, backup anchor */
                    i_lba = i_last_sector - 256;                
                }
                else
                {
                    /* unable to find last sector */
                    return 0;
                }
            }
        }
        else
        {
            /* it is an anchor! continue... */
            break;
        }
    }

    /* main volume descriptor */
    UDFExtentAD( &pi_anchor[16], &i_MVDS_length, &i_MVDS_location );
  
    p_partition->b_valid = 0;
    b_vol_valid = 0;
    p_partition->pi_volume_desc[0] = '\0';

    i = 1;
    /* Find Volume Descriptor */
    do
    {
        i_lba = i_MVDS_location;

        do
        {
            if( !UDFReadLB( p_partition->dvdhandle, i_lba++, 1, pi_lb ) )
            {
                i_tag_id = 0;
            }
            else
            {
                UDFDescriptor( pi_lb, &i_tag_id );
            }

            if( ( i_tag_id == 5 ) && ( !p_partition->b_valid ) )
            {
                /* Partition Descriptor */
                UDFPartition( pi_lb,
                              &p_partition->i_flags,
                              &p_partition->i_number,
                              p_partition->pi_contents,
                              &p_partition->i_start,
                              &p_partition->i_length );
                p_partition->b_valid = ( i_part_nb == p_partition->i_number );
            }
            else if( ( i_tag_id == 6 ) && ( !b_vol_valid) )
            {
                /* Logical Volume Descriptor */
                if( UDFLogVolume( pi_lb , p_partition->pi_volume_desc ) )
                {  
                /* TODO: sector size wrong! */
                }
                else
                {
                    b_vol_valid = 1;
                }
            }

        } while( ( i_lba <= i_MVDS_location +
                   ( i_MVDS_length - 1 ) / DVD_LB_SIZE )
                 && ( i_tag_id != 8 )
                 && ( ( !p_partition->b_valid ) || ( !b_vol_valid ) ) );
    
        if( ( !p_partition->b_valid ) || ( !b_vol_valid ) )
        {
            /* backup volume descriptor */
            UDFExtentAD( &pi_anchor[24], &i_MVDS_length, &i_MVDS_location );
        }
    } while( i-- && ( ( !p_partition->b_valid ) || ( !b_vol_valid ) ) );

    /* we only care for the partition, not the volume */
    return( p_partition->b_valid);
}


/*****************************************************************************
 * UDFFindFile: looks for a file on the UDF disc/imagefile
 *****************************************************************************
 * Path has to be the absolute pathname on the UDF filesystem,
 * starting with '/'.
 * returns absolute LB number, or 0 on error
 *****************************************************************************/
u32 UDFFindFile( dvdcss_handle dvdhandle, char * psz_path )
{
    struct partition_s  partition;
    struct ad_s         root_icb;
    struct ad_s         file;
    struct ad_s         icb;
    u32                 i_lba;
    u16                 i_tag_id;
    u8                  pi_lb[DVD_LB_SIZE];
    u8                  i_file_type;
    char                psz_tokenline[DVD_LB_SIZE] = "";
    char *              psz_token;
    int                 i_partition;
 
    strcat( psz_tokenline, psz_path );

    /* Init file descriptor of UDF filesystem (== DVD) */
    partition.dvdhandle = dvdhandle;

    /* Find partition 0, standard partition for DVD-Video */
    i_partition = 0;
    if( !UDFFindPartition( i_partition, &partition ) )
    {
        intf_ErrMsg( "UDF: Partition 0 not found" );
        return 0;
    }
  
    /* Find root dir ICB */
    i_lba = partition.i_start;

    do
    {
        if( !UDFReadLB( dvdhandle, i_lba++, 1, pi_lb ) )
        {
            i_tag_id = 0;
        }
        else
        {
            UDFDescriptor( pi_lb, &i_tag_id );
        }

        if( i_tag_id == 256 )
        {
            /* File Set Descriptor */
            UDFAD( &pi_lb[400], &root_icb, UDFADlong, partition );
        }

    } while( ( i_lba < partition.i_start + partition.i_length )
          && ( i_tag_id != 8) && ( i_tag_id != 256 ) );

    if( i_tag_id != 256 )
    {
        intf_ErrMsg( "UDF: Bad descriptor" );
        return 0;
    }
    if( root_icb.i_partition != i_partition )
    {
        intf_ErrMsg( "UDF: Bad partition" );
        return 0;
    }
  
    /* Find root dir */
    if( !UDFMapICB( root_icb, &i_file_type, &file, partition ) )
    {
        intf_ErrMsg( "UDF: Can't find root dir" );
        return 0;
    }

    /* root dir should be dir */
    if( i_file_type != 4 )
    {
        intf_ErrMsg( "UDF: Root dir error" );
        return 0;
    }

    /* Tokenize filepath */
    psz_token = strtok( psz_tokenline, "/" );
    while( psz_token )
    {
        if( !UDFScanDir( file, psz_token, &icb, partition ) )
        {
            intf_ErrMsg( "UDF: Scan dir error" );
            return 0;
        }

        if( !UDFMapICB ( icb, &i_file_type, &file, partition ) )
        {
            intf_ErrMsg( "UDF: ICB error" );
            return 0;
        }

        psz_token = strtok( NULL, "/" );
    }

    return partition.i_start + file.i_location;
}

