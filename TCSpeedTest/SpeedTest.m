/*
 *  @file SpeedTest.m
 *  @author Jeremy W. Sherman (Big Nerd Ranch, Inc.)
 *  @date 2010-01-28
 */


#import "SpeedTest.h"
#import <inttypes.h>
#import <stdbool.h>
#import "BNRTCBackend.h"

static bool GetElapsedMs(uint64_t start, uint64_t stop, uint64_t *out_elapsed);

BNRStore *
CreateStoreAtPath(NSString *path) {
	
	// For fault tolerance freaks, turn on transactions and sync-every-write.
	// Store will be nearly impervious to power outages/crashes, but bulk 
	// inserts/updates will be much slower. writeSync can only be specified
	// when a store is opened (TC requirement). For best of both worlds, 
	// before bulk operations such as imports or all-object upgrades, 
	// close store, reopen with writeSync off, perform bulk operations, 
	// then close and reopen with writeSync on for day-to-day use.
	//
	// Transactions can be toggled without closing and reopening, and 
	// provide some protection; all writes in a save will succeed, or none. 
	// But even so, due to caches within storage devices etc, writes may not 
	// yet be on the physical storage medium. So in event of a crash/power loss, 
	// "successful" writes could still be lost. To prevent this, writeSync
	// causes every write to be flushed to the storage medium. However with
	// large bulk writes the loss of caching will cause a dramatic loss
	// of speed versus unsynced writes. For such cases, writing without
	// sync is much faster. 
	//
	// Note that the design of TC is such that crashes are very unlikely to
	// corrupt the store even if transactions and writeSync are not used.
	// However in such cases it is possible that written data that has not
	// yet been flushed from caches within storage devices or from system caches might be lost.
	// One way to cause data loss is to kill a running app in the debugger after making a store edit.
	// If writeSync is on, data loss is difficult to achieve; if it is off, not so difficult.
	BOOL useTransactionsFlag = NO;
	BOOL useWriteSyncFlag = NO;
    NSError *error = nil;
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
											   useTransactions:useTransactionsFlag 
										useWriteSyncronization:useWriteSyncFlag
                                                         error:&error];
    if (!backend) {
        NSLog(@"%s: Unable to create database at %@: %@",
              getprogname(), path, error);
        return nil;
    }
    
    BNRStore *store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    
    // For speed/size freaks -- turnoff per-instance versioning
    [store setUsesPerInstanceVersioning:YES];

    return store;
}

// Returns true if |*out_elapsed| contains milliseconds, false if simply
// the elapsed timepbase units.
static bool
GetElapsedMs(uint64_t start, uint64_t stop, uint64_t *out_elapsed) {
    NSCAssert1(NULL != out_elapsed, @"%s: NULL value passed for out_elapsed - "
               @"either supply a pointer or omit this function call",
               __FUNCTION__);
    NSCAssert3(stop >= start, @"%s: stop timestamp (%"PRIu64") prior to "
               @"start timestamp ("PRIu64") - did you reverse the arguments?",
              __FUNCTION__, stop, start);
    uint64_t elapsed = stop - start;

    mach_timebase_info_data_t info;
    mach_error_t merr = mach_timebase_info(&info);
    if (KERN_SUCCESS != merr) {
        fprintf(stderr, "%s: mach_timebase_info failed [%s]: %s\n",
                getprogname(), mach_error_type(merr), mach_error_string(merr));
        return false;
    }

    uint64_t nanosecs = elapsed * info.numer / info.denom;
    *out_elapsed = nanosecs / 1000000;
    return true;
}

void
LogElapsedTime(uint64_t start, uint64_t stop) {
    uint64_t ms = 0;
    if (GetElapsedMs(start, stop, &ms)) {
        fprintf(stderr, "%s: Elapsed time: %"PRIu64" ms\n",
                getprogname(), ms);
    } else {
        fprintf(stderr, "%s: Elapsed mach timebase units: %"PRIu64"\n",
                getprogname(), ms);
    }
}
