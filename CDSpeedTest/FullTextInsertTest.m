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
    
    FILE *fileHandle = fopen("eopub1m.txt", "r");
    
    char orgNameBuffer[107];
    
    while (fread(orgNameBuffer, 1, 106, fileHandle) == 106) {
        orgNameBuffer[106] = '\0';
        NSMutableString *orgName = [[NSMutableString alloc] initWithCString:orgNameBuffer
                                                                   encoding:NSASCIIStringEncoding];
        CFStringTrimWhitespace((CFMutableStringRef)orgName);
        
        fseek(fileHandle, 42, SEEK_CUR);
        
        NSManagedObject *song = [NSEntityDescription insertNewObjectForEntityForName:@"Song"
                                                              inManagedObjectContext:context];
        [song setValue:orgName forKey:@"title"];
        [orgName release];        
    }
    
    uint64_t start = mach_absolute_time();

	// Save the managed object context
    NSError *error = nil;    
    if (![context save:&error]) {
        NSLog(@"Error while saving\n%@",
              ([error localizedDescription] != nil) ? [error localizedDescription] : @"Unknown Error");
        exit(1);
    }
    
    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return 0;
}

