#import "SpeedTest.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    uint64_t start = mach_absolute_time();


    BNRStore *store = CreateStoreAtPath(@SIMPLETEST_PATH);
    if (!store) exit(EXIT_FAILURE);

    [store addClass:[Song class]];

    // Get all the songs
    NSArray *allSongs = [store allObjectsForClass:[Song class]];
    NSLog(@"%s: allSongs has %lu songs",
          getprogname(), (unsigned long)[allSongs count]);

    [store release];
    [pool drain];

    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
