/*****************************************************************************
 * dialogProvider.m: Minimal Dialog Provider for Mac OS X
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <stdlib.h>                                      /* malloc(), free() */
#import <string.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_dialog.h>
#import <vlc_interface.h>

#import <Cocoa/Cocoa.h>
#import "VLCLoginPanel.h"
#import "VLCProgressPanel.h"

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static int  OpenIntf(vlc_object_t *);
static void CloseIntf(vlc_object_t *);
static void Run(intf_thread_t * );

static int DisplayError(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayCritical(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayQuestion(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayLogin(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayProgressPanelAction(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

struct intf_sys_t
{
    VLCProgressPanel *currentProgressBarPanel;

    vlc_mutex_t lock;
    vlc_cond_t wait;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
/* Minimal interface. see intf.m */
set_shortname("Mac OS X Dialogs")
add_shortcut("macosx_dialog_provider")
add_shortcut("miosx")
set_description("Minimal Mac OS X Dialog Provider")
set_capability("interface", 50)
set_callbacks(OpenIntf, CloseIntf)
set_category(CAT_INTERFACE)
set_subcategory(SUBCAT_INTERFACE_MAIN)
vlc_module_end()

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc(sizeof(intf_sys_t));
    if(!p_intf->p_sys)
        return VLC_ENOMEM;

    memset(p_intf->p_sys,0,sizeof(*p_intf->p_sys));

    p_intf->b_should_run_on_first_thread = true;
    p_intf->pf_run = Run;

    msg_Dbg(p_intf,"Opening Mac OS X dialog provider");
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Run: waiting for the death
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    /* subscribe to various interactive dialogues */
    var_Create(p_intf,"dialog-error",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-error",DisplayError,p_intf);
    var_Create(p_intf,"dialog-critical",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    var_Create(p_intf,"dialog-login",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_Create(p_intf,"dialog-question",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_Create(p_intf,"dialog-progress-bar",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
    dialog_Register(p_intf);

    msg_Dbg(p_intf,"Mac OS X dialog provider initialised");

    /* idle */
    while(vlc_object_alive(p_intf))
        msleep(INTF_IDLE_SLEEP);
    
    /* unsubscribe from the interactive dialogues */
    dialog_Unregister(p_intf);
    var_DelCallback(p_intf,"dialog-error",DisplayError,p_intf);
    var_DelCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    var_DelCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_DelCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_DelCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
}
/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    msg_Dbg(p_intf,"Mac OS X dialog provider closed");
    free(p_intf->p_sys);
}


/*****************************************************************************
 * Callbacks triggered by the "dialog-*" variables
 *****************************************************************************/
static int DisplayError(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *p_dialog = (dialog_fatal_t *)value.p_address;
    NSRunInformationalAlertPanel([NSString stringWithUTF8String:p_dialog->title],
                            [NSString stringWithUTF8String:p_dialog->message],
                            @"OK", nil, nil);
    [o_pool release];
    return VLC_SUCCESS;
}

static int DisplayCritical(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *p_dialog = (dialog_fatal_t *)value.p_address;
    NSRunCriticalAlertPanel([NSString stringWithUTF8String:p_dialog->title],
                            [NSString stringWithUTF8String:p_dialog->message],
                            @"OK", nil, nil);
    [o_pool release];
    return VLC_SUCCESS;
}

static int DisplayQuestion(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    dialog_question_t *p_dialog = (dialog_question_t *)value.p_address;
    NSAlert *o_alert;
    NSString *o_yes, *o_no, *o_cancel;
    NSInteger i_returnValue = 0;

    if (p_dialog->yes != NULL)
        o_yes = [NSString stringWithUTF8String:p_dialog->yes];
    if (p_dialog->no != NULL)
        o_no = [NSString stringWithUTF8String:p_dialog->no];
    if (p_dialog->cancel != NULL)
        o_cancel = [NSString stringWithUTF8String:p_dialog->cancel];

    o_alert = [NSAlert alertWithMessageText:[NSString stringWithUTF8String:p_dialog->title]
                              defaultButton:o_yes
                            alternateButton:o_no 
                                otherButton:o_cancel
                  informativeTextWithFormat:[NSString stringWithUTF8String:p_dialog->message]];
    [o_alert setAlertStyle:NSInformationalAlertStyle];
    i_returnValue = [o_alert runModal];

    if (i_returnValue == NSAlertDefaultReturn)
        p_dialog->answer = 1;
    if (i_returnValue == NSAlertAlternateReturn)
        p_dialog->answer = 2;
    if (i_returnValue == NSAlertOtherReturn)
        p_dialog->answer = 3;
    [o_pool release];
    return VLC_SUCCESS;
}

static int DisplayLogin(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    dialog_login_t *p_dialog = (dialog_login_t *)value.p_address;
    NSInteger i_returnValue = 0;
    VLCLoginPanel *thePanel = [[VLCLoginPanel alloc] init];
    [thePanel createContentView];
    [thePanel setDialogTitle:[NSString stringWithUTF8String:p_dialog->title]];
    [thePanel setDialogMessage:[NSString stringWithUTF8String:p_dialog->message]];
    [thePanel center];
    i_returnValue = [NSApp runModalForWindow:thePanel];
    [thePanel close];
    if (i_returnValue) {
        *p_dialog->username = strdup( [[thePanel userName] UTF8String] );
        *p_dialog->password = strdup( [[thePanel password] UTF8String] );
    } else
        *p_dialog->username = *p_dialog->password = NULL;
    [o_pool release];
    return VLC_SUCCESS;
}

static int DisplayProgressPanelAction(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    dialog_progress_bar_t * p_dialog = (dialog_progress_bar_t *)value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    if(p_sys->currentProgressBarPanel)
        [p_sys->currentProgressBarPanel release];

    p_sys->currentProgressBarPanel = [[VLCProgressPanel alloc] init];
    [p_sys->currentProgressBarPanel createContentView];
    if (p_dialog->title)
        [p_sys->currentProgressBarPanel setDialogTitle:[NSString stringWithUTF8String:p_dialog->title]];
    if (p_dialog->message)
        [p_sys->currentProgressBarPanel setDialogMessage:[NSString stringWithUTF8String:p_dialog->message]];
    if (p_dialog->cancel)
        [p_sys->currentProgressBarPanel setCancelButtonLabel:[NSString stringWithUTF8String:p_dialog->cancel]];
    [p_sys->currentProgressBarPanel center];
    [p_sys->currentProgressBarPanel makeKeyAndOrderFront:nil];

    p_dialog->pf_update = updateProgressPanel;
    p_dialog->pf_check = checkProgressPanel;
    p_dialog->pf_destroy = destroyProgressPanel;
    p_dialog->p_sys = p_intf->p_sys;

    [o_pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *p_sys = (intf_sys_t *)priv;

    if (text)
        [p_sys->currentProgressBarPanel setDialogMessage:[NSString stringWithUTF8String:text]];
    else
        [p_sys->currentProgressBarPanel setDialogMessage:@""];
    [p_sys->currentProgressBarPanel setProgressAsDouble:(double)(value * 1000.)];

    [o_pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *p_sys = (intf_sys_t *)priv;

    [p_sys->currentProgressBarPanel close];
    [p_sys->currentProgressBarPanel release];

    [o_pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *p_sys = (intf_sys_t *)priv;
    BOOL b_returned;

    b_returned = [p_sys->currentProgressBarPanel isCancelled];

    [o_pool release];
    return b_returned;
}

