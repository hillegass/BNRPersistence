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
#define SONG_COUNT (1000000)



int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"CoreData: Inserting %d songs",SONG_COUNT);
    uint64_t start = mach_absolute_time();
	
	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext();
    
    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
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

