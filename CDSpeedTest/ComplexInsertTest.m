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
#define SONG_COUNT (100000)
#define SONGS_PER_LIST (100)



int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int playlistCount = SONG_COUNT/SONGS_PER_LIST;

    NSLog(@"CoreData: Inserting %d playlists, %d songs, %d songs per playlist",
          playlistCount, SONG_COUNT, SONGS_PER_LIST);
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
    
    NSMutableArray *playlists = [[NSMutableArray alloc] initWithCapacity:playlistCount];
    
    for (int i = 0; i < playlistCount; i++) {
        NSManagedObject *playlist = [NSEntityDescription insertNewObjectForEntityForName:@"Playlist"
                                                              inManagedObjectContext:context];
        [playlist setValue:@"Test Playlist" forKey:@"title"];
        
        int startIndex = i * SONGS_PER_LIST;
        for (int j = 0; j < SONGS_PER_LIST; j++) {
            NSManagedObject *playage = [NSEntityDescription insertNewObjectForEntityForName:@"Playage"
                                                                      inManagedObjectContext:context];
            [playage setValue:[NSNumber numberWithInt:j] forKey:@"orderIndex"];
            
            NSManagedObject *s = [songs objectAtIndex:startIndex + j];
            [playage setValue:s forKey:@"song"];
            
            [[playlist mutableSetValueForKey:@"playages"] addObject:playage];
        }
        [playlists addObject:playlist];
        [playlist release];
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
    
	NSString *path = @"CDComplex.mom";
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
	
	NSURL *url = [NSURL fileURLWithPath:@"/tmp/complex.sql"];
    
	NSError *error;
    NSPersistentStore *newStore = [coordinator addPersistentStoreWithType:STORE_TYPE configuration:nil URL:url options:nil error:&error];
    
    if (newStore == nil) {
        NSLog(@"Store Configuration Failure\n%@",
              ([error localizedDescription] != nil) ?
              [error localizedDescription] : @"Unknown Error");
    }
    
    return context;
}

