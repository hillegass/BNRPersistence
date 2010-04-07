#import <Foundation/Foundation.h>
#import "BNRTCBackend.h"
#import "BNRStore.h"
#import "BNRStoredObject.h"
#import "BNRDataBuffer.h"
#import "Person.h"

int main (int argc, const char * argv[]) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSError *error;
    NSString *path = @"./database/";
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"Unable to create database at %@", path);
        [pool drain];
        return EXIT_FAILURE;
    }
    
    BNRStore *store = [[BNRStore alloc] init];
    [store setEncryptionKey:@"howdy"];
    [store setBackend:backend];
    [backend release];
    
    [store addClass:[Person class]];
    
//    for (Person *person in [store allObjectsForClass:[Person class]])
//        [store deleteObject:person];
//    
//    Person *newPerson = [[[Person alloc] init] autorelease];
//    [newPerson setName:@"Fred"];
//    [store insertObject:newPerson];
//    
//    [store saveChanges:&error];
    
    for (Person *person in [store allObjectsForClass:[Person class]])
    {
        [person checkForContent];
        NSLog(@"Person: %@", [person name]);
    }
    
    [store release];
    
    [pool drain];
    return 0;
}
