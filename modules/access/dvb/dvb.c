/*****************************************************************************
 * dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <saman@natlab.research.philips.com>
 *          Christopher Ross <ross@natlab.research.philips.com>
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
#include <string.h>
#include <errno.h>
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
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include <linux/errno.h>

#include "dvb.h"

static int ioctl_CheckQPSK( int );

/*****************************************************************************
 * ioctl_FrontendControl : commands the SEC device
 *****************************************************************************/
int ioctl_FrontendControl( int freq, int pol, int lnb_slof, int diseqc, unsigned int u_adapter, unsigned int u_device)
{
    struct dvb_diseqc_master_cmd  cmd;
    fe_sec_tone_mode_t tone;
    fe_sec_voltage_t voltage;
    int frontend;
	  char front[] = FRONTEND;
	  int i_len;

	  printf("ioclt_FrontEndControl: enter\n");
 	  i_len = sizeof(FRONTEND);
		if (snprintf(front, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  printf( "error: snprintf() truncated string for FRONTEND" );
			front[sizeof(FRONTEND)] = '\0';
    }

	  printf("ioclt_FrontEndControl: Opening frontend %s\n",front);	  
    if((frontend = open(front,O_RDWR)) < 0)
    {
        return -1;
    }

    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    tone = (freq<lnb_slof) ? SEC_TONE_OFF : SEC_TONE_ON;

    /* Set the polarisation of the transponder by setting the correct
       voltage on the universal LNB */
    voltage = (pol) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13;
    
    /* In case we have a DiSEqC, set it to the correct address */
    cmd.msg[0] =0x0;  /* framing */
    cmd.msg[1] =0x10; /* address */
    cmd.msg[2] =0x38; /* command */
    /* command parameters start at index 3 */
    cmd.msg[3] = 0xF0 | ((diseqc * 4) & 0x0F);   
    cmd.msg_len = 4;

    /* Reset everything before sending. */
#define CHECK_IOCTL(X) if(X<0) \
    { \
        close(frontend); \
        return -1;       \
    }
    
    CHECK_IOCTL(ioctl(frontend, FE_SET_TONE, SEC_TONE_OFF));   
    CHECK_IOCTL(ioctl(frontend, FE_SET_VOLTAGE, voltage));
    msleep(15);
    
    /* Send the data to the SEC device to prepare the LNB for tuning  */
    /*intf_Msg("Sec: Sending data\n");*/
    CHECK_IOCTL(ioctl(frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd));
    msleep(15);
    CHECK_IOCTL(ioctl(frontend, FE_DISEQC_SEND_BURST, &cmd));
    msleep(15);
    CHECK_IOCTL(ioctl(frontend, FE_SET_TONE, tone));
#undef CHECK_IOCTL

    close(frontend);
    printf("ioclt_FrontEndControl: exit\n");
    return 0;
}

/*****************************************************************************
 * ioctl_InfoFrontend : return information about given frontend
 *****************************************************************************/
int ioctl_InfoFrontend(struct dvb_frontend_info *info, unsigned int u_adapter, unsigned int u_device)
{
    int front;
	  char frontend[] = FRONTEND;
	  int i_len;

	  printf("ioclt_InfoFrontEnd: enter\n");
 	  i_len = sizeof(FRONTEND);
		if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  printf( "error: snprintf() truncated string for FRONTEND" );
			frontend[sizeof(FRONTEND)] = '\0';
    }

	  printf("ioclt_InfoFrontEnd: Opening device %s\n", frontend);
    if((front = open(frontend,O_RDWR)) < 0)
    {
        return -1;
    }

    /* Determine type of frontend */
    if (ioctl(front, FE_GET_INFO, info) < 0)
    {
      	close(front);
      	return -1;
    }
#if 1
    printf( "Frontend Info:\tname = %s\n\t\tfrequency_min = %d\n\t\tfrequency_max = %d\n\t\tfrequency_stepsize = %d\n\t\tfrequency_tolerance = %d\n\t\tsymbol_rate_min = %d\n\t\tsymbol_rate_max = %d\n\t\tsymbol_rate_tolerance (ppm) = %d\n\t\tnotifier_delay (ms)= %d\n",
        		info->name,
        		info->frequency_min,
        		info->frequency_max,
        		info->frequency_stepsize,
        		info->frequency_tolerance,
        		info->symbol_rate_min,
        		info->symbol_rate_max,
        		info->symbol_rate_tolerance,
        		info->notifier_delay );
#endif    
    close(front);
	  printf("ioclt_InfoFrontEnd: exit\n");
    return 0;
}

// CPR ---> ===================================================================

struct diseqc_cmd {
	struct dvb_diseqc_master_cmd cmd;
	uint32_t wait;
};

struct diseqc_cmd switch_cmds[] = {
	{ { { 0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf2, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf1, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf3, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf4, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf6, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf5, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf7, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf8, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfa, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xf9, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfb, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfc, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfe, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xfd, 0x00, 0x00 }, 4 }, 0 },
	{ { { 0xe0, 0x10, 0x38, 0xff, 0x00, 0x00 }, 4 }, 0 }
};


int diseqc_send_msg (int fd, fe_sec_voltage_t v, struct diseqc_cmd **cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
	int err;

  printf("diseqc_send_msg: enter\n");
	if ((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)))
		return err;

	if ((err = ioctl(fd, FE_SET_VOLTAGE, v)))
		return err;

	msleep(15);
	while (*cmd) {
		printf("msg: %02x %02x %02x %02x %02x %02x\n",
		    (*cmd)->cmd.msg[0], (*cmd)->cmd.msg[1],
		    (*cmd)->cmd.msg[2], (*cmd)->cmd.msg[3],
		    (*cmd)->cmd.msg[4], (*cmd)->cmd.msg[5]);

		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd)))
			return err;

		msleep((*cmd)->wait);
		cmd++;
	}

	msleep(15);

	if ((err = ioctl(fd, FE_DISEQC_SEND_BURST, b)))
		return err;

	msleep(15);
	
  printf("diseqc_send_msg: exit\n");
	return ioctl(fd, FE_SET_TONE, t);
}

int setup_switch (int frontend_fd, int switch_pos, int voltage_18, int hiband)
{
	struct diseqc_cmd *cmd[2] = { NULL, NULL };
	int i = 4 * switch_pos + 2 * hiband + (voltage_18 ? 1 : 0);

  printf("setup_switch: enter\n");
	printf("switch pos %i, %sV, %sband\n",
	    switch_pos, voltage_18 ? "18" : "13", hiband ? "hi" : "lo");

	printf("index %i\n", i);

	if (i < 0 || i >= (int)(sizeof(switch_cmds)/sizeof(struct diseqc_cmd)))
		return -EINVAL;

	cmd[0] = &switch_cmds[i];

  printf("setup_switch: exit\n");
	return diseqc_send_msg (frontend_fd,
				i % 2 ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
				cmd,
				(i/2) % 2 ? SEC_TONE_ON : SEC_TONE_OFF,
				(i/4) % 2 ? SEC_MINI_B : SEC_MINI_A);
}

// <--- CPR ===================================================================
#define SWITCHFREQ 11700000
#define LOF_HI     10600000
#define LOF_LO      9750000

/*****************************************************************************
 * ioctl_SetQPSKFrontend : controls the FE device
 *****************************************************************************/
int ioctl_SetQPSKFrontend ( struct dvb_frontend_parameters fep, int b_polarisation, unsigned int u_adapter, unsigned int u_device  )
{
    int front;
    int rc;
    int i;
    int hiband;
	  char frontend[] = FRONTEND;
	  int i_len;

 	  printf("ioctl_SetQPSKFrontend: enter\n");
 	  i_len = sizeof(FRONTEND);
		if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  printf( "error: snprintf() truncated string for FRONTEND" );
			frontend[sizeof(FRONTEND)] = '\0';
    }
    
 	  printf("ioctl_SetQPSKFrontend: Opening frontend %s\n", frontend);
    /* Open the frontend device */
    if((front = open(frontend,O_RDWR)) < 0)
    {
        return -1;
    }
    
    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    hiband = (fep.frequency >= SWITCHFREQ);
    setup_switch (front, 0, b_polarisation, hiband );

    if (hiband)
    	fep.frequency -= LOF_HI;
    else
    	fep.frequency -= LOF_LO;

    /* Now send it all to the frontend device */
    if (ioctl(front, FE_SET_FRONTEND, &fep) < 0)
    {
		 	  printf("ioctl_SetQPSKFrontend: ioctl FE_SET_FRONTEND failed\n");
      	close(front);
        return -1;
    }

    for (i=0; i<3; i++)
    {
        fe_status_t s;
        ioctl(front, FE_READ_STATUS, &s);
       	printf("ioctl_SetQPSKFrontend: tuning status == 0x%02x!!! ...", s);
        if (s & FE_HAS_LOCK)
        {
		        printf( "tuning succeeded\n" );
      		  rc = 0;
        }
        else
        {
        		printf( "tuning failed\n");
        		rc = -1;
      	}
        usleep( 500000 );
    }

    /* Close front end device */
    close(front);
 	  printf("ioctl_SetQPSKFrontend: exit\n");
    return rc;
}

/******************************************************************
 * Check completion of the frontend control sequence
 ******************************************************************/
static int ioctl_CheckQPSK(int front)
{
    struct pollfd pfd[1];
    struct dvb_frontend_event event;
    /* poll for QPSK event to check if tuning worked */
    pfd[0].fd = front;
    pfd[0].events = POLLIN;

 	  printf("ioctl_CheckQPSK: enter\n");
    if (poll(pfd,1,3000))
    {
        if (pfd[0].revents & POLLIN)
        {
            if ( ioctl(front, FE_GET_EVENT, &event) < 0)
            {
 	  					  printf("ioctl_CheckQPSK: ioctl FE_GET_EVENT failed\n");
                return -5;
            }

            switch(event.status)
            {
             	case FE_HAS_SIGNAL:  /* found something above the noise level */
							  printf("ioctl_CheckQPSK: FE_HAS_SIGNAL\n");
							  break;
             	case FE_HAS_CARRIER: /* found a DVB signal  */
							  printf("ioctl_CheckQPSK: FE_HAS_CARRIER\n");
							  break;
             	case FE_HAS_VITERBI: /* FEC is stable  */
							  printf("ioctl_CheckQPSK: FE_HAS_VITERBI\n");
							  break;              
             	case FE_HAS_SYNC:    /* found sync bytes  */
							  printf("ioctl_CheckQPSK: FE_HAS_SYNC\n");
							  break;              
             	case FE_HAS_LOCK:    /* everything's working... */
							  printf("ioctl_CheckQPSK: FE_HAS_LOCK\n");
							  break;
             	case FE_TIMEDOUT:    /*  no lock within the last ~2 seconds */
							  printf("ioctl_CheckQPSK: FE_TIMEDOUT\n");
             	  return -2;
             	case FE_REINIT:      /*  frontend was reinitialized,  */
             	                     /*  application is recommned to reset */
                                         /*  DiSEqC, tone and parameters */
							  printf("ioctl_CheckQPSK: FE_REINIT\n");
         	      return -1;
            }
        }
        else
        {
            /* should come here */
            printf("ioctl_CheckQPSK: event() failed\n");
            return -3;
        }
    }
    else
    {
		 	  printf("ioctl_CheckQPSK: poll() failed\n");
        return -4;
    }

 	  printf("ioctl_CheckQPSK: exit\n");
    return 0;
}

/*****************************************************************************
 * ioctl_SetDMXFilter : controls the demux to add a filter
 *****************************************************************************/
int ioctl_SetDMXFilter( int i_pid, int * pi_fd , int i_type, unsigned int u_adapter, unsigned int u_device )
{
    struct dmx_pes_filter_params s_filter_params;
    char dmx[] = DMX;
	  int i_len;
	  int result;

    /* We first open the device */
    printf("ioctl_SetDMXFIlter: enter\n");
 	  i_len = sizeof(DMX);
		if (snprintf( dmx, sizeof(DMX), DMX, u_adapter, u_device) >= i_len)
		{
		  printf( "error: snprintf() truncated string for DMX" );
			dmx[sizeof(DMX)] = '\0';
    }

    printf("ioctl_SetDMXFIlter: Opening demux device %s\n", dmx);
    if ((*pi_fd = open(dmx, O_RDWR|O_NONBLOCK))  < 0)
    {
        return -1;
    }

    printf( "@@@ Trying to set PMT id to=%d for type %d\n", i_pid, i_type );
    /* We fill the DEMUX structure : */
    s_filter_params.pid     =   i_pid;
    s_filter_params.input   =   DMX_IN_FRONTEND;
    s_filter_params.output  =   DMX_OUT_TS_TAP;
    switch ( i_type )
    {
        case 1:
            printf("ioctl_SetDMXFIlter: DMX_PES_VIDEO\n");
            s_filter_params.pes_type = DMX_PES_VIDEO;
            break;
        case 2:
            printf("ioctl_SetDMXFIlter: DMX_PES_AUDIO\n");
            s_filter_params.pes_type = DMX_PES_AUDIO;
            break;
        case 3:
            printf("ioctl_SetDMXFIlter: DMX_PES_OTHER\n");
            s_filter_params.pes_type = DMX_PES_OTHER;
            break;
    }
    s_filter_params.flags = DMX_IMMEDIATE_START;

    /* We then give the order to the device : */
    if (result = ioctl(*pi_fd, DMX_SET_PES_FILTER, &s_filter_params) < 0)
    {
		    printf("ioctl_SetDMXFIlter: ioctl failed with %d (%s)\n",result, strerror(errno));
        return -1;
    }
    printf("ioctl_SetDMXFIlter: exit\n");
    return 0;
}

/*****************************************************************************
 * ioctl_UnsetDMXFilter : removes a filter
 *****************************************************************************/
int ioctl_UnsetDMXFilter(int demux)
{
    printf("ioctl_UnsetDMXFIlter: enter\n");
    ioctl(demux, DMX_STOP);
    close(demux);
    printf("ioctl_UnsetDMXFIlter: exit\n");
    return 0;
}
