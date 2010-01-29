#import "SpeedTest.h"

int main (int argc, const char * argv[]) {    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"%s: %d songs", getprogname(), SONG_COUNT);

    uint64_t start = mach_absolute_time();
    
    BNRStore *store = CreateStoreAtPath(@SIMPLETEST_PATH);
    if (!store) exit(EXIT_FAILURE);

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

    NSError *error = nil;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"%s: Error saving changes: %@",
              getprogname(), [error localizedDescription]);
        return EXIT_FAILURE;
    }

    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];

    [pool drain];

    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
