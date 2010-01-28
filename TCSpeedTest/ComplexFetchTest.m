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
