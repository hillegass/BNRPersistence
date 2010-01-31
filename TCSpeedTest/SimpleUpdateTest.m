#import "SpeedTest.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];


    BNRStore *store = CreateStoreAtPath(@COMPLEXTEST_PATH);
    if (!store) exit(EXIT_FAILURE);

    [store addClass:[Song class]];
    [store addClass:[Playlist class]];
    
    // Get all the songs
    NSArray *allSongs = [store allObjectsForClass:[Song class]];
    int count = [allSongs count];

    NSLog(@"%s: allSongs has %lu songs",
          getprogname(), (unsigned long)[allSongs count]);
    
    uint64_t start = mach_absolute_time();
    
    for (int i = 0; i < count; i+=3) {
        Song *s = [allSongs objectAtIndex:i]; 
        [store willUpdateObject:s];
        [s setTitle:@"New Song Title"];
    }
    [store saveChanges:NULL];
    
    [store release];

    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
