#import <Foundation/Foundation.h>
#import "Song.h"
#import "Playlist.h"
#import "BNRStore.h"
#import "BNRTCBackend.h"

#import <mach/mach_time.h>

#define SONG_COUNT (100000)
#define SONGS_PER_LIST (100)

int main (int argc, const char * argv[]) {
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int playlistCount = SONG_COUNT/SONGS_PER_LIST;

    uint64_t start = mach_absolute_time();
    
    NSError *error;
    NSString *path = @"/tmp/complextest/";
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
    [store addClass:[Playlist class]];

    // Get all the playlists
    NSArray *allPlaylists = [store allObjectsForClass:[Playlist class]];
    NSLog(@"allPlaylists has %d playlists", [allPlaylists count]);
    
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
    uint64_t elapsed = end - start;
    
    mach_timebase_info_data_t info;
    if (mach_timebase_info (&info) != KERN_SUCCESS) {
        printf ("mach_timebase_info failed\n");
    }
    
    uint64_t nanosecs = elapsed * info.numer / info.denom;
    uint64_t millisecs = nanosecs / 1000000;

    fprintf(stderr, "Elapsed time: %lu\n", (long unsigned int)millisecs); 
    
    return 0;
}
