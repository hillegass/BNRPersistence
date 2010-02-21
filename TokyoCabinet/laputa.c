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


#include "laputa.h"
#include "myconf.h"

#define JDBDIRMODE     00755             // permission of created directories
#define JDBIOBUFSIZ    65536             // size of an I/O buffer
#define JDBDEFFWMMAX   2048              // default maximum number forward matching expansion
#define JDBTXDBNAME    "laputa.tch"      // name of the text database
#define JDBLSDBNAME    "list.tcb"        // name of the word list database
#define JDBTXDBMAGIC   0x4a              // magic data for identification
#define JDBAVGWSIZ     16                // average size of words

#define JDBDEFERNUM    1000000           // default expected record number
#define JDBDEFETNUM    1000000           // default expected token number
#define JDBDEFIUSIZ    (1024LL*1024*512) // default expected token number
#define JDBTXBNUMCO    2                 // coefficient of the bucket number
#define JDBTXAPOW      3                 // alignment power of the text database
#define JDBTXFPOW      10                // free block pool power of the text database
#define JDBLSLMEMB     256               // alignment power of the text database
#define JDBLSNMEMB     256               // free block pool power of the text database


/* private function prototypes */
static bool tcjdblockmethod(TCJDB *jdb, bool wr);
static bool tcjdbunlockmethod(TCJDB *jdb);
static bool tcjdbsynccb(int total, int current, const char *msg, TCJDB *jdb);
static bool tcjdbaddcb(const char *word, TCJDB *jdb);
static bool tcjdbopenimpl(TCJDB *jdb, const char *path, int omode);
static bool tcjdbcloseimpl(TCJDB *jdb);
static bool tcjdbputimpl(TCJDB *jdb, int64_t id, const TCLIST *words);
static bool tcjdboutimpl(TCJDB *jdb, int64_t id);
static char *tcjdbgetimpl(TCJDB *jdb, int64_t id);
static uint64_t *tcjdbsearchimpl(TCJDB *jdb, const char *word, int smode, int *np);
static uint64_t *tcjdbsearchword(TCJDB *jdb, const char *word, int *np);
static uint64_t *tcjdbsearchtoken(TCJDB *jdb, const char *token, int *np);
static bool tcjdboptimizeimpl(TCJDB *jdb);
static bool tcjdbvanishimpl(TCJDB *jdb);
static bool tcjdbcopyimpl(TCJDB *jdb, const char *path);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Get the message string corresponding to an error code. */
const char *tcjdberrmsg(int ecode){
  return tchdberrmsg(ecode);
}


/* Create a tagged database object. */
TCJDB *tcjdbnew(void){
  TCJDB *jdb = tcmalloc(sizeof(*jdb));
  jdb->mmtx = tcmalloc(sizeof(pthread_rwlock_t));
  if(pthread_rwlock_init(jdb->mmtx, NULL) != 0) tcmyfatal("pthread_rwlock_init failed");
  jdb->txdb = tchdbnew();
  if(!tchdbsetmutex(jdb->txdb)) tcmyfatal("tchdbsetmutex failed");
  jdb->lsdb = tcbdbnew();
  TCWDB **idxs = jdb->idxs;
  for(int i = 0; i < JDBWDBMAX; i++){
    idxs[i] = tcwdbnew();
    tcwdbsetsynccb(idxs[i], (bool (*)(int, int, const char *, void *))tcjdbsynccb, jdb);
    tcwdbsetaddcb(idxs[i], (bool (*)(const char *, void *))tcjdbaddcb, jdb);
  }
  jdb->inum = 0;
  jdb->cnum = 0;
  jdb->path = NULL;
  jdb->wmode = false;
  jdb->wopts = 0;
  jdb->womode = 0;
  jdb->ernum = JDBDEFERNUM;
  jdb->etnum = JDBDEFETNUM;
  jdb->iusiz = JDBDEFIUSIZ;
  jdb->opts = 0;
  jdb->synccb = NULL;
  jdb->syncopq = NULL;
  jdb->exopts = 0;
  return jdb;
}


/* Delete a tagged database object. */
void tcjdbdel(TCJDB *jdb){
  assert(jdb);
  if(jdb->path) tcjdbclose(jdb);
  TCWDB **idxs = jdb->idxs;
  for(int i = JDBWDBMAX - 1; i >= 0; i--){
    tcwdbdel(idxs[i]);
  }
  tcbdbdel(jdb->lsdb);
  tchdbdel(jdb->txdb);
  pthread_rwlock_destroy(jdb->mmtx);
  tcfree(jdb->mmtx);
  tcfree(jdb);
}


/* Get the last happened error code of a tagged database object. */
int tcjdbecode(TCJDB *jdb){
  assert(jdb);
  return tchdbecode(jdb->txdb);
}


/* Set the tuning parameters of a tagged database object. */
bool tcjdbtune(TCJDB *jdb, int64_t ernum, int64_t etnum, int64_t iusiz, uint8_t opts){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  jdb->ernum = (ernum > 0) ? ernum : JDBDEFERNUM;
  jdb->etnum = (etnum > 0) ? etnum : JDBDEFETNUM;
  jdb->iusiz = (iusiz > 0) ? iusiz : JDBDEFIUSIZ;
  jdb->opts = opts;
  tcjdbunlockmethod(jdb);
  return true;
}


/* Set the caching parameters of a tagged database object. */
bool tcjdbsetcache(TCJDB *jdb, int64_t icsiz, int32_t lcnum){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  TCWDB **idxs = jdb->idxs;
  for(int i = 0; i < JDBWDBMAX; i++){
    tcwdbsetcache(idxs[i], icsiz, lcnum);
  }
  tcjdbunlockmethod(jdb);
  return true;
}


/* Set the maximum number of forward matching expansion of a tagged database object. */
bool tcjdbsetfwmmax(TCJDB *jdb, uint32_t fwmmax){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  TCWDB **idxs = jdb->idxs;
  for(int i = 0; i < JDBWDBMAX; i++){
    tcwdbsetfwmmax(idxs[i], fwmmax);
  }
  tcjdbunlockmethod(jdb);
  return true;
}


/* Open a tagged database object. */
bool tcjdbopen(TCJDB *jdb, const char *path, int omode){
  assert(jdb && path);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbopenimpl(jdb, path, omode);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Close a tagged database object. */
bool tcjdbclose(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbcloseimpl(jdb);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Store a record into a tagged database object. */
bool tcjdbput(TCJDB *jdb, int64_t id, const TCLIST *words){
  assert(jdb && id > 0 && words);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbputimpl(jdb, id, words);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Store a record with a text string into a tagged database object. */
bool tcjdbput2(TCJDB *jdb, int64_t id, const char *text, const char *delims){
  assert(jdb && id > 0 && text);
  TCLIST *words = tcstrsplit(text, delims ? delims : WDBSPCCHARS);
  bool rv = tcjdbput(jdb, id, words);
  tclistdel(words);
  return rv;
}


/* Remove a record of a tagged database object. */
bool tcjdbout(TCJDB *jdb, int64_t id){
  assert(jdb && id > 0);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdboutimpl(jdb, id);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Retrieve a record of a tagged database object. */
TCLIST *tcjdbget(TCJDB *jdb, int64_t id){
  assert(jdb && id > 0);
  char *text = tcjdbget2(jdb, id);
  if(!text) return NULL;
  TCLIST *words = tcstrsplit(text, "\t");
  tcfree(text);
  return words;
}


/* Retrieve a record as a string of a tagged database object. */
char *tcjdbget2(TCJDB *jdb, int64_t id){
  assert(jdb && id > 0);
  if(!tcjdblockmethod(jdb, false)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return NULL;
  }
  char *rv = tcjdbgetimpl(jdb, id);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Search a tagged database. */
uint64_t *tcjdbsearch(TCJDB *jdb, const char *word, int smode, int *np){
  assert(jdb && word && np);
  if(!tcjdblockmethod(jdb, false)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  char *nword = tcstrdup(word);
  tctextnormalize(nword, TCTNLOWER | TCTNNOACC | TCTNSPACE);
  uint64_t *rv = tcjdbsearchimpl(jdb, nword, smode, np);
  tcfree(nword);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Search a tagged database with a compound expression. */
uint64_t *tcjdbsearch2(TCJDB *jdb, const char *expr, int *np){
  assert(jdb && expr && np);
  TCLIST *terms = tclistnew();
  char *nexpr = tcstrdup(expr);
  tctextnormalize(nexpr, TCTNSPACE);
  const char *rp = nexpr;
  while(*rp != '\0'){
    if(*rp == ' '){
      rp++;
      while(*rp == ' '){
        rp++;
      }
    } else if(*rp == '"'){
      const char *pv = rp;
      rp++;
      while(*rp != '\0' && !(*rp == '"' && *(++rp) != '"')){
        rp++;
      }
      if(*rp == '"') rp++;
      tclistpush(terms, pv, rp - pv);
    } else if(rp[0] == '[' && rp[1] == '['){
      const char *pv = rp;
      rp += 2;
      while(*rp != '\0' && !(rp[0] == ']' && rp[1] == ']')){
        rp++;
      }
      if(rp[0] == ']' && rp[1] == ']') rp += 2;
      tclistpush(terms, pv, rp - pv);
    } else {
      const char *pv = rp;
      rp++;
      while(*rp != '\0' && *rp != ' ' && *rp != '"'){
        rp++;
      }
      tclistpush(terms, pv, rp - pv);
    }
  }
  tcfree(nexpr);
  int tnum = tclistnum(terms);
  if(tnum < 1){
    tclistdel(terms);
    *np = 0;
    return tcmalloc(1);
  }
  if(tnum == 1){
    uint64_t *res = tcjdbsearchtoken(jdb, tclistval2(terms, 0), np);
    tclistdel(terms);
    return res;
  }
  QDBRSET *rsets = tcmalloc(tnum * sizeof(*rsets));
  int rsnum = 0;
  bool sign = true;
  int ti = 0;
  while(ti < tnum){
    const char *term = tclistval2(terms, ti);
    if(!strcmp(term, "&&") || !strcmp(term, "||")){
      sign = true;
    } else if(!strcmp(term, "!!")){
      sign = false;
    } else {
      rsets[rsnum].ids = tcjdbsearchtoken(jdb, term, &rsets[rsnum].num);
      int rsover = 0;
      while(ti + 2 < tnum && !strcmp(tclistval2(terms, ti + 1), "||")){
        rsover++;
        int ri = rsnum + rsover;
        rsets[ri].ids = tcjdbsearchtoken(jdb, tclistval2(terms, ti + 2), &rsets[ri].num);
        ti += 2;
      }
      if(rsover > 0){
        int rnum;
        uint64_t *res = tcqdbresunion(rsets + rsnum, rsover + 1, &rnum);
        for(int i = 0; i <= rsover; i++){
          tcfree(rsets[rsnum+i].ids);
        }
        rsets[rsnum].ids = res;
        rsets[rsnum].num = rnum;
      }
      if(!sign) rsets[rsnum].num *= -1;
      rsnum++;
      sign = true;
    }
    ti++;
  }
  uint64_t *res;
  int rnum;
  while(rsnum > 1){
    if(rsets[0].num < 0) rsets[0].num = 0;
    int unum = 0;
    for(int i = 1; i < rsnum; i++){
      if(rsets[i].num < 0) break;
      unum++;
    }
    if(unum > 0){
      res = tcqdbresisect(rsets, unum + 1, &rnum);
      for(int i = 0; i <= unum; i++){
        tcfree(rsets[i].ids);
      }
      rsets[0].ids = res;
      rsets[0].num = rnum;
      memmove(rsets + 1, rsets + unum + 1, (rsnum - unum - 1) * sizeof(*rsets));
      rsnum -= unum;
    }
    if(rsnum > 1){
      unum = 0;
      for(int i = 1; i < rsnum; i++){
        if(rsets[i].num >= 0) break;
        rsets[i].num *= -1;
        unum++;
      }
      if(unum > 0){
        res = tcqdbresdiff(rsets, unum + 1, &rnum);
        for(int i = 0; i <= unum; i++){
          tcfree(rsets[i].ids);
        }
        rsets[0].ids = res;
        rsets[0].num = rnum;
        memmove(rsets + 1, rsets + unum + 1, (rsnum - unum - 1) * sizeof(*rsets));
        rsnum -= unum;
      }
    }
  }
  if(rsnum < 1){
    res = tcmalloc(1);
    rnum = 0;
  } else {
    if(!rsets[0].ids || rsets[0].num < 0) rsets[0].num = 0;
    res = rsets[0].ids;
    rnum = rsets[0].num;
    rsnum--;
  }
  for(int i = 0; i < rsnum; i++){
    tcfree(rsets[i].ids);
  }
  tcfree(rsets);
  tclistdel(terms);
  *np = rnum;
  return res;
}


/* Initialize the iterator of a tagged database object. */
bool tcjdbiterinit(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tchdbiterinit(jdb->txdb);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Get the next ID number of the iterator of a tagged database object. */
uint64_t tcjdbiternext(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  uint64_t rv = 0;
  int vsiz;
  char *vbuf = tchdbiternext(jdb->txdb, &vsiz);
  if(vbuf){
    TDREADVNUMBUF64(vbuf, rv, vsiz);
    tcfree(vbuf);
  }
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Synchronize updated contents of a tagged database object with the files and the device. */
bool tcjdbsync(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbmemsync(jdb, 2);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Optimize the files of a tagged database object. */
bool tcjdboptimize(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdboptimizeimpl(jdb);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Remove all records of a tagged database object. */
bool tcjdbvanish(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, true)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbvanishimpl(jdb);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Copy the database directory of a tagged database object. */
bool tcjdbcopy(TCJDB *jdb, const char *path){
  assert(jdb);
  if(!tcjdblockmethod(jdb, false)) return false;
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return false;
  }
  bool rv = tcjdbcopyimpl(jdb, path);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Get the directory path of a tagged database object. */
const char *tcjdbpath(TCJDB *jdb){
  assert(jdb);
  return jdb->path;
}


/* Get the number of records of a tagged database object. */
uint64_t tcjdbrnum(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, false)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return 0;
  }
  uint64_t rv = tchdbrnum(jdb->txdb);
  tcjdbunlockmethod(jdb);
  return rv;
}


/* Get the total size of the database files of a tagged database object. */
uint64_t tcjdbfsiz(TCJDB *jdb){
  assert(jdb);
  if(!tcjdblockmethod(jdb, false)) return false;
  if(!jdb->path){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcjdbunlockmethod(jdb);
    return 0;
  }
  uint64_t rv = tchdbfsiz(jdb->txdb);
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  for(int i = 0; i < inum; i++){
    rv += tcwdbfsiz(idxs[i]);
  }
  tcjdbunlockmethod(jdb);
  return rv;
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set the file descriptor for debugging output. */
void tcjdbsetdbgfd(TCJDB *jdb, int fd){
  assert(jdb);
  tchdbsetdbgfd(jdb->txdb, fd);
  TCWDB **idxs = jdb->idxs;
  for(int i = 0; i < JDBWDBMAX; i++){
    tcwdbsetdbgfd(idxs[i], fd);
  }
}


/* Get the file descriptor for debugging output. */
int tcjdbdbgfd(TCJDB *jdb){
  assert(jdb);
  return tchdbdbgfd(jdb->txdb);
}


/* Synchronize updating contents on memory of a tagged database object. */
bool tcjdbmemsync(TCJDB *jdb, int level){
  assert(jdb);
  if(!jdb->path || !jdb->wmode){
    tchdbsetecode(jdb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  TCHDB *txdb = jdb->txdb;
  TCBDB *lsdb = jdb->lsdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  char *txopq = tchdbopaque(txdb);
  *(uint8_t *)(txopq + sizeof(uint8_t)) = inum;
  bool err = false;
  if(!tchdbmemsync(txdb, false)) err = true;
  if(!tcbdbmemsync(lsdb, false)) err = true;
  for(int i = 0; i < inum; i++){
    if(!tcwdbmemsync(idxs[i], level)){
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Get the inode number of the database file of a tagged database object. */
uint64_t tcjdbinode(TCJDB *jdb){
  assert(jdb);
  return tchdbinode(jdb->txdb);
}


/* Get the modification time of the database file of a tagged database object. */
time_t tcjdbmtime(TCJDB *jdb){
  assert(jdb);
  return tchdbmtime(jdb->txdb);
}


/* Get the options of a tagged database object. */
uint8_t tcjdbopts(TCJDB *jdb){
  assert(jdb);
  return jdb->opts;
}


/* Set the callback function for sync progression of a tagged database object. */
void tcjdbsetsynccb(TCJDB *jdb, bool (*cb)(int, int, const char *, void *), void *opq){
  assert(jdb);
  jdb->synccb = cb;
  jdb->syncopq = opq;
}


/* Set the expert options of a tagged database object. */
void tcjdbsetexopts(TCJDB *jdb, uint32_t exopts){
  assert(jdb);
  jdb->exopts = exopts;
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/


/* Lock a method of the tagged database object.
   `jdb' specifies the tagged database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
static bool tcjdblockmethod(TCJDB *jdb, bool wr){
  assert(jdb);
  if(wr ? pthread_rwlock_wrlock(jdb->mmtx) != 0 : pthread_rwlock_rdlock(jdb->mmtx) != 0){
    tchdbsetecode(jdb->txdb, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Unlock a method of the tagged database object.
   `bdb' specifies the tagged database object.
   If successful, the return value is true, else, it is false. */
static bool tcjdbunlockmethod(TCJDB *jdb){
  assert(jdb);
  if(pthread_rwlock_unlock(jdb->mmtx) != 0){
    tchdbsetecode(jdb->txdb, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Call the callback for sync progression.
   `total' specifies the number of tokens to be synchronized.
   `current' specifies the number of processed tokens.
   `msg' specifies the message string.
   `jdb' specifies the tagged database object.
   The return value is true usually, or false if the operation should be terminated. */
static bool tcjdbsynccb(int total, int current, const char *msg, TCJDB *jdb){
  assert(msg && jdb);
  bool rv = jdb->synccb ? jdb->synccb(total, current, msg, jdb->syncopq) : true;
  if((total|current) == 0 && !strcmp(msg, QDBSYNCMSGL) &&
     tcwdbfsiz(jdb->idxs[jdb->cnum]) >= jdb->iusiz && jdb->inum > 0){
    TCWDB **idxs = jdb->idxs;
    if(jdb->synccb && !jdb->synccb(total, current, "to be cycled", jdb->syncopq)) rv = false;
    if(!tcwdbcacheclear(jdb->idxs[jdb->cnum])){
      tchdbsetecode(jdb->txdb, tcwdbecode(jdb->idxs[jdb->cnum]), __FILE__, __LINE__, __func__);
      rv = false;
    }
    int inum = jdb->inum;
    jdb->cnum = 0;
    uint64_t min = UINT64_MAX;
    for(int i = 0; i < inum; i++){
      uint64_t fsiz = tcwdbfsiz(idxs[i]);
      if(fsiz < min){
        jdb->cnum = i;
        min = fsiz;
      }
    }
    if(min > jdb->iusiz && inum < JDBWDBMAX) jdb->cnum = inum;
  }
  return rv;
}


/* Call the callback for word addition.
   `word' specifies the word.
   `jdb' specifies the tagged database object.
   The return value is true usually, or false if the operation should be terminated. */
static bool tcjdbaddcb(const char *word, TCJDB *jdb){
  assert(word && jdb);
  tcbdbputkeep(jdb->lsdb, word, strlen(word), "", 0);
  return true;
}


/* Open a tagged database object.
   `jdb' specifies the tagged database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode.
   If successful, the return value is true, else, it is false. */
static bool tcjdbopenimpl(TCJDB *jdb, const char *path, int omode){
  assert(jdb && path);
  char pbuf[strlen(path)+TDNUMBUFSIZ];
  if(omode & JDBOWRITER){
    if(omode & JDBOCREAT){
      if(mkdir(path, JDBDIRMODE) == -1 && errno != EEXIST){
        int ecode = TCEMKDIR;
        switch(errno){
        case EACCES: ecode = TCENOPERM; break;
        case ENOENT: ecode = TCENOFILE; break;
        }
        tchdbsetecode(jdb->txdb, ecode, __FILE__, __LINE__, __func__);
        return false;
      }
    }
    if(omode & JDBOTRUNC){
      sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBTXDBNAME);
      if(unlink(pbuf) == -1 && errno != ENOENT){
        tchdbsetecode(jdb->txdb, TCEUNLINK, __FILE__, __LINE__, __func__);
        return false;
      }
      sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBLSDBNAME);
      if(unlink(pbuf) == -1 && errno != ENOENT){
        tchdbsetecode(jdb->txdb, TCEUNLINK, __FILE__, __LINE__, __func__);
        return false;
      }
      for(int i = 0; i < JDBWDBMAX; i++){
        sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
        if(unlink(pbuf) == -1 && errno != ENOENT){
          tchdbsetecode(jdb->txdb, TCEUNLINK, __FILE__, __LINE__, __func__);
          return false;
        }
      }
    }
  }
  struct stat sbuf;
  if(stat(path, &sbuf) == -1){
    int ecode = TCEOPEN;
    switch(errno){
    case EACCES: ecode = TCENOPERM; break;
    case ENOENT: ecode = TCENOFILE; break;
    }
    tchdbsetecode(jdb->txdb, ecode, __FILE__, __LINE__, __func__);
    return false;
  }
  if(!S_ISDIR(sbuf.st_mode)){
    tchdbsetecode(jdb->txdb, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  TCHDB *txdb = jdb->txdb;
  TCBDB *lsdb = jdb->lsdb;
  TCWDB **idxs = jdb->idxs;
  int homode = HDBOREADER;
  uint8_t hopts = 0;
  int bomode = BDBOREADER;
  uint8_t bopts = 0;
  int womode = WDBOREADER;
  uint8_t wopts = 0;
  int64_t etnum = jdb->etnum;
  int64_t iusiz = jdb->iusiz;
  if(omode & JDBOWRITER){
    homode = HDBOWRITER;
    bomode = BDBOWRITER;
    womode = WDBOWRITER;
    if(omode & JDBOCREAT){
      homode |= HDBOCREAT;
      bomode |= BDBOCREAT;
      womode |= WDBOCREAT;
    }
    if(omode & JDBOTRUNC){
      homode |= HDBOTRUNC;
      bomode |= BDBOTRUNC;
      womode |= WDBOTRUNC;
    }
    int64_t bnum = jdb->ernum * JDBTXBNUMCO + 1;
    if(jdb->opts & JDBTLARGE){
      hopts |= HDBTLARGE;
      bopts |= BDBTLARGE;
      wopts |= WDBTLARGE;
    }
    if(jdb->opts & JDBTDEFLATE) wopts |= WDBTDEFLATE;
    if(jdb->opts & JDBTBZIP) wopts |= WDBTBZIP;
    if(jdb->opts & JDBTTCBS){
      hopts |= HDBTTCBS;
      bopts |= BDBTTCBS;
      wopts |= WDBTTCBS;
    }
    if(jdb->exopts & JDBXNOTXT){
      if(!tchdbtune(txdb, 1, 0, 0, 0)) return false;
    } else {
      if(!tchdbtune(txdb, bnum, JDBTXAPOW, JDBTXFPOW, hopts)) return false;
    }
    if(!tcbdbtune(lsdb, JDBLSLMEMB, JDBLSNMEMB, (jdb->etnum / JDBLSLMEMB) * 4, -1, -1, bopts)){
      tchdbsetecode(txdb, tcbdbecode(lsdb), __FILE__, __LINE__, __func__);
      return false;
    }
  }
  if(omode & JDBONOLCK){
    homode |= HDBONOLCK;
    bomode |= BDBONOLCK;
    womode |= WDBONOLCK;
  }
  if(omode & JDBOLCKNB){
    homode |= HDBOLCKNB;
    bomode |= BDBOLCKNB;
    womode |= WDBOLCKNB;
  }
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBTXDBNAME);
  if(!tchdbopen(txdb, pbuf, homode)) return false;
  char *txopq = tchdbopaque(txdb);
  uint8_t magic = *(uint8_t *)txopq;
  if(magic == 0 && (omode & JDBOWRITER)){
    *(uint8_t *)txopq = JDBTXDBMAGIC;
    *(uint8_t *)(txopq + sizeof(magic) + sizeof(uint8_t)) = wopts;
    uint64_t llnum = TDHTOILL(etnum);
    memcpy(txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(wopts), &llnum, sizeof(llnum));
    llnum = TDHTOILL(iusiz);
    memcpy(txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(wopts) + sizeof(llnum),
           &llnum, sizeof(llnum));
  } else {
    wopts = *(uint8_t *)(txopq + sizeof(magic) + sizeof(uint8_t));
    memcpy(&etnum, txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(wopts), sizeof(etnum));
    etnum = TDITOHLL(etnum);
    memcpy(&iusiz, txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(wopts) + sizeof(etnum),
           sizeof(iusiz));
    iusiz = TDITOHLL(iusiz);
  }
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBLSDBNAME);
  if(!tcbdbopen(lsdb, pbuf, bomode)) return false;
  if(omode & JDBOWRITER){
    for(int i = 0; i < JDBWDBMAX; i++){
      if(!tcwdbtune(idxs[i], etnum, wopts)){
        tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
        return false;
      }
    }
  }
  jdb->opts = 0;
  if(wopts & WDBTLARGE) jdb->opts |= WDBTLARGE;
  if(wopts & WDBTDEFLATE) jdb->opts |= WDBTDEFLATE;
  if(wopts & WDBTBZIP) jdb->opts |= WDBTBZIP;
  if(wopts & WDBTTCBS) jdb->opts |= JDBTTCBS;
  uint8_t inum;
  memcpy(&inum, txopq + sizeof(magic), sizeof(inum));
  if(inum > JDBWDBMAX){
    tchdbclose(txdb);
    tchdbsetecode(txdb, TCEMETA, __FILE__, __LINE__, __func__);
    return false;
  }
  jdb->cnum = 0;
  uint64_t min = UINT64_MAX;
  for(int i = 0; i < inum; i++){
    sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
    if(!tcwdbopen(idxs[i], pbuf, womode)){
      tchdbclose(txdb);
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      for(int j = i - 1; j >= 0; j--){
        tcwdbclose(idxs[i]);
      }
      return false;
    }
    uint64_t fsiz = tcwdbfsiz(idxs[i]);
    if(fsiz < min){
      jdb->cnum = i;
      min = fsiz;
    }
  }
  jdb->inum = inum;
  jdb->path = tcstrdup(path);
  jdb->wmode = omode & JDBOWRITER;
  jdb->wopts = wopts;
  jdb->womode = womode;
  return true;
}


/* Close a tagged database object.
   `jdb' specifies the tagged database object.
   If successful, the return value is true, else, it is false. */
static bool tcjdbcloseimpl(TCJDB *jdb){
  assert(jdb);
  bool err = false;
  TCHDB *txdb = jdb->txdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  if(jdb->wmode){
    char *txopq = tchdbopaque(txdb);
    *(uint8_t *)(txopq + sizeof(uint8_t)) = inum;
  }
  jdb->inum = 0;
  for(int i = 0; i < inum; i++){
    if(!tcwdbclose(idxs[i])){
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  if(!tchdbclose(txdb)) err = true;
  tcfree(jdb->path);
  jdb->path = NULL;
  return !err;
}


/* Store a record into a tagged database object.
   `jdb' specifies the tagged database object.
   `id' specifies the ID number of the record.
   `words' specifies a list object contains the words of the record.
   If successful, the return value is true, else, it is false. */
static bool tcjdbputimpl(TCJDB *jdb, int64_t id, const TCLIST *words){
  assert(jdb && id > 0 && words);
  TCHDB *txdb = jdb->txdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  uint8_t cnum = jdb->cnum;
  if(cnum >= inum){
    char pbuf[strlen(jdb->path)+TDNUMBUFSIZ];
    sprintf(pbuf, "%s%c%04d", jdb->path, MYPATHCHR, inum + 1);
    TCWDB *nidx = idxs[inum];
    if(!tcwdbopen(nidx, pbuf, jdb->womode | JDBOCREAT)){
      tchdbsetecode(txdb, tcwdbecode(nidx), __FILE__, __LINE__, __func__);
      return false;
    }
    jdb->cnum = jdb->inum;
    cnum = jdb->cnum;
    jdb->inum++;
  }
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  char stack[JDBIOBUFSIZ];
  int vsiz = tchdbget3(txdb, kbuf, ksiz, stack, JDBIOBUFSIZ);
  if(vsiz > 0){
    int ocnum = tcatoi(stack);
    if(ocnum < 0 || ocnum >= JDBWDBMAX){
      tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    TCWDB *oidx = idxs[ocnum];
    if(vsiz >= JDBIOBUFSIZ){
      char *vbuf = tchdbget(txdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        TCLIST *owords = tcstrsplit(vbuf, "\t");
        tcfree(tclistshift2(owords));
        int ownum = tclistnum(owords);
        for(int i = 0; i < ownum; i++){
          int wsiz;
          char *word = (char *)tclistval(owords, i, &wsiz);
          tctextnormalize(word, TCTNLOWER | TCTNNOACC | TCTNSPACE);
        }
        if(!tcwdbout(oidx, id, owords)){
          tchdbsetecode(txdb, tcwdbecode(oidx), __FILE__, __LINE__, __func__);
          tclistdel(owords);
          return false;
        }
        tclistdel(owords);
        tcfree(vbuf);
      } else {
        tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
        return false;
      }
    } else {
      stack[vsiz] = '\0';
      TCLIST *owords = tcstrsplit(stack, "\t");
      tcfree(tclistshift2(owords));
      int ownum = tclistnum(owords);
      for(int i = 0; i < ownum; i++){
        int wsiz;
        char *word = (char *)tclistval(owords, i, &wsiz);
        tctextnormalize(word, TCTNLOWER | TCTNNOACC | TCTNSPACE);
      }
      if(!tcwdbout(oidx, id, owords)){
        tchdbsetecode(txdb, tcwdbecode(oidx), __FILE__, __LINE__, __func__);
        tclistdel(owords);
        return false;
      }
      tclistdel(owords);
    }
    if(!tchdbout(txdb, kbuf, ksiz)) return false;
  }
  int wnum = tclistnum(words);
  TCXSTR *xstr = tcxstrnew3(wnum * JDBAVGWSIZ + 1);
  TCLIST *nwords = tclistnew2(wnum);
  tcxstrprintf(xstr, "%d", cnum);
  for(int i = 0; i < wnum; i++){
    int wsiz;
    const char *word = tclistval(words, i, &wsiz);
    if(wsiz >= JDBIOBUFSIZ) continue;
    memcpy(stack, word, wsiz);
    stack[wsiz] = '\0';
    for(int j = 0; j < wsiz; j++){
      if(((unsigned char *)stack)[j] < ' ') stack[j] = ' ';
    }
    tcxstrcat(xstr, "\t", 1);
    tcxstrcat(xstr, stack, wsiz);
    tctextnormalize(stack, TCTNLOWER | TCTNNOACC | TCTNSPACE);
    if(stack[0] != '\0') tclistpush2(nwords, stack);
  }
  if(!(jdb->exopts & JDBXNOTXT) &&
     !tchdbputkeep(txdb, kbuf, ksiz, tcxstrptr(xstr), tcxstrsize(xstr))){
    return false;
  }
  TCWDB *cidx = idxs[cnum];
  if(!tcwdbput(cidx, id, nwords)){
    tchdbsetecode(txdb, tcwdbecode(cidx), __FILE__, __LINE__, __func__);
    tclistdel(nwords);
    tcxstrdel(xstr);
    return false;
  }
  tclistdel(nwords);
  tcxstrdel(xstr);
  return true;
}


/* Remove a record of a tagged database object.
   `jdb' specifies the tagged database object.
   `id' specifies the ID number of the record.
   If successful, the return value is true, else, it is false. */
static bool tcjdboutimpl(TCJDB *jdb, int64_t id){
  TCHDB *txdb = jdb->txdb;
  TCWDB **idxs = jdb->idxs;
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  char stack[JDBIOBUFSIZ];
  int vsiz = tchdbget3(txdb, kbuf, ksiz, stack, JDBIOBUFSIZ);
  if(vsiz > 0){
    int ocnum = tcatoi(stack);
    if(ocnum < 0 || ocnum >= JDBWDBMAX){
      tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    TCWDB *oidx = idxs[ocnum];
    if(vsiz >= JDBIOBUFSIZ){
      char *vbuf = tchdbget(txdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        TCLIST *owords = tcstrsplit(vbuf, "\t");
        tcfree(tclistshift2(owords));
        int ownum = tclistnum(owords);
        for(int i = 0; i < ownum; i++){
          int wsiz;
          char *word = (char *)tclistval(owords, i, &wsiz);
          tctextnormalize(word, TCTNLOWER | TCTNNOACC | TCTNSPACE);
        }
        if(!tcwdbout(oidx, id, owords)){
          tchdbsetecode(txdb, tcwdbecode(oidx), __FILE__, __LINE__, __func__);
          tclistdel(owords);
          return false;
        }
        tclistdel(owords);
        tcfree(vbuf);
      } else {
        tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
        return false;
      }
    } else {
      stack[vsiz] = '\0';
      TCLIST *owords = tcstrsplit(stack, "\t");
      tcfree(tclistshift2(owords));
      int ownum = tclistnum(owords);
      for(int i = 0; i < ownum; i++){
        int wsiz;
        char *word = (char *)tclistval(owords, i, &wsiz);
        tctextnormalize(word, TCTNLOWER | TCTNNOACC | TCTNSPACE);
      }
      if(!tcwdbout(oidx, id, owords)){
        tchdbsetecode(txdb, tcwdbecode(oidx), __FILE__, __LINE__, __func__);
        tclistdel(owords);
        return false;
      }
      tclistdel(owords);
    }
    if(!tchdbout(txdb, kbuf, ksiz)) return false;
  } else {
    tchdbsetecode(txdb, TCENOREC, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Retrieve a record of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is the string of the corresponding record, else, it is
   `NULL'. */
static char *tcjdbgetimpl(TCJDB *jdb, int64_t id){
  assert(jdb && id > 0);
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  int vsiz;
  char *vbuf = tchdbget(jdb->txdb, kbuf, ksiz, &vsiz);
  if(!vbuf) return NULL;
  char *pv = strchr(vbuf, '\t');
  if(!pv){
    tchdbsetecode(jdb->txdb, TCEMISC, __FILE__, __LINE__, __func__);
    tcfree(vbuf);
    return NULL;
  }
  pv++;
  vsiz = strlen(pv);
  memmove(vbuf, pv, vsiz);
  vbuf[vsiz] = '\0';
  return vbuf;
}


/* Search a tagged database.
   `jdb' specifies the tagged database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure. */
static uint64_t *tcjdbsearchimpl(TCJDB *jdb, const char *word, int smode, int *np){
  assert(jdb && word && np);
  TCBDB *lsdb = jdb->lsdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  if(inum < 1){
    *np = 0;
    return tcmalloc(1);
  }
  if(smode != JDBSSUBSTR){
    for(int i = 0; i < inum; i++){
      TCWDB *idx = idxs[i];
      if(tcwdbcnum(idx) > 0 && !tcwdbmemsync(idx, 0)){
        tchdbsetecode(jdb->txdb, tcwdbecode(idx), __FILE__, __LINE__, __func__);
        return NULL;
      }
    }
  }
  int fwmmax = tcwdbfwmmax(idxs[0]);
  if(fwmmax < 1) fwmmax = 1;
  TCLIST *words = tclistnew();
  if(smode == JDBSSUBSTR){
    BDBCUR *cur = tcbdbcurnew(lsdb);
    tcbdbcurfirst(cur);
    int ksiz;
    char *kbuf;
    while(tclistnum(words) < fwmmax && (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL){
      if(strstr(kbuf, word)){
        tclistpushmalloc(words, kbuf, ksiz);
      } else {
        tcfree(kbuf);
      }
      tcbdbcurnext(cur);
    }
    tcbdbcurdel(cur);
  } else if(smode == JDBSPREFIX){
    tclistdel(words);
    words = tcbdbfwmkeys2(lsdb, word, fwmmax);
  } else if(smode == JDBSSUFFIX){
    BDBCUR *cur = tcbdbcurnew(lsdb);
    tcbdbcurfirst(cur);
    int ksiz;
    char *kbuf;
    while(tclistnum(words) < fwmmax && (kbuf = tcbdbcurkey(cur, &ksiz)) != NULL){
      if(tcstrbwm(kbuf, word)){
        tclistpushmalloc(words, kbuf, ksiz);
      } else {
        tcfree(kbuf);
      }
      tcbdbcurnext(cur);
    }
    tcbdbcurdel(cur);
  } else {
    tclistpush2(words, word);
  }
  int wnum = tclistnum(words);
  if(wnum < 1){
    tclistdel(words);
    *np = 0;
    return tcmalloc(1);
  }
  uint64_t *res;
  if(wnum == 1){
    res = tcjdbsearchword(jdb, tclistval2(words, 0), np);
  } else {
    QDBRSET *rsets = tcmalloc(wnum * sizeof(*rsets));
    for(int i = 0; i < wnum; i++){
      rsets[i].ids = tcjdbsearchword(jdb, tclistval2(words, i), &rsets[i].num);
      if(!rsets[i].ids) rsets[i].num = 0;
    }
    res = tcqdbresunion(rsets, wnum, np);
    for(int i = 0; i < wnum; i++){
      tcfree(rsets[i].ids);
    }
    tcfree(rsets);
  }
  tclistdel(words);
  return res;
}


/* Search a tagged database for a word.
   `jdb' specifies the tagged database object.
   `word' specifies the string of the word to be matched to.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure. */
static uint64_t *tcjdbsearchword(TCJDB *jdb, const char *word, int *np){
  assert(jdb && word && np);
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  if(inum == 1){
    uint64_t *res = tcwdbsearch(idxs[0], word, np);
    if(!res) tchdbsetecode(jdb->txdb, tcwdbecode(idxs[0]), __FILE__, __LINE__, __func__);
    return res;
  }
  QDBRSET rsets[inum];
  for(int i = 0; i < inum; i++){
    rsets[i].ids = tcwdbsearch(idxs[i], word, &rsets[i].num);
  }
  uint64_t *res = tcqdbresunion(rsets, inum, np);
  for(int i = 0; i < inum; i++){
    tcfree(rsets[i].ids);
  }
  return res;
}


/* Search a tagged database with a token expression.
   `jdb' specifies the tagged database object.
   `token' specifies the string of the token expression.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure. */
static uint64_t *tcjdbsearchtoken(TCJDB *jdb, const char *token, int *np){
  assert(jdb && token && np);
  int len = strlen(token);
  if(*token == '"'){
    char *bare = tcmalloc(len + 1);
    char *wp = bare;
    const char *rp = token + 1;
    while(*rp != '\0'){
      if(rp[0] == '"'){
        if(rp[1] == '"'){
          *(wp++) = '"';
        }
      } else {
        *(wp++) = *rp;
      }
      rp++;
    }
    *wp = '\0';
    uint64_t *res = tcjdbsearch(jdb, bare, JDBSFULL, np);
    tcfree(bare);
    return res;
  }
  if(len < 4) return tcjdbsearch(jdb, token, JDBSFULL, np);
  if(token[0] == '[' && token[1] == '[' && token[2] == '[' && token[3] == '['){
    char *bare = tcmemdup(token + 4, len - 4);
    uint64_t *res = tcjdbsearch(jdb, bare, JDBSPREFIX, np);
    tcfree(bare);
    return res;
  }
  if(token[len-1] == ']' && token[len-2] == ']' && token[len-3] == ']' && token[len-4] == ']'){
    char *bare = tcmemdup(token, len - 4);
    uint64_t *res = tcjdbsearch(jdb, bare, JDBSSUFFIX, np);
    tcfree(bare);
    return res;
  }
  if(token[0] != '[' || token[1] != '[' || token[len-1] != ']' || token[len-2] != ']')
    return tcjdbsearch(jdb, token, JDBSFULL, np);
  len -= 4;
  char *bare = tcmemdup(token + 2, len);
  bool prefix = false;
  bool suffix = false;
  if(len > 0 && bare[0] == '*'){
    memmove(bare, bare + 1, len);
    len--;
    suffix = true;
  }
  if(len > 0 && bare[len-1] == '*'){
    bare[len-1] = '\0';
    len--;
    prefix = true;
  }
  if(len < 1){
    tcfree(bare);
    *np = 0;
    return tcmalloc(1);
  }
  int smode = JDBSFULL;
  if(prefix && suffix){
    smode = JDBSSUBSTR;
  } else if(prefix){
    smode = JDBSPREFIX;
  } else if(suffix){
    smode = JDBSSUFFIX;
  }
  uint64_t *res = tcjdbsearch(jdb, bare, smode, np);
  tcfree(bare);
  return res;
}


/* Optimize the file of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   If successful, the return value is true, else, it is false. */
static bool tcjdboptimizeimpl(TCJDB *jdb){
  assert(jdb);
  TCHDB *txdb = jdb->txdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  bool err = false;
  if(!tchdboptimize(txdb, -1, -1, -1, UINT8_MAX)) err = true;
  for(int i = 0; i < inum; i++){
    if(!tcwdboptimize(idxs[i])){
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Remove all records of a tagged database object.
   `jdb' specifies the tagged database object connected as a writer.
   If successful, the return value is true, else, it is false. */
static bool tcjdbvanishimpl(TCJDB *jdb){
  assert(jdb);
  TCHDB *txdb = jdb->txdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  bool err = false;
  if(!tchdbvanish(txdb)) err = true;
  char *txopq = tchdbopaque(txdb);
  *(uint8_t *)(txopq + sizeof(uint8_t) + sizeof(uint8_t)) = jdb->wopts;
  for(int i = 0; i < inum; i++){
    if(!tcwdbvanish(idxs[i])){
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Copy the database directory of a tagged database object.
   `jdb' specifies the tagged database object.
   `path' specifies the path of the destination directory.
   If successful, the return value is true, else, it is false. */
static bool tcjdbcopyimpl(TCJDB *jdb, const char *path){
  assert(jdb && path);
  TCHDB *txdb = jdb->txdb;
  TCBDB *lsdb = jdb->lsdb;
  TCWDB **idxs = jdb->idxs;
  uint8_t inum = jdb->inum;
  bool err = false;
  if(mkdir(path, JDBDIRMODE) == -1 && errno != EEXIST){
    int ecode = TCEMKDIR;
    switch(errno){
    case EACCES: ecode = TCENOPERM; break;
    case ENOENT: ecode = TCENOFILE; break;
    }
    tchdbsetecode(txdb, ecode, __FILE__, __LINE__, __func__);
    return false;
  }
  char pbuf[strlen(path)+TDNUMBUFSIZ];
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBTXDBNAME);
  if(!tchdbcopy(txdb, pbuf)) err = true;
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, JDBLSDBNAME);
  if(!tcbdbcopy(lsdb, pbuf)){
    tchdbsetecode(txdb, tcbdbecode(lsdb), __FILE__, __LINE__, __func__);
    err = true;
  }
  for(int i = 0; i < inum; i++){
    sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
    if(!tcwdbcopy(idxs[i], pbuf)){
      tchdbsetecode(txdb, tcwdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}



// END OF FILE
