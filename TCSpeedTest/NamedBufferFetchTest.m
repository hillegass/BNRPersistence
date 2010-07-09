#import "SpeedTest.h"

#define NAMED_SONGS (10)

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"%s: Fetching named songs", getprogname());

    uint64_t start = mach_absolute_time();

    BNRStore *store = CreateStoreAtPath(@NAMEDBUFFER_PATH);
    if (!store) exit(EXIT_FAILURE);
    [store addClass:[Song class]];

    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:NAMED_SONGS];
    
    BNRStoreBackend *backend = [store backend];

    NSSet *allNames = [backend allNames];
    
    for (NSString *name in allNames) {
        BNRDataBuffer *buffer = [backend dataBufferForName:name];
        Song *song = [buffer readObjectReferenceOfClass:[Song class]
                                             usingStore:store];
        NSLog(@"%@ -> %@", name, song);
    }
        
    // Interestingly, it is quicker to release the store before the StoredObjects
    [store release];
    [songs release];
    
    [pool drain];

    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
