/*****************************************************************************
 * dvd_udf.c: udf filesystem tools.
 * ---
 * Mainly used to find asolute logical block adress of *.ifo files
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_udf.c,v 1.1 2001/02/15 21:03:27 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - dvdudf by Christian Wolff <scarabaeus@convergence.de>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "common.h"
#include "intf_msg.h"
#include "input_dvd.h"

/*
 * Local structures
 */
typedef struct partition_s
{
	int     i_valid;
	u8      pi_volume_desc[128];
	u16     i_flags;
	u16     i_number;
	u8      pi_contents[32];
	u32     i_accessType;
	u32     i_start;
	u32     i_length;
} partition_t;

typedef struct ad_s
{
	u32     i_location;
	u32     i_length;
	u8      i_flags;
	u16     i_partition;
} ad_t;

/*
 * Local functions
 */

/*****************************************************************************
 * UDFDecode: decode udf data that is unicode encoded
 *****************************************************************************/
static int UDFDecode( u8 * pi_data, int i_len, char * psz_target )
{
	int p = 1;
    int i = 0;

	if( !( pi_data[0] & 0x18 ) )
    {
		psz_target[0] ='\0';
		return 0;
	}

	if( data[0] & 0x10 )
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

/*****************************************************************************
 * UDFInit: some check and initialization in udf filesystem
 *****************************************************************************/
int UDFInit( u8 * pi_data, udf_t * p_udf )
{
	u32i    lbsize,MT_L,N_PM;

	UDFDecode( &pi_data[84], 128, p_udf->psz_volume_desc );
	lbsize	= GETN4(212);		// should be 2048
	MT_L	= GETN4(264);		// should be 6
	N_PM	= GETN4(268);		// should be 1

	if( lbsize != DVD_LB_SIZE )
    {
		return 1;
    }

    return 0;
}

/*****************************************************************************
 * UDFFileEntry:
 *****************************************************************************/
int UDFFileEntry (uint8_t *data, uint8_t *FileType, struct AD *ad)
{
	uint8_t filetype;
	uint16_t flags;
	uint32_t L_EA,L_AD;
	int p;

	UDFICB(&data[16],&filetype,&flags);
	FileType[0]=filetype;
	L_EA=GETN4(168);
	L_AD=GETN4(172);
	p=176+L_EA;

	while (p<176+L_EA+L_AD) {
		switch (flags&0x07) {
		case 0:
			UDFAD (&data[p], ad, UDFADshort);
			p += 0x08;
			break;
		case 1:
			UDFAD (&data[p], ad, UDFADlong);
			p += 0x10;
			break;
		case 2: UDFAD (&data[p], ad, UDFADext);
			p += 0x14;
			break;
		case 3:
			switch (L_AD) {
			case 0x08:
				UDFAD (&data[p], ad, UDFADshort);
				break;
			case 0x10:
				UDFAD (&data[p], ad, UDFADlong);
				break;
			case 0x14:
				UDFAD (&data[p], ad, UDFADext);
				break;
			}
		default:
			p += L_AD;
			break;
		}
	}

	return 0;
}

/*****************************************************************************
 * UDFFileIdentifier:
 *****************************************************************************/
int UDFFileIdentifier (uint8_t *data, uint8_t *FileCharacteristics, char *FileName, struct AD *FileICB)
{
	uint8_t L_FI;
	uint16_t L_IU;
  
	FileCharacteristics[0]=GETN1(18);
	L_FI=GETN1(19);
	UDFAD(&data[20],FileICB,UDFADlong);
	L_IU=GETN2(36);

	if (L_FI)
		_Unicodedecode (&data[38+L_IU],L_FI,FileName);
	else
		FileName[0]='\0';

	return 4*((38+L_FI+L_IU+3)/4);
}


