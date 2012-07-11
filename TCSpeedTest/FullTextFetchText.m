#import "SpeedTest.h"
#import "Song.h"
#import "BNRTCIndexManager.h"

// Uses http://www.irs.gov/pub/irs-utl/eopub78.zip

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    BNRStore *store = CreateStoreAtPath(@TEXTTEST_PATH);
    if (!store) exit(EXIT_FAILURE);
    
	// For fault tolerance freaks, pass YES for useWriteSyncronization.
	// Text index will be nearly impervious to power outages/crashes, but bulk 
	// inserts/updates will be much slower. For best of both worlds, 
	// close index and reopen with write sync off before bulk operations.
	//
	// Size freaks, pass YES for compressIndexFiles to get transparent compression.
	// The default size or a new or sparesly-populated index will drop from ~8.5MB to ~650K. 
	// Very large index files may be slower to open; need a speed test for that.
	// Note that if TC is asked to access a compressed index, it will always decompress
	// if necessary, but will only compress-on-save if asked via this flag.
	BOOL writeSyncFlag = NO;
	BOOL indexCompressionFlag = YES;
    NSError *err;
    BNRTCIndexManager *indexManager = [[BNRTCIndexManager alloc] initWithPath:@TEXTTEST_PATH
													   useWriteSyncronization:writeSyncFlag
														   compressIndexFiles:indexCompressionFlag
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
    //   for (Song *song in songsThatMatch) {
    //        [song title];
    //    }
    NSLog(@"%d song titles contain 'choice'", [songsThatMatch count]);
    
    songsThatMatch = [store objectsForClass:[Song class]
                               matchingText:@"[[*brain*]]"
                                     forKey:@"title"];
    NSLog(@"%d song titles contain 'brain'", [songsThatMatch count]);
    
    songsThatMatch = [store objectsForClass:[Song class]
                               matchingText:@"[[*pets*]]"
                                     forKey:@"title"];
    NSLog(@"%d song titles contain 'pets'", [songsThatMatch count]);
    
    uint64_t end = mach_absolute_time();
    
    
    [store release];
    
    [pool drain];
    
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
