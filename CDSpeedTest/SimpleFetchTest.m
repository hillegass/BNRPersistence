//
//  CDSpeedTest.m
//  CDSpeedTest
//
//  Created by Aaron Hillegass on 1/23/10.
//  Copyright Big Nerd Ranch 2010 . All rights reserved.
//

#import <mach/mach_time.h>
#import "SpeedTest.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
	
	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDSimple");

    NSFetchRequest *fr = [[NSFetchRequest alloc] init];
    NSEntityDescription *ed = [NSEntityDescription entityForName:@"Song"
                                          inManagedObjectContext:context];
    [fr setEntity:ed];
    NSArray *allSongs = [context executeFetchRequest:fr
                                               error:NULL];

    NSLog(@"allSongs has %d songs", [allSongs count]);
    [fr release];
    
    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    
    return 0;
}
