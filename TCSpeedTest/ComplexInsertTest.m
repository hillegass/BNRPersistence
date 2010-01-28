#import <Foundation/Foundation.h>
#import "Song.h"
#import "Playlist.h"
#import "BNRStore.h"
#import "BNRTCBackend.h"

#import <mach/mach_time.h>
#import <mach/mach_error.h>

#define SONG_COUNT (100000)
#define SONGS_PER_LIST (100)

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int playlistCount = SONG_COUNT/SONGS_PER_LIST;

    NSLog(@"%s: Inserting %d playlists, %d songs, %d songs per playlist",
          getprogname(), playlistCount, SONG_COUNT, SONGS_PER_LIST);

    uint64_t start = mach_absolute_time();

    NSError *error = nil;
    NSString *path = @"/tmp/complextest/";
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"%s: Unable to create database at %@: %@",
              getprogname(), path, error);
        exit(EXIT_FAILURE);
    }
    
    BNRStore *store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    
    [store addClass:[Song class] expectedCount:SONG_COUNT];
    [store addClass:[Playlist class] expectedCount:playlistCount];

    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
        Song *song = [[Song alloc] init];
        [song setTitle:@"Test Song"];
        [song setSeconds:i];
        [songs addObject:song];
        [store insertObject:song];
        [song release];
    }
    
    NSMutableArray *playlists = [[NSMutableArray alloc] initWithCapacity:playlistCount];
    
    for (int i = 0; i < playlistCount; i++) {
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
    
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return -1;
    }
    
    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    [playlists release];    
    
    [pool drain];

    uint64_t end = mach_absolute_time();
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    mach_error_t merr = mach_timebase_info (&info);
    if (KERN_SUCCESS != merr) {
        fprintf(stderr, "%s: mach_timebase_info failed [%s]: %s\n",
                getprogname(), mach_error_type(merr), mach_error_string(merr));
    }
    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;

    fprintf(stderr, "%s: Elapsed time: %lu ms\n",
            getprogname(), (long unsigned int)millisecs); 
    return EXIT_SUCCESS;
}
