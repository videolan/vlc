//
//  main.m
//  test
//
//  Created by Pierre d'Herbemont on 13/04/07.
//  Copyright __MyCompanyName__ 2007. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <VLC/VLC.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    /* If we can't load the VLC.framework dyld will tell us anyway
     * But this is cool */
    if(test_vlc_framework() == 0xbabe)
        printf("We are linked to VLC.framework!\n");
    else
    {
        fprintf(stderr, "*** Can't load the VLC.framework\n");
        return -1;
    }
    
    return NSApplicationMain(argc, (const char **) argv);
}
