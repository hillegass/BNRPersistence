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
    NSManagedObjectContext *context = managedObjectContext(@"CDText");
    
    NSFetchRequest *fr = [[NSFetchRequest alloc] init];
    NSEntityDescription *ed = [NSEntityDescription entityForName:@"Song"
                                          inManagedObjectContext:context];
    [fr setEntity:ed];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"title contains[c] 'choice'"];
    [fr setPredicate:predicate];
    
    NSArray *songsThatMatch = [context executeFetchRequest:fr
                                               error:NULL];
        
    for (NSManagedObject *song in songsThatMatch) {
        [song valueForKey:@"title"];
    }
    NSLog(@"%d songs", [songsThatMatch count]);
        
    uint64_t end = mach_absolute_time();

    [context release];
    [pool drain];
    
    LogElapsedTime(start, end);
    return 0;
}

