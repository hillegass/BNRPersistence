// The MIT License
//
// Copyright (c) 2008 Big Nerd Ranch, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#import "BNRStoreDocument.h"
#import "BNRStore.h"
#import "BNRTCBackend.h"

// For timing
#import <mach/mach_time.h>

@implementation BNRStoreDocument

- (id)init
{
    self = [super init];
    if (self) {
		store = [[BNRStore alloc] init];
		[store setUndoManager:[self undoManager]];
    }
    return self;
}

- (void)dealloc
{
    [store release];
    [super dealloc];
}

- (BOOL)writeSafelyToURL:(NSURL *)absoluteURL 
                  ofType:(NSString *)typeName 
        forSaveOperation:(NSSaveOperationType)saveOperation 
                   error:(NSError **)outError
{
    NSString *newPath = [absoluteURL path];
    BNRStoreBackend *backend = [store backend];
    
    // I need this path to make it work as a document,
    // but I don't really want to add a path method to 
    // BNRStoreBackend
    NSString *oldPath = [(id)backend path];
    
    NSFileManager *fm = [NSFileManager defaultManager];
    
    // Do I need to delete the file that is there already?
    if ((saveOperation == NSSaveAsOperation) && 
        ([fm fileExistsAtPath:newPath])) {
        
        // Remove it
        BOOL removed = [fm removeItemAtPath:newPath 
                                      error:outError];
        
        // Removal failed?
        if (!removed) {
            return NO;
        }
    }
    
    BOOL saved;
    
    if (![newPath isEqualToString:oldPath]) {
        BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:newPath
                                                             error:outError];
        if (!backend) {
            return NO;
        }
        
        // This should close the old backend
        [store setBackend:backend];
        [backend release];
        saved = [store saveChanges:outError];
    } else {
        saved = [store saveChanges:outError];
    }
    
    
    return saved;
}

- (void)readFromStore
{
    // no op: to be overriden
}

- (BOOL)readFromURL:(NSURL *)absoluteURL 
             ofType:(NSString *)typeName 
              error:(NSError **)outError
{
    [[self undoManager] removeAllActions];
    NSString *path = [absoluteURL path];
    
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path 
                                                         error:outError];
    if (!backend) {
        return NO;
    }
    [store setBackend:backend];
    [backend release];
    [self readFromStore];
    return YES;
}

- (BNRStore *)store
{
    return store;
}


@end
