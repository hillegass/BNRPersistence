//
//  CDSpeedTest.m
//  CDSpeedTest
//
//  Created by Aaron Hillegass on 1/23/10.
//  Copyright Big Nerd Ranch 2010 . All rights reserved.
//

NSManagedObjectModel *managedObjectModel();
NSManagedObjectContext *managedObjectContext();
#import <mach/mach_time.h>

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
	
	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext();

    NSFetchRequest *fr = [[NSFetchRequest alloc] init];
    NSEntityDescription *ed = [NSEntityDescription entityForName:@"Song"
                                          inManagedObjectContext:context];
    [fr setEntity:ed];
    NSArray *allSongs = [context executeFetchRequest:fr
                                               error:NULL];

    NSLog(@"allSongs has %d songs", [allSongs count]);
    [fr release];
    
    
//    NSMutableArray *titles = [[NSMutableArray alloc] init];
//    for (NSManagedObject *p in allPlaylists) {
//        NSSet *playages = [p valueForKey:@"playages"];
//        NSArray *sortedArray = [playages sortedArrayUsingDescriptors:sortArray];
//        NSManagedObject *song = [[sortedArray objectAtIndex:0] valueForKey:@"song"];
//        NSString *title = [song valueForKey:@"title"];
//
//        [titles addObject:title];
//    }
//    [titles release];
    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    mach_timebase_info (&info);    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    fprintf(stderr, "Elapsed time: %lu\n", (long unsigned int)millisecs); 
    
    
    return 0;
}



NSManagedObjectModel *managedObjectModel() {
    
    static NSManagedObjectModel *model = nil;
    
    if (model != nil) {
        return model;
    }
    
	NSString *path = @"CDSimple.mom";
	NSURL *modelURL = [NSURL fileURLWithPath:path];
    model = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    if (!model) {
        NSLog(@"unable to create model");
    }

    return model;
}



NSManagedObjectContext *managedObjectContext() {
	
    static NSManagedObjectContext *context = nil;
    if (context != nil) {
        return context;
    }
    
    context = [[NSManagedObjectContext alloc] init];
    NSManagedObjectModel *model = managedObjectModel();
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    [model release];
    [context setPersistentStoreCoordinator: coordinator];
    [coordinator release];
    
    NSString *STORE_TYPE = NSSQLiteStoreType;
	
	NSURL *url = [NSURL fileURLWithPath:@"/tmp/simple.sql"];
    
	NSError *error;
    NSPersistentStore *newStore = [coordinator addPersistentStoreWithType:STORE_TYPE configuration:nil URL:url options:nil error:&error];
    
    if (newStore == nil) {
        NSLog(@"Store Configuration Failure\n%@",
              ([error localizedDescription] != nil) ?
              [error localizedDescription] : @"Unknown Error");
    }
    
    return context;
}

