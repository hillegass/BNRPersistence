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

// MARK: -
// MARK: Private Classes
// MARK: -
// =============================//

@interface BNRTextIndex : NSObject {
	NSString	*indexDirectoryPath;				// path of the form /.../<BNRStoreName>.bnrtc/<BNRStoredObjectName>-<propertyName>.tindex 
    TCIDB		*TCIDBFile;
	BOOL		usesIndexFileWriteSync;
	BOOL		usesIndexFileCompression;
}
@property (nonatomic, readonly)	NSString	*indexDirectoryPath;
@property (nonatomic, readonly) TCIDB		*TCIDBFile;
@property (nonatomic, readonly) BOOL		usesIndexFileWriteSync;
@property (nonatomic, readonly) BOOL		usesIndexFileCompression;	


- (id)initIndexWithBNRStorePath:(NSString *)storePath
					   forClass:(Class)c
							key:(NSString *)k
		 usesIndexFileWriteSync:(BOOL)writeSyncFlag
		   usesIndexCompression:(BOOL)compressionFlag;

- (id)initIndexWithBNRStorePath:(NSString *)storePath			
					   forClass:(Class)c
							key:(NSString *)k;

+ (BNRTextIndex *)createIndexForClass:(Class)c
								  key:(NSString *)k
							   atPath:(NSString *)path				
			   usesIndexFileWriteSync:(BOOL)writeSyncFlag
				 usesIndexCompression:(BOOL)indexCompressionFlag;

+ (NSString *)standardIndexFilenameForClass:(Class)c key:(NSString *)k;

- (TCIDB *)loadTCIndexFile:(NSError **)errorPtr;

@end;

@implementation BNRTextIndex

@synthesize indexDirectoryPath, TCIDBFile, usesIndexFileWriteSync, usesIndexFileCompression;	// all readonly

// Initializers

// designated initializer
- (id)initIndexWithBNRStorePath:(NSString *)storePath			
					   forClass:(Class)c
							key:(NSString *)k
		 usesIndexFileWriteSync:(BOOL)writeSyncFlag
		   usesIndexCompression:(BOOL)indexCompressionFlag
{
	self = [super init];
	if (self) {
		usesIndexFileWriteSync = writeSyncFlag;
		usesIndexFileCompression = indexCompressionFlag;
		
		NSString *indexName = [BNRTextIndex standardIndexFilenameForClass:c key:k];
		indexDirectoryPath = [[storePath stringByAppendingPathComponent:indexName] retain];
	}
	return self;
}

- (id)initIndexWithBNRStorePath:(NSString *)storePath			
					   forClass:(Class)c
							key:(NSString *)k
{
	return [self initIndexWithBNRStorePath:storePath
								  forClass:c
									   key:k
					usesIndexFileWriteSync:NO
					  usesIndexCompression:NO];
}

- (id)init
{
	NSLog(@"%@", @"Please use one of the BNRTextIndex -initIndexWithBNRStorePath... initializers (followed by -loadTCIndexFile), or use +createIndexForClass... factory method");
	return nil;
}

// Factory methods

+ (NSString *)standardIndexFilenameForClass:(Class)c key:(NSString *)k
{
	return [NSString stringWithFormat:@"%@-%@.tindx", NSStringFromClass(c), k];
}

+ (BNRTextIndex *)createIndexForClass:(Class)c
								  key:(NSString *)k
							   atPath:(NSString *)path	// a path of the form /.../<NameOfBNRStoreName.bnrtc
			   usesIndexFileWriteSync:(BOOL)writeSyncFlag
				 usesIndexCompression:(BOOL)indexCompressionFlag
{
	BNRTextIndex *ti;
	ti = [[[self alloc] initIndexWithBNRStorePath:path
												   forClass:c
														key:k
									 usesIndexFileWriteSync:writeSyncFlag
									   usesIndexCompression:indexCompressionFlag] autorelease];
	if (!ti) {
		NSString *msg = [NSString stringWithFormat:@"Can't init BNRTextIndex for class:%@ key:%@ dbPath:%@", NSStringFromClass(c), k, path];
		NSLog(@"%@", msg);
		@throw [NSException exceptionWithName:@"DB Error (full text indexing)" 
									   reason:msg
									 userInfo:nil];
		return nil;
	}
	
	NSError *error;
	if(![ti loadTCIndexFile:&error]) {
		NSLog(@"%@", [error localizedDescription]);
		// Note: ti is already autoreleased
		@throw [NSException exceptionWithName:@"DB Error (full text indexing)" 
									   reason:[error localizedDescription]
									 userInfo:[error userInfo]];
		return nil;
	}
	
	return ti;
}

- (TCIDB *)loadTCIndexFile:(NSError **)errorPtr
{
	// Call loadTCIndexFile at least once after initing a BNRTextIndex.
	// This is most easily done  using the +createIndexForClass... factory method.
	//
	// Alternatelively, using separate alloc/init followed later by -loadIndexFile allows
	// lazy loading (or threaded  load) of TC index files, which may be useful with very large compressed
	// indexes. While compressed indexes offer dramatic on-disk size savings, they can be slow to decompress
	// at hundreds-of-megabytes sizes, or if the TC 64-bit IDBTLARGE option is set (all this could use benchmarking).
	
	if (TCIDBFile) {
		return TCIDBFile;
	}
	
	int mode = HDBOREADER | HDBOWRITER | HDBONOLCK | HDBOCREAT; // FIXME: need to watch out for read-only media

	if (usesIndexFileWriteSync) {
		// BMonk 5/22/11 HDBOTSYNC ensures TC will immediately sync all inserts and updates to the storage device's physical media,
		// writing through any cache in the OS or on the device itself. This is much slower, but safer in case of crash or power outage.
		// The performance hit is not noticable for typical-case small writes, but the slowdown is extreme for bulk operations
		// (such as in the BNRPersistence test apps).
		//
		// For bulk operations, full speed can be obtained by turning off write sync, performing the bulk operation 
		// (perhaps with transactions turned on), then turnign write sync back on.
		//
		// Unfortunately, due to BNRTCIndexManager caching the db files for Classes, usesIndexFileWriteSync must
		// be set at init time. So to turn off write sync on an index, you must close it and create a new one with write sync
		// turned off.
		//
		// Perhaps there should be separate write-syncing subclass of BNRTCIndexManager instead of all these options?
		mode |= HDBOTSYNC; 
	}

	TCIDB *newDB = tcidbnew();
	
	if (usesIndexFileCompression) {
		// When combined with tcidboptimize() (below), reduces the default size of
		// new or sparsely-populated index files from ~8.5MB to ~600K. 
		//
		// Not well-tested with extremely large index files with many entires.
		//
		// Note that opening a previously-compressed TCIDB without specifying
		// usesIndexFileCompression here still works fine - TC will decompress as needed.
		//
		// Because of that, it might make sense to remove the usesIndexFileCompression
		// option and always compress indexes. But *that* might be a problem with
		// very large indexes (needs profiling).
		(void)tcidbtune(newDB, 0, 0, 0, IDBTBZIP);
	}
	
	if (!tcidbopen(newDB, [indexDirectoryPath cStringUsingEncoding:NSUTF8StringEncoding], mode)) {
		int ecode = tcidbecode(newDB);
		NSLog(@"Error opening TC index file %@: %s\n", indexDirectoryPath, tcidberrmsg(ecode));
		tcidbdel(newDB);
				
		if (errorPtr) {
			NSString *msg = [NSString stringWithFormat:@"Unable to open text index file at path:%@, error %s", indexDirectoryPath, tcidberrmsg(ecode)];
			NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:msg, NSLocalizedDescriptionKey, nil];
			*errorPtr = [NSError errorWithDomain:@"BNRTextIndex domain" code:ecode userInfo:userInfo];
		}
		return NULL;
	}
	
	if (usesIndexFileCompression) {
		(void)tcidboptimize(newDB); // besides compressing, also reduces fragmentation, which may be slow with large indexs (needs more profiling)
	}
	
	TCIDBFile = newDB;
	return TCIDBFile;
}

#if kImplementCacheUnloading
- (void)unloadIndexFile
{
    // Could be called to reduce memory footprint of this object.
    // If the index is needed later, -loadTCIndexFile can be called
    // without reinstantiating the entire object. 
	// See comments at BNRTCIndexManager -initWithPath.
    
    if (TCIDBFile) {            
        tcidbdel(TCIDBFile);
	}	
    TCIDBFile = NULL;
}
#endif // kImplementCacheUnloading

- (void)dealloc
{
  //  tcidbclose(TCIDBFile);	// BMonk: If TCIDBFile is NULL, this will crash.
                                //
    if (TCIDBFile) {            //
        tcidbdel(TCIDBFile);	// OTOH, tcidbdel() is documented to call tcidbclose()if needed,
	}							// so the call to tcidbclose() is superfluous. Perhaps remove?
								//
								// Later: works fine with tcidbclose() commented out.
								//
								// But of course tcidbdel() also crashes if passed a NULL (which
								// *can* happen if -loadTCIndexFile fails (as if it is asked to
								// open the same TC index twice - tcidbopen() fails with "threading error")
								// and the object is dealloced).
								//
								// So either should test (TCIDBFile != NULL) here, or remove my lazy-loading
								// design and move -loadTCIndexFile's code into the designated
								// initializer. But will very large compressed indexes then
								// be slow to init? TODO: Benchmark this.
    
	[indexDirectoryPath release];
	
    [super dealloc];
}

@end

// MARK: -
// =============================//

@interface BNRClassKey : NSObject <NSCopying>
{
    Class keyClass;
    NSString *key;
}

@property (nonatomic, assign) Class keyClass;
// @property (nonatomic) Class keyClass;	// •• analyzer complains

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

// MARK: -
// MARK: Public Classes
// MARK: -
// =============================//

@implementation BNRTCIndexManager

@synthesize usesIndexFileWriteSync, usesIndexFileCompression; // read-only properties; must be set at init time

// designated initializer
- (id)initWithPath:(NSString *)p useWriteSyncronization:(BOOL)writeSyncFlag compressIndexFiles:(BOOL)compressIndexFilesFlag error:(NSError **)err
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
					*err = [NSError errorWithDomain:@"BNRPersistence (BNRTCIndexManager)"
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
		
		// Note that textIndexes is essentially a lazy-loading cache of BNRTextIndex,
		// which each hold onto a TCIDBFile for each BNRStoredObject subclass
		// which is text-indexed. The TCIDBFile can be quite large
		// resulting in a jump in memory consumption the first time
		// a particular text-indexed BNRStoredObject is accessed,
		// and this can give the false appearance of leaks. 
		// The increase in memory consumption stops when all text-indexed
		// BNRStoredObject subclasses have been written once. This cache
		// can be invalidated by calling the -close method to remove all the text indexes,
		// but perhaps there should be a finer-grained way, using BNRTextIndex's 
		// -unloadIndexFile, to remove only TCIDBFiles that are known not to be needed
		// again soon. This would leave the BNRTextIndex objects in the cache, but
		// greatly reducing their memory usage. (BNRTextIndex already has the ability to
		// lazy-load and reload its TCIDBFile when necessary).
		// Some support is available; toggle kImplementCacheUnloading to 1
		textIndexes = [[NSMutableDictionary alloc] init];
		
		usesIndexFileWriteSync = writeSyncFlag;
		usesIndexFileCompression = compressIndexFilesFlag;
	}
    
    return self;
}

- (id)initWithPath:(NSString *)p error:(NSError **)err
{
	return [self initWithPath:p useWriteSyncronization:NO compressIndexFiles:NO error:err];
}

#if kImplementCacheUnloading 
- (void)unloadTCIDBFile:(TCIDB *)ti
{
	// See comments at initWithPath. 
	// Could be used to selectively reduce memory
	// footprint after accessing a text-indeed BNRStored object. 
	// This could be useful if the index is large but it is known won't be
	// needed again soon. The TCIDBFile (can be large with very large indexes)
	// will be unloaded by the BNTCIndex object remains in the textIndexes cache
	// and will reload its TCIDBFile the next time it is needed.
	NSArray *allIndexes = [textIndexes allValues];
	for (BNRTextIndex *textIndex in allIndexes) {
		if ([textIndex TCIDBFile] == ti)  {
			[textIndex unloadIndexFile];
		}
	}
}

- (void)unloadTCIDBFiles
{
	NSArray *allIndexes = [textIndexes allValues];
	for (BNRTextIndex *textIndex in allIndexes) {
		[textIndex unloadIndexFile];
	}
}
#endif // kImplementCacheUnloading

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
    
    BNRClassKey *ck = [[[BNRClassKey alloc] init] autorelease];  // autorelease this to avoid annoyance of having to special-case release it if a TC creation error occurs
    [ck setKeyClass:c];
    [ck setKey:k];
    
    BNRTextIndex *ti = [textIndexes objectForKey:ck];
    
    if (!ti) {
		ti = [BNRTextIndex createIndexForClass:c
										   key:k
										atPath:path
						usesIndexFileWriteSync:usesIndexFileWriteSync
						  usesIndexCompression:usesIndexFileCompression];	// throws on failure, so no nil should escape
        [textIndexes setObject:ti forKey:ck];
}

    return [ti TCIDBFile];	
}

- (UInt32)countOfRowsInClass:(Class)c 
                matchingText:(NSString *)toMatch
                      forKey:(NSString *)key
                        list:(UInt32 **)listPtr
{
    TCIDB *ti = [self textIndexForClass:c 
                                    key:key];
	if (!ti) {
		NSLog(@"textIndexForClass failed for class:%@ key:%@key in %s", NSStringFromClass(c), key, __PRETTY_FUNCTION__);
		return 0UL;
	}
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

- (void)insertValueOfObject:(BNRStoredObject *)obj forKey:(NSString *)key withRowID:(UInt32)rowID intoTextIndex:(TCIDB *)ti
{
	NSString *value = [obj valueForKey:key];
	const char * cValue = [value cStringUsingEncoding:NSUTF8StringEncoding];
	// If a BNRStoredObject's text-indexed property value is nil or empty (@""), 
	// there's no text to index and cValue == NULL.
	// But passing NULL to tcidbput() will crash, so in that case do nothing.
	//
	// (Fixes crashes in -insertObjectInIndexes and -updateObjectInIndexes for 
	// BNRStoredObjects with nil or empty indexed strings. Remove this comment when merging with
	// main branch.)
	//
	if ( cValue ) {
		BOOL success = tcidbput(ti, rowID, cValue);
		if (!success) {
			NSLog(@"Insert of value %@ into text index for (class:%@, key:%@) failed", value, NSStringFromClass([obj class]), key);
		} 
	}
}

- (void)insertObjectInIndexes:(BNRStoredObject *)obj
{
    Class c = [obj class];
    UInt32 rowID = [obj rowID];
    NSSet *indexKeys = [c textIndexedAttributes];
    for (NSString *key in indexKeys) {
        TCIDB *ti = [self textIndexForClass:c key:key];
		
		[self insertValueOfObject:obj forKey:key withRowID:rowID intoTextIndex:ti];
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
			// For any BNRStoredObject with text-indexed properties that are currently 
			// nil or empty (@""), failure is expected, but is of no consequence. 
        }
        
		[self insertValueOfObject:obj forKey:key withRowID:rowID intoTextIndex:ti];
    }
    
      
}
@end
