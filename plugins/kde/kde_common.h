/***************************************************************************
                          kde_common.h  -  description
                             -------------------
    begin                : Mon Apr 9 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _INTF_PLUGIN_H_
#define _INTF_PLUGIN_H_

extern "C"
{
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"
}

#endif /* _INTF_PLUGIN_H_ */
