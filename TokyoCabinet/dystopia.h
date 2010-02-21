/*************************************************************************************************
 * The core API of Tokyo Dystopia
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


#ifndef _DYSTOPIA_H                      /* duplication check */
#define _DYSTOPIA_H


#if defined(__cplusplus)
#define __DYSTOPIA_CLINKAGEBEGIN extern "C" {
#define __DYSTOPIA_CLINKAGEEND }
#else
#define __DYSTOPIA_CLINKAGEBEGIN
#define __DYSTOPIA_CLINKAGEEND
#endif
__DYSTOPIA_CLINKAGEBEGIN


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


#define IDBQDBMAX      32                /* maximum number of the internal databases */

typedef struct {                         /* type of structure for an indexed database object */
  void *mmtx;                            /* mutex for method */
  char *path;                            /* path of the database directory */
  bool wmode;                            /* whether to be writable */
  uint8_t qopts;                         /* tuning options of q-gram databases */
  int qomode;                            /* open mode of q-gram databases */
  TCHDB *txdb;                           /* text database object */
  TCQDB *idxs[IDBQDBMAX];                /* q-gram database objects */
  uint8_t inum;                          /* number of the q-gram database objects */
  uint8_t cnum;                          /* current number of the q-gram database */
  uint32_t ernum;                        /* expected number of records */
  uint32_t etnum;                        /* expected number of tokens */
  uint64_t iusiz;                        /* unit size of each index file */
  uint8_t opts;                          /* options */
  bool (*synccb)(int, int, const char *, void *);  /* callback function for sync progression */
  void *syncopq;                         /* opaque for the sync callback function */
  uint8_t exopts;                        /* expert options */
} TCIDB;

enum {                                   /* enumeration for tuning options */
  IDBTLARGE = 1 << 0,                    /* use 64-bit bucket array */
  IDBTDEFLATE = 1 << 1,                  /* compress each page with Deflate */
  IDBTBZIP = 1 << 2,                     /* compress each record with BZIP2 */
  IDBTTCBS = 1 << 3                      /* compress each page with TCBS */
};

enum {                                   /* enumeration for open modes */
  IDBOREADER = 1 << 0,                   /* open as a reader */
  IDBOWRITER = 1 << 1,                   /* open as a writer */
  IDBOCREAT = 1 << 2,                    /* writer creating */
  IDBOTRUNC = 1 << 3,                    /* writer truncating */
  IDBONOLCK = 1 << 4,                    /* open without locking */
  IDBOLCKNB = 1 << 5                     /* lock without blocking */
};

enum {                                   /* enumeration for get modes */
  IDBSSUBSTR = QDBSSUBSTR,               /* substring matching */
  IDBSPREFIX = QDBSPREFIX,               /* prefix matching */
  IDBSSUFFIX = QDBSSUFFIX,               /* suffix matching */
  IDBSFULL = QDBSFULL,                   /* full matching */
  IDBSTOKEN,                             /* token matching */
  IDBSTOKPRE,                            /* token prefix matching */
  IDBSTOKSUF                             /* token suffix matching */
};


/* Get the message string corresponding to an error code.
   `ecode' specifies the error code.
   The return value is the message string of the error code. */
const char *tcidberrmsg(int ecode);


/* Create an indexed database object.
   The return value is the new indexed database object. */
TCIDB *tcidbnew(void);


/* Delete an indexed database object.
   `idb' specifies the indexed database object.
   If the database is not closed, it is closed implicitly.  Note that the deleted object and its
   derivatives can not be used anymore. */
void tcidbdel(TCIDB *idb);


/* Get the last happened error code of an indexed database object.
   `idb' specifies the indexed database object.
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
int tcidbecode(TCIDB *idb);


/* Set the tuning parameters of an indexed database object.
   `idb' specifies the indexed database object which is not opened.
   `ernum' specifies the expected number of records to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `etnum' specifies the expected number of tokens to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `iusiz' specifies the unit size of each index file.  If it is not more than 0, the default
   value is specified.  The default value is 536870912.
   `opts' specifies options by bitwise-or: `IDBTLARGE' specifies that the size of the database
   can be larger than 2GB by using 64-bit bucket array, `IDBTDEFLATE' specifies that each page
   is compressed with Deflate encoding, `IDBTBZIP' specifies that each page is compressed with
   BZIP2 encoding, `IDBTTCBS' specifies that each page is compressed with TCBS encoding.
   If successful, the return value is true, else, it is false.
   Note that the tuning parameters should be set before the database is opened. */
bool tcidbtune(TCIDB *idb, int64_t ernum, int64_t etnum, int64_t iusiz, uint8_t opts);


/* Set the caching parameters of an indexed database object.
   `idb' specifies the indexed database object which is not opened.
   `icsiz' specifies the capacity size of the token cache.  If it is not more than 0, the default
   value is specified.  The default value is 134217728.
   `lcnum' specifies the maximum number of cached leaf nodes of B+ tree.  If it is not more than
   0, the default value is specified.  The default value is 64 for writer or 1024 for reader.
   If successful, the return value is true, else, it is false.
   Note that the caching parameters should be set before the database is opened. */
bool tcidbsetcache(TCIDB *idb, int64_t icsiz, int32_t lcnum);


/* Set the maximum number of forward matching expansion of an indexed database object.
   `idb' specifies the indexed database object.
   `fwmmax' specifies the maximum number of forward matching expansion.
   If successful, the return value is true, else, it is false.
   Note that the matching parameters should be set before the database is opened. */
bool tcidbsetfwmmax(TCIDB *idb, uint32_t fwmmax);


/* Open an indexed database object.
   `idb' specifies the indexed database object.
   `path' specifies the path of the database directory.
   `omode' specifies the connection mode: `IDBOWRITER' as a writer, `IDBOREADER' as a reader.
   If the mode is `IDBOWRITER', the following may be added by bitwise-or: `IDBOCREAT', which
   means it creates a new database if not exist, `IDBOTRUNC', which means it creates a new
   database regardless if one exists.  Both of `IDBOREADER' and `IDBOWRITER' can be added to by
   bitwise-or: `IDBONOLCK', which means it opens the database directory without file locking, or
   `IDBOLCKNB', which means locking is performed without blocking.
   If successful, the return value is true, else, it is false. */
bool tcidbopen(TCIDB *idb, const char *path, int omode);


/* Close an indexed database object.
   `idb' specifies the indexed database object.
   If successful, the return value is true, else, it is false.
   Update of a database is assured to be written when the database is closed.  If a writer opens
   a database but does not close it appropriately, the database will be broken. */
bool tcidbclose(TCIDB *idb);


/* Store a record into an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, whose encoding should be UTF-8.
   If successful, the return value is true, else, it is false. */
bool tcidbput(TCIDB *idb, int64_t id, const char *text);


/* Remove a record of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is true, else, it is false. */
bool tcidbout(TCIDB *idb, int64_t id);


/* Retrieve a record of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is the string of the corresponding record, else, it is `NULL'.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcidbget(TCIDB *idb, int64_t id);


/* Search an indexed database.
   `idb' specifies the indexed database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode: `IDBSSUBSTR' as substring matching, `IDBSPREFIX' as prefix
   matching, `IDBSSUFFIX' as suffix matching, `IDBSFULL' as full matching, `IDBSTOKEN' as token
   matching, `IDBSTOKPRE' as token prefix matching, or `IDBSTOKSUF' as token suffix matching.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcidbsearch(TCIDB *idb, const char *word, int smode, int *np);


/* Search an indexed database with a compound expression.
   `idb' specifies the indexed database object.
   `expr' specifies the string of the compound expression.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcidbsearch2(TCIDB *idb, const char *expr, int *np);


/* Initialize the iterator of an indexed database object.
   `idb' specifies the indexed database object.
   If successful, the return value is true, else, it is false.
   The iterator is used in order to access the ID number of every record stored in a database. */
bool tcidbiterinit(TCIDB *idb);


/* Get the next ID number of the iterator of an indexed database object.
   `idb' specifies the indexed database object.
   If successful, the return value is the ID number of the next record, else, it is 0.  0 is
   returned when no record is to be get out of the iterator.
   It is possible to access every record by iteration of calling this function.  It is allowed to
   update or remove records whose keys are fetched while the iteration.  However, it is not
   assured if updating the database is occurred while the iteration.  Besides, the order of this
   traversal access method is arbitrary, so it is not assured that the order of storing matches
   the one of the traversal access. */
uint64_t tcidbiternext(TCIDB *idb);


/* Synchronize updated contents of an indexed database object with the files and the device.
   `idb' specifies the indexed database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful when another process connects the same database directory. */
bool tcidbsync(TCIDB *idb);


/* Optimize the files of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful to reduce the size of the database files with data fragmentation by
   successive updating. */
bool tcidboptimize(TCIDB *idb);


/* Remove all records of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   If successful, the return value is true, else, it is false. */
bool tcidbvanish(TCIDB *idb);


/* Copy the database directory of an indexed database object.
   `idb' specifies the indexed database object.
   `path' specifies the path of the destination directory.  If it begins with `@', the trailing
   substring is executed as a command line.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code.
   The database directory is assured to be kept synchronized and not modified while the copying or
   executing operation is in progress.  So, this function is useful to create a backup directory
   of the database directory. */
bool tcidbcopy(TCIDB *idb, const char *path);


/* Get the directory path of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the path of the database directory or `NULL' if the object does not
   connect to any database directory. */
const char *tcidbpath(TCIDB *idb);


/* Get the number of records of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the number of records or 0 if the object does not connect to any database
   directory. */
uint64_t tcidbrnum(TCIDB *idb);


/* Get the total size of the database files of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the size of the database files or 0 if the object does not connect to any
   database directory. */
uint64_t tcidbfsiz(TCIDB *idb);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


enum {                                   /* enumeration for expert options */
  IDBXNOTXT = 1 << 0                     /* no text mode */
};


/* Set the file descriptor for debugging output.
   `idb' specifies the indexed database object.
   `fd' specifies the file descriptor for debugging output. */
void tcidbsetdbgfd(TCIDB *idb, int fd);


/* Get the file descriptor for debugging output.
   `idb' specifies the indexed database object.
   The return value is the file descriptor for debugging output. */
int tcidbdbgfd(TCIDB *idb);


/* Synchronize updating contents on memory of an indexed database object.
   `idb' specifies the indexed database object.
   `level' specifies the synchronization lavel; 0 means cache synchronization, 1 means database
   synchronization, and 2 means file synchronization.
   If successful, the return value is true, else, it is false. */
bool tcidbmemsync(TCIDB *idb, int level);


/* Get the inode number of the database directory of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the inode number of the database directory or 0 the object does not
   connect to any database directory. */
uint64_t tcidbinode(TCIDB *idb);


/* Get the modification time of the database directory of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the inode number of the database directory or 0 the object does not
   connect to any database directory. */
time_t tcidbmtime(TCIDB *idb);


/* Get the options of an indexed database object.
   `idb' specifies the indexed database object.
   The return value is the options. */
uint8_t tcidbopts(TCIDB *idb);


/* Set the callback function for sync progression of an indexed database object.
   `idb' specifies the indexed database object.
   `cb' specifies the pointer to the callback function for sync progression.  Its first argument
   specifies the number of tokens to be synchronized.  Its second argument specifies the number
   of processed tokens.  Its third argument specifies the message string.  The fourth argument
   specifies an arbitrary pointer.  Its return value should be true usually, or false if the sync
   operation should be terminated.
   `opq' specifies the arbitrary pointer to be given to the callback function. */
void tcidbsetsynccb(TCIDB *idb, bool (*cb)(int, int, const char *, void *), void *opq);


/* Set the expert options of an indexed database object.
   `idb' specifies the indexed database object.
   `exopts' specifies options by bitwise-or: `IDBXNOTXT' specifies that the text database does
   not record any record. */
void tcidbsetexopts(TCIDB *idb, uint32_t exopts);



__DYSTOPIA_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
