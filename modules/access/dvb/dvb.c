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
#include <vlc/input.h>

#include <sys/ioctl.h>
#include <stdio.h>
#ifdef HAVE_ERRNO_H
#    include <string.h>
#    include <errno.h>
#endif

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

struct diseqc_cmd_t
{
	struct dvb_diseqc_master_cmd cmd;
	uint32_t wait;
};

struct diseqc_cmd_t switch_cmds[] =
{
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

static int ioctl_CheckQPSK(input_thread_t * p_input, int front);

/*****************************************************************************
 * ioctl_FrontendControl : commands the SEC device
 *****************************************************************************/
int ioctl_FrontendControl(input_thread_t * p_input, int freq, int pol, int lnb_slof,
								          int diseqc, unsigned int u_adapter, unsigned int u_device)
{
    struct dvb_diseqc_master_cmd  cmd;
    fe_sec_tone_mode_t tone;
    fe_sec_voltage_t voltage;
    int frontend;
	  char front[] = FRONTEND;
	  int i_len;

 	  i_len = sizeof(FRONTEND);
		if (snprintf(front, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  msg_Err(p_input, "snprintf() truncated string for FRONTEND" );
			front[sizeof(FRONTEND)] = '\0';
    }

	  msg_Dbg(p_input, "Opening frontend %s",front);	  
    if((frontend = open(front,O_RDWR)) < 0)
    {
#   ifdef HAVE_ERRNO_H
	  		msg_Err(p_input, "ioctl_FrontEndControl: Opening frontend failed (%s)",strerror(errno));
#   else
	  		msg_Err(p_input, "ioctl_FrontEndControl: Opening frontend failed");
#   endif
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
#ifdef HAVE_ERRNO_H 
#   define CHECK_IOCTL(X) if(X<0) \
    { \
        msg_Err( p_input, "InfoFrontend: ioctl failed (%s)", strerror(errno)); \
        close(frontend); \
        return -1; \
    }
#else
#   define CHECK_IOCTL(X) if(X<0) \
    { \
        msg_Err( p_input, "InfoFrontend: ioctl failed"); \
        close(frontend); \
        return -1; \
    }
#endif

    CHECK_IOCTL(ioctl(frontend, FE_SET_TONE, SEC_TONE_OFF));   
    CHECK_IOCTL(ioctl(frontend, FE_SET_VOLTAGE, voltage));
    msleep(15);
    
    /* Send the data to the SEC device to prepare the LNB for tuning  */
    CHECK_IOCTL(ioctl(frontend, FE_DISEQC_SEND_MASTER_CMD, &cmd));
    msleep(15);
    CHECK_IOCTL(ioctl(frontend, FE_DISEQC_SEND_BURST, &cmd));
    msleep(15);
    CHECK_IOCTL(ioctl(frontend, FE_SET_TONE, tone));
#undef CHECK_IOCTL

    close(frontend);
    return 0;
}

/*****************************************************************************
 * ioctl_InfoFrontend : return information about given frontend
 *****************************************************************************/
int ioctl_InfoFrontend(input_thread_t * p_input, struct dvb_frontend_info *info,
                       unsigned int u_adapter, unsigned int u_device)
{
    int front;
    int ret;
	  char frontend[] = FRONTEND;
	  int i_len;

 	  i_len = sizeof(FRONTEND);
		if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  msg_Err(p_input, "snprintf() truncated string for FRONTEND" );
			frontend[sizeof(FRONTEND)] = '\0';
    }

	  msg_Dbg(p_input, "Opening device %s", frontend);
    if((front = open(frontend,O_RDWR)) < 0)
    {
#   ifdef HAVE_ERRNO_H
		  msg_Err(p_input, "ioctl_InfoFrontEnd: opening device failed (%s)", strerror(errno));
#   else
		  msg_Err(p_input, "ioctl_InfoFrontEnd: opening device failed");
#   endif
      return -1;
    }

    /* Determine type of frontend */
    if ((ret=ioctl(front, FE_GET_INFO, info)) < 0)
    {
      	close(front);
#   ifdef HAVE_ERRNO_H
        msg_Err(p_input, "ioctl FE_GET_INFO failed (%d) %s", ret, strerror(errno));
#   else
        msg_Err(p_input, "ioctl FE_GET_INFO failed (%d)", ret);
#   endif
      	return -1;
    }
    msg_Dbg(p_input,  "Frontend Info:\tname = %s\n\t\tfrequency_min = %d\n\t\tfrequency_max = %d\n\t\tfrequency_stepsize = %d\n\t\tfrequency_tolerance = %d\n\t\tsymbol_rate_min = %d\n\t\tsymbol_rate_max = %d\n\t\tsymbol_rate_tolerance (ppm) = %d\n\t\tnotifier_delay (ms)= %d\n",
        		info->name,
        		info->frequency_min,
        		info->frequency_max,
        		info->frequency_stepsize,
        		info->frequency_tolerance,
        		info->symbol_rate_min,
        		info->symbol_rate_max,
        		info->symbol_rate_tolerance,
        		info->notifier_delay );

    close(front);
    return 0;
}

int ioctl_DiseqcSendMsg (input_thread_t *p_input, int fd, fe_sec_voltage_t v, struct diseqc_cmd_t **cmd,
		     fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
    int err;

  	if ((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF))<0)
  	{
#   ifdef HAVE_ERRNO_H
    		msg_Err(p_input, "ioclt FE_SET_TONE failed, tone=%s (%d) %s", SEC_TONE_ON ? "on" : "off", err, strerror(errno));
#   else
  		  msg_Err(p_input, "ioclt FE_SET_TONE failed, tone=%s (%d)", SEC_TONE_ON ? "on" : "off", err);
#   endif
  		  return err;
    }
  	if ((err = ioctl(fd, FE_SET_VOLTAGE, v))<0)
  	{
#   ifdef HAVE_ERRNO_H
  		  msg_Err(p_input, "ioclt FE_SET_VOLTAGE failed, voltage=%d (%d) %s", v, err, strerror(errno));
#   else
  		  msg_Err(p_input, "ioclt FE_SET_VOLTAGE failed, voltage=%d (%d)", v, err);
#   endif
  		  return err;
    }

  	msleep(15);
  	while (*cmd)
  	{
    		msg_Dbg(p_input, "msg: %02x %02x %02x %02x %02x %02x",
    		    (*cmd)->cmd.msg[0], (*cmd)->cmd.msg[1],
    		    (*cmd)->cmd.msg[2], (*cmd)->cmd.msg[3],
    		    (*cmd)->cmd.msg[4], (*cmd)->cmd.msg[5]);

    		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd))<0)
    		{
#       ifdef HAVE_ERRNO_H
    			  msg_Err(p_input, "ioclt FE_DISEQC_SEND_MASTER_CMD failed (%d) %s", err, strerror(errno));
#       else
    			  msg_Err(p_input, "ioclt FE_DISEQC_SEND_MASTER_CMD failed (%d)", err);
#       endif
    			return err;
        }

    		msleep((*cmd)->wait);
    		cmd++;
  	}

  	msleep(15);

  	if ((err = ioctl(fd, FE_DISEQC_SEND_BURST, b))<0)
  	{
#   ifdef HAVE_ERRNO_H
  		  msg_Err(p_input, "ioctl FE_DISEQC_SEND_BURST failed, burst=%d (%d) %s",b, err, strerror(errno));
#   else
  		  msg_Err(p_input, "ioctl FE_DISEQC_SEND_BURST failed, burst=%d (%d)",b, err);
#   endif
      return err;
    }
  	msleep(15);

    if ((err = ioctl(fd, FE_SET_TONE, t))<0)
    {
#   ifdef HAVE_ERRNO_H
  		  msg_Err(p_input, "ioctl FE_SET_TONE failed, tone=%d (%d) %s", t, err, strerror(errno));
#   else
  		  msg_Err(p_input, "ioctl FE_SET_TONE failed, tone=%d (%d)", t, err);
#   endif
  		  return err;
  	}
  	return err; 
}

int ioctl_SetupSwitch (input_thread_t *p_input, int frontend_fd, int switch_pos, int voltage_18, int hiband)
{
    int ret;
  	struct diseqc_cmd_t *cmd[2] = { NULL, NULL };
  	int i = 4 * switch_pos + 2 * hiband + (voltage_18 ? 1 : 0);

  	msg_Dbg(p_input, "ioctl_SetupSwitch: switch pos %i, %sV, %sband",
  	        switch_pos, voltage_18 ? "18" : "13", hiband ? "hi" : "lo");
  	msg_Dbg(p_input, "ioctl_SetupSwitch: index %i", i);

  	if ((i < 0) || (i >= (int)(sizeof(switch_cmds)/sizeof(struct diseqc_cmd_t))))
        return -EINVAL;

  	cmd[0] = &switch_cmds[i];

  	if ((ret = ioctl_DiseqcSendMsg (p_input, frontend_fd,
  				(i % 2) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
  				cmd,
  				(i/2) % 2 ? SEC_TONE_ON : SEC_TONE_OFF,
  				(i/4) % 2 ? SEC_MINI_B : SEC_MINI_A))<0)
  	{
    		msg_Err(p_input, "ioctl_DiseqcSendMsg() failed (%d)", ret);
    		return ret;
  	}

  	return ret;
}

#define SWITCHFREQ 11700000
#define LOF_HI     10600000
#define LOF_LO      9750000

/*****************************************************************************
 * ioctl_SetQPSKFrontend : controls the FE device
 *****************************************************************************/
int ioctl_SetQPSKFrontend (input_thread_t * p_input, struct dvb_frontend_parameters fep,
                           int b_polarisation, unsigned int u_adapter, unsigned int u_device  )
{
    int front;
    int ret;
    int i;
    int hiband;
	  char frontend[] = FRONTEND;
	  int i_len;

 	  i_len = sizeof(FRONTEND);
		if (snprintf(frontend, sizeof(FRONTEND), FRONTEND, u_adapter, u_device) >= i_len)
		{
		  msg_Err(p_input,  "error: snprintf() truncated string for FRONTEND" );
			frontend[sizeof(FRONTEND)] = '\0';
    }
    
    /* Open the frontend device */
 	  msg_Dbg(p_input, "Opening frontend %s", frontend);
    if((front = open(frontend,O_RDWR)) < 0)
    {
#   ifdef HAVE_ERRNO_H
        msg_Err(p_input, "failed to open frontend (%s)", strerror(errno));
#   else
        msg_Err(p_input, "failed to open frontend");
#   endif
        return -1;
    }
    
    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    hiband = (fep.frequency >= SWITCHFREQ);
    if ((ret=ioctl_SetupSwitch (p_input, front, 0, b_polarisation, hiband))<0)
    {
  			msg_Err(p_input, "ioctl_SetupSwitch failed (%d)", ret);
  			return -1;
		}

    if (hiband)
    	fep.frequency -= LOF_HI;
    else
    	fep.frequency -= LOF_LO;

    /* Now send it all to the frontend device */
    if ((ret=ioctl(front, FE_SET_FRONTEND, &fep)) < 0)
    {
      	close(front);
#   ifdef HAVE_ERRNO_H
		 	  msg_Err(p_input, "ioctl_SetQPSKFrontend: ioctl FE_SET_FRONTEND failed (%d) %s", ret, strerror(errno));
#   else
		 	  msg_Err(p_input, "ioctl_SetQPSKFrontend: ioctl FE_SET_FRONTEND failed (%d)", ret);
#   endif
        return -1;
    }

    for (i=0; i<3; i++)
    {
        fe_status_t s;
        if ((ret=ioctl(front, FE_READ_STATUS, &s))<0)
        {
#       ifdef HAVE_ERRNO_H
            msg_Err(p_input, "ioctl FE_READ_STATUS failed (%d) %s", ret, strerror(errno));
#       else
            msg_Err(p_input, "ioctl FE_READ_STATUS failed (%d)", ret);
#       endif				  
				}

        if (s & FE_HAS_LOCK)
        {
		        msg_Dbg(p_input, "ioctl_SetQPSKFrontend: tuning status == 0x%02x!!! ..."
                             "tuning succeeded", s);
      		  ret = 0;
        }
        else
        {
        		msg_Dbg(p_input, "ioctl_SetQPSKFrontend: tuning status == 0x%02x!!! ..."
                             "tuning failed", s);
        		ret = -1;
      	}
        usleep( 500000 );
    }

    /* Close front end device */
    close(front);
    return ret;
}

/******************************************************************
 * Check completion of the frontend control sequence
 ******************************************************************/
static int ioctl_CheckQPSK(input_thread_t * p_input, int front)
{
    int ret;
    struct pollfd pfd[1];
    struct dvb_frontend_event event;
    /* poll for QPSK event to check if tuning worked */
    pfd[0].fd = front;
    pfd[0].events = POLLIN;

    if (poll(pfd,1,3000))
    {
        if (pfd[0].revents & POLLIN)
        {
            if ( (ret=ioctl(front, FE_GET_EVENT, &event)) < 0)
            {
#           ifdef HAVE_ERRNO_H
                msg_Err(p_input, "ioctl_CheckQPSK: ioctl FE_GET_EVENT failed (%d) %s", ret, strerror(errno));
#           else
                msg_Err(p_input, "ioctl_CheckQPSK: ioctl FE_GET_EVENT failed (%d)", ret);
#           endif
                return -5;
            }

            switch(event.status)
            {
             	case FE_HAS_SIGNAL:  /* found something above the noise level */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_HAS_SIGNAL");
							  break;
             	case FE_HAS_CARRIER: /* found a DVB signal  */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_HAS_CARRIER");
							  break;
             	case FE_HAS_VITERBI: /* FEC is stable  */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_HAS_VITERBI");
							  break;              
             	case FE_HAS_SYNC:    /* found sync bytes  */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_HAS_SYNC");
							  break;              
             	case FE_HAS_LOCK:    /* everything's working... */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_HAS_LOCK");
							  break;
             	case FE_TIMEDOUT:    /*  no lock within the last ~2 seconds */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_TIMEDOUT");
             	  return -2;
             	case FE_REINIT:      /*  frontend was reinitialized,  */
             	                     /*  application is recommned to reset */
                                         /*  DiSEqC, tone and parameters */
							  msg_Dbg(p_input, "ioctl_CheckQPSK: FE_REINIT");
         	      return -1;
            }
        }
        else
        {
            /* should come here */
            msg_Err(p_input, "ioctl_CheckQPSK: event() failed");
            return -3;
        }
    }
    else
    {
#   ifdef HAVE_ERRNO_H
        msg_Err(p_input, "ioctl_CheckQPSK: poll() failed (%s)", strerror(errno));
#   else
        msg_Err(p_input, "ioctl_CheckQPSK: poll() failed");
#   endif
        return -4;
    }

    return 0;
}

/*****************************************************************************
 * ioctl_SetDMXFilter : controls the demux to add a filter
 *****************************************************************************/
int ioctl_SetDMXFilter(input_thread_t * p_input, int i_pid, int * pi_fd , int i_type,
                       unsigned int u_adapter, unsigned int u_device )
{
    struct dmx_pes_filter_params s_filter_params;
    char dmx[] = DMX;
	  int i_len;
	  int result;

    /* We first open the device */
 	  i_len = sizeof(DMX);
		if (snprintf( dmx, sizeof(DMX), DMX, u_adapter, u_device) >= i_len)
		{
		  msg_Err(p_input,  "snprintf() truncated string for DMX" );
			dmx[sizeof(DMX)] = '\0';
    }

    msg_Dbg(p_input, "Opening demux device %s", dmx);
    if ((*pi_fd = open(dmx, O_RDWR|O_NONBLOCK))  < 0)
    {
#   ifdef HAVE_ERRNO_H
		 	  msg_Err(p_input, "ioctl_SetDMXFIlter: opening device failed (%s)", strerror(errno));
#   else
		 	  msg_Err(p_input, "ioctl_SetDMXFIlter: opening device failed");
#   endif
        return -1;
    }

    /* We fill the DEMUX structure : */
    s_filter_params.pid     =   i_pid;
    s_filter_params.input   =   DMX_IN_FRONTEND;
    s_filter_params.output  =   DMX_OUT_TS_TAP;
    switch ( i_type )
    {
        case 1:
            msg_Dbg(p_input, "ioctl_SetDMXFIlter: DMX_PES_VIDEO for PMT %d", i_pid);
            s_filter_params.pes_type = DMX_PES_VIDEO;
            break;
        case 2:
            msg_Dbg(p_input, "ioctl_SetDMXFIlter: DMX_PES_AUDIO for PMT %d", i_pid);
            s_filter_params.pes_type = DMX_PES_AUDIO;
            break;
        case 3:
            msg_Dbg(p_input, "ioctl_SetDMXFIlter: DMX_PES_OTHER for PMT %d", i_pid);
            s_filter_params.pes_type = DMX_PES_OTHER;
            break;
        default:
            msg_Err(p_input, "trying to set PMT id to=%d for unknown type %d", i_pid, i_type );
            break;
    }
    s_filter_params.flags = DMX_IMMEDIATE_START;

    /* We then give the order to the device : */
    if ((result = ioctl(*pi_fd, DMX_SET_PES_FILTER, &s_filter_params)) < 0)
    {
#   ifdef HAVE_ERRNO_H
		    msg_Err(p_input, "ioctl_SetDMXFIlter: ioctl failed with %d (%s)",result, strerror(errno));
#   else
		    msg_Err(p_input, "ioctl_SetDMXFIlter: ioctl failed with %d",result);
#   endif
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * ioctl_UnsetDMXFilter : removes a filter
 *****************************************************************************/
int ioctl_UnsetDMXFilter(input_thread_t * p_input, int demux)
{
    int ret;
    
    if ((ret=ioctl(demux, DMX_STOP))<0)
    {
#   ifdef HAVE_ERRNO_H
        msg_Err(p_input, "ioctl DMX_STOP failed for demux %d (%d) %s", demux, ret, strerror(errno));
#   else
        msg_Err(p_input, "ioctl DMX_STOP failed for demux %d (%d)", demux, ret);
#   endif
			return -1;
    }
    close(demux);
    return 0;
}
