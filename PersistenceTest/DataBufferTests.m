#import "DataBufferTests.h"
#import "BNRDataBuffer.h"

@implementation DataBufferTests

- (void)testString
{
    BNRDataBuffer *db = [[BNRDataBuffer alloc] initWithCapacity:1024];
    NSString *origString = @"This freakin' string better stay the same.";
    [db writeString:origString];
    [db resetCursor];
    NSString *nString = [db readString];
    STAssertEqualObjects(origString, nString, @"read != written");
}

- (void)testUInt32
{
    BNRDataBuffer *db = [[BNRDataBuffer alloc] initWithCapacity:1024];
    UInt32 orig = random();
    [db writeUInt32:orig];
    [db resetCursor];
    UInt32 readInt = [db readUInt32];
    STAssertEquals(orig, readInt, @"read != written");
}
@end
