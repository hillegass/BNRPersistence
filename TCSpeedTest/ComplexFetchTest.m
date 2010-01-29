#import "SpeedTest.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int playlistCount = PLAYLIST_COUNT;

    uint64_t start = mach_absolute_time();

    BNRStore *store = CreateStoreAtPath(@COMPLEXTEST_PATH);
    if (!store) exit(EXIT_FAILURE);

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
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
