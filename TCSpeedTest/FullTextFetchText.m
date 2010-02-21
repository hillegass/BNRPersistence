#import "SpeedTest.h"
#import "Song.h"
#import "BNRTCIndexManager.h"

// Uses http://www.irs.gov/pub/irs-utl/eopub78.zip

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    BNRStore *store = CreateStoreAtPath(@TEXTTEST_PATH);
    if (!store) exit(EXIT_FAILURE);

    NSError *err;
    BNRTCIndexManager *indexManager = [[BNRTCIndexManager alloc] initWithPath:@TEXTTEST_PATH
                                                                        error:&err];

    if (!indexManager) {
        NSLog(@"error = %@", [err localizedDescription]);
        exit(EXIT_FAILURE);
    }
    
    [store setIndexManager:indexManager];
    [indexManager release];
    
    [store addClass:[Song class]];
         
    uint64_t start = mach_absolute_time();

    NSMutableArray *songsThatMatch = [store objectsForClass:[Song class]
                                              matchingText:@"[[*choice*]]"
                                                    forKey:@"title"];
    for (Song *song in songsThatMatch) {
        [song title];
    }
    NSLog(@"%d songs fetched", [songsThatMatch count]);
    
    [store release];
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
