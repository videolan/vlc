//
//  PXSourceListRuntimeAdditions.m
//  PXSourceList
//
//  Created by Alex Rozanski on 25/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceListRuntimeAdditions.h"

NSString * const px_protocolMethodNameKey = @"methodName";
NSString * const px_protocolMethodArgumentTypesKey = @"types";
NSString * const px_protocolIsRequiredMethodKey = @"isRequired";

NSArray *px_allProtocolMethods(Protocol *protocol)
{
    NSMutableArray *methodList = [[NSMutableArray alloc] init];

    // We have 4 permutations as protocol_copyMethodDescriptionList() takes two BOOL arguments for the types of methods to return.
    for (NSUInteger i = 0; i < 4; ++i) {
        BOOL isRequiredMethod = (i / 2) % 2;

        unsigned int numberOfMethodDescriptions = 0;
        struct objc_method_description *methodDescriptions = protocol_copyMethodDescriptionList(protocol, isRequiredMethod, i % 2, &numberOfMethodDescriptions);

        for (unsigned int j = 0; j < numberOfMethodDescriptions; ++j) {
            struct objc_method_description methodDescription = methodDescriptions[j];
            [methodList addObject:@{px_protocolMethodNameKey: px_methodNameForSelector(methodDescription.name),
                                    px_protocolMethodArgumentTypesKey: [NSString stringWithUTF8String:methodDescription.types],
                                    px_protocolIsRequiredMethodKey: @(isRequiredMethod)}];
        }

        free(methodDescriptions);
    }

    return methodList;
}

NSArray *px_methodNamesForProtocol(Protocol *protocol)
{
    NSMutableArray *methodNames = [[NSMutableArray alloc] init];

    for (NSDictionary *methodInfo in px_allProtocolMethods(protocol))
        [methodNames addObject:methodInfo[px_protocolMethodNameKey]];

    return methodNames;
}

id px_methodNameForSelector(SEL selector)
{
    return NSStringFromSelector(selector);
}

struct objc_method_description px_methodDescriptionForProtocolMethod(Protocol *protocol, SEL selector)
{
    struct objc_method_description description = {NULL, NULL};

    // We have 4 permutations to check for.
    for (NSUInteger i = 0; i < 4; ++i) {
        description = protocol_getMethodDescription(protocol, selector, (i / 2) % 2, i % 2);
        if (description.types != NULL && description.name != NULL)
            break;
    }

    return description;
}
