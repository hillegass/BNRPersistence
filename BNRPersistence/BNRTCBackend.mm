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

@synthesize usesTransactions, usesWriteSync;

// designated initializer
- (id)initWithPath:(NSString *)p useTransactions:(BOOL)useTransactionsFlag useWriteSyncronization:(BOOL)useWriteSyncronizationFlag error:(NSError **)err;
{
    self = [super init];
    if (self) {
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

        openTransactions = [[NSMutableSet alloc] init];
        usesTransactions = useTransactionsFlag;
        usesWriteSync = useWriteSyncronizationFlag;
	}
    return self;
}

- (id)initWithPath:(NSString *)p error:(NSError **)err;
{
	return [self initWithPath:(NSString *)p useTransactions:NO useWriteSyncronization:NO error:err];
}
			
- (void)dealloc
{
	[openTransactions release];
    [self close];
    [path release];
    delete dbTable;
    [super dealloc];
}

#pragma mark Transaction support

- (BOOL)beginTransactionForClasses:(NSSet *)classesForTransaction
{
	if (usesTransactions) {		
		
		for (Class c in classesForTransaction) {
			
			TCHDB *db = [self fileForClass:c];
			bool result = tchdbtranbegin(db);
			if (!result) {
				int ecode = tchdbecode(db);
				// TODO: need a real error alert here?
				NSLog(@"tchdbtranbegin() failed for Class:%@, error:%s", NSStringFromClass(c), tchdberrmsg(ecode));
				[openTransactions removeAllObjects];
				return NO;
			}
			[openTransactions addObject:c];
		}
	}
    return YES;
}

- (BOOL)commitTransaction
{
	if (usesTransactions) {
		
		BOOL cumulativeResult = YES;
		NSMutableSet *committedTransactions = [NSMutableSet set];
		
		for (Class c in openTransactions) {
			
			TCHDB *db = [self fileForClass:c];
			bool result = tchdbtrancommit(db);
			if (!result) {
				// TODO: Should c be removed from openTransactions even if the commit fails?
				// What can we really do with transactions which fail to commit?
				int ecode = tchdbecode(db);
				NSLog(@"tchdbtrancommit() failed for Class:%@, error:%s", NSStringFromClass(c), tchdberrmsg(ecode));
			}
			else {
				[committedTransactions addObject:c];
			}
			cumulativeResult &= result;
		}
		[openTransactions minusSet:committedTransactions];

		if ([openTransactions count]) {
			NSLog(@"Will roll back transactions which failed to commit:%@", [openTransactions description]);
		}
		
		return cumulativeResult;
	}
	
    return YES;
}

- (BOOL)abortTransaction
{
	if (usesTransactions && [openTransactions count]) {

		BOOL cumulativeResult = YES;

		NSMutableSet *rolledBackTransactions = [NSMutableSet set];
		for (Class c in openTransactions) {
			TCHDB *db = [self fileForClass:c];
			bool result = tchdbtranabort(db);
			if (!result) {
				// Should c be left in openTransactions if a rollback fails?
				// What could we possibly do about them then?
				int ecode = tchdbecode(db);
				NSLog(@"Can't roll  back transaction for Class:%@, error:%s", NSStringFromClass(c), tchdberrmsg(ecode));
			}
			//else // TODO: if it fails, do we remove it from the set of open transactions? Or not?
			{
				[rolledBackTransactions addObject:c];
			}
			cumulativeResult &= result;
		}
		[openTransactions minusSet:rolledBackTransactions];
		
		// At this point, if openTransactions contains any objects, they are for transactions which
		// both failed to commit and failed to roll back. 
		// TODO: What to do? Leave them in the array of open transactions? Or remove them since there's
		// little else we can do?
		return cumulativeResult;
	}
	
    return YES;
}

- (BOOL)hasOpenTransaction
{
	if (usesTransactions) {
		return ([openTransactions count] > 0);
	}
    return NO;
}

#pragma mark Reading and writing

- (void)setFile:(TCHDB *)f forClass:(Class)c
{
    (*dbTable)[c] = f;
}

- (TCHDB *)fileForClass:(Class)c;
{
    TCHDB *dbFile = (*dbTable)[c];
    if (!dbFile) {
        NSString *classPath = [path stringByAppendingPathComponent:NSStringFromClass(c)];
        
        int mode = HDBOREADER | HDBOWRITER | HDBONOLCK| HDBOCREAT;	// FIXME: need to watch out for read-only media
        
		if (usesWriteSync) {
			// BMonk 5/22/11 HDBOTSYNC ensures TC will immediately sync all inserts and updates to the storage device's physical media,
			// writing through any caching in the OS or on the device itself. This is much slower, but safer in case fo crash or power outage.
			// The performance hit is not noticable for typical-case small writes, but the slowdown is extreme for bulk operations
			// (such as in the BNRPersistence test apps).
			//
			// For bulk operations, full speed can be obtained by turning off write sync, performing the bulk operation 
			// (perhaps with transactions turned on), then turnign write sync back on.
			//
			// Unfortunately, due to this class caching the db files for Classes, usesWriteSync must be set at init time. 
			// So to turn off write sync, you must close the TC db and reopen it with write sync turned off.
			//
			// Perhaps there should be separate write-syncing subclass of BNRTCBackend instead these options?
			mode |= HDBOTSYNC; 
		}
		
        dbFile = tchdbnew();
        
        if (!tchdbopen(dbFile, [classPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {
        
			// FIXME: I think we'll need to do this before throwing
			[self abortTransaction];
        
			int ecode = tchdbecode(dbFile);
            NSLog(@"Error opening %@: %s\n", classPath, tchdberrmsg(ecode));
			tchdbdel(dbFile); // •• don't leak a TCIDB * if tcidbopen() fails 6/9/11
            @throw [NSException exceptionWithName:@"DB Error" 
                                           reason:[NSString stringWithFormat:@"Unable to open file at classPath:%@, error:%s", classPath, tchdberrmsg(ecode)]
                                         userInfo:nil];
            return NULL;
        }
        [self setFile:dbFile forClass:c];
    }
    return dbFile;
}

- (TCHDB *)namedBufferDB;
{
    if (!namedBufferDB) {
        
        NSString *classPath = [path stringByAppendingPathComponent:@"NamedBuffers"];
        
        int mode = HDBOREADER | HDBOWRITER | HDBONOLCK| HDBOCREAT;	// FIXME: need to watch out for read-only media

		if (usesWriteSync) {
			// BMonk 5/22/11 HDBOTSYNC ensures TC will immediately sync all inserts and updates to the storage device's physical media,
			// writing through any caching in the OS or on the device itself. This is much slower, but safer in case fo crash or power outage.
			// The performance hit is not noticable for typical-case small writes, but the slowdown is extreme for bulk operations
			// (such as in the BNRPersistence test apps).
			//
			// For bulk operations, full speed can be obtained by turning off write sync, performing the bulk operation 
			// (perhaps with transactions turned on), then turning write sync back on.
			//
			// Unfortunately, due to this class caching the db files for Classes, usesWriteSync must be set at init time. 
			// So to turn off write sync, you must close the TC db and reopen it with write sync turned off.
			//
			// Perhaps there should be separate write-syncing subclass of BNRTCBackend instead these options?
			//
			mode |= HDBOTSYNC; 
		}
        
        namedBufferDB = tchdbnew();
        
        if (!tchdbopen(namedBufferDB, [classPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {

			// FIXME: I think we'll need to do this before throwing
			[self abortTransaction];

            int ecode = tchdbecode(namedBufferDB);
            NSLog(@"Error opening %@: %s\n", classPath, tchdberrmsg(ecode));
			tchdbdel(namedBufferDB); // •• don't leak the TCIDB * if tcidbopen() fails 6/9/11
			namedBufferDB = NULL;
            @throw [NSException exceptionWithName:@"DB Error (named data buffers)" 
                                           reason:[NSString stringWithFormat:@"Unable to open namedBufferDB at classPath:%@, error %s", classPath, tchdberrmsg(ecode)]
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
		
		// FIXME: I think we'll need to do this before throwing
		[self abortTransaction];
		
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
		
		// FIXME: I think we'll need to do this before throwing
		[self abortTransaction];
		
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
        tchdbclose(namedBufferDB); // namedBufferDB may not be open if tchdbopen() failed in -namedBufferDB
        tchdbdel(namedBufferDB);	// but tchdbdel() implicitly closes if the TCHDB is open
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
		
		// FIXME: I think we'll need to do this before throwing
		[self abortTransaction];

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
		
		// FIXME: I think we'll need to do this before throwing
		[self abortTransaction];
		
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
    while ((buffer = (char *)tchdbiternext(db, &size)) != nil) {
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
