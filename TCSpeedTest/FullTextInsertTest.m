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
    
    FILE *fileHandle = fopen("eopub1m.txt", "r");
    
    char orgNameBuffer[107];
    
    while (fread(orgNameBuffer, 1, 106, fileHandle) == 106) {
        orgNameBuffer[106] = '\0';
        NSMutableString *orgName = [[NSMutableString alloc] initWithCString:orgNameBuffer
                                                                   encoding:NSASCIIStringEncoding];
        CFStringTrimWhitespace((CFMutableStringRef)orgName);
   
        fseek(fileHandle, 42, SEEK_CUR);
        
        Song *song = [[Song alloc] init];
        [song setTitle:orgName];
        [orgName release];
        [store insertObject:song];
        [song release];
        
    }
     
    uint64_t start = mach_absolute_time();

    NSError *error = nil;
    BOOL success = [store saveChanges:&error];
    if (!success) {
        NSLog(@"error = %@", [error localizedDescription]);
        return EXIT_FAILURE;
    }
    [store release];
    
    [pool drain];
    
    uint64_t end = mach_absolute_time();
    LogElapsedTime(start, end);
    return EXIT_SUCCESS;
}
