//
//  PXSourceListRuntimeAdditions.h
//  PXSourceList
//
//  Created by Alex Rozanski on 25/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

extern NSString * const px_protocolMethodNameKey;
extern NSString * const px_protocolMethodArgumentTypesKey;
extern NSString * const px_protocolIsRequiredMethodKey;

NSArray *px_allProtocolMethods(Protocol *protocol);
NSArray *px_methodNamesForProtocol(Protocol *protocol);
id px_methodNameForSelector(SEL selector);

struct objc_method_description px_methodDescriptionForProtocolMethod(Protocol *protocol, SEL selector);