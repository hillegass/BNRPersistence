//
//  IntDictTests.m
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/23/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "IntDictTests.h"
#import "BNRIntDictionary.h"

@implementation IntDictTests

- (void)testCase1
{
    BNRIntDictionary *dict = [[BNRIntDictionary alloc] init];
    [dict setObject:@"Hey" forInt:9000];
    [dict setObject:@"Go" forInt:9001];
    [dict setObject:@"Hello" forInt:9000];
    [dict setObject:@"Test" forInt:0];
    [dict setObject:@"Well" forInt:1];
    
    NSString *result = [dict objectForInt:9000];
    STAssertEqualObjects(result, @"Hello", @"Should have replaced it");
    
    result = [dict objectForInt:12000];
    STAssertNil(result, @"Should be nil");

}
@end
