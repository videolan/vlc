/*******************************************************************************
 * all.h: all headers
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
#include <pthread.h>
#include <netinet/in.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

/* Common headers */
#include "config.h"
#include "common.h"
#include "mtime.h"

/* Input */
#include "input.h"
#include "input_vlan.h"
#include "decoder_fifo.h"

/* Audio */
#include "audio_output.h"
#include "audio_decoder.h"

/* Video */
#include "video.h"
#include "video_output.h"
#include "video_decoder.h"

/* Interface */
#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"

/* Shared resources */
#include "pgm_data.h"



