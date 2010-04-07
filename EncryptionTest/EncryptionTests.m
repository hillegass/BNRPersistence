//
//  EncryptionTests.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "EncryptionTests.h"
#import "Person.h"
#import "BNRTCBackend.h"
#import "BNRStore.h"

@interface EncryptionTests ()
- (void)reopen;
@end

NSString * const EncryptionTestsDatabasePath = @"./database/";

@implementation EncryptionTests

- (void)setUp
{
    [[NSFileManager defaultManager] removeItemAtPath:EncryptionTestsDatabasePath error:nil]; // erase in case we were debugging and uncerimoniously exited
    [self reopen];
}
- (void)tearDown
{
    [store release];
    [[NSFileManager defaultManager] removeItemAtPath:EncryptionTestsDatabasePath error:nil];
}

- (void)reopen
{
    [store release];
    
    NSError *error;
    NSString *path = EncryptionTestsDatabasePath;
    
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
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
    // BUG? Have to saveChanges or it won't appear below (even without reopen)...?
    NSError *error;
    [store saveChanges:&error];

    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], @"Fred", @"Name not Fred");
}

- (void)testBlankKeyDoesntEncrypt
{
    [store setEncryptionKey:@""];
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
    // BUG? Have to saveChanges or it won't appear below (even without reopen)...?
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], @"Fred", @"Name not Fred");
}


- (void)testBasicsNoSave
{
    STAssertEquals([[store allObjectsForClass:[Person class]] count], (NSUInteger)0, @"expected 0 people");
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
//    NSError *error;
//    [store saveChanges:&error];
    
    [self reopen];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)0, @"expected 0 Persons");
}

- (void)testEncryption
{
    [store setEncryptionKey:@"howdy"];
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
    // BUG? Have to saveChanges or it won't appear below (even without reopen)...?
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    [store setEncryptionKey:@"howdy"];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertEqualObjects([[people objectAtIndex:0] name], @"Fred", @"Name not Fred");
}

- (void)testEncryptionNoKey
{
    [store setEncryptionKey:@"howdy"];
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
    // BUG? Have to saveChanges or it won't appear below (even without reopen)...?
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
    [store setEncryptionKey:@"howdy"];
    
    Person *newPerson = [[[Person alloc] init] autorelease];
    [newPerson setName:@"Fred"];
    [store insertObject:newPerson];
    
    // BUG? Have to saveChanges or it won't appear below (even without reopen)...?
    NSError *error;
    [store saveChanges:&error];
    
    [self reopen];
    
    [store setEncryptionKey:@"howdyhowdy"];
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"nil Person classes?");
    STAssertEquals([people count], (NSUInteger)1, @"expected 1 Person");
    STAssertNil([[people objectAtIndex:0] name], @"Name should be nil");
}

@end
