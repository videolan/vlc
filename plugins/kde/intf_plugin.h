/***************************************************************************
                          intf_plugin.h  -  description
                             -------------------
    begin                : Mon Apr 9 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef _INTF_PLUGIN_H_
#define _INTF_PLUGIN_H_

extern "C"
{
#include "modules_inner.h"
#include "defs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_msg.h"
#include "intf_playlist.h"
#include "interface.h"

#include "main.h"
}

#endif /* _INTF_PLUGIN_H_ */