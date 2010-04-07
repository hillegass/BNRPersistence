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


#import <Foundation/Foundation.h>
@class BNRClassDictionary;
@class BNRUniquingTable;
@class BNRStoreBackend;
@class BNRStoredObject;
@class BNRDataBuffer;
@class BNRStore;
@class BNRIndexManager;

@protocol BNRStoreDelegate

- (void)store:(BNRStore *)st willInsertObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st willDeleteObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st willUpdateObject:(BNRStoredObject *)so;
- (void)store:(BNRStore *)st didChangeObject:(BNRStoredObject *)so;

@end

//! BNRStore


/*! BNRStore is analogous to NSManagedObjectContext.  You use it to fetch, insert
 delete, and update objects to the persistent store. It can have a delegate and an undo
 manager. */
 
@interface BNRStore : NSObject {
    
    BNRUniquingTable *uniquingTable; /**< Maps (Class, rowID) -> BNRStoreddObject */
    
    BNRStoreBackend *backend; /**< Actually saves the data */
    
    NSUndoManager *undoManager; /**< If non-nil, undo actions are automatically registered */
    
    BNRIndexManager *indexManager; 
    
    id <BNRStoreDelegate> delegate; /*< Gets told when an object is to be updated, inserted, or deleted */
    
    // Pending edits are stored in the toBe.. sets
    NSMutableSet *toBeInserted;
    NSMutableSet *toBeDeleted;
    NSMutableSet *toBeUpdated;
    
    // Class meta data
    BNRClassDictionary *classMetaData; /*< Maps Class->BNRClassMetaData */
    Class classes[256];  /*< Maps int (the class ID) -> Class */
    
    BOOL usesPerInstanceVersioning; /*< Prepends version number on data buffer; Default = YES */
    
    NSString *encryptionKey; /**< Password to be used in reading and writing objects to/from the store. */
}

@property (nonatomic, retain) BNRIndexManager *indexManager;
@property (nonatomic, retain) NSUndoManager *undoManager;
@property (nonatomic, assign) id <BNRStoreDelegate> delegate;
@property (nonatomic, assign) BOOL usesPerInstanceVersioning;
@property (nonatomic, retain) NSString *encryptionKey;

- (id)init;

#pragma mark Fetching

// Fetches the single object of class |c| at row |n|.
// The object's content will be fetched if necessary if |yn| is YES.
// |yn| does not affect any content the object already has.
- (BNRStoredObject *)objectForClass:(Class)c 
                              rowID:(UInt32)n 
                       fetchContent:(BOOL)yn;

// Fetches all stored objects of a particular class.
// All returned objects have content.
- (NSMutableArray *)allObjectsForClass:(Class)c;

// Full-text search
- (NSMutableArray *)objectsForClass:(Class)c
                       matchingText:(NSString *)toMatch
                             forKey:(NSString *)key;

#pragma mark Saving

// 'hasUnsavedChanges' is observable
- (BOOL)hasUnsavedChanges;

// Mark object for insertion into object store
- (void)insertObject:(BNRStoredObject *)obj;

// Mark object for deletion from object store
- (void)deleteObject:(BNRStoredObject *)obj;

// Mark object to be updated in object store
- (void)willUpdateObject:(BNRStoredObject *)obj;

- (BOOL)saveChanges:(NSError **)errorPtr;

#pragma mark Backend

- (BNRStoreBackend *)backend;
- (void)setBackend:(BNRStoreBackend *)be;

#pragma mark Class metadata

- (void)addClass:(Class)c;
- (unsigned)nextRowIDForClass:(Class)c;
- (unsigned char)versionForClass:(Class)c;
- (Class)classForClassID:(unsigned char)c;
- (unsigned char)classIDForClass:(Class)c;

#pragma mark Retain-cycle breaking

- (void)dissolveAllRelationships;

@end

