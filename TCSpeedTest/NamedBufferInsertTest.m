#import "SpeedTest.h"
#import "BNRStoreBackend.h"

#define NAMED_SONGS (10)

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"%s: Inserting %d named songs",
          getprogname(), NAMED_SONGS);

    uint64_t start = mach_absolute_time();

    BNRStore *store = CreateStoreAtPath(@NAMEDBUFFER_PATH);
    if (!store) exit(EXIT_FAILURE);
    [store addClass:[Song class]];

    NSMutableArray *songs = [[NSMutableArray alloc] initWithCapacity:NAMED_SONGS];
    
    BNRDataBuffer *dataBuffer = [[BNRDataBuffer alloc] initWithCapacity:128];
    BNRStoreBackend *backend = [store backend];

    for (int i = 0; i < NAMED_SONGS; i++) {
        Song *song = [[Song alloc] init];
        [song setTitle:[NSString stringWithFormat:@"Untitled %d", i]];
        [song setSeconds:i];
        [songs addObject:song];
        [store insertObject:song];
        [song release];
        
        NSString *key = [NSString stringWithFormat:@"Favorite:%d", i];
        [dataBuffer writeObjectReference:song];
        [backend insertDataBuffer:dataBuffer forName:key];
        NSLog(@"%@ is %@", song, key);
        [dataBuffer clearBuffer];
    }
    
 
    NSError *error = nil;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
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
