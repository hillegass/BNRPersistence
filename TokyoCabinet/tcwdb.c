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


#include "tcwdb.h"
#include "myconf.h"

#define WDBMAGICDATA   "[word]"          // magic data for identification
#define WDBIOBUFSIZ    65536             // size of an I/O buffer
#define WDBMAXWORDLEN  1024              // maximum length of each search word
#define WDBWORDUNIT    1024              // unit number of word allocation
#define WDBRESUNIT     1024              // unit number of result allocation
#define WDBCCBNUM      1048573           // bucket number of the token cache
#define WDBCCDEFICSIZ  (1024LL*1024*128) // default capacity of the token cache
#define WDBDIDSBNUM    262139            // bucket number of the deleted ID set
#define WDBDTKNBNUM    262139            // bucket number of the deleted token map
#define WDBDEFFWMMAX   2048              // default maximum number forward matching expansion
#define WDBHJBNUMCO    4                 // coefficient of the bucket number for hash join

#define WDBDEFETNUM    1000000           // default expected token number
#define WDBLMEMB       256               // number of members in each leaf of the index
#define WDBNMEMB       512               // number of members in each node of the index
#define WDBAPOW        9                 // alignment power of the index
#define WDBFPOW        11                // free block pool power of the index
#define WDBLSMAX       8192              // maximum size of each leaf of the index
#define WDBLCNUMW      64                // number of cached leaf nodes for writer
#define WDBLCNUMR      1024              // number of cached leaf nodes for reader
#define WDBNCNUM       1024              // number of cached non-leaf nodes


/* private function prototypes */
static bool tcwdblockmethod(TCWDB *wdb, bool wr);
static bool tcwdbunlockmethod(TCWDB *wdb);
static bool tcwdbopenimpl(TCWDB *wdb, const char *path, int omode);
static bool tcwdbcloseimpl(TCWDB *wdb);
static bool tcwdbputimpl(TCWDB *wdb, int64_t id, const TCLIST *words);
static bool tcwdboutimpl(TCWDB *wdb, int64_t id, const TCLIST *words);
static uint64_t *tcwdbsearchimpl(TCWDB *wdb, const char *word, int *np);
static int tccmpwords(const char **a, const char **b);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Get the message string corresponding to an error code. */
const char *tcwdberrmsg(int ecode){
  return tcbdberrmsg(ecode);
}


/* Create a word database object. */
TCWDB *tcwdbnew(void){
  TCWDB *wdb = tcmalloc(sizeof(*wdb));
  wdb->mmtx = tcmalloc(sizeof(pthread_rwlock_t));
  if(pthread_rwlock_init(wdb->mmtx, NULL) != 0) tcmyfatal("pthread_rwlock_init failed");
  wdb->idx = tcbdbnew();
  if(!tcbdbsetmutex(wdb->idx)) tcmyfatal("tcbdbsetmutex failed");
  wdb->open = false;
  wdb->cc = NULL;
  wdb->icsiz = WDBCCDEFICSIZ;
  wdb->lcnum = 0;
  wdb->dtokens = NULL;
  wdb->dids = NULL;
  wdb->etnum = WDBDEFETNUM;
  wdb->opts = 0;
  wdb->fwmmax = WDBDEFFWMMAX;
  wdb->synccb = NULL;
  wdb->syncopq = NULL;
  wdb->addcb = NULL;
  wdb->addopq = NULL;
  return wdb;
}


/* Delete a word database object. */
void tcwdbdel(TCWDB *wdb){
  assert(wdb);
  if(wdb->open) tcwdbclose(wdb);
  tcbdbdel(wdb->idx);
  pthread_rwlock_destroy(wdb->mmtx);
  tcfree(wdb->mmtx);
  tcfree(wdb);
}


/* Get the last happened error code of a word database object. */
int tcwdbecode(TCWDB *wdb){
  assert(wdb);
  return tcbdbecode(wdb->idx);
}


/* Set the tuning parameters of a word database object. */
bool tcwdbtune(TCWDB *wdb, int64_t etnum, uint8_t opts){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  wdb->etnum = (etnum > 0) ? etnum : WDBDEFETNUM;
  wdb->opts = opts;
  tcwdbunlockmethod(wdb);
  return true;
}


/* Set the caching parameters of a word database object. */
bool tcwdbsetcache(TCWDB *wdb, int64_t icsiz, int32_t lcnum){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  wdb->icsiz = (icsiz > 0) ? icsiz : WDBCCDEFICSIZ;
  wdb->lcnum = (lcnum > 0) ? lcnum : 0;
  tcwdbunlockmethod(wdb);
  return true;
}


/* Set the maximum number of forward matching expansion of a word database object. */
bool tcwdbsetfwmmax(TCWDB *wdb, uint32_t fwmmax){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  wdb->fwmmax = fwmmax;
  tcwdbunlockmethod(wdb);
  return true;
}


/* Open a word database object. */
bool tcwdbopen(TCWDB *wdb, const char *path, int omode){
  assert(wdb && path);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool rv = tcwdbopenimpl(wdb, path, omode);
  tcwdbunlockmethod(wdb);
  return rv;
}


/* Close a word database object. */
bool tcwdbclose(TCWDB *wdb){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool rv = tcwdbcloseimpl(wdb);
  tcwdbunlockmethod(wdb);
  return rv;
}


/* Store a record into a word database object. */
bool tcwdbput(TCWDB *wdb, int64_t id, const TCLIST *words){
  assert(wdb && id > 0 && words);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  if(tcidsetcheck(wdb->dids, id) && !tcwdbmemsync(wdb, 0)){
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool rv = tcwdbputimpl(wdb, id, words);
  tcwdbunlockmethod(wdb);
  return rv;
}


/* Store a record with a text string into a word database object. */
bool tcwdbput2(TCWDB *wdb, int64_t id, const char *text, const char *delims){
  assert(wdb && id > 0 && text);
  TCLIST *words = tcstrsplit(text, delims ? delims : WDBSPCCHARS);
  bool rv = tcwdbput(wdb, id, words);
  tclistdel(words);
  return rv;
}


/* Remove a record of a word database object. */
bool tcwdbout(TCWDB *wdb, int64_t id, const TCLIST *words){
  assert(wdb && id > 0 && words);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  if(tcidsetcheck(wdb->dids, id)){
    tcwdbunlockmethod(wdb);
    return true;
  }
  if(tcmaprnum(wdb->cc) > 0 && !tcwdbmemsync(wdb, 0)){
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool rv = tcwdboutimpl(wdb, id, words);
  tcwdbunlockmethod(wdb);
  return rv;
}


/* Remove a record with a text string of a word database object. */
bool tcwdbout2(TCWDB *wdb, int64_t id, const char *text, const char *delims){
  assert(wdb && id > 0 && text);
  TCLIST *words = tcstrsplit(text, delims ? delims : WDBSPCCHARS);
  bool rv = tcwdbout(wdb, id, words);
  tclistdel(words);
  return rv;
}


/* Search a word database. */
uint64_t *tcwdbsearch(TCWDB *wdb, const char *word, int *np){
  assert(wdb && word && np);
  if(!tcwdblockmethod(wdb, false)) return NULL;
  if(!wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return NULL;
  }
  if(wdb->cc && (tcmaprnum(wdb->cc) > 0 || tcmaprnum(wdb->dtokens) > 0)){
    tcwdbunlockmethod(wdb);
    if(!tcwdblockmethod(wdb, true)) return NULL;
    if(!tcwdbmemsync(wdb, 0)){
      tcwdbunlockmethod(wdb);
      return NULL;
    }
    tcwdbunlockmethod(wdb);
    if(!tcwdblockmethod(wdb, false)) return NULL;
  }
  uint64_t *rv = tcwdbsearchimpl(wdb, word, np);
  tcwdbunlockmethod(wdb);
  return rv;
}


/* Synchronize updated contents of a word database object with the file and the device. */
bool tcwdbsync(TCWDB *wdb){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool err = false;
  if(!tcwdbmemsync(wdb, 2)) err = true;
  tcwdbunlockmethod(wdb);
  return !err;
}


/* Optimize the file of a word database object. */
bool tcwdboptimize(TCWDB *wdb){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool err = false;
  if(!tcwdbmemsync(wdb, 1)) err = true;
  if(!tcbdboptimize(wdb->idx, 0, 0, 0, -1, -1, UINT8_MAX)) err = true;
  tcwdbunlockmethod(wdb);
  return !err;
}


/* Remove all records of a word database object. */
bool tcwdbvanish(TCWDB *wdb){
  assert(wdb);
  if(!tcwdblockmethod(wdb, true)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool err = false;
  tcmapclear(wdb->cc);
  tcmapclear(wdb->dtokens);
  if(!tcwdbmemsync(wdb, 1)) err = true;
  if(!tcbdbvanish(wdb->idx)) err = true;
  tcwdbunlockmethod(wdb);
  return !err;
}


/* Copy the database file of a word database object. */
bool tcwdbcopy(TCWDB *wdb, const char *path){
  assert(wdb && path);
  if(!tcwdblockmethod(wdb, false)) return false;
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcwdbunlockmethod(wdb);
    return false;
  }
  bool err = false;
  if(!tcwdbmemsync(wdb, 1)) err = true;
  if(!tcbdbcopy(wdb->idx, path)) err = true;
  tcwdbunlockmethod(wdb);
  return !err;
}


/* Get the file path of a word database object. */
const char *tcwdbpath(TCWDB *wdb){
  assert(wdb);
  return tcbdbpath(wdb->idx);
}


/* Get the number of tokens of a word database object. */
uint64_t tcwdbtnum(TCWDB *wdb){
  assert(wdb);
  return tcbdbrnum(wdb->idx);
}


/* Get the size of the database file of a word database object. */
uint64_t tcwdbfsiz(TCWDB *wdb){
  assert(wdb);
  return tcbdbfsiz(wdb->idx);
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set the file descriptor for debugging output. */
void tcwdbsetdbgfd(TCWDB *wdb, int fd){
  assert(wdb && fd >= 0);
  tcbdbsetdbgfd(wdb->idx, fd);
}


/* Get the file descriptor for debugging output. */
int tcwdbdbgfd(TCWDB *wdb){
  assert(wdb);
  return tcbdbdbgfd(wdb->idx);
}


/* Synchronize updating contents on memory of a word database object. */
bool tcwdbmemsync(TCWDB *wdb, int level){
  assert(wdb);
  if(!wdb->open || !wdb->cc){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  bool err = false;
  bool (*synccb)(int, int, const char *, void *) = wdb->synccb;
  void *syncopq = wdb->syncopq;
  bool (*addcb)(const char *, void *) = wdb->addcb;
  void *addopq = wdb->addopq;
  TCBDB *idx = wdb->idx;
  TCMAP *cc = wdb->cc;
  if(synccb && !synccb(0, 0, "started", syncopq)){
    tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  if(tcmaprnum(cc) > 0){
    if(synccb && !synccb(0, 0, "getting tokens", syncopq)){
      tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    int kn;
    const char **keys = tcmapkeys2(cc, &kn);
    if(synccb && !synccb(kn, 0, "sorting tokens", syncopq)){
      tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      tcfree(keys);
      return false;
    }
    qsort(keys, kn, sizeof(*keys), (int(*)(const void *, const void *))tccmpwords);
    for(int i = 0; i < kn; i++){
      if(synccb && !synccb(kn, i + 1, "storing tokens", syncopq)){
        tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
        tcfree(keys);
        return false;
      }
      const char *kbuf = keys[i];
      int ksiz = strlen(kbuf);
      int vsiz;
      const char *vbuf = tcmapget(cc, kbuf, ksiz, &vsiz);
      if(!tcbdbputcat(idx, kbuf, ksiz, vbuf, vsiz)) err = true;
    }
    if(addcb){
      if(synccb && !synccb(0, 0, "storing keyword list", syncopq)){
        tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
        tcfree(keys);
        return false;
      }
      for(int i = 0; i < kn; i++){
        if(!addcb(keys[i], addopq)){
          tcfree(keys);
          return false;
        }
      }
    }
    tcfree(keys);
    tcmapclear(cc);
  }
  TCMAP *dtokens = wdb->dtokens;
  TCIDSET *dids = wdb->dids;
  if(tcmaprnum(dtokens) > 0){
    if(synccb && !synccb(0, 0, "getting deleted tokens", syncopq)){
      tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    int kn;
    const char **keys = tcmapkeys2(dtokens, &kn);
    if(synccb && !synccb(kn, 0, "sorting deleted tokens", syncopq)){
      tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      tcfree(keys);
      return false;
    }
    qsort(keys, kn, sizeof(*keys), (int(*)(const void *, const void *))tccmpwords);
    for(int i = 0; i < kn; i++){
      if(synccb && !synccb(kn, i + 1, "storing deleted tokens", syncopq)){
        tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
        tcfree(keys);
        return false;
      }
      const char *kbuf = keys[i];
      int ksiz = strlen(kbuf);
      int vsiz;
      const char *vbuf = tcbdbget3(idx, kbuf, ksiz, &vsiz);
      if(!vbuf) continue;
      char *nbuf = tcmalloc(vsiz + 1);
      char *wp = nbuf;
      const char *pv;
      while(vsiz > 0){
        pv = vbuf;
        int step;
        uint64_t id;
        TDREADVNUMBUF64(vbuf, id, step);
        vbuf += step;
        vsiz -= step;
        if(!tcidsetcheck(dids, id)){
          int len = vbuf - pv;
          memcpy(wp, pv, len);
          wp += len;
        }
      }
      int nsiz = wp - nbuf;
      if(nsiz > 0){
        if(!tcbdbput(idx, kbuf, ksiz, nbuf, nsiz)) err = true;
      } else {
        if(!tcbdbout(idx, kbuf, ksiz)) err = true;
      }
      tcfree(nbuf);
    }
    tcfree(keys);
    tcmapclear(dtokens);
    tcidsetclear(dids);
  }
  if(level > 0){
    if(synccb && !synccb(0, 0, "synchronizing database", syncopq)){
      tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    if(!tcbdbmemsync(idx, level > 1)) err = true;
  }
  if(synccb && !synccb(0, 0, "finished", syncopq)){
    tcbdbsetecode(wdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  return !err;
}


/* Clear the cache of a word database object. */
bool tcwdbcacheclear(TCWDB *wdb){
  assert(wdb);
  if(!wdb->open){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  return tcbdbcacheclear(wdb->idx);
}


/* Get the inode number of the database file of a word database object. */
uint64_t tcwdbinode(TCWDB *wdb){
  assert(wdb);
  return tcbdbinode(wdb->idx);
}


/* Get the modification time of the database file of a word database object. */
time_t tcwdbmtime(TCWDB *wdb){
  assert(wdb);
  return tcbdbmtime(wdb->idx);
}


/* Get the options of a word database object. */
uint8_t tcwdbopts(TCWDB *wdb){
  assert(wdb);
  return tcbdbopts(wdb->idx);
}


/* Get the maximum number of forward matching expansion of a word database object. */
uint32_t tcwdbfwmmax(TCWDB *wdb){
  assert(wdb);
  return wdb->fwmmax;
}


/* Get the number of records in the cache of a word database object. */
uint32_t tcwdbcnum(TCWDB *wdb){
  assert(wdb);
  if(!wdb->cc) return 0;
  return tcmaprnum(wdb->cc);
}


/* Set the callback function for sync progression of a word database object. */
void tcwdbsetsynccb(TCWDB *wdb, bool (*cb)(int, int, const char *, void *), void *opq){
  assert(wdb);
  wdb->synccb = cb;
  wdb->syncopq = opq;
}


/* Set the callback function for word addition of a word database object. */
void tcwdbsetaddcb(TCWDB *wdb, bool (*cb)(const char *, void *), void *opq){
  assert(wdb);
  wdb->addcb = cb;
  wdb->addopq = opq;
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/


/* Lock a method of the word database object.
   `wdb' specifies the word database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
static bool tcwdblockmethod(TCWDB *wdb, bool wr){
  assert(wdb);
  if(wr ? pthread_rwlock_wrlock(wdb->mmtx) != 0 : pthread_rwlock_rdlock(wdb->mmtx) != 0){
    tcbdbsetecode(wdb->idx, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Unlock a method of the word database object.
   `bdb' specifies the word database object.
   If successful, the return value is true, else, it is false. */
static bool tcwdbunlockmethod(TCWDB *wdb){
  assert(wdb);
  if(pthread_rwlock_unlock(wdb->mmtx) != 0){
    tcbdbsetecode(wdb->idx, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Open a word database object.
   `wdb' specifies the word database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode.
   If successful, the return value is true, else, it is false. */
static bool tcwdbopenimpl(TCWDB *wdb, const char *path, int omode){
  assert(wdb && path);
  int bomode = BDBOREADER;
  if(omode & WDBOWRITER){
    bomode = BDBOWRITER;
    if(omode & WDBOCREAT) bomode |= BDBOCREAT;
    if(omode & WDBOTRUNC) bomode |= BDBOTRUNC;
    int64_t bnum = (wdb->etnum / WDBLMEMB) * 2 + 1;
    int bopts = 0;
    if(wdb->opts & WDBTLARGE) bopts |= BDBTLARGE;
    if(wdb->opts & WDBTDEFLATE) bopts |= BDBTDEFLATE;
    if(wdb->opts & WDBTBZIP) bopts |= BDBTBZIP;
    if(wdb->opts & WDBTTCBS) bopts |= BDBTTCBS;
    if(!tcbdbtune(wdb->idx, WDBLMEMB, WDBNMEMB, bnum, WDBAPOW, WDBFPOW, bopts)) return false;
    if(!tcbdbsetlsmax(wdb->idx, WDBLSMAX)) return false;
  }
  if(wdb->lcnum > 0){
    if(!tcbdbsetcache(wdb->idx, wdb->lcnum, wdb->lcnum / 4 + 1)) return false;
  } else {
    if(!tcbdbsetcache(wdb->idx, (omode & WDBOWRITER) ? WDBLCNUMW : WDBLCNUMR, WDBNCNUM))
      return false;
  }
  if(omode & WDBONOLCK) bomode |= BDBONOLCK;
  if(omode & WDBOLCKNB) bomode |= BDBOLCKNB;
  if(!tcbdbopen(wdb->idx, path, bomode)) return false;
  if((omode & WDBOWRITER) && tcbdbrnum(wdb->idx) < 1){
    memcpy(tcbdbopaque(wdb->idx), WDBMAGICDATA, strlen(WDBMAGICDATA));
  } else if(!(omode & WDBONOLCK) &&
            memcmp(tcbdbopaque(wdb->idx), WDBMAGICDATA, strlen(WDBMAGICDATA))){
    tcbdbclose(wdb->idx);
    tcbdbsetecode(wdb->idx, TCEMETA, __FILE__, __LINE__, __func__);
    return 0;
  }
  if(omode & WDBOWRITER){
    wdb->cc = tcmapnew2(WDBCCBNUM);
    wdb->dtokens = tcmapnew2(WDBDTKNBNUM);
    wdb->dids = tcidsetnew(WDBDIDSBNUM);
  }
  wdb->open = true;
  return true;
}


/* Close a word database object.
   `wdb' specifies the word database object.
   If successful, the return value is true, else, it is false. */
static bool tcwdbcloseimpl(TCWDB *wdb){
  assert(wdb);
  bool err = false;
  if(wdb->cc){
    if((tcmaprnum(wdb->cc) > 0 || tcmaprnum(wdb->dtokens) > 0) && !tcwdbmemsync(wdb, 0))
      err = true;
    tcidsetdel(wdb->dids);
    tcmapdel(wdb->dtokens);
    tcmapdel(wdb->cc);
    wdb->cc = NULL;
  }
  if(!tcbdbclose(wdb->idx)) err = true;
  wdb->open = false;
  return !err;
}


/* Store a record into a q-gram database object.
   `wdb' specifies the q-gram database object.
   `id' specifies the ID number of the record.
   `words' specifies a list object contains the words of the record.
   If successful, the return value is true, else, it is false. */
static bool tcwdbputimpl(TCWDB *wdb, int64_t id, const TCLIST *words){
  assert(wdb && id > 0 && words);
  char idbuf[TDNUMBUFSIZ*2];
  int idsiz;
  TDSETVNUMBUF64(idsiz, idbuf, id);
  TCMAP *cc = wdb->cc;
  int wn = tclistnum(words);
  TCMAP *uniq = tcmapnew2(wn + 1);
  for(int i = 0; i < wn; i++){
    int wsiz;
    const char *word = tclistval(words, i, &wsiz);
    if(!tcmapputkeep(uniq, word, wsiz, "", 0)) continue;
    if(*word != '\0') tcmapputcat(cc, word, wsiz, idbuf, idsiz);
  }
  tcmapdel(uniq);
  bool err = false;
  if(tcmapmsiz(cc) >= wdb->icsiz && !tcwdbmemsync(wdb, 1)) err = true;
  return !err;
}


/* Remove a record of a q-gram database object.
   `wdb' specifies the q-gram database object.
   `id' specifies the ID number of the record.
   `words' specifies a list object contains the words of the record.
   If successful, the return value is true, else, it is false. */
static bool tcwdboutimpl(TCWDB *wdb, int64_t id, const TCLIST *words){
  assert(wdb && id > 0 && words);
  char idbuf[TDNUMBUFSIZ*2];
  int idsiz;
  TDSETVNUMBUF64(idsiz, idbuf, id);
  TCMAP *dtokens = wdb->dtokens;
  int wn = tclistnum(words);
  for(int i = 0; i < wn; i++){
    int wsiz;
    const char *word = tclistval(words, i, &wsiz);
    if(*word != '\0') tcmapputkeep(dtokens, word, wsiz, "", 0);
  }
  tcidsetmark(wdb->dids, id);
  bool err = false;
  if(tcmapmsiz(dtokens) >= wdb->icsiz && !tcwdbmemsync(wdb, 1)) err = true;
  return !err;
}


/* Search a q-gram database.
   `wdb' specifies the q-gram database object.
   `word' specifies the string of the word to be matched to.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records. */
static uint64_t *tcwdbsearchimpl(TCWDB *wdb, const char *word, int *np){
  assert(wdb && word && np);
  int wlen = strlen(word);
  if(wlen > WDBMAXWORDLEN){
    tcbdbsetecode(wdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    return NULL;
  }
  int vsiz;
  const char *vbuf = tcbdbget3(wdb->idx, word, wlen, &vsiz);
  if(!vbuf){
    vbuf = "";
    vsiz = 0;
  }
  uint64_t *res = tcmalloc(WDBRESUNIT * sizeof(*res));
  int rnum = 0;
  int ranum = WDBRESUNIT;
  while(vsiz > 0){
    int step;
    uint64_t id;
    TDREADVNUMBUF64(vbuf, id, step);
    vbuf += step;
    vsiz -= step;
    if(rnum >= ranum){
      ranum *= 2;
      res = tcrealloc(res, ranum * sizeof(*res));
    }
    res[rnum++] = id;
  }
  *np = rnum;
  return res;
}


/* Compare two list elements in lexical order.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tccmpwords(const char **a, const char **b){
  assert(a && b);
  return strcmp(*a, *b);
}



// END OF FILE
