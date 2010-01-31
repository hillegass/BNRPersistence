/*
 *  @file SpeedTest.h
 *  @author Jeremy W. Sherman (Big Nerd Ranch, Inc.)
 *  @date 2010-01-28
 *
 *  Shared header file for all (Simple|Complex)(Insert|Fetch)Tests.
 */


#import <Foundation/Foundation.h>

#import <mach/mach_time.h>
#import <mach/mach_error.h>

#define BIG_SONG_COUNT (1000000)
#define SONG_COUNT (1000000)
#define SONGS_PER_LIST (100)
// Ensure playlists is large enough to hold a SONG_COUNT not evenly divisible by
// SONGS_PER_LIST.
#define PLAYLIST_COUNT (((SONG_COUNT) + (SONGS_PER_LIST) - 1)/(SONGS_PER_LIST))

#define SIMPLETEST_PATH @"/tmp/simpletest.sql"
#define COMPLEXTEST_PATH @"/tmp/complextest.sql"

NSManagedObjectContext *managedObjectContext(NSString *name);
void LogElapsedTime(uint64_t start, uint64_t stop);