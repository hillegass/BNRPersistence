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

    NSLog(@"CoreData: Inserting %d songs",BIG_SONG_COUNT);
    uint64_t start = mach_absolute_time();
	
	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDSimple");
    
    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:BIG_SONG_COUNT];
    
    for (int i = 0; i < BIG_SONG_COUNT; i++) {
        NSManagedObject *song = [NSEntityDescription insertNewObjectForEntityForName:@"Song"
                               inManagedObjectContext:context];
        [song setValue:@"Test Song" forKey:@"title"];
        [song setValue:[NSNumber numberWithInt:i] forKey:@"seconds"];
        [songs addObject:song];
    }
    
	// Save the managed object context
    NSError *error = nil;    
    if (![context save:&error]) {
        NSLog(@"Error while saving\n%@",
              ([error localizedDescription] != nil) ? [error localizedDescription] : @"Unknown Error");
        exit(1);
    }
    
    [songs release];

    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return 0;
}

