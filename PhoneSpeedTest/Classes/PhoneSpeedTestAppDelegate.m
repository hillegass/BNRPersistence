//
//  PhoneSpeedTestAppDelegate.m
//  PhoneSpeedTest
//
//  Created by Aaron Hillegass on 1/26/10.
//  Copyright Big Nerd Ranch 2010. All rights reserved.
//
#import <mach/mach_time.h>
#import <CoreData/CoreData.h>
#import "PhoneSpeedTestAppDelegate.h"
#import "BNRTCBackend.h"
#import "BNRStore.h"
#import "Song.h"
#import "Playlist.h"

#define SONG_COUNT (100000)
#define SONGS_PER_LIST (100)
#define PLAYLIST_COUNT ((SONG_COUNT)/(SONGS_PER_LIST))


NSString *docPathWithName(NSString *name)
{
    NSArray *documentDirectories = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentDirectory = [documentDirectories objectAtIndex:0];
    return [documentDirectory stringByAppendingPathComponent:name];
}

NSManagedObjectModel *managedObjectModel(NSString *testName ) {
    
    NSString *path = [[NSBundle mainBundle] pathForResource:testName
                                                     ofType:@"mom"];
    
	NSURL *modelURL = [NSURL fileURLWithPath:path];
    NSManagedObjectModel *result = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    if (!result) {
        NSLog(@"unable to create model");
    }
    
    return result;
}

NSManagedObjectContext *managedObjectContext(NSString *testName) {
	
    NSManagedObjectContext * context = [[NSManagedObjectContext alloc] init];
    NSManagedObjectModel *model = managedObjectModel(testName);
    NSPersistentStoreCoordinator *coordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:model];
    [model release];
    [context setPersistentStoreCoordinator:coordinator];
    [coordinator release];
    
    NSString *STORE_TYPE = NSSQLiteStoreType;
	
    NSString *path = docPathWithName(testName);
    
	NSURL *url = [NSURL fileURLWithPath:path];
    
	NSError *error;
    NSPersistentStore *newStore = [coordinator addPersistentStoreWithType:STORE_TYPE configuration:nil URL:url options:nil error:&error];
    
    if (newStore == nil) {
        NSLog(@"Store Configuration Failure\n%@",
              ([error localizedDescription] != nil) ?
              [error localizedDescription] : @"Unknown Error");
    }
    
    return context;
}


@implementation PhoneSpeedTestAppDelegate

@synthesize window;


- (void)applicationDidFinishLaunching:(UIApplication *)application {    

    // Override point for customization after application launch
    [window makeKeyAndVisible];
}


- (void)dealloc {
    [window release];
    [super dealloc];
}

- (void)showLapsedTime:(uint64_t)start to:(uint64_t)stop
{    
    uint64_t elapsed = stop - start;
    
    mach_timebase_info_data_t info;
    mach_timebase_info (&info);    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    
    NSString *labelString = [NSString stringWithFormat:@"%lu ms", (long unsigned int)millisecs];
    [label setText:labelString];
}



- (BNRStore *)newStoreWithPath:(NSString *)path
{
    NSError *error;
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"Unable to create database at %@", path);
        exit(-1);
    }
    
    BNRStore *store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    return store;
}
    
    
- (IBAction)simpleInsertTC:(id)sender
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"BNRPersistence: Inserting %d songs",SONG_COUNT);
    
    uint64_t start = mach_absolute_time();
    NSString *path = docPathWithName(@"TCSimple");
    BNRStore *store = [self newStoreWithPath:path];
    
    [store addClass:[Song class]];
    
    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
        Song *song = [[Song alloc] init];
        [song setTitle:@"Test Song"];
        [song setSeconds:i];
        [songs addObject:song];
        [store insertObject:song];
        [song release];
    }
    
    NSError *error;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return;
    }
    
    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    
    [pool drain];
    
    uint64_t stop = mach_absolute_time();
    [self showLapsedTime:start to:stop];
}

- (IBAction)simpleInsertCD:(id)sender
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"CoreData: Inserting %d songs",SONG_COUNT);
    uint64_t start = mach_absolute_time();

	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDSimple");
    
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
    
    uint64_t stop = mach_absolute_time();
    [self showLapsedTime:start to:stop];
}

- (IBAction)complexInsertTC:(id)sender{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"BNRPersistence: Inserting %d playlists, %d songs, %d songs per playlist",
          PLAYLIST_COUNT, SONG_COUNT, SONGS_PER_LIST);
    
    uint64_t start = mach_absolute_time();
    
    NSString *path = docPathWithName(@"TCComplex");
    BNRStore *store = [self newStoreWithPath:path];
        
    [store addClass:[Song class]];
    [store addClass:[Playlist class]];
    
    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
        Song *song = [[Song alloc] init];
        [song setTitle:@"Test Song"];
        [song setSeconds:i];
        [songs addObject:song];
        [store insertObject:song];
        [song release];
    }
    
    NSMutableArray *playlists = [[NSMutableArray alloc] initWithCapacity:PLAYLIST_COUNT];
    
    for (int i = 0; i < PLAYLIST_COUNT; i++) {
        Playlist *playlist = [[Playlist alloc] init];
        [playlist setTitle:@"Test Playlist"];
        
        int startIndex = i * SONGS_PER_LIST;
        for (int j = 0; j < SONGS_PER_LIST; j++) {
            Song *s = [songs objectAtIndex:startIndex + j];
            [playlist insertObject:s inSongsAtIndex:j];
        }
        [playlists addObject:playlist];
        [store insertObject:playlist];
        [playlist release];
    }
    
    NSError *error = nil;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return;
    }
    
    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    [playlists release];    
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    [self showLapsedTime:start to:end];
}



- (IBAction)complexInsertCD:(id)sender
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"CoreData: Inserting %d playlists, %d songs, %d songs per playlist",
          PLAYLIST_COUNT, SONG_COUNT, SONGS_PER_LIST);
    uint64_t start = mach_absolute_time();
	
	// Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDComplex");
    
    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
        NSManagedObject *song = [NSEntityDescription insertNewObjectForEntityForName:@"Song"
                                                              inManagedObjectContext:context];
        [song setValue:@"Test Song" forKey:@"title"];
        [song setValue:[NSNumber numberWithInt:i] forKey:@"seconds"];
        [songs addObject:song];
    }
    
    NSMutableArray *playlists = [[NSMutableArray alloc] initWithCapacity:PLAYLIST_COUNT];
    
    for (int i = 0; i < PLAYLIST_COUNT; i++) {
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
    [self showLapsedTime:start to:end];    
}
- (IBAction)simpleFetchTC:(id)sender{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
    NSString *path = docPathWithName(@"TCSimple");
    BNRStore *store = [self newStoreWithPath:path];
        
    [store addClass:[Song class]];
    
    // Get all the song
    NSArray *allSongs = [store allObjectsForClass:[Song class]];
    NSLog(@"allSongs has %d songs", [allSongs count]);
    
    [store release];
    
    [pool drain];
    uint64_t stop = mach_absolute_time();
    [self showLapsedTime:start to:stop];
}

- (IBAction)simpleFetchCD:(id)sender{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
    // Create the managed object context
    NSManagedObjectContext *context = managedObjectContext(@"CDSimple");

	// Create the managed object context    
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
    
    uint64_t stop = mach_absolute_time();
    [self showLapsedTime:start to:stop];
}
- (IBAction)complexFetchTC:(id)sender{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
    
    NSString *path = docPathWithName(@"ComplexTC");
    BNRStore *store = [self newStoreWithPath:path];
    
    [store addClass:[Song class]];
    [store addClass:[Playlist class]];
    
    // Get all the playlists
    NSArray *allPlaylists = [store allObjectsForClass:[Playlist class]];
    NSLog(@"%s: allPlaylists has %lu playlists",
          getprogname(), (unsigned long)[allPlaylists count]);
    
    // Get the title of the first song in each playlist
    NSMutableArray *titles = [[NSMutableArray alloc] init];
    for (Playlist *p in allPlaylists) {
        Song *song = [[p songs] objectAtIndex:0];
        [titles addObject:[song title]];
    }
    [store release];
    
    [titles release];
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    [self showLapsedTime:start to:end];
}
- (IBAction)complexFetchCD:(id)sender
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
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
        NSArray *playagesAsArray = [playages allObjects];
        NSArray *sortedArray = [playagesAsArray sortedArrayUsingDescriptors:sortArray];
        NSManagedObject *song = [[sortedArray objectAtIndex:0] valueForKey:@"song"];
        NSString *title = [song valueForKey:@"title"];
        
        [titles addObject:title];
    }
    [titles release];
    [context release];
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    [self showLapsedTime:start to:end];

}
- (IBAction)clearFiles:(id)sender{ 
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *path;
    path = docPathWithName(@"TCComplex");
    [fm removeItemAtPath:path
                   error:NULL];
    path = docPathWithName(@"TCSimple");
    [fm removeItemAtPath:path
                   error:NULL];
    
    path = docPathWithName(@"CDComplex");
    [fm removeItemAtPath:path
                   error:NULL];

    path = docPathWithName(@"CDSimple");
    [fm removeItemAtPath:path
                   error:NULL];
    
    [label setText:@"Files Deleted"];

}


@end
