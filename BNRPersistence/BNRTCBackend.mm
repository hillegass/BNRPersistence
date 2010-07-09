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

#import "BNRTCBackend.h"
#import "BNRDataBuffer.h"
#import "BNRTCBackendCursor.h"

const char *BNRToCString(NSString *str, int *lenPtr)
{
    const char *cString = [str cStringUsingEncoding:NSUTF8StringEncoding];
    if (lenPtr) {
        *lenPtr = strlen(cString);
    }
    return cString;
}

@implementation BNRTCBackend



- (id)initWithPath:(NSString *)p error:(NSError **)err;
{
    [super init];
    path = [p copy];
    
    BOOL isDir, exists;
    exists = [[NSFileManager defaultManager] fileExistsAtPath:path
                                                  isDirectory:&isDir];
    
    if (exists) {
        if (!isDir)
        {
            if (err) {
                NSMutableDictionary *ui = [NSMutableDictionary dictionary];
                [ui setObject:[NSString stringWithFormat:@"%@ is a file", path]
                       forKey:NSLocalizedDescriptionKey];
                *err = [NSError errorWithDomain:@"BNRPersistence"
                                           code:4
                                       userInfo:ui];
            }
            [self dealloc];
            return nil;
        }
    } else {
        BOOL success = [[NSFileManager defaultManager] createDirectoryAtPath:path
                                                   withIntermediateDirectories:YES
                                                                  attributes:nil
                                                                       error:err];
        if (!success) {
            [self dealloc];
            return nil;
        }
    }
    
    dbTable = new hash_map<Class, TCHDB *, hash<Class>, equal_to<Class> >(389);

    return self;
}

- (void)dealloc
{
    [self close];
    [path release];
    delete dbTable;
    [super dealloc];
}

#pragma mark Transaction support

// We don't really do transactions in TC (we could, though)
- (BOOL)beginTransaction
{
    return YES;
}

- (BOOL)commitTransaction
{
    return YES;
}

- (BOOL)abortTransaction
{
    return NO;
}

- (BOOL)hasOpenTransaction
{
    return NO;
}

#pragma mark Reading and writing

- (void)setFile:(TCHDB *)f forClass:(Class)c
{
    (*dbTable)[c] = f;
}

- (TCHDB *)fileForClass:(Class)c;
{
    TCHDB *result = (*dbTable)[c];
    if (!result) {
        NSString *classPath = [path stringByAppendingPathComponent:NSStringFromClass(c)];
        
        int mode = HDBOREADER | HDBOWRITER | HDBONOLCK| HDBOCREAT;
        
        result = tchdbnew();
        
        if (!tchdbopen(result, [classPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {
            int ecode = tchdbecode(result);
            NSLog(@"Error opening %@: %s\n", classPath, tchdberrmsg(ecode));
            @throw [NSException exceptionWithName:@"DB Error" 
                                           reason:@"Unable to open fil"
                                         userInfo:nil];
            return NULL;
        }
        [self setFile:result forClass:c];
    }
    return result;
}

- (TCHDB *)namedBufferDB;
{
    if (!namedBufferDB) {
        
        NSString *classPath = [path stringByAppendingPathComponent:@"NamedBuffers"];
        
        int mode = HDBOREADER | HDBOWRITER | HDBONOLCK| HDBOCREAT;
        
        namedBufferDB = tchdbnew();
        
        if (!tchdbopen(namedBufferDB, [classPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {
            int ecode = tchdbecode(namedBufferDB);
            NSLog(@"Error opening %@: %s\n", classPath, tchdberrmsg(ecode));
            @throw [NSException exceptionWithName:@"DB Error" 
                                           reason:@"Unable to open file"
                                         userInfo:nil];
            return NULL;
        }
    }
    return namedBufferDB;
    
}

- (void)insertData:(BNRDataBuffer *)d 
          forClass:(Class)c
             rowID:(UInt32)n
{    
    TCHDB *db = [self fileForClass:c];
    UInt32 key = CFSwapInt32HostToLittle(n);
    bool successful = tchdbput(db, &key, sizeof(UInt32), [d buffer], [d length]);
    if (!successful) {
        int ecode = tchdbecode(db);
        NSString *message = [NSString stringWithFormat:@"tchdbput in insertData: %s", tchdberrmsg(ecode)];
        
        NSException *e = [NSException exceptionWithName:@"BadInsert"
                                                 reason:message 
                                               userInfo:nil];
        @throw e;
    }
    
}

- (void)deleteDataForClass:(Class)c
                     rowID:(UInt32)n
{
    TCHDB * db = [self fileForClass:c];
    UInt32 key = CFSwapInt32HostToLittle(n);
    
    bool successful = tchdbout(db, &key, sizeof(UInt32));
    if (!successful) {
        NSLog(@"warning: tried to delete something that wasn't there");
    }
}

- (void)updateData:(BNRDataBuffer *)d 
          forClass:(Class)c 
             rowID:(UInt32)n
{
    TCHDB *db = [self fileForClass:c];
    UInt32 key = CFSwapInt32HostToLittle(n);    
    
    bool successful = tchdbput(db, &key, sizeof(UInt32), [d buffer], [d length]);
    if (!successful) {
        int ecode = tchdbecode(db);
        NSString *message = [NSString stringWithFormat:@"tchdbput in updateData: %s", tchdberrmsg(ecode)];
        
        NSException *e = [NSException exceptionWithName:@"BadUpdate"
                                                 reason:message 
                                               userInfo:nil];
        @throw e;
    }
}

#pragma mark Fetching

- (BNRDataBuffer *)dataForClass:(Class)c 
                          rowID:(UInt32)n
{
    TCHDB * db = [self fileForClass:c];
    UInt32 key = CFSwapInt32HostToLittle(n);
    
    int bufferSize;
    void *data = tchdbget(db, &key, sizeof(UInt32), &bufferSize);
    
    if (data == NULL) {
        return nil;
    }
        
    BNRDataBuffer *b = [[BNRDataBuffer alloc] initWithData:data
                                                    length:bufferSize];
    [b autorelease];
    return b;
}

- (BNRBackendCursor *)cursorForClass:(Class)c
{
    TCHDB *db = [self fileForClass:c];
    if (!db) {
        return nil;
    }
    BNRTCBackendCursor *cu = [[BNRTCBackendCursor alloc] initWithFile:db];
    [cu autorelease];
    return cu;
}

- (void)close
{
    hash_map<Class, TCHDB *, hash<Class>, equal_to<Class> >::iterator iter = dbTable->begin();
    while (iter != dbTable->end()) {
        TCFileHashedPair currentPair = *iter;
        TCHDB * dbp = currentPair.second;
        tchdbclose(dbp);
        tchdbdel(dbp);
        iter++;
    }
    dbTable->clear();
    
    if (namedBufferDB) {
        tchdbclose(namedBufferDB);
        tchdbdel(namedBufferDB);
        namedBufferDB = NULL;
    }
}

- (void)insertDataBuffer:(BNRDataBuffer *)d
            forName:(NSString *)key
{
    NSAssert (key != nil, @"key must not be nil");
    int bytes;
    const char *cString = BNRToCString(key, &bytes);
 
    TCHDB *db = [self namedBufferDB];
     
    bool successful = tchdbput(db, cString, bytes, [d buffer], [d length]);
    if (!successful) {
        int ecode = tchdbecode(db);
        NSString *message = [NSString stringWithFormat:@"tchdbput in insertData:forKey: %s", tchdberrmsg(ecode)];
        
        NSException *e = [NSException exceptionWithName:@"BadInsert"
                                                 reason:message 
                                               userInfo:nil];
        @throw e;
    }
}

- (void)deleteDataBufferForName:(NSString *)key
{
    NSAssert (key != nil, @"key must not be nil");
    int bytes;
    const char *cString = BNRToCString(key, &bytes);
    
    TCHDB *db = [self namedBufferDB];
    
    bool successful = tchdbout(db, cString, bytes);
    if (!successful) {
        int ecode = tchdbecode(db);
        NSString *message = [NSString stringWithFormat:@"tchdbput in deleteDataForKey: %s", tchdberrmsg(ecode)];
        
        NSException *e = [NSException exceptionWithName:@"BadDelete"
                                                 reason:message 
                                               userInfo:nil];
        @throw e;
    }
}

- (void)updateDataBuffer:(BNRDataBuffer *)d forName:(NSString *)key
{
    [self insertDataBuffer:d forName:key];
}

- (BNRDataBuffer *)dataBufferForName:(NSString *)key
{
    NSAssert (key != nil, @"key must not be nil");
    int bytes;
    const char *cString = BNRToCString(key, &bytes);
    
    TCHDB *db = [self namedBufferDB];
    
    int bufferSize;
    void *data = tchdbget(db, cString, bytes, &bufferSize);
        
    if (data == NULL) {
        return nil;
    }
    
    BNRDataBuffer *b = [[BNRDataBuffer alloc] initWithData:data
                                                    length:bufferSize];
    [b autorelease];
    return b;
}

- (NSSet *)allNames
{
    TCHDB *db = [self namedBufferDB];

    bool successful = tchdbiterinit(db);
    if (!successful) {
        int ecode = tchdbecode(db);
        NSLog(@"Bad tchdbiterinit in initWithFile: %s", tchdberrmsg(ecode));
    }
    
    NSMutableSet *result = [NSMutableSet set];
    char *buffer;
    int size;
    while (buffer = (char *)tchdbiternext(db, &size)) {
        NSString *newKey = [[NSString alloc] initWithBytesNoCopy:buffer
                                                          length:size
                                                        encoding:NSUTF8StringEncoding
                                                    freeWhenDone:YES];
        [result addObject:newKey];
        [newKey release];
    }

    return result;
}


- (NSString *)path
{
    return path;
}

@end
