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

// BNRStore tracks objects and stores them to a BNRStoreBackend

#import <Cocoa/Cocoa.h>
@class BNRClassDictionary;
@class BNRStoreBackend;
@class BNRStoredObject;
@class BNRDataBuffer;
@class BNRStore;

@protocol BNRStoreDelegate

- (void)store:(BNRStore *)st willInsertObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st willDeleteObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st willUpdateObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st didChangeObject:(BNRStoredObject *)so;

@end

@interface BNRStore : NSObject {
    
    // The Uniquing table holds BNRIntDictionarys.  Thus, if
    // you know an object's class and rowID, you can find it in
    // the uniquingTable
    BNRClassDictionary *uniquingTable;
    
    // The backend actually stores the data
    BNRStoreBackend *backend;
    
    // If the undo manager is non-nil,  undo actions for inserts,
    // updates and deletes are automatically registered with it.
    NSUndoManager *undoManager;
    
    // The delegate gets told when an object is to be updated, inserted, or deleted
    id <BNRStoreDelegate> delegate;
    
    // Pending edits are stored in the toBe.. sets
    NSMutableSet *toBeInserted;
    NSMutableSet *toBeDeleted;
    NSMutableSet *toBeUpdated;
    
    // Class meta data
    BNRClassDictionary *classMetaData;
    Class classes[256];
    
    // This capability is not implemented yet...
    BOOL savesDataForSync;
}

@property (nonatomic) BOOL savesDataForSync;

- (id)init;
- (void)setDelegate:(id <BNRStoreDelegate>)obj;
- (void)setUndoManager:(NSUndoManager *)ud;

#pragma mark Fetching

// Fetching a single object
- (BNRStoredObject *)objectForClass:(Class)c 
                              rowID:(UInt32)n 
                       fetchContent:(BOOL)yn;

// Fetching all of a particular class
- (NSMutableArray *)allObjectsForClass:(Class)c;


#pragma mark Saving

// 'hasUnsavedChanges' is observable
- (BOOL)hasUnsavedChanges;

- (void)insertObject:(BNRStoredObject *)obj;
- (void)deleteObject:(BNRStoredObject *)obj;
- (void)willUpdateObject:(BNRStoredObject *)obj;

- (BOOL)saveChanges:(NSError **)errorPtr;

#pragma mark Backend

- (BNRStoreBackend *)backend;
- (void)setBackend:(BNRStoreBackend *)be;

#pragma mark Class metadata

- (void)addClass:(Class)c expectedCount:(UInt32)eCount;
- (unsigned)nextRowIDForClass:(Class)c;
- (unsigned char)versionForClass:(Class)c;
- (Class)classForClassID:(unsigned char)c;
- (unsigned char)classIDForClass:(Class)c;

#pragma mark Retain-cycle breaking

- (void)dissolveAllRelationships;

@end

