/*****************************************************************************
 * dvb.c : functions to control a DVB card under Linux
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

#include <vlc/vlc.h>

#include <sys/ioctl.h>
#include <stdio.h>
#ifdef HAVE_INTTYPES_H
#   include <inttypes.h>                                       /* int16_t .. */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

/* DVB Card Drivers */
#include <ost/sec.h>
#include <ost/dmx.h>
#include <ost/frontend.h>


#include "dvb.h"

/*****************************************************************************
 * ioctl_SECControl : commands the SEC device
 *****************************************************************************/


int ioctl_SECControl( int sec_nb, int freq, int pol, int lnb_slof, int diseqc )
{
    struct secCommand scmd;
    struct secCmdSequence scmds;
    int sec;
    char psz_sec[255];

    snprintf(psz_sec, sizeof(psz_sec), SEC "%d", sec_nb);

    if((sec = open(psz_sec, O_RDWR)) < 0)
    {
        return -1;
    }

    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    scmds.continuousTone = (freq<lnb_slof) ? SEC_TONE_OFF : SEC_TONE_ON;

    /* Set the polarity of the transponder by setting the correct
       voltage on the universal LNB */
    scmds.voltage = (pol) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13;

    /* In case we have a DiSEqC, set it to the correct address */
    scmd.type=0;
    scmd.u.diseqc.addr=0x10;
    scmd.u.diseqc.cmd=0x38;
    scmd.u.diseqc.numParams=1;
    scmd.u.diseqc.params[0] = 0xF0 | ((diseqc * 4) & 0x0F) | 
            (scmds.continuousTone == SEC_TONE_ON ? 1 : 0) |
            (scmds.voltage==SEC_VOLTAGE_18 ? 2 : 0);

    scmds.miniCommand=SEC_MINI_NONE;
    scmds.numCommands=1;
    scmds.commands=&scmd;

    /* Send the data to the SEC device to prepare the LNB for tuning  */
    /*intf_Msg("Sec: Sending data\n");*/
    if (ioctl(sec, SEC_SEND_SEQUENCE, &scmds) < 0)
    {
        return -1;
    }

    close(sec);

    return 0;
}

static int check_qpsk( int );

/*****************************************************************************
 * ioctl_SetQPSKFrontend : controls the FE device
 *****************************************************************************/

int ioctl_SetQPSKFrontend (int fe_nb, int freq, int srate, int fec,\
                        int lnb_lof1, int lnb_lof2, int lnb_slof)
{
    FrontendParameters fep;
    int front;
    int rc;
    char psz_fe[255];

    snprintf(psz_fe, sizeof(psz_fe), FRONTEND "%d", fe_nb);

    /* Open the frontend device */
    if((front = open(psz_fe, O_RDWR)) < 0)
    {
        return -1;
    }

    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    fep.Frequency = (freq < lnb_slof) ? freq - lnb_lof1 : freq - lnb_lof2; 

 /* Set symbol rate and FEC */
    fep.u.qpsk.SymbolRate = srate;
    fep.u.qpsk.FEC_inner = FEC_AUTO;

    /* Now send it all to the frontend device */
    if (ioctl(front, FE_SET_FRONTEND, &fep) < 0)
    {
        return -1;
    }

    /* Check if it worked */
    rc=check_qpsk(front);

    /* Close front end device */
    close(front);
    
    return rc;
}



/******************************************************************
 * Check completion of the frontend control sequence
 ******************************************************************/
static int check_qpsk(int front)
{
    struct pollfd pfd[1];
    FrontendEvent event; 
    /* poll for QPSK event to check if tuning worked */
    pfd[0].fd = front;
    pfd[0].events = POLLIN;

    if (poll(pfd,1,3000))
    {
        if (pfd[0].revents & POLLIN)
        {
            if ( ioctl(front, FE_GET_EVENT, &event) == -EBUFFEROVERFLOW)
            {
                return -5;
            }
        
            switch(event.type)
            {
                case FE_UNEXPECTED_EV:
                    return -2;
                case FE_FAILURE_EV:
                    return -1;
                case FE_COMPLETION_EV:
                    break;
            }
        }
        else
        {
            /* should come here */
            return -3;
        }
    }
    else
    {
        return -4;
    }
    
    return 0;
}


/*****************************************************************************
 * ioctl_SetDMXAudioFilter : controls the demux to add a filter
 *****************************************************************************/

int ioctl_SetDMXFilter( int dmx_nb, int i_pid, int * pi_fd , int i_type ) 
{
    struct dmxPesFilterParams s_filter_params;
    char psz_dmx[255];

    snprintf(psz_dmx, sizeof(psz_dmx), DMX "%d", dmx_nb);

    /* We first open the device */
    if ((*pi_fd = open(psz_dmx, O_RDWR|O_NONBLOCK))  < 0)
    {
        return -1;
    }

    /* We fill the DEMUX structure : */
    s_filter_params.pid     =   i_pid;
    s_filter_params.input   =   DMX_IN_FRONTEND;
    s_filter_params.output  =   DMX_OUT_TS_TAP;
    switch ( i_type )
    {
        /* AFAIK you shouldn't use DMX_PES_VIDEO and DMX_PES_AUDIO
         * unless you want to use a hardware decoder. In all cases
         * I know DMX_PES_OTHER is quite enough for what we want to
         * do. In case you have problems, you can still try to
         * reenable them here : --Meuuh */
#if 0
        case 1:
            s_filter_params.pesType =   DMX_PES_VIDEO;
            break;
        case 2:
            s_filter_params.pesType =   DMX_PES_AUDIO;
            break;
        case 3:
#endif
        default:
            s_filter_params.pesType =   DMX_PES_OTHER;
            break;
    }
    s_filter_params.flags   =   DMX_IMMEDIATE_START;

    /* We then give the order to the device : */
    if (ioctl(*pi_fd, DMX_SET_PES_FILTER, &s_filter_params) < 0)
    {
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * ioctl_UnsetDMXFilter : removes a filter
 *****************************************************************************/
int ioctl_UnsetDMXFilter(int demux)
{
    ioctl(demux, DMX_STOP);
    close(demux);
    return 0;
}


/*****************************************************************************
 * ioctl_SetBufferSize :
 *****************************************************************************/
int ioctl_SetBufferSize(int handle, size_t size)
{
    return ioctl(handle, DMX_SET_BUFFER_SIZE, size);
}
