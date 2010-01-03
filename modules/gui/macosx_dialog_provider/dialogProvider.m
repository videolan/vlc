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

@interface VLCDialogDisplayer : NSObject
{
    VLCProgressPanel *_currentProgressBarPanel;
}

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel;

- (void)displayError:(NSDictionary *)dialog;
- (void)displayCritical:(NSDictionary *)dialog;
- (NSNumber *)displayQuestion:(NSDictionary *)dialog;
- (NSDictionary *)displayLogin:(NSDictionary *)dialog;

- (void)displayProgressBar:(NSDictionary *)dict;
- (void)updateProgressPanel:(NSDictionary *)dict;
- (void)destroyProgressPanel;
- (NSNumber *)checkProgressPanel;

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object;
@end

static inline NSDictionary *DictFromDialogFatal(dialog_fatal_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :NULL];
}
static inline NSDictionary *DictFromDialogLogin(dialog_login_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :NULL];
}
static inline NSDictionary *DictFromDialogQuestion(dialog_question_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :dialog->yes :dialog->no :dialog->cancel];
}
static inline NSDictionary *DictFromDialogProgressBar(dialog_progress_bar_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :dialog->cancel];
}

struct intf_sys_t
{
    VLCDialogDisplayer *displayer;

    vlc_mutex_t lock;
    vlc_cond_t wait;
    bool is_hidding_noaction_dialogs;
};


#define T_HIDE_NOACTION N_("Hide no user action dialogs")
#define LT_HIDE_NOACTION N_("Don't display dialogs that don't require user action (Critical and error panel).")

#define prefix "macosx-dialog-provider"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    /* Minimal interface. see intf.m */
    set_shortname("Mac OS X Dialogs")
    add_shortcut("macosx_dialog_provider")
    add_shortcut("miosx")
    set_description("Minimal Mac OS X Dialog Provider")
    set_capability("interface", 0)

    /* This setting is interesting, because when used with a libvlc app
     * it's almost certain that the client program will display error by
     * itself. Moreover certain action might end up in an error, but 
     * the client wants to ignored them completely. */
    add_bool(prefix "hide-no-user-action-dialogs", true, NULL, T_HIDE_NOACTION, LT_HIDE_NOACTION, false)

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

    p_intf->p_sys->displayer = [[VLCDialogDisplayer alloc] init];

    bool hide = var_CreateGetBool(p_intf, prefix "hide-no-user-action-dialogs");
    p_intf->p_sys->is_hidding_noaction_dialogs = hide;

    /* subscribe to various interactive dialogues */
    
    if (!hide)
    {
        var_Create(p_intf,"dialog-error",VLC_VAR_ADDRESS);
        var_AddCallback(p_intf,"dialog-error",DisplayError,p_intf);
        var_Create(p_intf,"dialog-critical",VLC_VAR_ADDRESS);
        var_AddCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);        
    }
    var_Create(p_intf,"dialog-login",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_Create(p_intf,"dialog-question",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_Create(p_intf,"dialog-progress-bar",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
    dialog_Register(p_intf);

    msg_Dbg(p_intf,"Mac OS X dialog provider initialised");
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister(p_intf);
    
    if (!p_intf->p_sys->is_hidding_noaction_dialogs)
    {
        var_DelCallback(p_intf,"dialog-error",DisplayError,p_intf);
        var_DelCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    }
    var_DelCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_DelCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_DelCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
    
    [p_intf->p_sys->displayer release];

    msg_Dbg(p_intf,"Mac OS X dialog provider closed");
    free(p_intf->p_sys);
}


/*****************************************************************************
 * Callbacks triggered by the "dialog-*" variables
 *****************************************************************************/
static int DisplayError(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    [sys->displayer performSelectorOnMainThread:@selector(displayError:) withObject:DictFromDialogFatal(dialog) waitUntilDone:NO];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayCritical(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    [sys->displayer performSelectorOnMainThread:@selector(displayCritical:) withObject:DictFromDialogFatal(dialog) waitUntilDone:NO];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayQuestion(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_question_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    dialog->answer = [[sys->displayer resultFromSelectorOnMainThread:@selector(displayQuestion:) withObject:DictFromDialogQuestion(dialog)] intValue];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayLogin(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_login_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    NSDictionary *dict = [sys->displayer resultFromSelectorOnMainThread:@selector(displayLogin:) withObject:DictFromDialogLogin(dialog)];
    if (dict) {
        *dialog->username = strdup([[dict objectForKey:@"username"] UTF8String]);
        *dialog->password = strdup([[dict objectForKey:@"password"] UTF8String]);
    }
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayProgressPanelAction(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    dialog_progress_bar_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;

    [sys->displayer performSelectorOnMainThread:@selector(displayProgressBar:) withObject:DictFromDialogProgressBar(dialog) waitUntilDone:YES];

    dialog->pf_update = updateProgressPanel;
    dialog->pf_check = checkProgressPanel;
    dialog->pf_destroy = destroyProgressPanel;
    dialog->p_sys = p_intf->p_sys;

    [pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSNumber numberWithFloat:value], @"value",
                          text ? [NSString stringWithUTF8String:text] : nil, @"text",
                          nil];
                          
    [sys->displayer performSelectorOnMainThread:@selector(updateProgressPanel:) withObject:dict waitUntilDone:YES];

    [pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;

    [sys->displayer performSelectorOnMainThread:@selector(destroyProgressPanel) withObject:nil waitUntilDone:YES];
    
    [pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;
    BOOL ret;

    ret = [[sys->displayer resultFromSelectorOnMainThread:@selector(checkProgressPanel) withObject:nil] boolValue];

    [pool release];
    return ret;
}


@implementation VLCDialogDisplayer
- (void)dealloc
{
    assert(!_currentProgressBarPanel); // This has to be closed on main thread.
    [super dealloc];
}

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (title)
        [dict setObject:[NSString stringWithUTF8String:title] forKey:@"title"];
    if (message)
        [dict setObject:[NSString stringWithUTF8String:message] forKey:@"message"];
    if (yes)
        [dict setObject:[NSString stringWithUTF8String:yes] forKey:@"yes"];
    if (no)
        [dict setObject:[NSString stringWithUTF8String:no] forKey:@"no"];
    if (cancel)
        [dict setObject:[NSString stringWithUTF8String:cancel] forKey:@"cancel"];
    
    return dict;
}
#define VLCAssertIsMainThread() assert([NSThread isMainThread])

- (void)displayError:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();
    
    NSRunInformationalAlertPanel([dialog objectForKey:@"title"],
                                 [dialog objectForKey:@"message"],
                                 @"OK", nil, nil);
}

- (void)displayCritical:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();
    
    NSRunCriticalAlertPanel([dialog objectForKey:@"title"],
                                 [dialog objectForKey:@"message"],
                                 @"OK", nil, nil);
}

- (NSNumber *)displayQuestion:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();
    
    NSInteger alertRet = 0;
    
    NSAlert *alert = [NSAlert alertWithMessageText:[dialog objectForKey:@"title"]
                              defaultButton:[dialog objectForKey:@"yes"]
                            alternateButton:[dialog objectForKey:@"no"] 
                                otherButton:[dialog objectForKey:@"cancel"]
                  informativeTextWithFormat:[dialog objectForKey:@"message"]];
    [alert setAlertStyle:NSInformationalAlertStyle];
    alertRet = [alert runModal];

    int ret;
    switch (alertRet) {
        case NSAlertDefaultReturn:
            ret = 1;
            break;
        case NSAlertAlternateReturn:
            ret = 2;
            break;
        case NSAlertOtherReturn:
            ret = 3;
            break;
        default:
            assert(0);
            ret = 0;
            break;
    }

    return [NSNumber numberWithInt:ret];
}

- (NSDictionary *)displayLogin:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    VLCLoginPanel *panel = [[VLCLoginPanel alloc] init];
    [panel createContentView];
    [panel setDialogTitle:[dialog objectForKey:@"title"]];
    [panel setDialogMessage:[dialog objectForKey:@"message"]];
    [panel center];
    NSInteger ret = [NSApp runModalForWindow:panel];
    [panel close];
    
    if (!ret)
        return nil;
    
    return [NSDictionary dictionaryWithObjectsAndKeys:
            [panel userName], @"username",
            [panel password], @"password",
            nil];
}

- (void)displayProgressBar:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    if(_currentProgressBarPanel)
        [self destroyProgressPanel];

    assert(!_currentProgressBarPanel);
    _currentProgressBarPanel = [[VLCProgressPanel alloc] init];
    [_currentProgressBarPanel createContentView];
    [_currentProgressBarPanel setDialogTitle:[dialog objectForKey:@"title"]];
    [_currentProgressBarPanel setDialogMessage:[dialog objectForKey:@"message"] ?: @""];
    [_currentProgressBarPanel setCancelButtonLabel:[dialog objectForKey:@"cancel"]];

    [_currentProgressBarPanel center];
    [_currentProgressBarPanel makeKeyAndOrderFront:nil];
}

- (void)updateProgressPanel:(NSDictionary *)dict
{
    VLCAssertIsMainThread();

    assert(_currentProgressBarPanel);
    [_currentProgressBarPanel setDialogMessage:[dict objectForKey:@"text"] ?: @""];
    [_currentProgressBarPanel setProgressAsDouble:[[dict objectForKey:@"value"] doubleValue] * 1000.];
}

- (void)destroyProgressPanel
{
    VLCAssertIsMainThread();

    [_currentProgressBarPanel close];
    [_currentProgressBarPanel release];
    _currentProgressBarPanel = nil;
}

- (NSNumber *)checkProgressPanel
{
    VLCAssertIsMainThread();
    
    return [NSNumber numberWithBool:[_currentProgressBarPanel isCancelled]];
}


/**
 * Helper to execute a function on main thread and get its return value.
 */
- (void)execute:(NSDictionary *)dict
{
    SEL sel = [[dict objectForKey:@"sel"] pointerValue];
    id *result = [[dict objectForKey:@"result"] pointerValue];
    id object = [dict objectForKey:@"object"];

    NSAssert(sel, @"Try to execute a NULL selector");
    NSAssert(object, @"Try to execute from a nil object");

    *result = [self performSelector:sel withObject:object];
    [*result retain]; // Balanced in -resultFromSelectorOnMainThread
}

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object
{
    id result = nil;
    NSAssert(sel, @"Try to execute a NULL selector");
    NSAssert(sel, @"Try to execute from a nil object");
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
     [NSValue valueWithPointer:sel], @"sel",
     [NSValue valueWithPointer:&result], @"result",
     object, @"object", nil];
    [self performSelectorOnMainThread:@selector(execute:) withObject:dict waitUntilDone:YES];
    return [result autorelease];
}
@end
                                  
                                  

