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


	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDText");
    
    uint64_t start = mach_absolute_time();

    NSFetchRequest *fr = [[NSFetchRequest alloc] init];
    NSEntityDescription *ed = [NSEntityDescription entityForName:@"Song"
                                          inManagedObjectContext:context];
    [fr setEntity:ed];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"title contains[c] 'choice'"];
    [fr setPredicate:predicate];
    
    NSArray *songsThatMatch = [context executeFetchRequest:fr
                                               error:NULL];
        
    NSLog(@"%d songs contain the string 'choice'", [songsThatMatch count]);
    
    predicate = [NSPredicate predicateWithFormat:@"title contains[c] 'brain'"];
    [fr setPredicate:predicate];
    songsThatMatch = [context executeFetchRequest:fr
                                            error:NULL];
    
    NSLog(@"%d songs contain the string 'brain'", [songsThatMatch count]);

    predicate = [NSPredicate predicateWithFormat:@"title contains[c] 'pets'"];
    [fr setPredicate:predicate];
    songsThatMatch = [context executeFetchRequest:fr
                                            error:NULL];
    
    NSLog(@"%d songs contain the string 'pets'", [songsThatMatch count]);
    
        
    uint64_t end = mach_absolute_time();

    [context release];
    [pool drain];
    
    LogElapsedTime(start, end);
    return 0;
}

