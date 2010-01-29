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
	
    NSArray *documentDirectories = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentDirectory = [documentDirectories objectAtIndex:0];
    NSString *path = [documentDirectory stringByAppendingPathComponent:testName];
    
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

- (NSString *)simpleTCPath
{
    NSArray *documentDirectories = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentDirectory = [documentDirectories objectAtIndex:0];
    return [documentDirectory stringByAppendingPathComponent:@"SimpleTC"];
}
    
    
- (IBAction)simpleInsertTC:(id)sender
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"BNRPersistence: Inserting %d songs",SONG_COUNT);
    
    uint64_t start = mach_absolute_time();
    NSString *path = [self simpleTCPath];

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
    
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return;
    }
    
    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    mach_timebase_info (&info);    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    
    NSString *labelString = [NSString stringWithFormat:@"%lu ms", (long unsigned int)millisecs];
    [label setText:labelString];
}

- (IBAction)simpleInsertCD:(id)sender
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
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
    
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    mach_timebase_info (&info);    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    NSString *labelString = [NSString stringWithFormat:@"%lu ms", (long unsigned int)millisecs];
    [label setText:labelString];
}

- (IBAction)complexInsertTC:(id)sender{ }
- (IBAction)complexInsertCD:(id)sender{ }
- (IBAction)simpleFetchTC:(id)sender{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    uint64_t start = mach_absolute_time();
    NSString *path = [self simpleTCPath];

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
    
    [store addClass:[Song class]];
    
    // Get all the song
    NSArray *allSongs = [store allObjectsForClass:[Song class]];
    NSLog(@"allSongs has %d songs", [allSongs count]);
    
    [store release];
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    if (mach_timebase_info (&info) != KERN_SUCCESS) {
        printf ("mach_timebase_info failed\n");
    }
    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    NSString *labelString = [NSString stringWithFormat:@"%lu ms", (long unsigned int)millisecs];
    [label setText:labelString];
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
    
    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    mach_timebase_info (&info);    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;
    NSString *labelString = [NSString stringWithFormat:@"%lu ms", (long unsigned int)millisecs];
    [label setText:labelString];
}
- (IBAction)complexFetchTC:(id)sender{ }
- (IBAction)complexFetchCD:(id)sender{ }
- (IBAction)clearFiles:(id)sender{ }


@end
