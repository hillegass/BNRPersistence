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
    NSError *error = nil;
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
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
