#import <Foundation/Foundation.h>
#import "Song.h"
#import "Playlist.h"
#import "BNRStore.h"
#import "BNRTCBackend.h"

#import <mach/mach_time.h>
#import <mach/mach_error.h>

#define SONG_COUNT (1000000)

int main (int argc, const char * argv[]) {    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"%s: %d songs", getprogname(), SONG_COUNT);

    uint64_t start = mach_absolute_time();

    NSError *error;
    NSString *path = @"/tmp/simpletest/";
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
        NSLog(@"%s: Error saving changes: %@",
              getprogname(), [error localizedDescription]);
        return -1;
    }

    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];

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
