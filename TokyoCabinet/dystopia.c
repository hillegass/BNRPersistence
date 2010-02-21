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


#include "dystopia.h"
#include "myconf.h"

#define IDBDIRMODE     00755             // permission of created directories
#define IDBIOBUFSIZ    65536             // size of an I/O buffer
#define IDBDEFFWMMAX   2048              // default maximum number forward matching expansion
#define IDBTXDBNAME    "dystopia.tch"    // name of the text database
#define IDBTXDBMAGIC   0x49              // magic data for identification

#define IDBDEFERNUM    1000000           // default expected record number
#define IDBDEFETNUM    1000000           // default expected token number
#define IDBDEFIUSIZ    (1024LL*1024*512) // default unit size of each index file
#define IDBTXBNUMCO    2                 // coefficient of the bucket number
#define IDBTXAPOW      3                 // alignment power of the text database
#define IDBTXFPOW      10                // free block pool power of the text database


/* private function prototypes */
static bool tcidblockmethod(TCIDB *idb, bool wr);
static bool tcidbunlockmethod(TCIDB *idb);
static bool tcidbsynccb(int total, int current, const char *msg, TCIDB *idb);
static bool tcidbopenimpl(TCIDB *idb, const char *path, int omode);
static bool tcidbcloseimpl(TCIDB *idb);
static bool tcidbputimpl(TCIDB *idb, int64_t id, const char *text);
static bool tcidboutimpl(TCIDB *idb, int64_t id);
static char *tcidbgetimpl(TCIDB *idb, int64_t id);
static uint64_t *tcidbsearchimpl(TCIDB *idb, const char *word, int smode, int *np);
static uint64_t *tcidbsearchtoken(TCIDB *idb, const char *token, int *np);
static bool tcidboptimizeimpl(TCIDB *idb);
static bool tcidbvanishimpl(TCIDB *idb);
static bool tcidbcopyimpl(TCIDB *idb, const char *path);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Get the message string corresponding to an error code. */
const char *tcidberrmsg(int ecode){
  return tchdberrmsg(ecode);
}


/* Create an indexed database object. */
TCIDB *tcidbnew(void){
  TCIDB *idb = tcmalloc(sizeof(*idb));
  idb->mmtx = tcmalloc(sizeof(pthread_rwlock_t));
  if(pthread_rwlock_init(idb->mmtx, NULL) != 0) tcmyfatal("pthread_rwlock_init failed");
  idb->txdb = tchdbnew();
  if(!tchdbsetmutex(idb->txdb)) tcmyfatal("tchdbsetmutex failed");
  TCQDB **idxs = idb->idxs;
  for(int i = 0; i < IDBQDBMAX; i++){
    idxs[i] = tcqdbnew();
    tcqdbsetsynccb(idxs[i], (bool (*)(int, int, const char *, void *))tcidbsynccb, idb);
  }
  idb->inum = 0;
  idb->cnum = 0;
  idb->path = NULL;
  idb->wmode = false;
  idb->qopts = 0;
  idb->qomode = 0;
  idb->ernum = IDBDEFERNUM;
  idb->etnum = IDBDEFETNUM;
  idb->iusiz = IDBDEFIUSIZ;
  idb->opts = 0;
  idb->synccb = NULL;
  idb->syncopq = NULL;
  idb->exopts = 0;
  return idb;
}


/* Delete an indexed database object. */
void tcidbdel(TCIDB *idb){
  assert(idb);
  if(idb->path) tcidbclose(idb);
  TCQDB **idxs = idb->idxs;
  for(int i = IDBQDBMAX - 1; i >= 0; i--){
    tcqdbdel(idxs[i]);
  }
  tchdbdel(idb->txdb);
  pthread_rwlock_destroy(idb->mmtx);
  tcfree(idb->mmtx);
  tcfree(idb);
}


/* Get the last happened error code of an indexed database object. */
int tcidbecode(TCIDB *idb){
  assert(idb);
  return tchdbecode(idb->txdb);
}


/* Set the tuning parameters of an indexed database object. */
bool tcidbtune(TCIDB *idb, int64_t ernum, int64_t etnum, int64_t iusiz, uint8_t opts){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  idb->ernum = (ernum > 0) ? ernum : IDBDEFERNUM;
  idb->etnum = (etnum > 0) ? etnum : IDBDEFETNUM;
  idb->iusiz = (iusiz > 0) ? iusiz : IDBDEFIUSIZ;
  idb->opts = opts;
  tcidbunlockmethod(idb);
  return true;
}


/* Set the caching parameters of an indexed database object. */
bool tcidbsetcache(TCIDB *idb, int64_t icsiz, int32_t lcnum){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  TCQDB **idxs = idb->idxs;
  for(int i = 0; i < IDBQDBMAX; i++){
    tcqdbsetcache(idxs[i], icsiz, lcnum);
  }
  tcidbunlockmethod(idb);
  return true;
}


/* Set the maximum number of forward matching expansion of an indexed database object. */
bool tcidbsetfwmmax(TCIDB *idb, uint32_t fwmmax){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  TCQDB **idxs = idb->idxs;
  for(int i = 0; i < IDBQDBMAX; i++){
    tcqdbsetfwmmax(idxs[i], fwmmax);
  }
  tcidbunlockmethod(idb);
  return true;
}


/* Open an indexed database object. */
bool tcidbopen(TCIDB *idb, const char *path, int omode){
  assert(idb && path);
  if(!tcidblockmethod(idb, true)) return false;
  if(idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbopenimpl(idb, path, omode);
  tcidbunlockmethod(idb);
  return rv;
}


/* Close an indexed database object. */
bool tcidbclose(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbcloseimpl(idb);
  tcidbunlockmethod(idb);
  return rv;
}


/* Store a record into an indexed database object. */
bool tcidbput(TCIDB *idb, int64_t id, const char *text){
  assert(idb && id > 0 && text);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbputimpl(idb, id, text);
  tcidbunlockmethod(idb);
  return rv;
}


/* Remove a record of an indexed database object. */
bool tcidbout(TCIDB *idb, int64_t id){
  assert(idb && id > 0);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidboutimpl(idb, id);
  tcidbunlockmethod(idb);
  return rv;
}


/* Retrieve a record of an indexed database object. */
char *tcidbget(TCIDB *idb, int64_t id){
  assert(idb && id > 0);
  if(!tcidblockmethod(idb, false)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  char *rv = tcidbgetimpl(idb, id);
  tcidbunlockmethod(idb);
  return rv;
}


/* Search an indexed database. */
uint64_t *tcidbsearch(TCIDB *idb, const char *word, int smode, int *np){
  assert(idb && word && np);
  if(!tcidblockmethod(idb, false)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  char *nword = tcstrdup(word);
  tctextnormalize(nword, TCTNLOWER | TCTNNOACC | TCTNSPACE);
  uint64_t *rv;
  if(smode == IDBSTOKEN){
    QDBRSET rsets[4];
    char *token = tcmalloc(strlen(nword) + 3);
    sprintf(token, "%s", nword);
    rsets[0].ids = tcidbsearchimpl(idb, token, IDBSFULL, &rsets[0].num);
    sprintf(token, " %s ", nword);
    rsets[1].ids = tcidbsearchimpl(idb, token, IDBSSUBSTR, &rsets[1].num);
    sprintf(token, "%s ", nword);
    rsets[2].ids = tcidbsearchimpl(idb, token, IDBSPREFIX, &rsets[2].num);
    sprintf(token, " %s", nword);
    rsets[3].ids = tcidbsearchimpl(idb, token, IDBSSUFFIX, &rsets[3].num);
    rv = tcqdbresunion(rsets, 4, np);
    tcfree(rsets[3].ids);
    tcfree(rsets[2].ids);
    tcfree(rsets[1].ids);
    tcfree(rsets[0].ids);
    tcfree(token);
  } else if(smode == IDBSTOKPRE){
    QDBRSET rsets[2];
    char *token = tcmalloc(strlen(nword) + 3);
    sprintf(token, "%s", nword);
    rsets[0].ids = tcidbsearchimpl(idb, token, IDBSPREFIX, &rsets[0].num);
    sprintf(token, " %s", nword);
    rsets[1].ids = tcidbsearchimpl(idb, token, IDBSSUBSTR, &rsets[1].num);
    rv = tcqdbresunion(rsets, 2, np);
    tcfree(rsets[1].ids);
    tcfree(rsets[0].ids);
    tcfree(token);
  } else if(smode == IDBSTOKSUF){
    QDBRSET rsets[2];
    char *token = tcmalloc(strlen(nword) + 3);
    sprintf(token, "%s", nword);
    rsets[0].ids = tcidbsearchimpl(idb, token, IDBSSUFFIX, &rsets[0].num);
    sprintf(token, "%s ", nword);
    rsets[1].ids = tcidbsearchimpl(idb, token, IDBSSUBSTR, &rsets[1].num);
    rv = tcqdbresunion(rsets, 2, np);
    tcfree(rsets[1].ids);
    tcfree(rsets[0].ids);
    tcfree(token);
  } else {
    rv = tcidbsearchimpl(idb, nword, smode, np);
  }
  tcfree(nword);
  tcidbunlockmethod(idb);
  return rv;
}


/* Search an indexed database with a compound expression. */
uint64_t *tcidbsearch2(TCIDB *idb, const char *expr, int *np){
  assert(idb && expr && np);
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
    uint64_t *res = tcidbsearchtoken(idb, tclistval2(terms, 0), np);
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
      rsets[rsnum].ids = tcidbsearchtoken(idb, term, &rsets[rsnum].num);
      int rsover = 0;
      while(ti + 2 < tnum && !strcmp(tclistval2(terms, ti + 1), "||")){
        rsover++;
        int ri = rsnum + rsover;
        rsets[ri].ids = tcidbsearchtoken(idb, tclistval2(terms, ti + 2), &rsets[ri].num);
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


/* Initialize the iterator of an indexed database object. */
bool tcidbiterinit(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tchdbiterinit(idb->txdb);
  tcidbunlockmethod(idb);
  return rv;
}


/* Get the next ID number of the iterator of an indexed database object. */
uint64_t tcidbiternext(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  uint64_t rv = 0;
  int vsiz;
  char *vbuf = tchdbiternext(idb->txdb, &vsiz);
  if(vbuf){
    TDREADVNUMBUF64(vbuf, rv, vsiz);
    tcfree(vbuf);
  }
  tcidbunlockmethod(idb);
  return rv;
}


/* Synchronize updated contents of an indexed database object with the files and the device. */
bool tcidbsync(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbmemsync(idb, 2);
  tcidbunlockmethod(idb);
  return rv;
}


/* Optimize the files of an indexed database object. */
bool tcidboptimize(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidboptimizeimpl(idb);
  tcidbunlockmethod(idb);
  return rv;
}


/* Remove all records of an indexed database object. */
bool tcidbvanish(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, true)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbvanishimpl(idb);
  tcidbunlockmethod(idb);
  return rv;
}


/* Copy the database directory of an indexed database object. */
bool tcidbcopy(TCIDB *idb, const char *path){
  assert(idb);
  if(!tcidblockmethod(idb, false)) return false;
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return false;
  }
  bool rv = tcidbcopyimpl(idb, path);
  tcidbunlockmethod(idb);
  return rv;
}


/* Get the directory path of an indexed database object. */
const char *tcidbpath(TCIDB *idb){
  assert(idb);
  return idb->path;
}


/* Get the number of records of an indexed database object. */
uint64_t tcidbrnum(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, false)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return 0;
  }
  uint64_t rv = tchdbrnum(idb->txdb);
  tcidbunlockmethod(idb);
  return rv;
}


/* Get the total size of the database files of an indexed database object. */
uint64_t tcidbfsiz(TCIDB *idb){
  assert(idb);
  if(!tcidblockmethod(idb, false)) return false;
  if(!idb->path){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    tcidbunlockmethod(idb);
    return 0;
  }
  uint64_t rv = tchdbfsiz(idb->txdb);
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  for(int i = 0; i < inum; i++){
    rv += tcqdbfsiz(idxs[i]);
  }
  tcidbunlockmethod(idb);
  return rv;
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set the file descriptor for debugging output. */
void tcidbsetdbgfd(TCIDB *idb, int fd){
  assert(idb);
  tchdbsetdbgfd(idb->txdb, fd);
  TCQDB **idxs = idb->idxs;
  for(int i = 0; i < IDBQDBMAX; i++){
    tcqdbsetdbgfd(idxs[i], fd);
  }
}


/* Get the file descriptor for debugging output. */
int tcidbdbgfd(TCIDB *idb){
  assert(idb);
  return tchdbdbgfd(idb->txdb);
}


/* Synchronize updating contents on memory of an indexed database object. */
bool tcidbmemsync(TCIDB *idb, int level){
  assert(idb);
  if(!idb->path || !idb->wmode){
    tchdbsetecode(idb->txdb, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  char *txopq = tchdbopaque(txdb);
  *(uint8_t *)(txopq + sizeof(uint8_t)) = inum;
  bool err = false;
  if(!tchdbmemsync(txdb, false)) err = true;
  for(int i = 0; i < inum; i++){
    if(!tcqdbmemsync(idxs[i], level)){
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Get the inode number of the database file of an indexed database object. */
uint64_t tcidbinode(TCIDB *idb){
  assert(idb);
  return tchdbinode(idb->txdb);
}


/* Get the modification time of the database file of an indexed database object. */
time_t tcidbmtime(TCIDB *idb){
  assert(idb);
  return tchdbmtime(idb->txdb);
}


/* Get the options of an indexed database object. */
uint8_t tcidbopts(TCIDB *idb){
  assert(idb);
  return idb->opts;
}


/* Set the callback function for sync progression of an indexed database object. */
void tcidbsetsynccb(TCIDB *idb, bool (*cb)(int, int, const char *, void *), void *opq){
  assert(idb);
  idb->synccb = cb;
  idb->syncopq = opq;
}


/* Set the expert options of an indexed database object. */
void tcidbsetexopts(TCIDB *idb, uint32_t exopts){
  assert(idb);
  idb->exopts = exopts;
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/


/* Lock a method of the indexed database object.
   `idb' specifies the indexed database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
static bool tcidblockmethod(TCIDB *idb, bool wr){
  assert(idb);
  if(wr ? pthread_rwlock_wrlock(idb->mmtx) != 0 : pthread_rwlock_rdlock(idb->mmtx) != 0){
    tchdbsetecode(idb->txdb, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Unlock a method of the indexed database object.
   `idb' specifies the indexed database object.
   If successful, the return value is true, else, it is false. */
static bool tcidbunlockmethod(TCIDB *idb){
  assert(idb);
  if(pthread_rwlock_unlock(idb->mmtx) != 0){
    tchdbsetecode(idb->txdb, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Call the callback for sync progression.
   `total' specifies the number of tokens to be synchronized.
   `current' specifies the number of processed tokens.
   `msg' specifies the message string.
   `idb' specifies the indexed database object.
   The return value is true usually, or false if the sync operation should be terminated. */
static bool tcidbsynccb(int total, int current, const char *msg, TCIDB *idb){
  bool rv = idb->synccb ? idb->synccb(total, current, msg, idb->syncopq) : true;
  if((total|current) == 0 && !strcmp(msg, QDBSYNCMSGL) &&
     tcqdbfsiz(idb->idxs[idb->cnum]) >= idb->iusiz && idb->inum > 0){
    TCQDB **idxs = idb->idxs;
    if(idb->synccb && !idb->synccb(total, current, "to be cycled", idb->syncopq)) rv = false;
    if(!tcqdbcacheclear(idxs[idb->cnum])){
      tchdbsetecode(idb->txdb, tcqdbecode(idxs[idb->cnum]), __FILE__, __LINE__, __func__);
      rv = false;
    }
    int inum = idb->inum;
    idb->cnum = 0;
    uint64_t min = UINT64_MAX;
    for(int i = 0; i < inum; i++){
      uint64_t fsiz = tcqdbfsiz(idxs[i]);
      if(fsiz < min){
        idb->cnum = i;
        min = fsiz;
      }
    }
    if(min > idb->iusiz && inum < IDBQDBMAX) idb->cnum = inum;
  }
  return rv;
}


/* Open an indexed database object.
   `idb' specifies the indexed database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode.
   If successful, the return value is true, else, it is false. */
static bool tcidbopenimpl(TCIDB *idb, const char *path, int omode){
  assert(idb && path);
  char pbuf[strlen(path)+TDNUMBUFSIZ];
  if(omode & IDBOWRITER){
    if(omode & IDBOCREAT){
      if(mkdir(path, IDBDIRMODE) == -1 && errno != EEXIST){
        int ecode = TCEMKDIR;
        switch(errno){
        case EACCES: ecode = TCENOPERM; break;
        case ENOENT: ecode = TCENOFILE; break;
        }
        tchdbsetecode(idb->txdb, ecode, __FILE__, __LINE__, __func__);
        return false;
      }
    }
    if(omode & IDBOTRUNC){
      sprintf(pbuf, "%s%c%s", path, MYPATHCHR, IDBTXDBNAME);
      if(unlink(pbuf) == -1 && errno != ENOENT){
        tchdbsetecode(idb->txdb, TCEUNLINK, __FILE__, __LINE__, __func__);
        return false;
      }
      for(int i = 0; i < IDBQDBMAX; i++){
        sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
        if(unlink(pbuf) == -1 && errno != ENOENT){
          tchdbsetecode(idb->txdb, TCEUNLINK, __FILE__, __LINE__, __func__);
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
    tchdbsetecode(idb->txdb, ecode, __FILE__, __LINE__, __func__);
    return false;
  }
  if(!S_ISDIR(sbuf.st_mode)){
    tchdbsetecode(idb->txdb, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  int homode = HDBOREADER;
  uint8_t hopts = 0;
  int qomode = QDBOREADER;
  uint8_t qopts = 0;
  int64_t etnum = idb->etnum;
  int64_t iusiz = idb->iusiz;
  if(omode & IDBOWRITER){
    homode = HDBOWRITER;
    qomode = QDBOWRITER;
    if(omode & IDBOCREAT){
      homode |= HDBOCREAT;
      qomode |= QDBOCREAT;
    }
    if(omode & IDBOTRUNC){
      homode |= HDBOTRUNC;
      qomode |= QDBOTRUNC;
    }
    int64_t bnum = idb->ernum * IDBTXBNUMCO + 1;
    if(idb->opts & IDBTLARGE){
      hopts |= HDBTLARGE;
      qopts |= QDBTLARGE;
    }
    if(idb->opts & IDBTDEFLATE) qopts |= QDBTDEFLATE;
    if(idb->opts & IDBTBZIP) qopts |= QDBTBZIP;
    if(idb->opts & IDBTTCBS){
      hopts |= HDBTTCBS;
      qopts |= QDBTTCBS;
    }
    if(idb->exopts & IDBXNOTXT){
      if(!tchdbtune(txdb, 1, 0, 0, 0)) return false;
    } else {
      if(!tchdbtune(txdb, bnum, IDBTXAPOW, IDBTXFPOW, hopts)) return false;
    }
  }
  if(omode & IDBONOLCK){
    homode |= HDBONOLCK;
    qomode |= QDBONOLCK;
  }
  if(omode & IDBOLCKNB){
    homode |= HDBOLCKNB;
    qomode |= QDBOLCKNB;
  }
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, IDBTXDBNAME);
  if(!tchdbopen(txdb, pbuf, homode)) return false;
  char *txopq = tchdbopaque(txdb);
  uint8_t magic = *(uint8_t *)txopq;
  if(magic == 0 && (omode & IDBOWRITER)){
    *(uint8_t *)txopq = IDBTXDBMAGIC;
    *(uint8_t *)(txopq + sizeof(magic) + sizeof(uint8_t)) = qopts;
    uint64_t llnum = TDHTOILL(etnum);
    memcpy(txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(qopts), &llnum, sizeof(llnum));
    llnum = TDHTOILL(iusiz);
    memcpy(txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(qopts) + sizeof(llnum),
           &llnum, sizeof(llnum));
  } else {
    qopts = *(uint8_t *)(txopq + sizeof(magic) + sizeof(uint8_t));
    memcpy(&etnum, txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(qopts), sizeof(etnum));
    etnum = TDITOHLL(etnum);
    memcpy(&iusiz, txopq + sizeof(magic) + sizeof(uint8_t) + sizeof(qopts) + sizeof(etnum),
           sizeof(iusiz));
    iusiz = TDITOHLL(iusiz);
  }
  if(omode & IDBOWRITER){
    for(int i = 0; i < IDBQDBMAX; i++){
      if(!tcqdbtune(idxs[i], etnum, qopts)){
        tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
        return false;
      }
    }
  }
  idb->opts = 0;
  if(qopts & QDBTLARGE) idb->opts |= QDBTLARGE;
  if(qopts & QDBTDEFLATE) idb->opts |= QDBTDEFLATE;
  if(qopts & QDBTBZIP) idb->opts |= QDBTBZIP;
  if(qopts & QDBTTCBS) idb->opts |= IDBTTCBS;
  uint8_t inum;
  memcpy(&inum, txopq + sizeof(magic), sizeof(inum));
  if(inum > IDBQDBMAX){
    tchdbclose(txdb);
    tchdbsetecode(txdb, TCEMETA, __FILE__, __LINE__, __func__);
    return false;
  }
  idb->cnum = 0;
  uint64_t min = UINT64_MAX;
  for(int i = 0; i < inum; i++){
    sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
    if(!tcqdbopen(idxs[i], pbuf, qomode)){
      tchdbclose(txdb);
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      for(int j = i - 1; j >= 0; j--){
        tcqdbclose(idxs[i]);
      }
      return false;
    }
    uint64_t fsiz = tcqdbfsiz(idxs[i]);
    if(fsiz < min){
      idb->cnum = i;
      min = fsiz;
    }
  }
  idb->inum = inum;
  idb->path = tcstrdup(path);
  idb->wmode = omode & IDBOWRITER;
  idb->qopts = qopts;
  idb->qomode = qomode;
  idb->iusiz = iusiz;
  return true;
}


/* Close an indexed database object.
   `idb' specifies the indexed database object.
   If successful, the return value is true, else, it is false. */
static bool tcidbcloseimpl(TCIDB *idb){
  assert(idb);
  bool err = false;
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  if(idb->wmode){
    char *txopq = tchdbopaque(txdb);
    *(uint8_t *)(txopq + sizeof(uint8_t)) = inum;
  }
  idb->inum = 0;
  for(int i = 0; i < inum; i++){
    if(!tcqdbclose(idxs[i])){
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  if(!tchdbclose(txdb)) err = true;
  tcfree(idb->path);
  idb->path = NULL;
  return !err;
}


/* Store a record into an indexed database object.
   `idb' specifies the indexed database object.
   `id' specifies the ID number of the record.
   `text' specifies the string of the record.
   If successful, the return value is true, else, it is false. */
static bool tcidbputimpl(TCIDB *idb, int64_t id, const char *text){
  assert(idb && id > 0 && text);
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  uint8_t cnum = idb->cnum;
  if(cnum >= inum){
    char pbuf[strlen(idb->path)+TDNUMBUFSIZ];
    sprintf(pbuf, "%s%c%04d", idb->path, MYPATHCHR, inum + 1);
    TCQDB *nidx = idxs[inum];
    if(!tcqdbopen(nidx, pbuf, idb->qomode | IDBOCREAT)){
      tchdbsetecode(txdb, tcqdbecode(nidx), __FILE__, __LINE__, __func__);
      return false;
    }
    idb->cnum = idb->inum;
    cnum = idb->cnum;
    idb->inum++;
  }
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  char stack[IDBIOBUFSIZ];
  int vsiz = tchdbget3(txdb, kbuf, ksiz, stack, IDBIOBUFSIZ);
  uint8_t ocnum;
  if(vsiz >= sizeof(ocnum)){
    ocnum = ((uint8_t *)stack)[vsiz-1];
    if(ocnum >= IDBQDBMAX){
      tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    TCQDB *oidx = idxs[ocnum];
    if(vsiz >= IDBIOBUFSIZ){
      char *vbuf = tchdbget(txdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        if(vsiz < sizeof(ocnum)){
          tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
          tcfree(vbuf);
          return false;
        }
        vbuf[vsiz-1] = '\0';
        tctextnormalize(vbuf, TCTNLOWER | TCTNNOACC | TCTNSPACE);
        if(!tcqdbout(oidx, id, vbuf)){
          tchdbsetecode(txdb, tcqdbecode(oidx), __FILE__, __LINE__, __func__);
          tcfree(vbuf);
          return false;
        }
        tcfree(vbuf);
      } else {
        tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
        return false;
      }
    } else {
      stack[vsiz-1] = '\0';
      tctextnormalize(stack, TCTNLOWER | TCTNNOACC | TCTNSPACE);
      if(!tcqdbout(oidx, id, stack)){
        tchdbsetecode(txdb, tcqdbecode(oidx), __FILE__, __LINE__, __func__);
        return false;
      }
    }
    if(!tchdbout(txdb, kbuf, ksiz)) return false;
  }
  int tlen = strlen(text);
  char *vbuf = (tlen < IDBIOBUFSIZ - sizeof(cnum)) ? stack : tcmalloc(tlen + sizeof(cnum));
  memcpy(vbuf, text, tlen);
  ((uint8_t *)vbuf)[tlen] = cnum;
  if(!(idb->exopts & IDBXNOTXT) && !tchdbputkeep(txdb, kbuf, ksiz, vbuf, tlen + sizeof(cnum))){
    if(vbuf != stack) tcfree(vbuf);
    return false;
  }
  vbuf[tlen] = '\0';
  tctextnormalize(vbuf, TCTNLOWER | TCTNNOACC | TCTNSPACE);
  TCQDB *cidx = idxs[cnum];
  if(!tcqdbput(cidx, id, vbuf)){
    tchdbsetecode(txdb, tcqdbecode(cidx), __FILE__, __LINE__, __func__);
    if(vbuf != stack) tcfree(vbuf);
    return false;
  }
  if(vbuf != stack) tcfree(vbuf);
  return true;
}


/* Remove a record of an indexed database object.
   `idb' specifies the indexed database object.
   `id' specifies the ID number of the record.
   If successful, the return value is true, else, it is false. */
static bool tcidboutimpl(TCIDB *idb, int64_t id){
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  char stack[IDBIOBUFSIZ];
  int vsiz = tchdbget3(txdb, kbuf, ksiz, stack, IDBIOBUFSIZ);
  uint8_t ocnum;
  if(vsiz >= sizeof(ocnum)){
    ocnum = ((uint8_t *)stack)[vsiz-1];
    if(ocnum >= IDBQDBMAX){
      tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    TCQDB *oidx = idxs[ocnum];
    if(vsiz >= IDBIOBUFSIZ){
      char *vbuf = tchdbget(txdb, kbuf, ksiz, &vsiz);
      if(vbuf){
        if(vsiz < sizeof(ocnum)){
          tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
          tcfree(vbuf);
          return false;
        }
        vbuf[vsiz-1] = '\0';
        tctextnormalize(vbuf, TCTNLOWER | TCTNNOACC | TCTNSPACE);
        if(!tcqdbout(oidx, id, vbuf)){
          tchdbsetecode(txdb, tcqdbecode(oidx), __FILE__, __LINE__, __func__);
          tcfree(vbuf);
          return false;
        }
        tcfree(vbuf);
      } else {
        tchdbsetecode(txdb, TCEMISC, __FILE__, __LINE__, __func__);
        return false;
      }
    } else {
      stack[vsiz-1] = '\0';
      tctextnormalize(stack, TCTNLOWER | TCTNNOACC | TCTNSPACE);
      if(!tcqdbout(oidx, id, stack)){
        tchdbsetecode(txdb, tcqdbecode(oidx), __FILE__, __LINE__, __func__);
        return false;
      }
    }
    if(!tchdbout(txdb, kbuf, ksiz)) return false;
    return true;
  }
  tchdbsetecode(txdb, TCENOREC, __FILE__, __LINE__, __func__);
  return false;
}


/* Retrieve a record of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   `id' specifies the ID number of the record.  It should be positive.
   If successful, the return value is the string of the corresponding record, else, it is
   `NULL'. */
static char *tcidbgetimpl(TCIDB *idb, int64_t id){
  assert(idb && id > 0);
  char kbuf[TDNUMBUFSIZ];
  int ksiz;
  TDSETVNUMBUF64(ksiz, kbuf, id);
  int vsiz;
  char *vbuf = tchdbget(idb->txdb, kbuf, ksiz, &vsiz);
  if(!vbuf) return NULL;
  if(vsiz < sizeof(uint8_t)){
    tcfree(vbuf);
    tchdbsetecode(idb->txdb, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  vbuf[vsiz-1] = '\0';
  return vbuf;
}


/* Search an indexed database.
   `idb' specifies the indexed database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure. */
static uint64_t *tcidbsearchimpl(TCIDB *idb, const char *word, int smode, int *np){
  assert(idb && word && np);
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  if(inum < 1){
    *np = 0;
    return tcmalloc(1);
  }
  if(inum == 1){
    uint64_t *res = tcqdbsearch(idxs[0], word, smode, np);
    if(!res) tchdbsetecode(idb->txdb, tcqdbecode(idxs[0]), __FILE__, __LINE__, __func__);
    return res;
  }
  QDBRSET rsets[inum];
  for(int i = 0; i < inum; i++){
    rsets[i].ids = tcqdbsearch(idxs[i], word, smode, &rsets[i].num);
  }
  uint64_t *res = tcqdbresunion(rsets, inum, np);
  for(int i = 0; i < inum; i++){
    tcfree(rsets[i].ids);
  }
  return res;
}


/* Search an indexed database with a token expression.
   `idb' specifies the indexed database object.
   `token' specifies the string of the token expression.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records.  `NULL' is returned on failure. */
static uint64_t *tcidbsearchtoken(TCIDB *idb, const char *token, int *np){
  assert(idb && token && np);
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
    uint64_t *res = tcidbsearch(idb, bare, IDBSSUBSTR, np);
    tcfree(bare);
    return res;
  }
  if(len < 4) return tcidbsearch(idb, token, IDBSSUBSTR, np);
  if(token[0] == '[' && token[1] == '[' && token[2] == '[' && token[3] == '['){
    char *bare = tcmemdup(token + 4, len - 4);
    uint64_t *res = tcidbsearch(idb, bare, IDBSPREFIX, np);
    tcfree(bare);
    return res;
  }
  if(token[len-1] == ']' && token[len-2] == ']' && token[len-3] == ']' && token[len-4] == ']'){
    char *bare = tcmemdup(token, len - 4);
    uint64_t *res = tcidbsearch(idb, bare, IDBSSUFFIX, np);
    tcfree(bare);
    return res;
  }
  if(token[0] != '[' || token[1] != '[' || token[len-1] != ']' || token[len-2] != ']')
    return tcidbsearch(idb, token, IDBSSUBSTR, np);
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
  int smode = IDBSTOKEN;
  if(prefix && suffix){
    smode = IDBSSUBSTR;
  } else if(prefix){
    smode = IDBSTOKPRE;
  } else if(suffix){
    smode = IDBSTOKSUF;
  }
  uint64_t *res = tcidbsearch(idb, bare, smode, np);
  tcfree(bare);
  return res;
}


/* Optimize the file of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   If successful, the return value is true, else, it is false. */
static bool tcidboptimizeimpl(TCIDB *idb){
  assert(idb);
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  bool err = false;
  if(!tchdboptimize(txdb, -1, -1, -1, UINT8_MAX)) err = true;
  for(int i = 0; i < inum; i++){
    if(!tcqdboptimize(idxs[i])){
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Remove all records of an indexed database object.
   `idb' specifies the indexed database object connected as a writer.
   If successful, the return value is true, else, it is false. */
static bool tcidbvanishimpl(TCIDB *idb){
  assert(idb);
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  bool err = false;
  if(!tchdbvanish(txdb)) err = true;
  char *txopq = tchdbopaque(txdb);
  *(uint8_t *)(txopq + sizeof(uint8_t) + sizeof(uint8_t)) = idb->qopts;
  for(int i = 0; i < inum; i++){
    if(!tcqdbvanish(idxs[i])){
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}


/* Copy the database directory of an indexed database object.
   `idb' specifies the indexed database object.
   `path' specifies the path of the destination directory.
   If successful, the return value is true, else, it is false. */
static bool tcidbcopyimpl(TCIDB *idb, const char *path){
  assert(idb && path);
  TCHDB *txdb = idb->txdb;
  TCQDB **idxs = idb->idxs;
  uint8_t inum = idb->inum;
  bool err = false;
  if(mkdir(path, IDBDIRMODE) == -1 && errno != EEXIST){
    int ecode = TCEMKDIR;
    switch(errno){
    case EACCES: ecode = TCENOPERM; break;
    case ENOENT: ecode = TCENOFILE; break;
    }
    tchdbsetecode(txdb, ecode, __FILE__, __LINE__, __func__);
    return false;
  }
  char pbuf[strlen(path)+TDNUMBUFSIZ];
  sprintf(pbuf, "%s%c%s", path, MYPATHCHR, IDBTXDBNAME);
  if(!tchdbcopy(txdb, pbuf)) err = true;
  for(int i = 0; i < inum; i++){
    sprintf(pbuf, "%s%c%04d", path, MYPATHCHR, i + 1);
    if(!tcqdbcopy(idxs[i], pbuf)){
      tchdbsetecode(txdb, tcqdbecode(idxs[i]), __FILE__, __LINE__, __func__);
      err = true;
    }
  }
  return !err;
}



// END OF FILE
