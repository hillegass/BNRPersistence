/*************************************************************************************************
 * The word database API of Tokyo Dystopia
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


#ifndef _TCWDB_H                         /* duplication check */
#define _TCWDB_H


#if defined(__cplusplus)
#define __TCWDB_CLINKAGEBEGIN extern "C" {
#define __TCWDB_CLINKAGEEND }
#else
#define __TCWDB_CLINKAGEBEGIN
#define __TCWDB_CLINKAGEEND
#endif
__TCWDB_CLINKAGEBEGIN


#include <tcutil.h>
#include <tchdb.h>
#include <tcbdb.h>
#include <tcqdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>



/*************************************************************************************************
 * API
 *************************************************************************************************/


#define WDBSPCCHARS    "\b\t\n\v\f\r "   /* space characters */

typedef struct {                         /* type of structure for a word database */
  void *mmtx;                            /* mutex for method */
  TCBDB *idx;                            /* internal database object */
  bool open;                             /* whether the internal database is opened */
  TCMAP *cc;                             /* cache of word tokens */
  uint64_t icsiz;                        /* capacity of the cache */
  uint32_t lcnum;                        /* max number of cached leaves */
  TCMAP *dtokens;                        /* deleted tokens */
  struct _TCIDSET *dids;                 /* deleted ID numbers */
  uint32_t etnum;                        /* expected number of tokens */
  uint8_t opts;                          /* options */
  uint32_t fwmmax;                       /* maximum number of forward matching expansion */
  bool (*synccb)(int, int, const char *, void *);  /* callback function for sync progression */
  void *syncopq;                         /* opaque for the sync callback function */
  bool (*addcb)(const char *, void *);   /* callback function for word addition progression */
  void *addopq;                          /* opaque for the word addition callback function */
} TCWDB;

enum {                                   /* enumeration for tuning options */
  WDBTLARGE = 1 << 0,                    /* use 64-bit bucket array */
  WDBTDEFLATE = 1 << 1,                  /* compress each page with Deflate */
  WDBTBZIP = 1 << 2,                     /* compress each record with BZIP2 */
  WDBTTCBS = 1 << 3                      /* compress each page with TCBS */
};

enum {                                   /* enumeration for open modes */
  WDBOREADER = 1 << 0,                   /* open as a reader */
  WDBOWRITER = 1 << 1,                   /* open as a writer */
  WDBOCREAT = 1 << 2,                    /* writer creating */
  WDBOTRUNC = 1 << 3,                    /* writer truncating */
  WDBONOLCK = 1 << 4,                    /* open without locking */
  WDBOLCKNB = 1 << 5                     /* lock without blocking */
};

enum {                                   /* enumeration for get modes */
  WDBSSUBSTR,                            /* substring matching */
  WDBSPREFIX,                            /* prefix matching */
  WDBSSUFFIX,                            /* suffix matching */
  WDBSFULL                               /* full matching */
};


/* Get the message string corresponding to an error code.
   `ecode' specifies the error code.
   The return value is the message string of the error code. */
const char *tcwdberrmsg(int ecode);


/* Create a word database object.
   The return value is the new word database object. */
TCWDB *tcwdbnew(void);


/* Delete a word database object.
   `wdb' specifies the word database object.
   If the database is not closed, it is closed implicitly.  Note that the deleted object and its
   derivatives can not be used anymore. */
void tcwdbdel(TCWDB *wdb);


/* Get the last happened error code of a word database object.
   `wdb' specifies the word database object.
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
int tcwdbecode(TCWDB *wdb);


/* Set the tuning parameters of a word database object.
   `wdb' specifies the word database object which is not opened.
   `etnum' specifies the expected number of tokens to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `opts' specifies options by bitwise-or: `WDBTLARGE' specifies that the size of the database
   can be larger than 2GB by using 64-bit bucket array, `WDBTDEFLATE' specifies that each page
   is compressed with Deflate encoding, `WDBTBZIP' specifies that each page is compressed with
   BZIP2 encoding, `WDBTTCBS' specifies that each page is compressed with TCBS encoding.
   If successful, the return value is true, else, it is false.
   Note that the tuning parameters should be set before the database is opened. */
bool tcwdbtune(TCWDB *wdb, int64_t etnum, uint8_t opts);


/* Set the caching parameters of a word database object.
   `wdb' specifies the word database object which is not opened.
   `icsiz' specifies the capacity size of the token cache.  If it is not more than 0, the default
   value is specified.  The default value is 134217728.
   `lcnum' specifies the maximum number of cached leaf nodes of B+ tree.  If it is not more than
   0, the default value is specified.  The default value is 64 for writer or 1024 for reader.
   If successful, the return value is true, else, it is false.
   Note that the caching parameters should be set before the database is opened. */
bool tcwdbsetcache(TCWDB *wdb, int64_t icsiz, int32_t lcnum);


/* Set the maximum number of forward matching expansion of a word database object.
   `wdb' specifies the word database object.
   `fwmmax' specifies the maximum number of forward matching expansion.
   If successful, the return value is true, else, it is false.
   Note that the matching parameters should be set before the database is opened. */
bool tcwdbsetfwmmax(TCWDB *wdb, uint32_t fwmmax);


/* Open a word database object.
   `wdb' specifies the word database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode: `WDBOWRITER' as a writer, `WDBOREADER' as a reader.
   If the mode is `WDBOWRITER', the following may be added by bitwise-or: `WDBOCREAT', which
   means it creates a new database if not exist, `WDBOTRUNC', which means it creates a new
   database regardless if one exists.  Both of `WDBOREADER' and `WDBOWRITER' can be added to by
   bitwise-or: `WDBONOLCK', which means it opens the database file without file locking, or
   `WDBOLCKNB', which means locking is performed without blocking.
   If successful, the return value is true, else, it is false. */
bool tcwdbopen(TCWDB *wdb, const char *path, int omode);


/* Close a word database object.
   `wdb' specifies the word database object.
   If successful, the return value is true, else, it is false.
   Update of a database is assured to be written when the database is closed.  If a writer opens
   a database but does not close it appropriately, the database will be broken. */
bool tcwdbclose(TCWDB *wdb);


/* Store a record into a word database object.
   `wdb' specifies the word database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `words' specifies a list object contains the words of the record, whose encoding should be
   UTF-8.
   If successful, the return value is true, else, it is false. */
bool tcwdbput(TCWDB *wdb, int64_t id, const TCLIST *words);


/* Store a record with a text string into a word database object.
   `wdb' specifies the word database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, whose encoding should be UTF-8.
   `delims' specifies a string containing delimiting characters of the text.  If it is `NULL',
   space characters are specified.
   If successful, the return value is true, else, it is false. */
bool tcwdbput2(TCWDB *wdb, int64_t id, const char *text, const char *delims);


/* Remove a record of a word database object.
   `wdb' specifies the word database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `words' specifies a list object contains the words of the record, which should be same as the
   stored ones.
   If successful, the return value is true, else, it is false. */
bool tcwdbout(TCWDB *wdb, int64_t id, const TCLIST *words);


/* Remove a record with a text string of a word database object.
   `wdb' specifies the word database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, which should be same as the stored one.
   `delims' specifies a string containing delimiting characters of the text.  If it is `NULL',
   space characters are specified.
   If successful, the return value is true, else, it is false. */
bool tcwdbout2(TCWDB *wdb, int64_t id, const char *text, const char *delims);


/* Search a word database.
   `wdb' specifies the word database object.
   `word' specifies the string of the word to be matched to.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcwdbsearch(TCWDB *wdb, const char *word, int *np);


/* Synchronize updated contents of a word database object with the file and the device.
   `wdb' specifies the word database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful when another process connects the same database file. */
bool tcwdbsync(TCWDB *wdb);


/* Optimize the file of a word database object.
   `wdb' specifies the word database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful to reduce the size of the database file with data fragmentation by
   successive updating. */
bool tcwdboptimize(TCWDB *wdb);


/* Remove all records of a word database object.
   `wdb' specifies the word database object connected as a writer.
   If successful, the return value is true, else, it is false. */
bool tcwdbvanish(TCWDB *wdb);


/* Copy the database file of a word database object.
   `wdb' specifies the word database object.
   `path' specifies the path of the destination file.  If it begins with `@', the trailing
   substring is executed as a command line.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code.
   The database file is assured to be kept synchronized and not modified while the copying or
   executing operation is in progress.  So, this function is useful to create a backup file of
   the database file. */
bool tcwdbcopy(TCWDB *wdb, const char *path);


/* Get the file path of a word database object.
   `wdb' specifies the word database object.
   The return value is the path of the database file or `NULL' if the object does not connect to
   any database file. */
const char *tcwdbpath(TCWDB *wdb);


/* Get the number of tokens of a word database object.
   `wdb' specifies the word database object.
   The return value is the number of tokens or 0 if the object does not connect to any database
   file. */
uint64_t tcwdbtnum(TCWDB *wdb);


/* Get the size of the database file of a word database object.
   `wdb' specifies the word database object.
   The return value is the size of the database file or 0 if the object does not connect to any
   database file. */
uint64_t tcwdbfsiz(TCWDB *wdb);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set the file descriptor for debugging output.
   `wdb' specifies the word database object.
   `fd' specifies the file descriptor for debugging output. */
void tcwdbsetdbgfd(TCWDB *wdb, int fd);


/* Get the file descriptor for debugging output.
   `wdb' specifies the word database object.
   The return value is the file descriptor for debugging output. */
int tcwdbdbgfd(TCWDB *wdb);


/* Synchronize updating contents on memory of a word database object.
   `wdb' specifies the word database object.
   `level' specifies the synchronization lavel; 0 means cache synchronization, 1 means database
   synchronization, and 2 means file synchronization.
   If successful, the return value is true, else, it is false. */
bool tcwdbmemsync(TCWDB *wdb, int level);


/* Clear the cache of a word database object.
   `qdb' specifies the word database object.
   If successful, the return value is true, else, it is false. */
bool tcwdbcacheclear(TCWDB *wdb);


/* Get the inode number of the database file of a word database object.
   `wdb' specifies the word database object.
   The return value is the inode number of the database file or 0 the object does not connect to
   any database file. */
uint64_t tcwdbinode(TCWDB *wdb);


/* Get the modification time of the database file of a word database object.
   `wdb' specifies the word database object.
   The return value is the inode number of the database file or 0 the object does not connect to
   any database file. */
time_t tcwdbmtime(TCWDB *wdb);


/* Get the options of a word database object.
   `wdb' specifies the word database object.
   The return value is the options. */
uint8_t tcwdbopts(TCWDB *wdb);


/* Get the maximum number of forward matching expansion of a word database object.
   `wdb' specifies the word database object.
   The return value is the maximum number of forward matching expansion. */
uint32_t tcwdbfwmmax(TCWDB *wdb);


/* Get the number of records in the cache of a word database object.
   `wdb' specifies the word database object.
   The return value is the number of records in the cache. */
uint32_t tcwdbcnum(TCWDB *wdb);


/* Set the callback function for sync progression of a word database object.
   `wdb' specifies the word database object.
   `cb' specifies the pointer to the callback function for sync progression.  Its first argument
   specifies the number of tokens to be synchronized.  Its second argument specifies the number
   of processed tokens.  Its third argument specifies the message string.  The fourth argument
   specifies an arbitrary pointer.  Its return value should be true usually, or false if the sync
   operation should be terminated.
   `opq' specifies the arbitrary pointer to be given to the callback function. */
void tcwdbsetsynccb(TCWDB *wdb, bool (*cb)(int, int, const char *, void *), void *opq);


/* Set the callback function for word addition of a word database object.
   `wdb' specifies the word database object.
   `cb' specifies the pointer to the callback function for sync progression.  Its first argument
   specifies the pointer to the region of a word.  Its second argument specifies an arbitrary
   pointer.  Its return value should be true usually, or false if the word addition operation
   should be terminated.
   `opq' specifies the arbitrary pointer to be given to the callback function. */
void tcwdbsetaddcb(TCWDB *wdb, bool (*cb)(const char *, void *), void *opq);



__TCWDB_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
