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
    NSManagedObjectContext *context = managedObjectContext(@"CDComplex");

    NSFetchRequest *fr = [[NSFetchRequest alloc] init];
    NSEntityDescription *ed = [NSEntityDescription entityForName:@"Playlist"
                                          inManagedObjectContext:context];
    [fr setEntity:ed];
    NSArray *allPlaylists = [context executeFetchRequest:fr
                                               error:NULL];

    NSLog(@"allPlaylists has %d playlists", [allPlaylists count]);
    [fr release];

    NSSortDescriptor *so = [[NSSortDescriptor alloc] initWithKey:@"orderIndex"
                                                       ascending:YES];
    NSArray *sortArray = [NSArray arrayWithObject:so];
    [so release];
    
    NSMutableArray *titles = [[NSMutableArray alloc] init];
    for (NSManagedObject *p in allPlaylists) {
        NSSet *playages = [p valueForKey:@"playages"];
        NSArray *sortedArray = [playages sortedArrayUsingDescriptors:sortArray];
        NSManagedObject *song = [[sortedArray objectAtIndex:0] valueForKey:@"song"];
        NSString *title = [song valueForKey:@"title"];

        [titles addObject:title];
    }
    [titles release];
    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    
    LogElapsedTime(start, end);
    return 0;
}

