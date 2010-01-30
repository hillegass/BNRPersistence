//
//  CDSpeedTest.m
//  CDSpeedTest
//
//  Created by Aaron Hillegass on 1/23/10.
//  Copyright Big Nerd Ranch 2010 . All rights reserved.
//

#import "SpeedTest.h"
#import <mach/mach_time.h>

int main (int argc, const char * argv[]) {
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
    LogElapsedTime(start, end);

    return 0;
}


