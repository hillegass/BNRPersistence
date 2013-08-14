//
//  IntegrationTests.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "Person.h"
#import "BNRTCBackend.h"
#import "BNRStore.h"
#import "BNRDataBuffer.h"

@interface IntegrationTests : SenTestCase {
    BNRStore *store;
}
- (void)reopen;
@end

NSString * const IntegrationTestsDatabasePath = @"./database/";
NSString * const TestPersonName = @"Fred";
NSString * const TestKey = @"howdy";

@implementation IntegrationTests

- (void)setUp
{
    [[NSFileManager defaultManager] removeItemAtPath:IntegrationTestsDatabasePath error:nil]; // erase in case we were debugging and uncerimoniously exited
    [self reopen];
}
- (void)tearDown
{
    [store release];
    [[NSFileManager defaultManager] removeItemAtPath:IntegrationTestsDatabasePath error:nil];
}

- (void)reopen
{
    [store release];
    
    NSError *error;
    NSString *path = IntegrationTestsDatabasePath;
    
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"Unable to create database at %@", path);
    }
    
    store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    
    [store addClass:[Person class]];
}

- (void)testCreation
{
    STAssertNotNil(store, @"Store not created");
}

- (void)testBasics
{
    STAssertEquals([[store allObjectsForClass:[Person class]] count], (NSUInteger)0, @"expected 0 people");
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];

    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], TestPersonName, @"Unexpected name");
}

- (void)testBlankKeyDoesntEncrypt
{
    [store setEncryptionKey:@""];
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], TestPersonName, @"Unexpected name");
}


- (void)testBasicsNoSave
{
    STAssertEquals([[store allObjectsForClass:[Person class]] count], (NSUInteger)0, @"expected 0 people");
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
//    NSError *error;
//    [store saveChanges:&error];
    
    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)0, @"expected 0 Persons");
}

- (void)testEncryption
{
    [store setEncryptionKey:TestKey];
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    [store setEncryptionKey:TestKey];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], TestPersonName, @"Unexpected name");
}

- (void)testEncryptionNoKey
{
    [store setEncryptionKey:TestKey];
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    // notice: no encryptionKey set
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertNil([[people objectAtIndex:0] name], @"Name should be nil");
}

- (void)testEncryptionBadKey
{
    [store setEncryptionKey:TestKey];
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    [store setEncryptionKey:[TestKey stringByAppendingString:TestKey]]; // Make sure the key is different.
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    Person *fetchedPerson = [people objectAtIndex:0];
    STAssertNil([fetchedPerson name], @"Name should be nil but is %@", [fetchedPerson name]);
}

- (void)testFetchUnencryptedObject
{
    [store setEncryptionKey:@""];
    
    [store insertObject:[Person personWithName:TestPersonName]];
    
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    [store setEncryptionKey:@"howdyhowdy"];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], TestPersonName, @"Unexpected name");
}

@end
