/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@saman>
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


/*****************************************************************************
 * Devices location
 *****************************************************************************/
#define DMX      "/dev/dvb/adapter%d/demux%d"
#define FRONTEND "/dev/dvb/adapter%d/frontend%d"
#define DVR      "/dev/dvb/adapter%d/dvr%d"


/*****************************************************************************
 * DVB input data structure
 *****************************************************************************/
typedef struct
{
    int                 i_frontend;
    unsigned int        u_adapter;
    unsigned int        u_device;
    unsigned int        u_freq;
    unsigned int        u_srate;
    unsigned int        u_lnb_lof1;
    unsigned int        u_lnb_lof2;
    unsigned int        u_lnb_slof;
    int                 i_bandwidth;
    int                 i_modulation;
    int                 i_guard;
    int                 i_transmission;
    int                 i_hierarchy;
    int                 i_polarisation;
    int                 i_fec;
    int                 i_code_rate_HP;
    int                 i_code_rate_LP;
    vlc_bool_t          b_diseqc;
    vlc_bool_t          b_probe;

    input_socket_t *    p_satellite;
} input_dvb_t;


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int ioctl_SetQPSKFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep );
int ioctl_SetOFDMFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep );
int ioctl_SetQAMFrontend( input_thread_t * p_input, struct dvb_frontend_parameters fep );
int ioctl_SetDMXFilter( input_thread_t * p_input, int i_pid, int *pi_fd, int i_type );
int ioctl_UnsetDMXFilter( input_thread_t * p_input, int pi_fd );
int ioctl_InfoFrontend( input_thread_t * p_input, struct dvb_frontend_info *info );

/*****************************************************************************
 * dvb argument helper functions 
 *****************************************************************************/
fe_bandwidth_t      dvb_DecodeBandwidth( input_thread_t * p_input, int bandwidth );
fe_code_rate_t      dvb_DecodeFEC( input_thread_t * p_input, int fec );
fe_modulation_t     dvb_DecodeModulation( input_thread_t * p_input, int modulation );
fe_transmit_mode_t  dvb_DecodeTransmission( input_thread_t * p_input, int transmission );
fe_guard_interval_t     dvb_DecodeGuardInterval( input_thread_t * p_input, int guard );
fe_hierarchy_t          dvb_DecodeHierarchy( input_thread_t * p_input, int hierarchy );
fe_spectral_inversion_t dvb_DecodeInversion( input_thread_t * p_input, int inversion);

