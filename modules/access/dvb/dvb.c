/*****************************************************************************
 * dvb.c : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Damien Lucas <nitrox@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
 *          Christopher Ross <chris@tebibyte.org>
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
#include <linux/dvb/version.h>
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

static int ioctl_CheckFrontend( input_thread_t * p_input, fe_type_t type );

/*****************************************************************************
 * ioctl_InfoFrontend : return information about given frontend
 *****************************************************************************/
int ioctl_InfoFrontend( input_thread_t * p_input, struct dvb_frontend_info *info )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          i_ret;

    /* Determine type of frontend */
    if( (i_ret = ioctl( fd_front, FE_GET_INFO, info )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "Getting info from frontend failed (%d) %s", i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "Getting info from frontend failed (%d)", i_ret );
#   endif
        return -1;
    }
    /* Print out frontend capabilities. */

    msg_Dbg( p_input, "Frontend Info:" );
    msg_Dbg( p_input, "  name = %s", info->name );
    switch( info->type )
    {
        case FE_QPSK:
            msg_Dbg( p_input, "  type = QPSK (DVB-S)" );
            break;
        case FE_QAM:
            msg_Dbg( p_input, "  type = QAM (DVB-C)" );
            break;
        case FE_OFDM:
            msg_Dbg( p_input, "  type = OFDM (DVB-T)" );
            break;
        default:
            msg_Err( p_input, "  unknown frontend found fe_type_t(%d)", info->type );
            return -1;
    }
    msg_Dbg( p_input, "  frequency_min = %u (kHz)", info->frequency_min );
    msg_Dbg( p_input, "  frequency_max = %u (kHz)", info->frequency_max );
    msg_Dbg( p_input, "  frequency_stepsize = %u",  info->frequency_stepsize );
    msg_Dbg( p_input, "  frequency_tolerance = %u", info->frequency_tolerance );
    msg_Dbg( p_input, "  symbol_rate_min = %u (kHz)", info->symbol_rate_min );
    msg_Dbg( p_input, "  symbol_rate_max = %u (kHz)", info->symbol_rate_max );
    msg_Dbg( p_input, "  symbol_rate_tolerance (ppm) = %u", info->symbol_rate_tolerance );
    msg_Dbg( p_input, "  notifier_delay (ms)= %u", info->notifier_delay );

    msg_Dbg( p_input, "Frontend Info capability list:" );
    if( info->caps & FE_IS_STUPID )
        msg_Dbg( p_input, "  no capabilities - frontend is stupid!" );
    if( info->caps & FE_CAN_INVERSION_AUTO )
        msg_Dbg( p_input, "  inversion auto" );
    if( info->caps & FE_CAN_FEC_1_2 )
        msg_Dbg( p_input, "  forward error correction 1/2" );
    if( info->caps & FE_CAN_FEC_2_3 )
        msg_Dbg( p_input, "  forward error correction 2/3" );
    if( info->caps & FE_CAN_FEC_3_4 )
        msg_Dbg( p_input, "  forward error correction 3/4" );
    if( info->caps & FE_CAN_FEC_4_5 )
        msg_Dbg( p_input, "  forward error correction 4/5" );
    if( info->caps & FE_CAN_FEC_5_6 )
        msg_Dbg( p_input, "  forward error correction 5/6" );
    if( info->caps & FE_CAN_FEC_6_7 )
        msg_Dbg( p_input, "  forward error correction 6/7" );
    if( info->caps & FE_CAN_FEC_7_8 )
        msg_Dbg( p_input, "  forward error correction 7/8" );
    if( info->caps & FE_CAN_FEC_8_9 )
        msg_Dbg( p_input, "  forward error correction 8/9" );
    if( info->caps & FE_CAN_FEC_AUTO )
        msg_Dbg( p_input, "  forward error correction auto" );
    if( info->caps & FE_CAN_QPSK )
        msg_Dbg( p_input, "  card can do QPSK" );
    if( info->caps & FE_CAN_QAM_16 )
        msg_Dbg( p_input, "  card can do QAM 16" );
    if( info->caps & FE_CAN_QAM_32 )
        msg_Dbg( p_input, "  card can do QAM 32" );
    if( info->caps & FE_CAN_QAM_64 )
        msg_Dbg( p_input, "  card can do QAM 64" );
    if( info->caps & FE_CAN_QAM_128 )
        msg_Dbg( p_input, "  card can do QAM 128" );
    if( info->caps & FE_CAN_QAM_256 )
        msg_Dbg( p_input, "  card can do QAM 256" );
    if( info->caps & FE_CAN_QAM_AUTO )
        msg_Dbg( p_input, "  card can do QAM auto" );
    if( info->caps & FE_CAN_TRANSMISSION_MODE_AUTO )
        msg_Dbg( p_input, "  transmission mode auto" );
    if( info->caps & FE_CAN_BANDWIDTH_AUTO )
        msg_Dbg(p_input, "  bandwidth mode auto" );
    if( info->caps & FE_CAN_GUARD_INTERVAL_AUTO )
        msg_Dbg( p_input, "  guard interval mode auto" );
    if( info->caps & FE_CAN_HIERARCHY_AUTO )
        msg_Dbg( p_input, "  hierarchy mode auto" );
    if( info->caps & FE_CAN_MUTE_TS )
        msg_Dbg( p_input, "  card can mute TS" );
    if( info->caps & FE_CAN_CLEAN_SETUP )
        msg_Dbg( p_input, "  clean setup" );        
    msg_Dbg( p_input,"End of capability list" );
    
    return 0;
}

/* QPSK only */
int ioctl_DiseqcSendMsg( input_thread_t *p_input, fe_sec_voltage_t v, struct diseqc_cmd_t **cmd,
                         fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          i_ret;
    
    if( (i_ret = ioctl( fd_front, FE_SET_TONE, SEC_TONE_OFF )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "ioclt FE_SET_TONE failed, tone=%s (%d) %s", SEC_TONE_ON ? "on" : "off", i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "ioclt FE_SET_TONE failed, tone=%s (%d)", SEC_TONE_ON ? "on" : "off", i_ret );
#   endif
        return i_ret;
    }
    if( (i_ret = ioctl( fd_front, FE_SET_VOLTAGE, v )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "ioclt FE_SET_VOLTAGE failed, voltage=%d (%d) %s", v, i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "ioclt FE_SET_VOLTAGE failed, voltage=%d (%d)", v, i_ret );
#   endif
        return i_ret;
    }

    msleep( 15 );
    while( *cmd )
    {
        msg_Dbg( p_input, "DiseqcSendMsg(): %02x %02x %02x %02x %02x %02x",
            (*cmd)->cmd.msg[0], (*cmd)->cmd.msg[1],
            (*cmd)->cmd.msg[2], (*cmd)->cmd.msg[3],
            (*cmd)->cmd.msg[4], (*cmd)->cmd.msg[5] );

        if( (i_ret = ioctl( fd_front, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd )) < 0 )
        {
#       ifdef HAVE_ERRNO_H
            msg_Err( p_input, "ioclt FE_DISEQC_SEND_MASTER_CMD failed (%d) %s", i_ret, strerror( errno ) );
#       else
            msg_Err( p_input, "ioclt FE_DISEQC_SEND_MASTER_CMD failed (%d)", i_ret );
#       endif
            return i_ret;
        }

        msleep( (*cmd)->wait );
        cmd++;
    }
    msleep( 15 );

    if( (i_ret = ioctl( fd_front, FE_DISEQC_SEND_BURST, b )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "ioctl FE_DISEQC_SEND_BURST failed, burst=%d (%d) %s", b, i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "ioctl FE_DISEQC_SEND_BURST failed, burst=%d (%d)", b, i_ret );
#   endif
      return i_ret;
    }
    msleep( 15 );

    if( (i_ret = ioctl( fd_front, FE_SET_TONE, t )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "ioctl FE_SET_TONE failed, tone=%d (%d) %s", t, i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "ioctl FE_SET_TONE failed, tone=%d (%d)", t, i_ret );
#   endif
        return i_ret;
    }
    return i_ret; 
}

/* QPSK only */
int ioctl_SetupSwitch( input_thread_t *p_input, int switch_pos,
                       int voltage_18, int hiband )
{
    struct diseqc_cmd_t *cmd[2] = { NULL, NULL };
    int i = 4 * switch_pos + 2 * hiband + (voltage_18 ? 1 : 0);
    int i_ret;

    msg_Dbg( p_input, "DVB-S: setup switch pos %i, %sV, %sband, index %i",
             switch_pos, voltage_18 ? "18" : "13", hiband ? "hi" : "lo", i );

    if( (i < 0) || (i >= (int)(sizeof(switch_cmds)/sizeof(struct diseqc_cmd_t))) )
        return -EINVAL;

    cmd[0] = &switch_cmds[i];

    if( (i_ret = ioctl_DiseqcSendMsg (p_input,
          (i % 2) ? SEC_VOLTAGE_18 : SEC_VOLTAGE_13,
          cmd,
          (i/2) % 2 ? SEC_TONE_ON : SEC_TONE_OFF,
          (i/4) % 2 ? SEC_MINI_B : SEC_MINI_A)) < 0 )
    {
        return i_ret;
    }

    return i_ret;
}

/*****************************************************************************
 * ioctl_SetQPSKFrontend : controls the FE device
 *****************************************************************************/
int ioctl_SetQPSKFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          hiband;
    int          i_ret;
    
    /* Set the frequency of the transponder, taking into account the
       local frequencies of the LNB */
    hiband = (fep.frequency >= p_dvb->u_lnb_slof);

    if( (i_ret=ioctl_SetupSwitch (p_input, 0, p_dvb->b_polarisation, hiband)) < 0 )
    {
        msg_Err( p_input, "DVB-S: Setup frontend switch failed (%d)", i_ret );
        return -1;
    }

    if( hiband )
        fep.frequency -= p_dvb->u_lnb_lof2;
    else
        fep.frequency -= p_dvb->u_lnb_lof1;

    /* Now send it all to the frontend device */
    if( (i_ret=ioctl(fd_front, FE_SET_FRONTEND, &fep)) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "DVB-S: setting frontend failed (%d) %s", i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "DVB-S: setting frontend  failed (%d)", i_ret );
#   endif
        return -1;
    }

    i_ret = ioctl_CheckFrontend( p_input, FE_QPSK );

    return i_ret;
}

int ioctl_SetOFDMFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          i_ret;

    /* Now send it all to the frontend device */
    if( (i_ret=ioctl(fd_front, FE_SET_FRONTEND, &fep)) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "DVB-T: setting frontend failed (%d) %s", i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "DVB-T: setting frontend failed (%d)", i_ret );
#   endif
        return -1;
    }

    i_ret = ioctl_CheckFrontend( p_input, FE_OFDM );

    return i_ret;
}

int ioctl_SetQAMFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          i_ret;

    /* Show more info on the tuning parameters used. */
    msg_Dbg( p_input, "DVB-C: Tuning with the following paramters:" );
    msg_Dbg( p_input, "DVB-C:   Frequency %d KHz", fep.frequency );
    msg_Dbg( p_input, "DVB-C:   Inversion/polarisation: %d", fep.inversion );
    msg_Dbg( p_input, "DVB-C:   Symbolrate %d", fep.u.qam.symbol_rate );
    msg_Dbg( p_input, "DVB-C:   Forward Error Correction Inner %d", fep.u.qam.fec_inner );
    msg_Dbg( p_input, "DVB-C:   Modulation %d", fep.u.qam.modulation );

    /* Now send it all to the frontend device */
    if( (i_ret=ioctl(fd_front, FE_SET_FRONTEND, &fep)) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "DVB-C: tuning channel failed (frontend returned %d:%s)", i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "DVB-C: tuning channel failed (frontend returned %d)", i_ret );
#   endif
        return -1;
    }

    /* Check Status of frontend */
    i_ret = ioctl_CheckFrontend( p_input, FE_QAM );

    return i_ret;
}

/******************************************************************
 * Check completion of the frontend control sequence
 ******************************************************************/
static int ioctl_CheckFrontend( input_thread_t * p_input, fe_type_t type )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    int          fd_front = p_dvb->i_frontend;
    int          i_ret;

    while( 1 )
    {
        int32_t value;
        fe_status_t status;

        if( (i_ret = ioctl( fd_front, FE_READ_STATUS, &status )) < 0 )
        {
#       ifdef HAVE_ERRNO_H
            msg_Err( p_input, "reading frontend status failed (%d) %s", i_ret, strerror( errno ) );
#       else
            msg_Err( p_input, "reading frontend status failed (%d)", i_ret );
#       endif
            return -1;
        }

        if( status & FE_HAS_SIGNAL ) /* found something above the noise level */
            msg_Dbg( p_input, "check frontend ... has signal" );

        if( status & FE_HAS_CARRIER ) /* found a DVB signal  */
            msg_Dbg( p_input, "check frontend ... has carrier" );

        if( status & FE_HAS_VITERBI ) /* FEC is stable  */
            msg_Dbg( p_input, "check frontend ... has stable forward error correction" );

        if( status & FE_HAS_SYNC )    /* found sync bytes  */
            msg_Dbg( p_input, "check frontend ... has sync" );

        if( status & FE_HAS_LOCK )    /* everything's working... */
        {
            msg_Dbg( p_input, "check frontend ... has lock" );
            msg_Dbg( p_input, "check frontend ... tuning status == 0x%02x ... tuning succeeded", status );
            return 0;
        }

        if( status & FE_TIMEDOUT )    /*  no lock within the last ~2 seconds */
        {
             msg_Dbg( p_input, "check frontend ... tuning status == 0x%02x ... timed out", status );
             msg_Err( p_input, "check frontend ... tuning failed" );
             return -2;
        }

        if( status & FE_REINIT )
        {
            /*  frontend was reinitialized,  */
            /*  application is recommned to reset */
            /*  DiSEqC, tone and parameters */
            switch( type )
            {
                case FE_OFDM:
                    msg_Dbg( p_input, "DVB-T: tuning status == 0x%02x", status );
                    break;
                case FE_QPSK:
                    msg_Dbg( p_input, "DVB-S: tuning status == 0x%02x", status );
                    break;
                case FE_QAM:
                    msg_Dbg( p_input, "DVB-C: tuning status == 0x%02x", status );
                    break;
                default:
                    break;
            }
            msg_Err( p_input, "check frontend ... resend frontend parameters" );
            msg_Err( p_input, "check frontend ... tuning failed" );
            return -1;
        }

        /* Read some statistics */
        value=0;
        if( ioctl( fd_front, FE_READ_BER, &value ) >= 0 )
            msg_Dbg( p_input, "Bit error rate: %d", value );

        value=0;
        if( ioctl( fd_front, FE_READ_SIGNAL_STRENGTH, &value ) >= 0 )
            msg_Dbg( p_input,"Signal strength: %d", value );

        value=0;
        if( ioctl( fd_front, FE_READ_SNR, &value ) >= 0 )
            msg_Dbg( p_input, "SNR: %d", value );

        usleep( 500000 );
    }

    return -1;
}

/*****************************************************************************
 * ioctl_SetDMXFilter : controls the demux to add a filter
 *****************************************************************************/
int ioctl_SetDMXFilter( input_thread_t * p_input, int i_pid, int * pi_fd , int i_type )
{
    input_dvb_t *p_dvb = (input_dvb_t *)p_input->p_access_data;
    struct dmx_pes_filter_params s_filter_params;
    char dmx[] = DMX;
    int i_len;
    int result;

    /* We first open the device */
    i_len = sizeof(DMX);
    if( snprintf( dmx, sizeof(DMX), DMX, p_dvb->u_adapter, p_dvb->u_device) >= i_len )
    {
        msg_Err( p_input, "snprintf() truncated string for DMX" );
        dmx[sizeof(DMX)] = '\0';
    }

    msg_Dbg( p_input, "Opening demux device %s", dmx );
    if( ( (*pi_fd) = open( dmx, O_RDWR|O_NONBLOCK )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "Demux set filter: opening device failed (%s)", strerror( errno ) );
#   else
        msg_Err( p_input, "Demux set filter: opening device failed" );
#   endif
        return -1;
    }

    /* We fill the DEMUX structure : */
    s_filter_params.pid     = i_pid;
    s_filter_params.input   = DMX_IN_FRONTEND;
    s_filter_params.output  = DMX_OUT_TS_TAP;
    switch( i_type )
    {   /* First device */
        case 1:
            msg_Dbg( p_input, "Demux set filter DMX_PES_VIDEO0 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_VIDEO0;
            break;
        case 2:
            msg_Dbg( p_input, "Demux set filter DMX_PES_AUDIO0 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_AUDIO0;
            break;
        case 3: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_TELETEXT0 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_TELETEXT0;
            break;
        case 4: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_SUBTITLE0 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_SUBTITLE0;
            break;
        case 5: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_PCR0 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_PCR0;
            break;
        /* Second device */    
        case 6:
            msg_Dbg( p_input, "Demux set filter DMX_PES_VIDEO1 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_VIDEO1;
            break;
        case 7:
            msg_Dbg( p_input, "Demux set filter DMX_PES_AUDIO1 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_AUDIO1;
            break;            
        case 8: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_TELETEXT1 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_TELETEXT1;
            break;
        case 9: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_SUBTITLE1 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_SUBTITLE1;
            break;
        case 10: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_PCR1 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_PCR1;
            break;
        /* Third device */
        case 11:
            msg_Dbg( p_input, "Demux set filter DMX_PES_VIDEO2 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_VIDEO2;
            break;
        case 12:
            msg_Dbg( p_input, "Demux set filter DMX_PES_AUDIO2 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_AUDIO2;
            break;            
        case 13: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_TELETEXT2 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_TELETEXT2;
            break;        
        case 14: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_SUBTITLE2 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_SUBTITLE2;
            break;
        case 15: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_PCR2 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_PCR2;
            break;
        /* Forth device */    
        case 16:
            msg_Dbg( p_input, "Demux set filter DMX_PES_VIDEO3 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_VIDEO3;
            break;
        case 17:
            msg_Dbg( p_input, "Demux set filter DMX_PES_AUDIO3 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_AUDIO3;
            break;
        case 18: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_TELETEXT3 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_TELETEXT3;
            break;
        case 19: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_SUBTITLE3 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_SUBTITLE3;
            break;
        case 20: 
            msg_Dbg( p_input, "Demux set filter DMX_PES_PCR3 for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_PCR3;
            break;
        /* Usually used by Nova cards */    
        case 21:
            msg_Dbg( p_input, "Demux set filter DMX_PES_OTHER for PMT %d", i_pid );
            s_filter_params.pes_type = DMX_PES_OTHER;
            break;
        /* What to do with i? */    
        default:
            msg_Err( p_input, "trying to set PMT id to=%d for unknown type %d", i_pid, i_type );
            break;
    }
    s_filter_params.flags = DMX_IMMEDIATE_START;

    /* We then give the order to the device : */
    if( (result = ioctl( *pi_fd, DMX_SET_PES_FILTER, &s_filter_params )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "Demux set filter ioctl failed with %d (%s)", result, strerror( errno ) );
#   else
        msg_Err( p_input, "Demux set filter ioctl failed with %d", result );
#   endif
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * ioctl_UnsetDMXFilter : removes a filter
 *****************************************************************************/
int ioctl_UnsetDMXFilter( input_thread_t * p_input, int pi_fd )
{
    int i_ret;
    
    if( (i_ret = ioctl( pi_fd, DMX_STOP )) < 0 )
    {
#   ifdef HAVE_ERRNO_H
        msg_Err( p_input, "ioctl DMX_STOP failed for demux %d (%d) %s", pi_fd, i_ret, strerror( errno ) );
#   else
        msg_Err( p_input, "ioctl DMX_STOP failed for demux %d (%d)", pi_fd, i_ret );
#   endif
        return -1;
    }
    msg_Dbg( p_input, "ioctl_UnsetDMXFilter closing demux %d", pi_fd );
    close( pi_fd );
    return 0;
}

/*****************************************************************************
 * dvb_DecodeBandwidth : decodes arguments for DVB S/C/T card
 *****************************************************************************/
fe_bandwidth_t dvb_DecodeBandwidth( input_thread_t * p_input, int bandwidth )
{
    fe_bandwidth_t      fe_bandwidth = 0;
    
    switch( bandwidth )
    {
        case 0:
            fe_bandwidth = BANDWIDTH_AUTO;
            break;
        case 6:
            fe_bandwidth = BANDWIDTH_6_MHZ;
            break;
        case 7:
            fe_bandwidth = BANDWIDTH_7_MHZ;
            break;
        case 8:
            fe_bandwidth = BANDWIDTH_8_MHZ;
            break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has bandwidth not set, using auto" );
            fe_bandwidth = BANDWIDTH_AUTO;
            break;
    }    
    return fe_bandwidth;
}

fe_code_rate_t dvb_DecodeFEC( input_thread_t * p_input, int fec )
{
    fe_code_rate_t fe_fec = FEC_NONE;
    
    switch( fec )
    {
        case 1:
            fe_fec = FEC_1_2;
            break;
        case 2:
            fe_fec = FEC_2_3;
            break;
        case 3:
            fe_fec = FEC_3_4;
            break;
        case 4:
            fe_fec = FEC_4_5;
            break;
        case 5:
            fe_fec = FEC_5_6;
            break;
        case 6:
            fe_fec = FEC_6_7;
            break;
        case 7:
            fe_fec = FEC_7_8;
            break;
        case 8:
            fe_fec = FEC_8_9;
            break;
        case 9:
            fe_fec = FEC_AUTO;
            break;
        default:
            /* cannot happen */
            fe_fec = FEC_NONE;
            msg_Err( p_input, "argument has invalid FEC (%d)", fec );
            break;
    }    
    return fe_fec;
}

fe_modulation_t dvb_DecodeModulation( input_thread_t * p_input, int modulation )
{
    fe_modulation_t     fe_modulation = 0;
    
    switch( modulation )
    {
        case -1:
            fe_modulation = QPSK;
            break;
        case 0:
            fe_modulation = QAM_AUTO;
            break;
        case 16:
            fe_modulation = QAM_16;
            break;
        case 32:
            fe_modulation = QAM_32;
            break;
        case 64:
            fe_modulation = QAM_64;
            break;
        case 128:
            fe_modulation = QAM_128;
            break;
        case 256:
            fe_modulation = QAM_256;
            break;
        default:
            msg_Dbg( p_input, "terrestrial/cable dvb has constellation/modulation not set, using auto" );
            fe_modulation = QAM_AUTO;
            break;
    }    
    return fe_modulation;
}

fe_transmit_mode_t dvb_DecodeTransmission( input_thread_t * p_input, int transmission )
{
    fe_transmit_mode_t  fe_transmission = 0;
    
    switch( transmission )
    {
        case 0:
            fe_transmission = TRANSMISSION_MODE_AUTO;
            break;
        case 2:
            fe_transmission = TRANSMISSION_MODE_2K;
            break;
        case 8:
            fe_transmission = TRANSMISSION_MODE_8K;
            break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has transmission mode not set, using auto" );
            fe_transmission = TRANSMISSION_MODE_AUTO;
            break;
    }    
    return fe_transmission;
}

fe_guard_interval_t dvb_DecodeGuardInterval( input_thread_t * p_input, int guard )
{
    fe_guard_interval_t fe_guard = 0;

    switch( guard )
    {
        case 0:
            fe_guard = GUARD_INTERVAL_AUTO;
            break;
        case 4:
            fe_guard = GUARD_INTERVAL_1_4;
            break;
        case 8:
            fe_guard = GUARD_INTERVAL_1_8;
            break;
        case 16:
            fe_guard = GUARD_INTERVAL_1_16;
            break;
        case 32:
            fe_guard = GUARD_INTERVAL_1_32;
            break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has guard interval not set, using auto" );
            fe_guard = GUARD_INTERVAL_AUTO;
            break;
    }
    return fe_guard;
}

fe_hierarchy_t dvb_DecodeHierarchy( input_thread_t * p_input, int hierarchy )
{
    fe_hierarchy_t      fe_hierarchy = 0;

    switch( hierarchy )
    {
        case -1:
            fe_hierarchy = HIERARCHY_NONE;
            break;
        case 0:
            fe_hierarchy = HIERARCHY_AUTO;
            break;
        case 1:
            fe_hierarchy = HIERARCHY_1;
            break;
        case 2:
            fe_hierarchy = HIERARCHY_2;
            break;
        case 4:
            fe_hierarchy = HIERARCHY_4;
            break;
        default:
            msg_Dbg( p_input, "terrestrial dvb has hierarchy not set, using auto" );
            fe_hierarchy = HIERARCHY_AUTO;
            break;
    }
    return fe_hierarchy;
}

fe_spectral_inversion_t dvb_DecodeInversion( input_thread_t * p_input, int inversion )
{
    fe_spectral_inversion_t fe_inversion=0;

    switch( inversion )
    {
        case 0:
            fe_inversion = INVERSION_OFF;
            break;
        case 1:
            fe_inversion = INVERSION_ON;
            break;
        case 2:
            fe_inversion = INVERSION_AUTO;
            break;
        default:
            msg_Dbg( p_input, "dvb has inversion/polarisation not set, using auto" );
            fe_inversion = INVERSION_AUTO;
            break;
    }
    return fe_inversion;
} 
