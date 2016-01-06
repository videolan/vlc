/*****************************************************************************
 * dialogProvider.m: iOS Dialog Provider
 *****************************************************************************
 * Copyright (C) 2009, 2014-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#import <vlc_modules.h>
#import <vlc_interface.h>

#import <UIKit/UIKit.h>

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
@interface VLCDialogDisplayer : NSObject

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

#if !TARGET_OS_TV
@interface VLCBlockingAlertView : UIAlertView <UIAlertViewDelegate>

@property (copy, nonatomic) void (^completion)(BOOL, NSInteger);

- (id)initWithTitle:(NSString *)title
            message:(NSString *)message
  cancelButtonTitle:(NSString *)cancelButtonTitle
  otherButtonTitles:(NSArray *)otherButtonTitles;

@end
#endif

static int  OpenIntf(vlc_object_t *);
static void CloseIntf(vlc_object_t *);
static void Run(intf_thread_t * );

static int DisplayError(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayCritical(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayQuestion(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayLogin(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayProgressPanelAction(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );

static void updateProgressPanel(void *, const char *, float);
static bool checkProgressPanel(void *);
static void destroyProgressPanel(void *);

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
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    set_shortname("iOS Dialogs")
    add_shortcut("ios_dialog_provider", "miosx")
    set_description("iOS Dialog Provider")
    set_capability("interface", 0)

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

    msg_Dbg(p_intf,"iOS dialog provider initialised");

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

    var_DelCallback(p_intf,"dialog-error",DisplayError,p_intf);
    var_DelCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    var_DelCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_DelCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_DelCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);

    [p_intf->p_sys->displayer release];

    msg_Dbg(p_intf,"iOS dialog provider closed");
    free(p_intf->p_sys);
}


/*****************************************************************************
 * Callbacks triggered by the "dialog-*" variables
 *****************************************************************************/
static int DisplayError(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        dialog_fatal_t *dialog = value.p_address;
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        intf_sys_t *sys = p_intf->p_sys;
        [sys->displayer performSelectorOnMainThread:@selector(displayError:)
                                         withObject:DictFromDialogFatal(dialog)
                                      waitUntilDone:NO];
        return VLC_SUCCESS;
    }
}

static int DisplayCritical(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        dialog_fatal_t *dialog = value.p_address;
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        intf_sys_t *sys = p_intf->p_sys;
        [sys->displayer performSelectorOnMainThread:@selector(displayCritical:)
                                         withObject:DictFromDialogFatal(dialog)
                                      waitUntilDone:NO];
        return VLC_SUCCESS;
    }
}

static int DisplayQuestion(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        dialog_question_t *dialog = value.p_address;
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        intf_sys_t *sys = p_intf->p_sys;
        dialog->answer = [[sys->displayer displayQuestion:DictFromDialogQuestion(dialog)] intValue];
        return VLC_SUCCESS;
    }
}

static int DisplayLogin(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        dialog_login_t *dialog = value.p_address;
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        intf_sys_t *sys = p_intf->p_sys;
        NSDictionary *dict = [sys->displayer displayLogin:DictFromDialogLogin(dialog)];
        if (dict) {
            NSString *username = dict[@"username"];
            if (username != NULL && username.length > 0)
                *dialog->username = strdup([username UTF8String]);
            NSString *password = dict[@"password"];
            if (password != NULL && password.length > 0)
                *dialog->password = strdup([password UTF8String]);
        }
        return VLC_SUCCESS;
    }
}

static int DisplayProgressPanelAction(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    @autoreleasepool {
        dialog_progress_bar_t *dialog = value.p_address;
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        intf_sys_t *sys = p_intf->p_sys;

        [sys->displayer performSelectorOnMainThread:@selector(displayProgressBar:)
                                         withObject:DictFromDialogProgressBar(dialog)
                                      waitUntilDone:YES];

        dialog->pf_update = updateProgressPanel;
        dialog->pf_check = checkProgressPanel;
        dialog->pf_destroy = destroyProgressPanel;
        dialog->p_sys = p_intf->p_sys;

        return VLC_SUCCESS;
    }
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    @autoreleasepool {
        intf_sys_t *sys = (intf_sys_t *)priv;

        NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                              @(value), @"value",
                              text ? @(text) : nil, @"text",
                              nil];

        [sys->displayer performSelectorOnMainThread:@selector(updateProgressPanel:)
                                         withObject:dict
                                      waitUntilDone:YES];
    }
}

void destroyProgressPanel (void *priv)
{
    @autoreleasepool {
        intf_sys_t *sys = (intf_sys_t *)priv;
        [sys->displayer performSelectorOnMainThread:@selector(destroyProgressPanel)
                                         withObject:nil
                                      waitUntilDone:YES];
    }
}

bool checkProgressPanel (void *priv)
{
    @autoreleasepool {
        intf_sys_t *sys = (intf_sys_t *)priv;
        return [[sys->displayer resultFromSelectorOnMainThread:@selector(checkProgressPanel)
                                                    withObject:nil] boolValue];
    }
}

@implementation VLCDialogDisplayer

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (title)
        [dict setObject:@(title) forKey:@"title"];
    if (message)
        [dict setObject:@(message) forKey:@"message"];
    if (yes)
        [dict setObject:@(yes) forKey:@"yes"];
    if (no)
        [dict setObject:@(no) forKey:@"no"];
    if (cancel)
        [dict setObject:@(cancel) forKey:@"cancel"];
    [dict retain];

    return dict;
}
#define VLCAssertIsMainThread() assert([NSThread isMainThread])

- (void)displayError:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

#if TARGET_OS_TV
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:dialog[@"title"]
                                                                             message:dialog[@"message"]
                                                                      preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction *action = [UIAlertAction actionWithTitle:NSLocalizedString(@"OK", nil)
                                                     style:UIAlertActionStyleDestructive
                                                   handler:^(UIAlertAction * _Nonnull action) {
                                                       [dialog release];
                                                   }];
    [alertController addAction:action];
    [alertController setPreferredAction:action];

    [[[[UIApplication sharedApplication].delegate.window rootViewController] presentedViewController] presentViewController:alertController animated:YES completion:nil];
#else
    VLCBlockingAlertView *alert = [[VLCBlockingAlertView alloc] initWithTitle:dialog[@"title"]
                                                                      message:dialog[@"message"]
                                                                     delegate:nil
                                                            cancelButtonTitle:NSLocalizedString(@"OK", nil)
                                                            otherButtonTitles:nil];
    alert.completion = ^(BOOL cancelled, NSInteger buttonIndex) {
        [alert release];
        [dialog release];
    };
    [alert show];
#endif
}

- (void)displayCritical:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

#if TARGET_OS_TV
    UIAlertController *alertController = [UIAlertController alertControllerWithTitle:dialog[@"title"]
                                                                             message:dialog[@"message"]
                                                                      preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction *action = [UIAlertAction actionWithTitle:NSLocalizedString(@"OK", nil)
                                                     style:UIAlertActionStyleDestructive
                                                   handler:^(UIAlertAction * _Nonnull action) {
                                                       [dialog release];
                                                   }];
    [alertController addAction:action];
    [alertController setPreferredAction:action];

    [[[[UIApplication sharedApplication].delegate.window rootViewController] presentedViewController] presentViewController:alertController animated:YES completion:nil];
#else
    VLCBlockingAlertView *alert = [[VLCBlockingAlertView alloc] initWithTitle:dialog[@"title"]
                                                                      message:dialog[@"message"]
                                                                     delegate:nil
                                                            cancelButtonTitle:NSLocalizedString(@"OK", nil)
                                                            otherButtonTitles:nil];
    alert.completion = ^(BOOL cancelled, NSInteger buttonIndex) {
        [alert release];
        [dialog release];
    };
    [alert show];
#endif
}

- (NSNumber *)displayQuestion:(NSDictionary *)dialog
{
#if TARGET_OS_TV
    __block int ret = 0;

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController *alertController = [UIAlertController alertControllerWithTitle:dialog[@"title"]
                                                                                 message:dialog[@"message"]
                                                                          preferredStyle:UIAlertControllerStyleAlert];

        NSString *cancelTitle = dialog[@"cancel"];
        [alertController addAction:[UIAlertAction actionWithTitle:cancelTitle != nil ? cancelTitle : NSLocalizedString(@"Cancel", nil)
                                                            style:UIAlertActionStyleDestructive
                                                          handler:^(UIAlertAction * _Nonnull action) {
                                                              ret = 3;
                                                              dispatch_semaphore_signal(sema);
                                                          }]];

        UIAlertAction *yesAction = [UIAlertAction actionWithTitle:dialog[@"yes"]
                                                            style:UIAlertActionStyleDestructive
                                                          handler:^(UIAlertAction * _Nonnull action) {
                                                              ret = 0;
                                                              dispatch_semaphore_signal(sema);
                                                          }];
        [alertController addAction:yesAction];
        [alertController setPreferredAction:yesAction];

        [alertController addAction:[UIAlertAction actionWithTitle:dialog[@"yes"]
                                                            style:UIAlertActionStyleDestructive
                                                          handler:^(UIAlertAction * _Nonnull action) {
                                                              ret = 1;
                                                              dispatch_semaphore_signal(sema);
                                                          }]];

        [[[[UIApplication sharedApplication].delegate.window rootViewController] presentedViewController] presentViewController:alertController animated:YES completion:nil];
    });

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    [dialog release];

    return @(ret);
#else
    __block int ret = 0;
    __block VLCBlockingAlertView *alert;

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    dispatch_async(dispatch_get_main_queue(), ^{
        alert = [[VLCBlockingAlertView alloc] initWithTitle:dialog[@"title"] message:dialog[@"message"]
                                                   delegate:nil
                                          cancelButtonTitle:dialog[@"cancel"]
                                          otherButtonTitles:dialog[@"yes"], dialog[@"no"], nil];
        alert.completion = ^(BOOL cancelled, NSInteger buttonIndex) {
            if (cancelled)
                ret = 3;
            else
                ret = buttonIndex;

            dispatch_semaphore_signal(sema);
        };
        alert.delegate = alert;
        [alert show];
    });

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    [alert release];
    [dialog release];

    return @(ret);
#endif
}

- (NSDictionary *)displayLogin:(NSDictionary *)dialog
{
#if TARGET_OS_TV
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSMutableDictionary *dict = [NSMutableDictionary dictionary];

    dispatch_async(dispatch_get_main_queue(), ^{
        UIAlertController *alertController = [UIAlertController alertControllerWithTitle:dialog[@"title"]
                                                                                 message:dialog[@"message"]
                                                                          preferredStyle:UIAlertControllerStyleAlert];

        __block UITextField *usernameField = nil;
        __block UITextField *passwordField = nil;
        [alertController addTextFieldWithConfigurationHandler:^(UITextField * _Nonnull textField) {
            usernameField = textField;
            textField.placeholder = NSLocalizedString(@"User", nil);
        }];
        [alertController addTextFieldWithConfigurationHandler:^(UITextField * _Nonnull textField) {
            textField.secureTextEntry = YES;
            textField.placeholder = NSLocalizedString(@"Password", nil);
            passwordField = textField;
        }];

        [alertController addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Login", nil)
                                                            style:UIAlertActionStyleDefault
                                                          handler:^(UIAlertAction * _Nonnull action) {
                                                              NSString *user = usernameField.text;
                                                              NSString *pass = passwordField.text;
                                                              [dict setObject:user ? user : @"" forKey:@"username"];
                                                              [dict setObject:pass ? pass : @"" forKey:@"password"];
                                                              dispatch_semaphore_signal(sema);
                                                          }]];
        [alertController addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Cancel", nil)
                                                            style:UIAlertActionStyleCancel
                                                          handler:^(UIAlertAction * _Nonnull action) {
                                                              dispatch_semaphore_signal(sema);
                                                          }]];

        [[[[UIApplication sharedApplication].delegate.window rootViewController] presentedViewController] presentViewController:alertController animated:YES completion:nil];
    });

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    [dialog release];
    return [dict copy];
#else
    __block NSDictionary *dict;
    __block VLCBlockingAlertView *alert;

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    dispatch_async(dispatch_get_main_queue(), ^{
        alert = [[VLCBlockingAlertView alloc] initWithTitle:dialog[@"title"]
                                                    message:dialog[@"message"]
                                                   delegate:nil
                                          cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
                                          otherButtonTitles:NSLocalizedString(@"Login", nil), nil];
        alert.alertViewStyle = UIAlertViewStyleLoginAndPasswordInput;
        alert.completion = ^(BOOL cancelled, NSInteger buttonIndex) {
            if (!cancelled) {
                NSString *user = [alert textFieldAtIndex:0].text;
                NSString *pass = [alert textFieldAtIndex:1].text;
                dict = [[NSDictionary dictionaryWithObjectsAndKeys:
                         user, @"username",
                         pass, @"password",
                         nil] retain];
            }

            dispatch_semaphore_signal(sema);
        };
        alert.delegate = alert;
        [alert show];
    });

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

    [alert release];
    [dialog release];
    return dict;
#endif
}

- (void)displayProgressBar:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();
#ifndef NDEBUG
    NSLog(@"%@", dialog);
#endif
}

- (void)updateProgressPanel:(NSDictionary *)dict
{
    VLCAssertIsMainThread();
#ifndef NDEBUG
    NSLog(@"%@", dialog);
#endif
}

- (void)destroyProgressPanel
{
    VLCAssertIsMainThread();
#ifndef NDEBUG
    NSLog(@"%@", dialog);
#endif
}

- (NSNumber *)checkProgressPanel
{
    VLCAssertIsMainThread();

    return @(NO);
}

/**
 * Helper to execute a function on main thread and get its return value.
 */
- (void)execute:(NSDictionary *)dict
{
    SEL sel = [dict[@"sel"] pointerValue];
    id *result = [dict[@"result"] pointerValue];
    id object = dict[@"object"];

    NSAssert(sel, @"Try to execute a NULL selector");

    *result = [self performSelector:sel withObject:object];
    [*result retain]; // Balanced in -resultFromSelectorOnMainThread
}

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object
{
    id result = nil;
    NSAssert(sel, @"Try to execute a NULL selector");
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSValue valueWithPointer:sel], @"sel",
                          [NSValue valueWithPointer:&result], @"result",
                          object, @"object", nil];
    [self performSelectorOnMainThread:@selector(execute:) withObject:dict waitUntilDone:YES];
    return [result autorelease];
}

@end

#if !TARGET_OS_TV
@implementation VLCBlockingAlertView

- (id)initWithTitle:(NSString *)title
            message:(NSString *)message
  cancelButtonTitle:(NSString *)cancelButtonTitle
  otherButtonTitles:(NSArray *)otherButtonTitles
{
    self = [self initWithTitle:title
                       message:message
                      delegate:self
             cancelButtonTitle:cancelButtonTitle
             otherButtonTitles:nil];

    if (self) {
        for (NSString *buttonTitle in otherButtonTitles)
            [self addButtonWithTitle:buttonTitle];
    }
    return self;
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    if (self.completion) {
        self.completion(buttonIndex == self.cancelButtonIndex, buttonIndex);
        self.completion = nil;
    }
}
@end

#endif
