/*************************************************************************************************
 * The q-gram database API of Tokyo Dystopia
 *                                                      Copyright (C) 2007-2009 Mikio Hirabayashi
 * This file is part of Tokyo Dystopia.
 * Tokyo Dystopia is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Dystopia is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Dystopia; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#ifndef _TCQDB_H                         /* duplication check */
#define _TCQDB_H


#if defined(__cplusplus)
#define __TCQDB_CLINKAGEBEGIN extern "C" {
#define __TCQDB_CLINKAGEEND }
#else
#define __TCQDB_CLINKAGEBEGIN
#define __TCQDB_CLINKAGEEND
#endif
__TCQDB_CLINKAGEBEGIN


#include <tcutil.h>
#include <tchdb.h>
#include <tcbdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>



/*************************************************************************************************
 * API
 *************************************************************************************************/


typedef struct {                         /* type of structure for a q-gram database */
  void *mmtx;                            /* mutex for method */
  TCBDB *idx;                            /* internal database object */
  bool open;                             /* whether the internal database is opened */
  TCMAP *cc;                             /* cache of q-gram tokens */
  uint64_t icsiz;                        /* capacity of the cache */
  uint32_t lcnum;                        /* max number of cached leaves */
  TCMAP *dtokens;                        /* deleted tokens */
  struct _TCIDSET *dids;                 /* deleted ID numbers */
  uint32_t etnum;                        /* expected number of tokens */
  uint8_t opts;                          /* options */
  uint32_t fwmmax;                       /* maximum number of forward matching expansion */
  bool (*synccb)(int, int, const char *, void *);  /* callback function for sync progression */
  void *syncopq;                         /* opaque for the sync callback function */
} TCQDB;

enum {                                   /* enumeration for tuning options */
  QDBTLARGE = 1 << 0,                    /* use 64-bit bucket array */
  QDBTDEFLATE = 1 << 1,                  /* compress each page with Deflate */
  QDBTBZIP = 1 << 2,                     /* compress each record with BZIP2 */
  QDBTTCBS = 1 << 3                      /* compress each page with TCBS */
};

enum {                                   /* enumeration for open modes */
  QDBOREADER = 1 << 0,                   /* open as a reader */
  QDBOWRITER = 1 << 1,                   /* open as a writer */
  QDBOCREAT = 1 << 2,                    /* writer creating */
  QDBOTRUNC = 1 << 3,                    /* writer truncating */
  QDBONOLCK = 1 << 4,                    /* open without locking */
  QDBOLCKNB = 1 << 5                     /* lock without blocking */
};

enum {                                   /* enumeration for get modes */
  QDBSSUBSTR,                            /* substring matching */
  QDBSPREFIX,                            /* prefix matching */
  QDBSSUFFIX,                            /* suffix matching */
  QDBSFULL                               /* full matching */
};


/* String containing the version information. */
extern const char *tdversion;


/* Get the message string corresponding to an error code.
   `ecode' specifies the error code.
   The return value is the message string of the error code. */
const char *tcqdberrmsg(int ecode);


/* Create a q-gram database object.
   The return value is the new q-gram database object. */
TCQDB *tcqdbnew(void);


/* Delete a q-gram database object.
   `qdb' specifies the q-gram database object.
   If the database is not closed, it is closed implicitly.  Note that the deleted object and its
   derivatives can not be used anymore. */
void tcqdbdel(TCQDB *qdb);


/* Get the last happened error code of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the last happened error code.
   The following error code is defined: `TCESUCCESS' for success, `TCETHREAD' for threading
   error, `TCEINVALID' for invalid operation, `TCENOFILE' for file not found, `TCENOPERM' for no
   permission, `TCEMETA' for invalid meta data, `TCERHEAD' for invalid record header, `TCEOPEN'
   for open error, `TCECLOSE' for close error, `TCETRUNC' for trunc error, `TCESYNC' for sync
   error, `TCESTAT' for stat error, `TCESEEK' for seek error, `TCEREAD' for read error,
   `TCEWRITE' for write error, `TCEMMAP' for mmap error, `TCELOCK' for lock error, `TCEUNLINK'
   for unlink error, `TCERENAME' for rename error, `TCEMKDIR' for mkdir error, `TCERMDIR' for
   rmdir error, `TCEKEEP' for existing record, `TCENOREC' for no record found, and `TCEMISC' for
   miscellaneous error. */
int tcqdbecode(TCQDB *qdb);


/* Set the tuning parameters of a q-gram database object.
   `qdb' specifies the q-gram database object which is not opened.
   `etnum' specifies the expected number of tokens to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `opts' specifies options by bitwise-or: `QDBTLARGE' specifies that the size of the database
   can be larger than 2GB by using 64-bit bucket array, `QDBTDEFLATE' specifies that each page
   is compressed with Deflate encoding, `QDBTBZIP' specifies that each page is compressed with
   BZIP2 encoding, `QDBTTCBS' specifies that each page is compressed with TCBS encoding.
   If successful, the return value is true, else, it is false.
   Note that the tuning parameters should be set before the database is opened. */
bool tcqdbtune(TCQDB *qdb, int64_t etnum, uint8_t opts);


/* Set the caching parameters of a q-gram database object.
   `qdb' specifies the q-gram database object which is not opened.
   `icsiz' specifies the capacity size of the token cache.  If it is not more than 0, the default
   value is specified.  The default value is 134217728.
   `lcnum' specifies the maximum number of cached leaf nodes of B+ tree.  If it is not more than
   0, the default value is specified.  The default value is 64 for writer or 1024 for reader.
   If successful, the return value is true, else, it is false.
   Note that the caching parameters should be set before the database is opened. */
bool tcqdbsetcache(TCQDB *qdb, int64_t icsiz, int32_t lcnum);


/* Set the maximum number of forward matching expansion of a q-gram database object.
   `qdb' specifies the q-gram database object.
   `fwmmax' specifies the maximum number of forward matching expansion.
   If successful, the return value is true, else, it is false.
   Note that the matching parameters should be set before the database is opened. */
bool tcqdbsetfwmmax(TCQDB *qdb, uint32_t fwmmax);


/* Open a q-gram database object.
   `qdb' specifies the q-gram database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode: `QDBOWRITER' as a writer, `QDBOREADER' as a reader.
   If the mode is `QDBOWRITER', the following may be added by bitwise-or: `QDBOCREAT', which
   means it creates a new database if not exist, `QDBOTRUNC', which means it creates a new
   database regardless if one exists.  Both of `QDBOREADER' and `QDBOWRITER' can be added to by
   bitwise-or: `QDBONOLCK', which means it opens the database file without file locking, or
   `QDBOLCKNB', which means locking is performed without blocking.
   If successful, the return value is true, else, it is false. */
bool tcqdbopen(TCQDB *qdb, const char *path, int omode);


/* Close a q-gram database object.
   `qdb' specifies the q-gram database object.
   If successful, the return value is true, else, it is false.
   Update of a database is assured to be written when the database is closed.  If a writer opens
   a database but does not close it appropriately, the database will be broken. */
bool tcqdbclose(TCQDB *qdb);


/* Store a record into a q-gram database object.
   `qdb' specifies the q-gram database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, whose encoding should be UTF-8.
   If successful, the return value is true, else, it is false. */
bool tcqdbput(TCQDB *qdb, int64_t id, const char *text);


/* Remove a record of a q-gram database object.
   `qdb' specifies the q-gram database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, which should be same as the stored one.
   If successful, the return value is true, else, it is false. */
bool tcqdbout(TCQDB *qdb, int64_t id, const char *text);


/* Search a q-gram database.
   `qdb' specifies the q-gram database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode: `QDBSSUBSTR' as substring matching, `QDBSPREFIX' as prefix
   matching, `QDBSSUFFIX' as suffix matching, or `QDBSFULL' as full matching.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcqdbsearch(TCQDB *qdb, const char *word, int smode, int *np);


/* Synchronize updated contents of a q-gram database object with the file and the device.
   `qdb' specifies the q-gram database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful when another process connects the same database file. */
bool tcqdbsync(TCQDB *qdb);


/* Optimize the file of a q-gram database object.
   `qdb' specifies the q-gram database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful to reduce the size of the database file with data fragmentation by
   successive updating. */
bool tcqdboptimize(TCQDB *qdb);


/* Remove all records of a q-gram database object.
   `qdb' specifies the q-gram database object connected as a writer.
   If successful, the return value is true, else, it is false. */
bool tcqdbvanish(TCQDB *qdb);


/* Copy the database file of a q-gram database object.
   `qdb' specifies the q-gram database object.
   `path' specifies the path of the destination file.  If it begins with `@', the trailing
   substring is executed as a command line.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code.
   The database file is assured to be kept synchronized and not modified while the copying or
   executing operation is in progress.  So, this function is useful to create a backup file of
   the database file. */
bool tcqdbcopy(TCQDB *qdb, const char *path);


/* Get the file path of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the path of the database file or `NULL' if the object does not connect to
   any database file. */
const char *tcqdbpath(TCQDB *qdb);


/* Get the number of tokens of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the number of tokens or 0 if the object does not connect to any database
   file. */
uint64_t tcqdbtnum(TCQDB *qdb);


/* Get the size of the database file of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the size of the database file or 0 if the object does not connect to any
   database file. */
uint64_t tcqdbfsiz(TCQDB *qdb);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


#define _TD_VERSION    "0.9.13"
#define _TD_LIBVER     114
#define _TD_FORMATVER  "0.9"

#define QDBSYNCMSGF    "started"         /* first message of sync progression */
#define QDBSYNCMSGL    "finished"        /* last message of sync progression */

typedef struct {                         /* type of structure for a result set */
  uint64_t *ids;                         /* array of ID numbers */
  int num;                               /* number of the array */
} QDBRSET;

typedef struct _TCIDSET {                /* type of structure for an ID set */
  uint64_t *buckets;                     /* bucket array */
  uint32_t bnum;                         /* number of buckets */
  TCMAP *trails;                         /* map of trailing records */
} TCIDSET;

enum {                                   /* enumeration for text normalization options */
  TCTNLOWER = 1 << 0,                    /* into lower cases */
  TCTNNOACC = 1 << 1,                    /* into ASCII alphabets */
  TCTNSPACE = 1 << 2                     /* into ASCII space */
};


/* Set the file descriptor for debugging output.
   `qdb' specifies the q-gram database object.
   `fd' specifies the file descriptor for debugging output. */
void tcqdbsetdbgfd(TCQDB *qdb, int fd);


/* Get the file descriptor for debugging output.
   `qdb' specifies the q-gram database object.
   The return value is the file descriptor for debugging output. */
int tcqdbdbgfd(TCQDB *qdb);


/* Synchronize updating contents on memory of a q-gram database object.
   `qdb' specifies the q-gram database object.
   `level' specifies the synchronization lavel; 0 means cache synchronization, 1 means database
   synchronization, and 2 means file synchronization.
   If successful, the return value is true, else, it is false. */
bool tcqdbmemsync(TCQDB *qdb, int level);


/* Clear the cache of a q-gram database object.
   `qdb' specifies the q-gram database object.
   If successful, the return value is true, else, it is false. */
bool tcqdbcacheclear(TCQDB *qdb);


/* Get the inode number of the database file of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the inode number of the database file or 0 the object does not connect to
   any database file. */
uint64_t tcqdbinode(TCQDB *qdb);


/* Get the modification time of the database file of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the inode number of the database file or 0 the object does not connect to
   any database file. */
time_t tcqdbmtime(TCQDB *qdb);


/* Get the options of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the options. */
uint8_t tcqdbopts(TCQDB *qdb);


/* Get the maximum number of forward matching expansion of a q-gram database object.
   `qdb' specifies the q-gram database object.
   The return value is the maximum number of forward matching expansion. */
uint32_t tcqdbfwmmax(TCQDB *qdb);


/* Get the number of records in the cache of a q-gram database object.
   `wdb' specifies the word database object.
   The return value is the number of records in the cache. */
uint32_t tcqdbcnum(TCQDB *qdb);


/* Set the callback function for sync progression of a q-gram database object.
   `qdb' specifies the q-gram database object.
   `cb' specifies the pointer to the callback function for sync progression.  Its first argument
   specifies the number of tokens to be synchronized.  Its second argument specifies the number
   of processed tokens.  Its third argument specifies the message string.  The fourth argument
   specifies an arbitrary pointer.  Its return value should be true usually, or false if the sync
   operation should be terminated.
   `opq' specifies the arbitrary pointer to be given to the callback function. */
void tcqdbsetsynccb(TCQDB *qdb, bool (*cb)(int, int, const char *, void *), void *opq);


/* Merge multiple result sets by union.
   `rsets' specifies the pointer to the array of result sets.
   `rsnum' specifies the number of the array.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the result.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcqdbresunion(QDBRSET *rsets, int rsnum, int *np);


/* Merge multiple result sets by intersection.
   `rsets' specifies the pointer to the array of result sets.
   `rsnum' specifies the number of the array.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the result.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcqdbresisect(QDBRSET *rsets, int rsnum, int *np);


/* Merge multiple result sets by difference.
   `rsets' specifies the pointer to the array of result sets.
   `rsnum' specifies the number of the array.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the result.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcqdbresdiff(QDBRSET *rsets, int rsnum, int *np);


/* Normalize a text.
   `text' specifies the string of the record, whose encoding should be UTF-8.
   `opts' specifies options by bitwise-or: `TCTNLOWER' specifies that alphabetical characters are
   normalized into lower cases, `TCTNNOACC' specifies that alphabetical characters with accent
   marks are normalized without accent marks, `TCTNSPACE' specifies that white space characters
   are normalized into the ASCII space and they are squeezed into one. */
void tctextnormalize(char *text, int opts);


/* Create an ID set object.
   `bnum' specifies the number of the buckets.
   The return value is the new ID set object. */
TCIDSET *tcidsetnew(uint32_t bnum);


/* Delete an ID set object.
   `idset' specifies the ID set object. */
void tcidsetdel(TCIDSET *idset);


/* Mark an ID number of an ID set object.
   `idset' specifies the ID set object.
   `id' specifies the ID number. */
void tcidsetmark(TCIDSET *idset, int64_t id);


/* Check an ID of an ID set object.
   `idset' specifies the ID set object.
   `id' specifies the ID number.
   The return value is true if the ID number is marked, else, it is false. */
bool tcidsetcheck(TCIDSET *idset, int64_t id);


/* Clear an ID set object.
   `idset' specifies the ID set object. */
void tcidsetclear(TCIDSET *idset);



__TCQDB_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
