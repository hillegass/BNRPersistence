/*************************************************************************************************
 * The simple API of Tokyo Dystopia
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


#ifndef _LAPUTA_H                        /* duplication check */
#define _LAPUTA_H


#if defined(__cplusplus)
#define __LAPUTA_CLINKAGEBEGIN extern "C" {
#define __LAPUTA_CLINKAGEEND }
#else
#define __LAPUTA_CLINKAGEBEGIN
#define __LAPUTA_CLINKAGEEND
#endif
__LAPUTA_CLINKAGEBEGIN


#include <tcutil.h>
#include <tchdb.h>
#include <tcbdb.h>
#include <tcqdb.h>
#include <tcwdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>



/*************************************************************************************************
 * API
 *************************************************************************************************/


#define JDBWDBMAX      32                /* maximum number of the internal databases */

typedef struct {                         /* type of structure for a tagged database object */
  void *mmtx;                            /* mutex for method */
  char *path;                            /* path of the database directory */
  bool wmode;                            /* whether to be writable */
  uint8_t wopts;                         /* tuning options of word databases */
  int womode;                            /* open mode of word databases */
  TCHDB *txdb;                           /* text database object */
  TCBDB *lsdb;                           /* word list database object */
  TCWDB *idxs[JDBWDBMAX];                /* word database objects */
  uint8_t inum;                          /* number of the word database objects */
  uint8_t cnum;                          /* current number of the word database */
  uint32_t ernum;                        /* expected number of records */
  uint32_t etnum;                        /* expected number of tokens */
  uint64_t iusiz;                        /* unit size of each index file */
  uint8_t opts;                          /* options */
  bool (*synccb)(int, int, const char *, void *);  /* callback function for sync progression */
  void *syncopq;                         /* opaque for the sync callback function */
  uint8_t exopts;                        /* expert options */
} TCJDB;

enum {                                   /* enumeration for tuning options */
  JDBTLARGE = 1 << 0,                    /* use 64-bit bucket array */
  JDBTDEFLATE = 1 << 1,                  /* compress each page with Deflate */
  JDBTBZIP = 1 << 2,                     /* compress each record with BZIP2 */
  JDBTTCBS = 1 << 3                      /* compress each page with TCBS */
};

enum {                                   /* enumeration for open modes */
  JDBOREADER = 1 << 0,                   /* open as a reader */
  JDBOWRITER = 1 << 1,                   /* open as a writer */
  JDBOCREAT = 1 << 2,                    /* writer creating */
  JDBOTRUNC = 1 << 3,                    /* writer truncating */
  JDBONOLCK = 1 << 4,                    /* open without locking */
  JDBOLCKNB = 1 << 5                     /* lock without blocking */
};

enum {                                   /* enumeration for get modes */
  JDBSSUBSTR,                            /* substring matching */
  JDBSPREFIX,                            /* prefix matching */
  JDBSSUFFIX,                            /* suffix matching */
  JDBSFULL                               /* full matching */
};


/* Get the message string corresponding to an error code.
   `ecode' specifies the error code.
   The return value is the message string of the error code. */
const char *tcjdberrmsg(int ecode);


/* Create a tagged database object.
   The return value is the new tagged database object. */
TCJDB *tcjdbnew(void);


/* Delete a tagged database object.
   `jdb' specifies the tagged database object.
   If the database is not closed, it is closed implicitly.  Note that the deleted object and its
   derivatives can not be used anymore. */
void tcjdbdel(TCJDB *jdb);


/* Get the last happened error code of a tagged database object.
   `jdb' specifies the tagged database object.
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
int tcjdbecode(TCJDB *jdb);


/* Set the tuning parameters of a tagged database object.
   `jdb' specifies the tagged database object which is not opened.
   `ernum' specifies the expected number of records to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `etnum' specifies the expected number of tokens to be stored.  If it is not more than 0, the
   default value is specified.  The default value is 1000000.
   `iusiz' specifies the unit size of each index file.  If it is not more than 0, the default
   value is specified.  The default value is 536870912.
   `opts' specifies options by bitwise-or: `JDBTLARGE' specifies that the size of the database
   can be larger than 2GB by using 64-bit bucket array, `JDBTDEFLATE' specifies that each page
   is compressed with Deflate encoding, `JDBTBZIP' specifies that each page is compressed with
   BZIP2 encoding, `JDBTTCBS' specifies that each page is compressed with TCBS encoding.
   If successful, the return value is true, else, it is false.
   Note that the tuning parameters should be set before the database is opened. */
bool tcjdbtune(TCJDB *jdb, int64_t ernum, int64_t etnum, int64_t iusiz, uint8_t opts);


/* Set the caching parameters of a tagged database object.
   `jdb' specifies the tagged database object which is not opened.
   `icsiz' specifies the capacity size of the token cache.  If it is not more than 0, the default
   value is specified.  The default value is 134217728.
   `lcnum' specifies the maximum number of cached leaf nodes of B+ tree.  If it is not more than
   0, the default value is specified.  The default value is 64 for writer or 1024 for reader.
   If successful, the return value is true, else, it is false.
   Note that the caching parameters should be set before the database is opened. */
bool tcjdbsetcache(TCJDB *jdb, int64_t icsiz, int32_t lcnum);


/* Set the maximum number of forward matching expansion of a tagged database object.
   `jdb' specifies the tagged database object.
   `fwmmax' specifies the maximum number of forward matching expansion.
   If successful, the return value is true, else, it is false.
   Note that the matching parameters should be set before the database is opened. */
bool tcjdbsetfwmmax(TCJDB *jdb, uint32_t fwmmax);


/* Open a tagged database object.
   `jdb' specifies the tagged database object.
   `path' specifies the path of the database directory.
   `omode' specifies the connection mode: `JDBOWRITER' as a writer, `JDBOREADER' as a reader.
   If the mode is `JDBOWRITER', the following may be added by bitwise-or: `JDBOCREAT', which
   means it creates a new database if not exist, `JDBOTRUNC', which means it creates a new
   database regardless if one exists.  Both of `JDBOREADER' and `JDBOWRITER' can be added to by
   bitwise-or: `JDBONOLCK', which means it opens the database directory without file locking, or
   `JDBOLCKNB', which means locking is performed without blocking.
   If successful, the return value is true, else, it is false. */
bool tcjdbopen(TCJDB *jdb, const char *path, int omode);


/* Close a tagged database object.
   `jdb' specifies the tagged database object.
   If successful, the return value is true, else, it is false.
   Update of a database is assured to be written when the database is closed.  If a writer opens
   a database but does not close it appropriately, the database will be broken. */
bool tcjdbclose(TCJDB *jdb);


/* Store a record into a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `words' specifies a list object contains the words of the record, whose encoding should be
   UTF-8.
   If successful, the return value is true, else, it is false. */
bool tcjdbput(TCJDB *jdb, int64_t id, const TCLIST *words);


/* Store a record with a text string into a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   `text' specifies the string of the record, whose encoding should be UTF-8.
   `delims' specifies a string containing delimiting characters of the text.  If it is `NULL',
   space characters are specified.
   If successful, the return value is true, else, it is false. */
bool tcjdbput2(TCJDB *jdb, int64_t id, const char *text, const char *delims);


/* Remove a record of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is true, else, it is false. */
bool tcjdbout(TCJDB *jdb, int64_t id);


/* Retrieve a record of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is the string of the corresponding record, else, it is `NULL'.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcjdbget(TCJDB *jdb, int64_t id);


/* Retrieve a record as a string of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is the string of the corresponding record, else, it is `NULL'.
   Each word is separated by a tab character.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcjdbget2(TCJDB *jdb, int64_t id);


/* Search a tagged database.
   `jdb' specifies the tagged database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode: `JDBSSUBSTR' as substring matching, `JDBSPREFIX' as prefix
   matching, `JDBSSUFFIX' as suffix matching, `JDBSFULL' as full matching.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcjdbsearch(TCJDB *jdb, const char *word, int smode, int *np);


/* Search a tagged database with a compound expression.
   `jdb' specifies the tagged database object.
   `expr' specifies the string of the compound expression.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
uint64_t *tcjdbsearch2(TCJDB *jdb, const char *expr, int *np);


/* Initialize the iterator of a tagged database object.
   `jdb' specifies the tagged database object.
   If successful, the return value is true, else, it is false.
   The iterator is used in order to access the ID number of every record stored in a database. */
bool tcjdbiterinit(TCJDB *jdb);


/* Get the next ID number of the iterator of a tagged database object.
   `jdb' specifies the tagged database object.
   If successful, the return value is the ID number of the next record, else, it is 0.  0 is
   returned when no record is to be get out of the iterator.
   It is possible to access every record by iteration of calling this function.  It is allowed to
   update or remove records whose keys are fetched while the iteration.  However, it is not
   assured if updating the database is occurred while the iteration.  Besides, the order of this
   traversal access method is arbitrary, so it is not assured that the order of storing matches
   the one of the traversal access. */
uint64_t tcjdbiternext(TCJDB *jdb);


/* Synchronize updated contents of a tagged database object with the files and the device.
   `jdb' specifies the tagged database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful when another process connects the same database directory. */
bool tcjdbsync(TCJDB *jdb);


/* Optimize the files of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   If successful, the return value is true, else, it is false.
   This function is useful to reduce the size of the database files with data fragmentation by
   successive updating. */
bool tcjdboptimize(TCJDB *jdb);


/* Remove all records of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   If successful, the return value is true, else, it is false. */
bool tcjdbvanish(TCJDB *jdb);


/* Copy the database directory of a tagged database object.
   `jdb' specifies the tagged database object.
   `path' specifies the path of the destination directory.  If it begins with `@', the trailing
   substring is executed as a command line.
   If successful, the return value is true, else, it is false.  False is returned if the executed
   command returns non-zero code.
   The database directory is assured to be kept synchronized and not modified while the copying or
   executing operation is in progress.  So, this function is useful to create a backup directory
   of the database directory. */
bool tcjdbcopy(TCJDB *jdb, const char *path);


/* Get the directory path of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the path of the database directory or `NULL' if the object does not
   connect to any database directory. */
const char *tcjdbpath(TCJDB *jdb);


/* Get the number of records of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the number of records or 0 if the object does not connect to any database
   directory. */
uint64_t tcjdbrnum(TCJDB *jdb);


/* Get the total size of the database files of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the size of the database files or 0 if the object does not connect to any
   database directory. */
uint64_t tcjdbfsiz(TCJDB *jdb);



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


enum {                                   /* enumeration for expert options */
  JDBXNOTXT = 1 << 0                     /* no text mode */
};


/* Set the file descriptor for debugging output.
   `jdb' specifies the tagged database object.
   `fd' specifies the file descriptor for debugging output. */
void tcjdbsetdbgfd(TCJDB *jdb, int fd);


/* Get the file descriptor for debugging output.
   `jdb' specifies the tagged database object.
   The return value is the file descriptor for debugging output. */
int tcjdbdbgfd(TCJDB *jdb);


/* Synchronize updating contents on memory of a tagged database object.
   `jdb' specifies the tagged database object.
   `level' specifies the synchronization lavel; 0 means cache synchronization, 1 means database
   synchronization, and 2 means file synchronization.
   If successful, the return value is true, else, it is false. */
bool tcjdbmemsync(TCJDB *jdb, int level);


/* Get the inode number of the database directory of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the inode number of the database directory or 0 the object does not
   connect to any database directory. */
uint64_t tcjdbinode(TCJDB *jdb);


/* Get the modification time of the database directory of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the inode number of the database directory or 0 the object does not
   connect to any database directory. */
time_t tcjdbmtime(TCJDB *jdb);


/* Get the options of a tagged database object.
   `jdb' specifies the tagged database object.
   The return value is the options. */
uint8_t tcjdbopts(TCJDB *jdb);


/* Set the callback function for sync progression of a tagged database object.
   `jdb' specifies the tagged database object.
   `cb' specifies the pointer to the callback function for sync progression.  Its first argument
   specifies the number of tokens to be synchronized.  Its second argument specifies the number
   of processed tokens.  Its third argument specifies the message string.  The fourth argument
   specifies an arbitrary pointer.  Its return value should be true usually, or false if the sync
   operation should be terminated.
   `opq' specifies the arbitrary pointer to be given to the callback function. */
void tcjdbsetsynccb(TCJDB *jdb, bool (*cb)(int, int, const char *, void *), void *opq);


/* Set the expert options of a tagged database object.
   `jdb' specifies the tagged database object.
   `exopts' specifies options by bitwise-or: `JDBXNOTXT' specifies that the text database does
   not record any record. */
void tcjdbsetexopts(TCJDB *jdb, uint32_t exopts);



__LAPUTA_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
