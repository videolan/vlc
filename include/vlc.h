/*******************************************************************************
 * vlc.h: all headers
 * (c)1998 VideoLAN
 *******************************************************************************
 * This header includes all vlc .h headers and depending headers. A source file
 * including it would also be able to use any of the structures of the project.
 * Note that functions or system headers specific to the file itself are not
 * included.
 *******************************************************************************
 * required headers:
 *  none
 *******************************************************************************/

/* System headers */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>

#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/soundcard.h>
#include <sys/uio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/xf86dga.h>



/* Common headers */
#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "netutils.h"
#include "debug.h"
#include "intf_msg.h"

/* Input */
#include "input.h"
#include "input_psi.h"
#include "input_pcr.h"
#include "input_netlist.h"
#include "input_vlan.h"
#include "decoder_fifo.h"
#include "input_file.h"
#include "input_network.h"
#include "input_ctrl.h"

/* Audio */
#include "audio_output.h"
#include "audio_decoder.h"
#include "ac3_decoder.h"

/* Video */
#include "video.h"
#include "video_output.h"

#ifdef OLD_DECODER
#include "video_decoder.h"
#else
#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"
#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "video_fifo.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#endif

/* Interface */
#include "intf_cmd.h"
#include "intf_ctrl.h"
#ifndef OLD_DECODER
#include "intf_sys.h"
#include "intf_console.h"
#endif
#include "interface.h"

#include "main.h"
