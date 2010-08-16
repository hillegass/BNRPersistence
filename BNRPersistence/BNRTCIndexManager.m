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
#import "BNRTCIndexManager.h"
#import "BNRStoredObject.h"

#pragma mark Private Classes

@interface BNRTextIndex : NSObject {
    TCIDB *file;
}
@property (nonatomic) TCIDB *file;
@end;

@implementation BNRTextIndex

@synthesize file;

- (void)dealloc
{
    tcidbclose(file);
    tcidbdel(file);
    [super dealloc];
}

@end

@interface BNRClassKey : NSObject
{
    Class keyClass;
    NSString *key;
}
@property (nonatomic) Class keyClass;
@property (nonatomic, copy) NSString * key;
@end

@implementation BNRClassKey
@synthesize keyClass, key;
- (void)dealloc
{
    [key release];
    [super dealloc];
}

- (NSUInteger)hash
{
    return (NSUInteger)keyClass + [key hash];
}

- (BOOL)isEqual:(id)object
{
    BNRClassKey *other = (BNRClassKey *)object;
    return ([other class] == [self class]) && [[other key] isEqual:[self key]];
}

- (id)copyWithZone:(NSZone *)zone
{
    [self retain];
    return self;
}

@end

@implementation BNRTCIndexManager

- (id)initWithPath:(NSString *)p error:(NSError **)err
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
    
    textIndexes = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (void)close
{
    [textIndexes removeAllObjects];
}
- (void)dealloc 
{
    [self close];
    [textIndexes release];
    [path release];
    [super dealloc];
}

- (NSString *)path
{
    return path;
}

- (TCIDB *)textIndexForClass:(Class)c 
                         key:(NSString *)k
{
    
    BNRClassKey *ck = [[BNRClassKey alloc] init];
    [ck setKeyClass:c];
    [ck setKey:k];
    
    BNRTextIndex *ti = [textIndexes objectForKey:ck];
    
    if (!ti) {
        
        NSString *filename = [[NSString alloc] initWithFormat:@"%@-%@.tindx", NSStringFromClass(c), k];
        NSString *tiPath = [path stringByAppendingPathComponent:filename];
        [filename release];
        
        int mode = HDBOREADER | HDBOWRITER | HDBONOLCK | HDBOCREAT;
        
        TCIDB *newDB = tcidbnew();
        
        if (!tcidbopen(newDB, [tiPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {
            int ecode = tcidbecode(newDB);
            NSLog(@"Error opening %@: %s\n", tiPath, tcidberrmsg(ecode));
            @throw [NSException exceptionWithName:@"DB Error" 
                                           reason:@"Unable to open file"
                                         userInfo:nil];
            return NULL;
        }
        ti = [[BNRTextIndex alloc] init];
        [ti setFile:newDB];
        [textIndexes setObject:ti forKey:ck];
        [ti autorelease];
    }
    [ck release];
    return [ti file];
}


- (UInt32)countOfRowsInClass:(Class)c 
                matchingText:(NSString *)toMatch
                      forKey:(NSString *)key
                        list:(UInt32 **)listPtr
{
    TCIDB *ti = [self textIndexForClass:c 
                                    key:key];
    const char * cMatch = [toMatch cStringUsingEncoding:NSUTF8StringEncoding];
    int recordCount;
    uint64_t *searchResults = tcidbsearch2(ti, cMatch, &recordCount);
    
    if (listPtr && recordCount > 0) {
        UInt32 *outputBuffer = (UInt32 *)malloc(recordCount * sizeof(UInt32));
        
        for (int i = 0; i < recordCount; i++) {
            outputBuffer[i] = (UInt32)searchResults[i];
        }
        *listPtr = outputBuffer;
    }
    free(searchResults);

    return (UInt32)recordCount;
}

- (void)insertObjectInIndexes:(BNRStoredObject *)obj
{
    Class c = [obj class];
    UInt32 rowID = [obj rowID];
    NSSet *indexKeys = [c textIndexedAttributes];
    for (NSString *key in indexKeys) {
        TCIDB *ti = [self textIndexForClass:c key:key];
        NSString *value = [obj valueForKey:key];
        const char * cValue = [value cStringUsingEncoding:NSUTF8StringEncoding];
        BOOL success = tcidbput(ti, rowID, cValue);
        if (!success) {
            NSLog(@"Insert of %@ into index (%@, %@) failed", value, NSStringFromClass(c), key);
        } 
    }
}

- (void)deleteObjectFromIndexes:(BNRStoredObject *)obj
{
    Class c = [obj class];
    UInt32 rowID = [obj rowID];
    NSSet *indexKeys = [c textIndexedAttributes];
    for (NSString *key in indexKeys) {
        TCIDB *ti = [self textIndexForClass:c key:key];
        BOOL success = tcidbout(ti, rowID);
        if (!success) {
            NSLog(@"Delete from index (%@, %@) failed", NSStringFromClass(c), key);
        }
    }
    
}
- (void)updateObjectInIndexes:(BNRStoredObject *)obj
{
    Class c = [obj class];
    UInt32 rowID = [obj rowID];
    
    NSSet *keys = [[obj class] textIndexedAttributes];
    
    for (NSString *key in keys) {
        TCIDB *ti = [self textIndexForClass:c key:key];
        BOOL success = tcidbout(ti, rowID);
        if (!success) {
            NSLog(@"Delete from index (%@, %@) failed", NSStringFromClass(c), key);
        }
        
        NSString *value = [obj valueForKey:key];
        const char * cValue = [value cStringUsingEncoding:NSUTF8StringEncoding];
        success = tcidbput(ti, rowID, cValue);
        if (!success) {
            NSLog(@"Insert of %@ into index (%@, %@) failed", value, NSStringFromClass(c), key);
        }        
    }
    
      
}
@end
