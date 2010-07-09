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

#import "../BNRPersistence/BNRStore.h"
#import "../BNRPersistence/BNRDataBuffer.h"
#import "Song.h"
#import "Playlist.h"

#define BIG_SONG_COUNT (1000000)
#define SONG_COUNT (100000)
#define SONGS_PER_LIST (100)
#define PLAYLIST_COUNT ((SONG_COUNT)/(SONGS_PER_LIST))

#define SIMPLETEST_PATH "/tmp/simpletest/"
#define COMPLEXTEST_PATH "/tmp/complextest/"
#define TEXTTEST_PATH "/tmp/texttest/"
#define NAMEDBUFFER_PATH "/tmp/namedbuffertest/"

BNRStore *CreateStoreAtPath(NSString *path);
void LogElapsedTime(uint64_t start, uint64_t stop);