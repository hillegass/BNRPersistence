#import "SpeedTest.h"

int main (int argc, const char * argv[]) {
	// Note that the elapsed time is affected by the size of the file stored in /var/complextest (@COMPLEXTEST_PATH).
	// If many (dozens of) tests runs are completed without deleting it, tests will begin to slow, 
	// with the time being spent in TC calls. A benchmark with millions of objects in the store,
	// like FullTextInsertTest, may be useful.
	
#define kNumRepetitions 10
	for (int i = 0; i < kNumRepetitions; i++) {


    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int playlistCount = PLAYLIST_COUNT;

	NSLog(@"Using %@\n", kUseBNRResizableUniquingTable ? @"BNRResizableUniquingTable" : @"BNRUniquingTable");
    NSLog(@"%s: Inserting %d playlists, %d songs, %d songs per playlist",
          getprogname(), playlistCount, SONG_COUNT, SONGS_PER_LIST);

    uint64_t start = mach_absolute_time();

    BNRStore *store = CreateStoreAtPath(@COMPLEXTEST_PATH);
    if (!store) exit(EXIT_FAILURE);
    [store addClass:[Song class]];
    [store addClass:[Playlist class]];

    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:SONG_COUNT];
    
    for (int i = 0; i < SONG_COUNT; i++) {
        Song *song = [[Song alloc] init];
        [song setTitle:[NSString stringWithFormat:@"Test Song %d", i]];
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

    NSError *error = nil;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return EXIT_FAILURE;
    }
    
	
	// uncomment to get get a sense of the time taken to release everything vs. time to build it.
	//uint64_t middle = mach_absolute_time();
    //LogElapsedTime(start, middle);

    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    [playlists release];    
    
    [pool drain];

    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
	}
    return EXIT_SUCCESS;
}
