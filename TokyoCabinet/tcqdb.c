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


#include "tcqdb.h"
#include "myconf.h"

#define QDBMAGICDATA   "[q-gram]"        // magic data for identification
#define QDBIOBUFSIZ    65536             // size of an I/O buffer
#define QDBMAXWORDLEN  1024              // maximum length of each search word
#define QDBOCRUNIT     1024              // unit number of occurrence allocation
#define QDBCCBNUM      1048573           // bucket number of the token cache
#define QDBCCDEFICSIZ  (1024LL*1024*128) // default capacity of the token cache
#define QDBZMMINSIZ    131072            // minimum memory size to use nullified region
#define QDBDIDSBNUM    262139            // bucket number of the deleted ID set
#define QDBDTKNBNUM    262139            // bucket number of the deleted token map
#define QDBBITMAPNUM   524287            // number of elements of search bitmap
#define QDBDEFFWMMAX   2048              // default maximum number forward matching expansion
#define QDBHJBNUMCO    4                 // coefficient of the bucket number for hash join

#define QDBDEFETNUM    1000000           // default expected token number
#define QDBLMEMB       256               // number of members in each leaf of the index
#define QDBNMEMB       512               // number of members in each node of the index
#define QDBAPOW        9                 // alignment power of the index
#define QDBFPOW        11                // free block pool power of the index
#define QDBLSMAX       8192              // maximum size of each leaf of the index
#define QDBLCNUMW      64                // number of cached leaf nodes for writer
#define QDBLCNUMR      1024              // number of cached leaf nodes for reader
#define QDBNCNUM       1024              // number of cached non-leaf nodes

typedef struct {                         // type of structure for a record occurrence
  uint64_t id;                           // ID number
  int32_t off;                           // offset from the first token
  uint16_t seq;                          // sequence number
  uint16_t hash;                         // hash value for counting sort
} QDBOCR;


/* private function prototypes */
static bool tcqdblockmethod(TCQDB *qdb, bool wr);
static bool tcqdbunlockmethod(TCQDB *qdb);
static bool tcqdbopenimpl(TCQDB *qdb, const char *path, int omode);
static bool tcqdbcloseimpl(TCQDB *qdb);
static bool tcqdbputimpl(TCQDB *qdb, int64_t id, const char *text);
static bool tcqdboutimpl(TCQDB *qdb, int64_t id, const char *text);
static uint64_t *tcqdbsearchimpl(TCQDB *qdb, const char *word, int smode, int *np);
static int tccmptokens(const uint16_t **a, const uint16_t **b);
static int tccmpocrs(QDBOCR *a, QDBOCR *b);
static int tccmpuint64(const uint64_t *a, const uint64_t *b);



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* String containing the version information. */
const char *tdversion = _TD_VERSION;


/* Get the message string corresponding to an error code. */
const char *tcqdberrmsg(int ecode){
  return tcbdberrmsg(ecode);
}


/* Create a q-gram database object. */
TCQDB *tcqdbnew(void){
  TCQDB *qdb = tcmalloc(sizeof(*qdb));
  qdb->mmtx = tcmalloc(sizeof(pthread_rwlock_t));
  if(pthread_rwlock_init(qdb->mmtx, NULL) != 0) tcmyfatal("pthread_rwlock_init failed");
  qdb->idx = tcbdbnew();
  if(!tcbdbsetmutex(qdb->idx)) tcmyfatal("tcbdbsetmutex failed");
  qdb->open = false;
  qdb->cc = NULL;
  qdb->icsiz = QDBCCDEFICSIZ;
  qdb->lcnum = 0;
  qdb->dtokens = NULL;
  qdb->dids = NULL;
  qdb->etnum = QDBDEFETNUM;
  qdb->opts = 0;
  qdb->fwmmax = QDBDEFFWMMAX;
  qdb->synccb = NULL;
  qdb->syncopq = NULL;
  return qdb;
}


/* Delete a q-gram database object. */
void tcqdbdel(TCQDB *qdb){
  assert(qdb);
  if(qdb->open) tcqdbclose(qdb);
  tcbdbdel(qdb->idx);
  pthread_rwlock_destroy(qdb->mmtx);
  tcfree(qdb->mmtx);
  tcfree(qdb);
}


/* Get the last happened error code of a q-gram database object. */
int tcqdbecode(TCQDB *qdb){
  assert(qdb);
  return tcbdbecode(qdb->idx);
}


/* Set the tuning parameters of a q-gram database object. */
bool tcqdbtune(TCQDB *qdb, int64_t etnum, uint8_t opts){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  qdb->etnum = (etnum > 0) ? etnum : QDBDEFETNUM;
  qdb->opts = opts;
  tcqdbunlockmethod(qdb);
  return true;
}


/* Set the caching parameters of a q-gram database object. */
bool tcqdbsetcache(TCQDB *qdb, int64_t icsiz, int32_t lcnum){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  qdb->icsiz = (icsiz > 0) ? icsiz : QDBCCDEFICSIZ;
  qdb->lcnum = (lcnum > 0) ? lcnum : 0;
  tcqdbunlockmethod(qdb);
  return true;
}


/* Set the maximum number of forward matching expansion of a q-gram database object. */
bool tcqdbsetfwmmax(TCQDB *qdb, uint32_t fwmmax){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  qdb->fwmmax = fwmmax;
  tcqdbunlockmethod(qdb);
  return true;
}


/* Open a q-gram database object. */
bool tcqdbopen(TCQDB *qdb, const char *path, int omode){
  assert(qdb && path);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool rv = tcqdbopenimpl(qdb, path, omode);
  tcqdbunlockmethod(qdb);
  return rv;
}


/* Close a q-gram database object. */
bool tcqdbclose(TCQDB *qdb){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool rv = tcqdbcloseimpl(qdb);
  tcqdbunlockmethod(qdb);
  return rv;
}


/* Store a record into a q-gram database object. */
bool tcqdbput(TCQDB *qdb, int64_t id, const char *text){
  assert(qdb && id > 0 && text);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  if(tcidsetcheck(qdb->dids, id) && !tcqdbmemsync(qdb, 0)){
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool rv = tcqdbputimpl(qdb, id, text);
  tcqdbunlockmethod(qdb);
  return rv;
}


/* Remove a record of a q-gram database object. */
bool tcqdbout(TCQDB *qdb, int64_t id, const char *text){
  assert(qdb && id > 0 && text);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  if(tcidsetcheck(qdb->dids, id)){
    tcqdbunlockmethod(qdb);
    return true;
  }
  if(tcmaprnum(qdb->cc) > 0 && !tcqdbmemsync(qdb, 0)){
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool rv = tcqdboutimpl(qdb, id, text);
  tcqdbunlockmethod(qdb);
  return rv;
}


/* Search a q-gram database. */
uint64_t *tcqdbsearch(TCQDB *qdb, const char *word, int smode, int *np){
  assert(qdb && word && np);
  if(!tcqdblockmethod(qdb, false)) return NULL;
  if(!qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return NULL;
  }
  if(qdb->cc && (tcmaprnum(qdb->cc) > 0 || tcmaprnum(qdb->dtokens) > 0)){
    tcqdbunlockmethod(qdb);
    if(!tcqdblockmethod(qdb, true)) return NULL;
    if(!tcqdbmemsync(qdb, 0)){
      tcqdbunlockmethod(qdb);
      return NULL;
    }
    tcqdbunlockmethod(qdb);
    if(!tcqdblockmethod(qdb, false)) return NULL;
  }
  uint64_t *rv = tcqdbsearchimpl(qdb, word, smode, np);
  tcqdbunlockmethod(qdb);
  return rv;
}


/* Synchronize updated contents of a q-gram database object with the file and the device. */
bool tcqdbsync(TCQDB *qdb){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool err = false;
  if(!tcqdbmemsync(qdb, 2)) err = true;
  tcqdbunlockmethod(qdb);
  return !err;
}


/* Optimize the file of a q-gram database object. */
bool tcqdboptimize(TCQDB *qdb){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool err = false;
  if(!tcqdbmemsync(qdb, 1)) err = true;
  if(!tcbdboptimize(qdb->idx, 0, 0, 0, -1, -1, UINT8_MAX)) err = true;
  tcqdbunlockmethod(qdb);
  return !err;
}


/* Remove all records of a q-gram database object. */
bool tcqdbvanish(TCQDB *qdb){
  assert(qdb);
  if(!tcqdblockmethod(qdb, true)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool err = false;
  tcmapclear(qdb->cc);
  tcmapclear(qdb->dtokens);
  if(!tcqdbmemsync(qdb, 1)) err = true;
  if(!tcbdbvanish(qdb->idx)) err = true;
  tcqdbunlockmethod(qdb);
  return !err;
}


/* Copy the database file of a q-gram database object. */
bool tcqdbcopy(TCQDB *qdb, const char *path){
  assert(qdb && path);
  if(!tcqdblockmethod(qdb, false)) return false;
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    tcqdbunlockmethod(qdb);
    return false;
  }
  bool err = false;
  if(!tcqdbmemsync(qdb, 1)) err = true;
  if(!tcbdbcopy(qdb->idx, path)) err = true;
  tcqdbunlockmethod(qdb);
  return !err;
}


/* Get the file path of a q-gram database object. */
const char *tcqdbpath(TCQDB *qdb){
  assert(qdb);
  return tcbdbpath(qdb->idx);
}


/* Get the number of tokens of a q-gram database object. */
uint64_t tcqdbtnum(TCQDB *qdb){
  assert(qdb);
  return tcbdbrnum(qdb->idx);
}


/* Get the size of the database file of a q-gram database object. */
uint64_t tcqdbfsiz(TCQDB *qdb){
  assert(qdb);
  return tcbdbfsiz(qdb->idx);
}



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


/* Set the file descriptor for debugging output. */
void tcqdbsetdbgfd(TCQDB *qdb, int fd){
  assert(qdb && fd >= 0);
  tcbdbsetdbgfd(qdb->idx, fd);
}


/* Get the file descriptor for debugging output. */
int tcqdbdbgfd(TCQDB *qdb){
  assert(qdb);
  return tcbdbdbgfd(qdb->idx);
}


/* Synchronize updating contents on memory of a q-gram database object. */
bool tcqdbmemsync(TCQDB *qdb, int level){
  assert(qdb);
  if(!qdb->open || !qdb->cc){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  bool err = false;
  bool (*synccb)(int, int, const char *, void *) = qdb->synccb;
  void *syncopq = qdb->syncopq;
  TCBDB *idx = qdb->idx;
  TCMAP *cc = qdb->cc;
  if(synccb && !synccb(0, 0, "started", syncopq)){
    tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  if(tcmaprnum(cc) > 0){
    if(synccb && !synccb(0, 0, "getting tokens", syncopq)){
      tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    int kn;
    const char **keys = tcmapkeys2(cc, &kn);
    if(synccb && !synccb(kn, 0, "sorting tokens", syncopq)){
      tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      tcfree(keys);
      return false;
    }
    qsort(keys, kn, sizeof(*keys), (int(*)(const void *, const void *))tccmptokens);
    char token[TDNUMBUFSIZ*2];
    for(int i = 0; i < kn; i++){
      if(synccb && !synccb(kn, i + 1, "storing tokens", syncopq)){
        tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
        tcfree(keys);
        return false;
      }
      const uint16_t *ary = (uint16_t *)keys[i];
      tcstrucstoutf(ary, 2, token);
      int tlen = strlen(token);
      int vsiz;
      const char *vbuf = tcmapget(cc, ary, 2 * sizeof(*ary), &vsiz);
      if(!tcbdbputcat(idx, token, tlen, vbuf, vsiz)) err = true;
    }
    tcfree(keys);
    tcmapclear(cc);
  }
  TCMAP *dtokens = qdb->dtokens;
  TCIDSET *dids = qdb->dids;
  if(tcmaprnum(dtokens) > 0){
    if(synccb && !synccb(0, 0, "getting deleted tokens", syncopq)){
      tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    int kn;
    const char **keys = tcmapkeys2(dtokens, &kn);
    if(synccb && !synccb(kn, 0, "sorting deleted tokens", syncopq)){
      tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      tcfree(keys);
      return false;
    }
    qsort(keys, kn, sizeof(*keys), (int(*)(const void *, const void *))tccmptokens);
    char token[TDNUMBUFSIZ*2];
    for(int i = 0; i < kn; i++){
      if(synccb && !synccb(kn, i + 1, "storing deleted tokens", syncopq)){
        tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
        tcfree(keys);
        return false;
      }
      const uint16_t *ary = (uint16_t *)keys[i];
      tcstrucstoutf(ary, 2, token);
      int tlen = strlen(token);
      int vsiz;
      const char *vbuf = tcbdbget3(idx, token, tlen, &vsiz);
      if(!vbuf) continue;
      char *nbuf = tcmalloc(vsiz + 1);
      char *wp = nbuf;
      const char *pv;
      while(vsiz > 1){
        pv = vbuf;
        int step;
        uint64_t id;
        TDREADVNUMBUF64(vbuf, id, step);
        vbuf += step;
        vsiz -= step;
        if(vsiz > 0){
          uint32_t off;
          TDREADVNUMBUF(vbuf, off, step);
          vbuf += step;
          vsiz -= step;
          if(!tcidsetcheck(dids, id)){
            int len = vbuf - pv;
            memcpy(wp, pv, len);
            wp += len;
          }
        }
      }
      int nsiz = wp - nbuf;
      if(nsiz > 0){
        if(!tcbdbput(idx, token, tlen, nbuf, nsiz)) err = true;
      } else {
        if(!tcbdbout(idx, token, tlen)) err = true;
      }
      tcfree(nbuf);
    }
    tcfree(keys);
    tcmapclear(dtokens);
    tcidsetclear(dids);
  }
  if(level > 0){
    if(synccb && !synccb(0, 0, "synchronizing database", syncopq)){
      tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
      return false;
    }
    if(!tcbdbmemsync(idx, level > 1)) err = true;
  }
  if(synccb && !synccb(0, 0, "finished", syncopq)){
    tcbdbsetecode(qdb->idx, TCEMISC, __FILE__, __LINE__, __func__);
    return false;
  }
  return !err;
}


/* Clear the cache of a q-gram database object. */
bool tcqdbcacheclear(TCQDB *qdb){
  assert(qdb);
  if(!qdb->open){
    tcbdbsetecode(qdb->idx, TCEINVALID, __FILE__, __LINE__, __func__);
    return false;
  }
  return tcbdbcacheclear(qdb->idx);
}


/* Get the inode number of the database file of a q-gram database object. */
uint64_t tcqdbinode(TCQDB *qdb){
  assert(qdb);
  return tcbdbinode(qdb->idx);
}


/* Get the modification time of the database file of a q-gram database object. */
time_t tcqdbmtime(TCQDB *qdb){
  assert(qdb);
  return tcbdbmtime(qdb->idx);
}


/* Get the options of a q-gram database object. */
uint8_t tcqdbopts(TCQDB *qdb){
  assert(qdb);
  return tcbdbopts(qdb->idx);
}


/* Get the maximum number of forward matching expansion of a q-gram database object. */
uint32_t tcqdbfwmmax(TCQDB *qdb){
  assert(qdb);
  return qdb->fwmmax;
}


/* Get the number of records in the cache of a q-gram database object. */
uint32_t tcqdbcnum(TCQDB *qdb){
  assert(qdb);
  if(!qdb->cc) return 0;
  return tcmaprnum(qdb->cc);
}


/* Set the callback function for sync progression of a q-gram database object. */
void tcqdbsetsynccb(TCQDB *qdb, bool (*cb)(int, int, const char *, void *), void *opq){
  assert(qdb);
  qdb->synccb = cb;
  qdb->syncopq = opq;
}


/* Merge multiple result sets by union. */
uint64_t *tcqdbresunion(QDBRSET *rsets, int rsnum, int *np){
  assert(rsets && rsnum >= 0 && np);
  if(rsnum == 0){
    *np = 0;
    return tcmalloc(1);
  }
  if(rsnum == 1){
    if(!rsets[0].ids){
      *np = 0;
      return tcmalloc(1);
    }
    *np = rsets[0].num;
    return tcmemdup(rsets[0].ids, rsets[0].num * sizeof(rsets[0].ids[0]));
  }
  int sum = 0;
  for(int i = 0; i < rsnum; i++){
    if(!rsets[i].ids) continue;
    sum += rsets[i].num;
  }
  uint64_t *res = tcmalloc(sum * sizeof(*res) + 1);
  int rnum = 0;
  for(int i = 0; i < rsnum; i++){
    if(!rsets[i].ids) continue;
    const uint64_t *ids = rsets[i].ids;
    int num = rsets[i].num;
    for(int j = 0; j < num; j++){
      res[rnum++] = ids[j];
    }
  }
  qsort(res, rnum, sizeof(*res), (int (*)(const void *, const void *))tccmpuint64);
  int nnum = 0;
  uint64_t lid = UINT64_MAX;
  for(int i = 0; i < rnum; i++){
    uint64_t id = res[i];
    if(lid == id) continue;
    res[nnum++] = id;
    lid = id;
  }
  *np = nnum;
  return res;
}


/* Merge multiple result sets by intersection. */
uint64_t *tcqdbresisect(QDBRSET *rsets, int rsnum, int *np){
  assert(rsets && rsnum >= 0 && np);
  for(int i = 0; i < rsnum; i++){
    if(!rsets[i].ids){
      *np = 0;
      return tcmalloc(1);
    }
  }
  if(rsnum == 0){
    *np = 0;
    return tcmalloc(1);
  }
  if(rsnum == 1){
    *np = rsets[0].num;
    return tcmemdup(rsets[0].ids, rsets[0].num * sizeof(rsets[0].ids[0]));
  }
  if(rsnum == 2){
    uint64_t *small, *large;
    int snum, lnum;
    if(rsets[0].num < rsets[1].num){
      small = rsets[0].ids;
      snum = rsets[0].num;
      large = rsets[1].ids;
      lnum = rsets[1].num;
    } else {
      small = rsets[1].ids;
      snum = rsets[1].num;
      large = rsets[0].ids;
      lnum = rsets[0].num;
    }
    uint64_t *res = tcmalloc(snum * sizeof(*res) + 1);
    int rnum = 0;
    TCIDSET *idset = tcidsetnew(snum * QDBHJBNUMCO + 1);
    for(int i = 0; i < snum; i++){
      tcidsetmark(idset, small[i]);
    }
    for(int i = 0; i < lnum; i++){
      if(tcidsetcheck(idset, large[i])){
        res[rnum++] = large[i];
        if(rnum >= snum) break;
      }
    }
    tcidsetdel(idset);
    *np = rnum;
    return res;
  }
  int sum = 0;
  for(int i = 0; i < rsnum; i++){
    sum += rsets[i].num;
  }
  uint64_t *res = tcmalloc(sum * sizeof(*res) + 1);
  int rnum = 0;
  for(int i = 0; i < rsnum; i++){
    const uint64_t *ids = rsets[i].ids;
    int num = rsets[i].num;
    for(int j = 0; j < num; j++){
      res[rnum++] = ids[j];
    }
  }
  qsort(res, rnum, sizeof(*res), (int (*)(const void *, const void *))tccmpuint64);
  int nnum = 0;
  uint64_t lid = UINT64_MAX;
  int dnum = 0;
  for(int i = 0; i < rnum; i++){
    uint64_t id = res[i];
    if(lid == id){
      dnum++;
      if(dnum == rsnum) res[nnum++] = id;
    } else {
      dnum = 1;
    }
    lid = id;
  }
  *np = nnum;
  return res;
}


/* Merge multiple result sets by difference. */
uint64_t *tcqdbresdiff(QDBRSET *rsets, int rsnum, int *np){
  assert(rsets && rsnum >= 0 && np);
  if(rsnum == 0 || !rsets[0].ids){
    *np = 0;
    return tcmalloc(1);
  }
  if(rsnum == 1){
    *np = rsets[0].num;
    return tcmemdup(rsets[0].ids, rsets[0].num * sizeof(rsets[0].ids[0]));
  }
  int sum = 0;
  for(int i = 1; i < rsnum; i++){
    if(!rsets[i].ids) continue;
    sum += rsets[i].num;
  }
  TCIDSET *idset = tcidsetnew(sum * QDBHJBNUMCO + 1);
  for(int i = 1; i < rsnum; i++){
    const uint64_t *ids = rsets[i].ids;
    if(!ids) continue;
    int num = rsets[i].num;
    for(int j = 0; j < num; j++){
      tcidsetmark(idset, ids[j]);
    }
  }
  uint64_t *res = tcmalloc(rsets[0].num * sizeof(*res) + 1);
  int rnum = 0;
  const uint64_t *ids = rsets[0].ids;
  int num = rsets[0].num;
  for(int i = 0; i < num; i++){
    if(!tcidsetcheck(idset, ids[i])) res[rnum++] = ids[i];
  }
  tcidsetdel(idset);
  *np = rnum;
  return res;
}


/* Normalize a text. */
void tctextnormalize(char *text, int opts){
  assert(text);
  bool lowmode = opts & TCTNLOWER;
  bool nacmode = opts & TCTNNOACC;
  bool spcmode = opts & TCTNSPACE;
  int len = strlen(text);
  uint16_t stack[QDBIOBUFSIZ];
  uint16_t *ary = (len < QDBIOBUFSIZ) ? stack : tcmalloc(sizeof(*ary) * (len + 1));
  int anum;
  tcstrutftoucs(text, ary, &anum);
  ary[anum] = 0x0000;
  int nnum = 0;
  for(int i = 0; i < anum; i++){
    int c = ary[i];
    int high = c >> 8;
    if(high == 0x00){
      if(c < 0x0020 || c == 0x007f){
        // control characters
        if(spcmode){
          ary[nnum++] = 0x0020;
        } else if(c == 0x0009 || c == 0x000a || c == 0x000d){
          ary[nnum++] = c;
        } else {
          ary[nnum++] = 0x0020;
        }
      } else if(c == 0x00a0){
        // no-break space
        ary[nnum++] = 0x0020;
      } else {
        // otherwise
        if(lowmode){
          if(c < 0x007f){
            if(c >= 0x0041 && c <= 0x005a) c += 0x20;
          } else if(c >= 0x00c0 && c <= 0x00de && c != 0x00d7){
            c += 0x20;
          }
        }
        if(nacmode){
          if(c >= 0x00c0 && c <= 0x00c5){
            c = 'A';
          } else if(c == 0x00c7){
            c = 'C';
          } if(c >= 0x00c7 && c <= 0x00cb){
            c = 'E';
          } if(c >= 0x00cc && c <= 0x00cf){
            c = 'I';
          } else if(c == 0x00d0){
            c = 'D';
          } else if(c == 0x00d1){
            c = 'N';
          } if((c >= 0x00d2 && c <= 0x00d6) || c == 0x00d8){
            c = 'O';
          } if(c >= 0x00d9 && c <= 0x00dc){
            c = 'U';
          } if(c == 0x00dd || c == 0x00de){
            c = 'Y';
          } else if(c == 0x00df){
            c = 's';
          } else if(c >= 0x00e0 && c <= 0x00e5){
            c = 'a';
          } else if(c == 0x00e7){
            c = 'c';
          } if(c >= 0x00e7 && c <= 0x00eb){
            c = 'e';
          } if(c >= 0x00ec && c <= 0x00ef){
            c = 'i';
          } else if(c == 0x00f0){
            c = 'd';
          } else if(c == 0x00f1){
            c = 'n';
          } if((c >= 0x00f2 && c <= 0x00f6) || c == 0x00f8){
            c = 'o';
          } if(c >= 0x00f9 && c <= 0x00fc){
            c = 'u';
          } if(c >= 0x00fd && c <= 0x00ff){
            c = 'y';
          }
        }
        ary[nnum++] = c;
      }
    } else if(high == 0x01){
      // latin-1 extended
      if(lowmode){
        if(c <= 0x0137){
          if((c & 1) == 0) c++;
        } else if(c == 0x0138){
          c += 0;
        } else if(c <= 0x0148){
          if((c & 1) == 1) c++;
        } else if(c == 0x0149){
          c += 0;
        } else if(c <= 0x0177){
          if((c & 1) == 0) c++;
        } else if(c == 0x0178){
          c = 0x00ff;
        } else if(c <= 0x017e){
          if((c & 1) == 1) c++;
        } else if(c == 0x017f){
          c += 0;
        }
      }
      if(nacmode){
        if(c == 0x00ff){
          c = 'y';
        } else if(c <= 0x0105){
          c = ((c & 1) == 0) ? 'A' : 'a';
        } else if(c <= 0x010d){
          c = ((c & 1) == 0) ? 'C' : 'c';
        } else if(c <= 0x0111){
          c = ((c & 1) == 0) ? 'D' : 'd';
        } else if(c <= 0x011b){
          c = ((c & 1) == 0) ? 'E' : 'e';
        } else if(c <= 0x0123){
          c = ((c & 1) == 0) ? 'G' : 'g';
        } else if(c <= 0x0127){
          c = ((c & 1) == 0) ? 'H' : 'h';
        } else if(c <= 0x0131){
          c = ((c & 1) == 0) ? 'I' : 'i';
        } else if(c == 0x0134){
          c = 'J';
        } else if(c == 0x0135){
          c = 'j';
        } else if(c == 0x0136){
          c = 'K';
        } else if(c == 0x0137){
          c = 'k';
        } else if(c == 0x0138){
          c = 'k';
        } else if(c >= 0x0139 && c <= 0x0142){
          c = ((c & 1) == 1) ? 'L' : 'l';
        } else if(c >= 0x0143 && c <= 0x0148){
          c = ((c & 1) == 1) ? 'N' : 'n';
        } else if(c >= 0x0149 && c <= 0x014b){
          c = ((c & 1) == 0) ? 'N' : 'n';
        } else if(c >= 0x014c && c <= 0x0151){
          c = ((c & 1) == 0) ? 'O' : 'o';
        } else if(c >= 0x0154 && c <= 0x0159){
          c = ((c & 1) == 0) ? 'R' : 'r';
        } else if(c >= 0x015a && c <= 0x0161){
          c = ((c & 1) == 0) ? 'S' : 's';
        } else if(c >= 0x0162 && c <= 0x0167){
          c = ((c & 1) == 0) ? 'T' : 't';
        } else if(c >= 0x0168 && c <= 0x0173){
          c = ((c & 1) == 0) ? 'U' : 'u';
        } else if(c == 0x0174){
          c = 'W';
        } else if(c == 0x0175){
          c = 'w';
        } else if(c == 0x0176){
          c = 'Y';
        } else if(c == 0x0177){
          c = 'y';
        } else if(c == 0x0178){
          c = 'Y';
        } else if(c >= 0x0179 && c <= 0x017e){
          c = ((c & 1) == 1) ? 'Z' : 'z';
        } else if(c == 0x017f){
          c = 's';
        }
      }
      ary[nnum++] = c;
    } else if(high == 0x03){
      // greek
      if(lowmode){
        if(c >= 0x0391 && c <= 0x03a9){
          c += 0x20;
        } else if(c >= 0x03d8 && c <= 0x03ef){
          if((c & 1) == 0) c++;
        } else if(c == 0x0374 || c == 0x03f7 || c == 0x03fa){
          c++;
        }
      }
      ary[nnum++] = c;
    } else if(high == 0x04){
      // cyrillic
      if(lowmode){
        if(c <= 0x040f){
          c += 0x50;
        } else if(c <= 0x042f){
          c += 0x20;
        } else if(c >= 0x0460 && c <= 0x0481){
          if((c & 1) == 0) c++;
        } else if(c >= 0x048a && c <= 0x04bf){
          if((c & 1) == 0) c++;
        } else if(c == 0x04c0){
          c = 0x04cf;
        } else if(c >= 0x04c1 && c <= 0x04ce){
          if((c & 1) == 1) c++;
        } else if(c >= 0x04d0){
          if((c & 1) == 0) c++;
        }
      }
      ary[nnum++] = c;
    } else if(high == 0x20){
      if(c == 0x2002){
        // en space
        ary[nnum++] = 0x0020;
      } else if(c == 0x2003){
        // em space
        ary[nnum++] = 0x0020;
      } else if(c == 0x2009){
        // thin space
        ary[nnum++] = 0x0020;
      } else if(c == 0x2010){
        // hyphen
        ary[nnum++] = 0x002d;
      } else if(c == 0x2015){
        // fullwidth horizontal line
        ary[nnum++] = 0x002d;
      } else if(c == 0x2019){
        // apostrophe
        ary[nnum++] = 0x0027;
      } else if(c == 0x2033){
        // double quotes
        ary[nnum++] = 0x0022;
      } else {
        // (otherwise)
        ary[nnum++] = c;
      }
    } else if(high == 0x22){
      if(c == 0x2212){
        // minus sign
        ary[nnum++] = 0x002d;
      } else {
        // (otherwise)
        ary[nnum++] = c;
      }
    } else if(high == 0x30){
      if(c == 0x3000){
        // fullwidth space
        if(spcmode){
          ary[nnum++] = 0x0020;
        } else {
          ary[nnum++] = c;
        }
      } else {
        // (otherwise)
        ary[nnum++] = c;
      }
    } else if(high == 0xff){
      if(c == 0xff01){
        // fullwidth exclamation
        ary[nnum++] = 0x0021;
      } else if(c == 0xff03){
        // fullwidth igeta
        ary[nnum++] = 0x0023;
      } else if(c == 0xff04){
        // fullwidth dollar
        ary[nnum++] = 0x0024;
      } else if(c == 0xff05){
        // fullwidth parcent
        ary[nnum++] = 0x0025;
      } else if(c == 0xff06){
        // fullwidth ampersand
        ary[nnum++] = 0x0026;
      } else if(c == 0xff0a){
        // fullwidth asterisk
        ary[nnum++] = 0x002a;
      } else if(c == 0xff0b){
        // fullwidth plus
        ary[nnum++] = 0x002b;
      } else if(c == 0xff0c){
        // fullwidth comma
        ary[nnum++] = 0x002c;
      } else if(c == 0xff0e){
        // fullwidth period
        ary[nnum++] = 0x002e;
      } else if(c == 0xff0f){
        // fullwidth slash
        ary[nnum++] = 0x002f;
      } else if(c == 0xff1a){
        // fullwidth colon
        ary[nnum++] = 0x003a;
      } else if(c == 0xff1b){
        // fullwidth semicolon
        ary[nnum++] = 0x003b;
      } else if(c == 0xff1d){
        // fullwidth equal
        ary[nnum++] = 0x003d;
      } else if(c == 0xff1f){
        // fullwidth question
        ary[nnum++] = 0x003f;
      } else if(c == 0xff20){
        // fullwidth atmark
        ary[nnum++] = 0x0040;
      } else if(c == 0xff3c){
        // fullwidth backslash
        ary[nnum++] = 0x005c;
      } else if(c == 0xff3e){
        // fullwidth circumflex
        ary[nnum++] = 0x005e;
      } else if(c == 0xff3f){
        // fullwidth underscore
        ary[nnum++] = 0x005f;
      } else if(c == 0xff5c){
        // fullwidth vertical line
        ary[nnum++] = 0x007c;
      } else if(c >= 0xff21 && c <= 0xff3a){
        // fullwidth alphabets
        if(lowmode){
          c -= 0xfee0;
          if(c >= 0x0041 && c <= 0x005a) c += 0x20;
          ary[nnum++] = c;
        } else {
          ary[nnum++] = c - 0xfee0;
        }
      } else if(c >= 0xff41 && c <= 0xff5a){
        // fullwidth small alphabets
        ary[nnum++] = c - 0xfee0;
      } else if(c >= 0xff10 && c <= 0xff19){
        // fullwidth numbers
        ary[nnum++] = c - 0xfee0;
      } else if(c == 0xff61){
        // halfwidth full stop
        ary[nnum++] = 0x3002;
      } else if(c == 0xff62){
        // halfwidth left corner
        ary[nnum++] = 0x300c;
      } else if(c == 0xff63){
        // halfwidth right corner
        ary[nnum++] = 0x300d;
      } else if(c == 0xff64){
        // halfwidth comma
        ary[nnum++] = 0x3001;
      } else if(c == 0xff65){
        // halfwidth middle dot
        ary[nnum++] = 0x30fb;
      } else if(c == 0xff66){
        // halfwidth wo
        ary[nnum++] = 0x30f2;
      } else if(c >= 0xff67 && c <= 0xff6b){
        // halfwidth small a-o
        ary[nnum++] = (c - 0xff67) * 2 + 0x30a1;
      } else if(c >= 0xff6c && c <= 0xff6e){
        // halfwidth small ya-yo
        ary[nnum++] = (c - 0xff6c) * 2 + 0x30e3;
      } else if(c == 0xff6f){
        // halfwidth small tu
        ary[nnum++] = 0x30c3;
      } else if(c == 0xff70){
        // halfwidth prolonged mark
        ary[nnum++] = 0x30fc;
      } else if(c >= 0xff71 && c <= 0xff75){
        // halfwidth a-o
        ary[nnum] = (c - 0xff71) * 2 + 0x30a2;
        if(c == 0xff73 && ary[i+1] == 0xff9e){
          ary[nnum] = 0x30f4;
          i++;
        }
        nnum++;
      } else if(c >= 0xff76 && c <= 0xff7a){
        // halfwidth ka-ko
        ary[nnum] = (c - 0xff76) * 2 + 0x30ab;
        if(ary[i+1] == 0xff9e){
          ary[nnum]++;
          i++;
        }
        nnum++;
      } else if(c >= 0xff7b && c <= 0xff7f){
        // halfwidth sa-so
        ary[nnum] = (c - 0xff7b) * 2 + 0x30b5;
        if(ary[i+1] == 0xff9e){
          ary[nnum]++;
          i++;
        }
        nnum++;
      } else if(c >= 0xff80 && c <= 0xff84){
        // halfwidth ta-to
        ary[nnum] = (c - 0xff80) * 2 + 0x30bf + (c >= 0xff82 ? 1 : 0);
        if(ary[i+1] == 0xff9e){
          ary[nnum]++;
          i++;
        }
        nnum++;
      } else if(c >= 0xff85 && c <= 0xff89){
        // halfwidth na-no
        ary[nnum++] = c - 0xcebb;
      } else if(c >= 0xff8a && c <= 0xff8e){
        // halfwidth ha-ho
        ary[nnum] = (c - 0xff8a) * 3 + 0x30cf;
        if(ary[i+1] == 0xff9e){
          ary[nnum]++;
          i++;
        } else if(ary[i+1] == 0xff9f){
          ary[nnum] += 2;
          i++;
        }
        nnum++;
      } else if(c >= 0xff8f && c <= 0xff93){
        // halfwidth ma-mo
        ary[nnum++] = c - 0xceb1;
      } else if(c >= 0xff94 && c <= 0xff96){
        // halfwidth ya-yo
        ary[nnum++] = (c - 0xff94) * 2 + 0x30e4;
      } else if(c >= 0xff97 && c <= 0xff9b){
        // halfwidth ra-ro
        ary[nnum++] = c - 0xceae;
      } else if(c == 0xff9c){
        // halfwidth wa
        ary[nnum++] = 0x30ef;
      } else if(c == 0xff9d){
        // halfwidth nn
        ary[nnum++] = 0x30f3;
      } else {
        // otherwise
        ary[nnum++] = c;
      }
    } else {
      // otherwise
      ary[nnum++] = c;
    }
  }
  tcstrucstoutf(ary, nnum, text);
  if(ary != stack) tcfree(ary);
  if(spcmode) tcstrsqzspc(text);
}


/* Create an ID set object. */
TCIDSET *tcidsetnew(uint32_t bnum){
  if(bnum < 1) bnum = 1;
  TCIDSET *idset = tcmalloc(sizeof(*idset));
  uint64_t *buckets;
  if(bnum >= QDBZMMINSIZ / sizeof(*buckets)){
    buckets = tczeromap(bnum * sizeof(*buckets));
  } else {
    buckets = tccalloc(bnum, sizeof(*buckets));
  }
  idset->buckets = buckets;
  idset->bnum = bnum;
  idset->trails = tcmapnew2((bnum >> 2) + 1);
  return idset;
}


/* Delete an ID set object. */
void tcidsetdel(TCIDSET *idset){
  assert(idset);
  tcmapdel(idset->trails);
  if(idset->bnum >= QDBZMMINSIZ / sizeof(idset->buckets[0])){
    tczerounmap(idset->buckets);
  } else {
    tcfree(idset->buckets);
  }
  tcfree(idset);
}


/* Mark an ID of an ID set object. */
void tcidsetmark(TCIDSET *idset, int64_t id){
  assert(idset && id > 0);
  uint32_t bidx = id % idset->bnum;
  uint64_t rec = idset->buckets[bidx];
  if(rec == 0){
    idset->buckets[bidx] = id;
  } else {
    if((rec & INT64_MAX) == id) return;
    idset->buckets[bidx] = rec | INT64_MIN;
    tcmapputkeep(idset->trails, &id, sizeof(id), "", 0);
  }
}


/* Check an ID of an ID set object. */
bool tcidsetcheck(TCIDSET *idset, int64_t id){
  assert(idset && id >= 1);
  uint32_t bidx = id % idset->bnum;
  uint64_t rec = idset->buckets[bidx];
  if(rec == 0) return false;
  if((rec & INT64_MAX) == id) return true;
  if(rec <= INT64_MAX) return false;
  int vsiz;
  return tcmapget(idset->trails, &id, sizeof(id), &vsiz) != NULL;
}


/* Clear an ID set object. */
void tcidsetclear(TCIDSET *idset){
  assert(idset);
  uint64_t *buckets = idset->buckets;
  uint32_t bnum = idset->bnum;
  for(int i = 0; i < bnum; i++){
    buckets[i] = 0;
  }
  tcmapclear(idset->trails);
}



/*************************************************************************************************
 * private features
 *************************************************************************************************/


/* Lock a method of the q-gram database object.
   `qdb' specifies the q-gram database object.
   `wr' specifies whether the lock is writer or not.
   If successful, the return value is true, else, it is false. */
static bool tcqdblockmethod(TCQDB *qdb, bool wr){
  assert(qdb);
  if(wr ? pthread_rwlock_wrlock(qdb->mmtx) != 0 : pthread_rwlock_rdlock(qdb->mmtx) != 0){
    tcbdbsetecode(qdb->idx, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Unlock a method of the q-gram database object.
   `bdb' specifies the q-gram database object.
   If successful, the return value is true, else, it is false. */
static bool tcqdbunlockmethod(TCQDB *qdb){
  assert(qdb);
  if(pthread_rwlock_unlock(qdb->mmtx) != 0){
    tcbdbsetecode(qdb->idx, TCETHREAD, __FILE__, __LINE__, __func__);
    return false;
  }
  return true;
}


/* Open a q-gram database object.
   `qdb' specifies the q-gram database object.
   `path' specifies the path of the database file.
   `omode' specifies the connection mode.
   If successful, the return value is true, else, it is false. */
static bool tcqdbopenimpl(TCQDB *qdb, const char *path, int omode){
  assert(qdb && path);
  int bomode = BDBOREADER;
  if(omode & QDBOWRITER){
    bomode = BDBOWRITER;
    if(omode & QDBOCREAT) bomode |= BDBOCREAT;
    if(omode & QDBOTRUNC) bomode |= BDBOTRUNC;
    int64_t bnum = (qdb->etnum / QDBLMEMB) * 2 + 1;
    int bopts = 0;
    if(qdb->opts & QDBTLARGE) bopts |= BDBTLARGE;
    if(qdb->opts & QDBTDEFLATE) bopts |= BDBTDEFLATE;
    if(qdb->opts & QDBTBZIP) bopts |= BDBTBZIP;
    if(qdb->opts & QDBTTCBS) bopts |= BDBTTCBS;
    if(!tcbdbtune(qdb->idx, QDBLMEMB, QDBNMEMB, bnum, QDBAPOW, QDBFPOW, bopts)) return false;
    if(!tcbdbsetlsmax(qdb->idx, QDBLSMAX)) return false;
  }
  if(qdb->lcnum > 0){
    if(!tcbdbsetcache(qdb->idx, qdb->lcnum, qdb->lcnum / 4 + 1)) return false;
  } else {
    if(!tcbdbsetcache(qdb->idx, (omode & QDBOWRITER) ? QDBLCNUMW : QDBLCNUMR, QDBNCNUM))
      return false;
  }
  if(omode & QDBONOLCK) bomode |= BDBONOLCK;
  if(omode & QDBOLCKNB) bomode |= BDBOLCKNB;
  if(!tcbdbopen(qdb->idx, path, bomode)) return false;
  if((omode & QDBOWRITER) && tcbdbrnum(qdb->idx) < 1){
    memcpy(tcbdbopaque(qdb->idx), QDBMAGICDATA, strlen(QDBMAGICDATA));
  } else if(!(omode & QDBONOLCK) &&
            memcmp(tcbdbopaque(qdb->idx), QDBMAGICDATA, strlen(QDBMAGICDATA))){
    tcbdbclose(qdb->idx);
    tcbdbsetecode(qdb->idx, TCEMETA, __FILE__, __LINE__, __func__);
    return 0;
  }
  if(omode & QDBOWRITER){
    qdb->cc = tcmapnew2(QDBCCBNUM);
    qdb->dtokens = tcmapnew2(QDBDTKNBNUM);
    qdb->dids = tcidsetnew(QDBDIDSBNUM);
  }
  qdb->open = true;
  return true;
}


/* Close a q-gram database object.
   `qdb' specifies the q-gram database object.
   If successful, the return value is true, else, it is false. */
static bool tcqdbcloseimpl(TCQDB *qdb){
  assert(qdb);
  bool err = false;
  if(qdb->cc){
    if((tcmaprnum(qdb->cc) > 0 || tcmaprnum(qdb->dtokens) > 0) && !tcqdbmemsync(qdb, 0))
      err = true;
    tcidsetdel(qdb->dids);
    tcmapdel(qdb->dtokens);
    tcmapdel(qdb->cc);
    qdb->cc = NULL;
  }
  if(!tcbdbclose(qdb->idx)) err = true;
  qdb->open = false;
  return !err;
}


/* Store a record into a q-gram database object.
   `qdb' specifies the q-gram database object.
   `id' specifies the ID number of the record.
   `text' specifies the string of the record.
   If successful, the return value is true, else, it is false. */
static bool tcqdbputimpl(TCQDB *qdb, int64_t id, const char *text){
  assert(qdb && id > 0 && text);
  int len = strlen(text);
  char idbuf[TDNUMBUFSIZ*2];
  int idsiz;
  TDSETVNUMBUF64(idsiz, idbuf, id);
  uint16_t stack[QDBIOBUFSIZ];
  uint16_t *ary = (len < QDBIOBUFSIZ) ? stack : tcmalloc(sizeof(*ary) * (len + 1));
  int anum;
  tcstrutftoucs(text, ary, &anum);
  ary[anum] = 0x0000;
  TCMAP *cc = qdb->cc;
  char *wp = idbuf + idsiz;
  for(int i = 0; i < anum; i++){
    int osiz;
    TDSETVNUMBUF(osiz, wp, i);
    tcmapputcat(cc, ary + i, 2 * sizeof(*ary), idbuf, idsiz + osiz);
  }
  if(ary != stack) tcfree(ary);
  bool err = false;
  if(tcmapmsiz(cc) >= qdb->icsiz && !tcqdbmemsync(qdb, 1)) err = true;
  return !err;
}


/* Remove a record of a q-gram database object.
   `qdb' specifies the q-gram database object.
   `id' specifies the ID number of the record.
   `text' specifies the string of the record.
   If successful, the return value is true, else, it is false. */
static bool tcqdboutimpl(TCQDB *qdb, int64_t id, const char *text){
  assert(qdb && id > 0 && text);
  int len = strlen(text);
  char idbuf[TDNUMBUFSIZ*2];
  int idsiz;
  TDSETVNUMBUF64(idsiz, idbuf, id);
  uint16_t stack[QDBIOBUFSIZ];
  uint16_t *ary = (len < QDBIOBUFSIZ) ? stack : tcmalloc(sizeof(*ary) * (len + 1));
  int anum;
  tcstrutftoucs(text, ary, &anum);
  ary[anum] = 0x0000;
  TCMAP *dtokens = qdb->dtokens;
  for(int i = 0; i < anum; i++){
    tcmapputkeep(dtokens, ary + i, 2 * sizeof(*ary), "", 0);
  }
  if(ary != stack) tcfree(ary);
  tcidsetmark(qdb->dids, id);
  bool err = false;
  if(tcmapmsiz(dtokens) >= qdb->icsiz && !tcqdbmemsync(qdb, 1)) err = true;
  return !err;
}


/* Search a q-gram database.
   `qdb' specifies the q-gram database object.
   `word' specifies the string of the word to be matched to.
   `smode' specifies the matching mode.
   `np' specifies the pointer to the variable into which the number of elements of the return
   value is assigned.
   If successful, the return value is the pointer to an array of ID numbers of the corresponding
   records. */
static uint64_t *tcqdbsearchimpl(TCQDB *qdb, const char *word, int smode, int *np){
  assert(qdb && word && np);
  TCBDB *idx = qdb->idx;
  uint64_t *res = NULL;
  int wsiz = strlen(word);
  uint16_t *ary = tcmalloc(sizeof(*ary) * (wsiz + 2));
  int anum;
  tcstrutftoucs(word, ary, &anum);
  for(int i = 0; i < 2; i++){
    ary[anum+i] = 0;
  }
  if(anum >= 2){
    QDBOCR *ocrs = tcmalloc(QDBOCRUNIT * sizeof(*ocrs));
    int onum = 0;
    int oanum = QDBOCRUNIT;
    int obase = 0;
    TCBITMAP *pkmap = TCBITMAPNEW(QDBBITMAPNUM);
    char token[2*3+1];
    uint16_t seq = 0;
    for(int i = 0; i < anum; i += 2){
      obase = onum;
      int diff = anum - i - 2;
      if(diff < 0){
        i += diff;
        diff = -diff;
      } else {
        diff = 0;
      }
      tcstrucstoutf(ary + i, 2, token);
      int tsiz = strlen(token);
      int csiz;
      const char *cbuf = tcbdbget3(idx, token, tsiz, &csiz);
      if(cbuf){
        while(csiz > 0){
          int64_t id = 0;
          int step;
          TDREADVNUMBUF64(cbuf, id, step);
          cbuf += step;
          csiz -= step;
          if(csiz > 0){
            int off;
            TDREADVNUMBUF(cbuf, off, step);
            cbuf += step;
            csiz -= step;
            off += diff;
            uint32_t hash = id % QDBBITMAPNUM;
            if(i == 0 || TCBITMAPCHECK(pkmap, hash)){
              if(onum >= oanum){
                oanum *= 2;
                ocrs = tcrealloc(ocrs, oanum * sizeof(*ocrs));
              }
              QDBOCR *ocr = ocrs + onum;
              ocr->id = id;
              ocr->off = off;
              ocr->seq = seq;
              ocr->hash = hash;
              onum++;
              if(i == 0) TCBITMAPON(pkmap, hash);
            }
          }
        }
        if(i == 0 && (smode == QDBSPREFIX || smode == QDBSFULL)){
          int nnum = 0;
          for(int j = 0; j < onum; j++){
            if(ocrs[j].off == 0) ocrs[nnum++] = ocrs[j];
          }
          onum = nnum;
        }
      }
      seq++;
      if(onum <= obase){
        onum = 0;
        break;
      }
    }
    if(smode == QDBSSUFFIX || smode == QDBSFULL){
      obase = onum;
      int diff = anum % 2 + 1;
      tcstrucstoutf(ary + (anum - 1), 2, token);
      int tsiz = strlen(token);
      int csiz;
      const char *cbuf = tcbdbget3(idx, token, tsiz, &csiz);
      if(cbuf){
        while(csiz > 0){
          int64_t id = 0;
          int step;
          TDREADVNUMBUF64(cbuf, id, step);
          cbuf += step;
          csiz -= step;
          if(csiz > 0){
            int off;
            TDREADVNUMBUF(cbuf, off, step);
            cbuf += step;
            csiz -= step;
            off += diff;
            uint32_t hash = id % QDBBITMAPNUM;
            if(TCBITMAPCHECK(pkmap, hash)){
              if(onum >= oanum){
                oanum *= 2;
                ocrs = tcrealloc(ocrs, oanum * sizeof(*ocrs));
              }
              QDBOCR *ocr = ocrs + onum;
              ocr->id = id;
              ocr->off = off;
              ocr->seq = seq;
              ocr->hash = hash;
              onum++;
            }
          }
        }
      }
      seq++;
      if(onum <= obase) onum = 0;
    }
    TCBITMAPDEL(pkmap);
    if(seq > 1){
      if(onum > UINT16_MAX){
        int flnum = onum * 16 + 1;
        TCBITMAP *flmap = TCBITMAPNEW(flnum);
        for(int i = obase; i < onum; i++){
          QDBOCR *ocr = ocrs + i;
          uint32_t hash = (((uint32_t)ocr->off << 16) | ocr->hash) % flnum;
          TCBITMAPON(flmap, hash);
        }
        int wi = 0;
        for(int i = 0; i < obase; i++){
          QDBOCR *ocr = ocrs + i;
          int rem = (seq - ocr->seq - 1) * 2;
          uint32_t hash = (((uint32_t)(ocr->off + rem) << 16) | ocr->hash) % flnum;
          if(TCBITMAPCHECK(flmap, hash)) ocrs[wi++] = *ocr;
        }
        for(int i = obase; i < onum; i++){
          ocrs[wi++] = ocrs[i];
        }
        onum = wi;
        TCBITMAPDEL(flmap);
      }
      if(onum > UINT16_MAX * 2){
        QDBOCR *rocrs = tcmalloc(sizeof(*rocrs) * onum);
        uint32_t counts[UINT16_MAX+1];
        memset(counts, 0, sizeof(counts));
        for(int i = 0; i < onum; i++){
          counts[ocrs[i].hash]++;
        }
        for(int i = 0; i < UINT16_MAX; i++){
          counts[i+1] += counts[i];
        }
        for(int i = onum - 1; i >= 0; i--){
          rocrs[--counts[ocrs[i].hash]] = ocrs[i];
        }
        for(int i = 0; i < UINT16_MAX; i++){
          int num = counts[i+1] - counts[i];
          if(num > 1) qsort(rocrs + counts[i], num, sizeof(*rocrs),
                            (int (*)(const void *, const void *))tccmpocrs);
        }
        int num = onum - counts[UINT16_MAX];
        if(num > 1) qsort(rocrs + counts[UINT16_MAX], num, sizeof(*rocrs),
                          (int (*)(const void *, const void *))tccmpocrs);
        tcfree(ocrs);
        ocrs = rocrs;
      } else if(onum > 1){
        qsort(ocrs, onum, sizeof(*ocrs),
              (int (*)(const void *, const void *))tccmpocrs);
      }
    }
    TCIDSET *idset = tcidsetnew(onum * QDBHJBNUMCO + 1);
    res = tcmalloc(sizeof(*res) * onum + 1);
    int rnum = 0;
    int rem = (seq - 1) * 2;
    int ri = 0;
    while(ri < onum){
      QDBOCR *ocr = ocrs + ri;
      ri++;
      if(ocr->seq > 0) continue;
      int64_t id = ocr->id;
      int32_t max = ocr->off;
      uint16_t seq = 1;
      for(int i = ri; i < onum; i++){
        QDBOCR *tocr = ocrs + i;
        if(tocr->id != id) break;
        if(tocr->seq == seq && tocr->off == max + 2){
          max = tocr->off;
          seq++;
        }
      }
      if(max == ocr->off + rem){
        if(!tcidsetcheck(idset, id)){
          res[rnum++] = id;
          tcidsetmark(idset, id);
        }
        while(ri < onum && ocrs[ri].id == id){
          ri++;
        }
      }
    }
    tcidsetdel(idset);
    tcfree(ocrs);
    *np = rnum;
  } else {
    TCIDSET *idset = tcidsetnew(QDBBITMAPNUM / 8 + 1);
    res = tcmalloc(sizeof(*res) * QDBOCRUNIT);
    int ranum = QDBOCRUNIT;
    int rnum = 0;
    BDBCUR *cur = tcbdbcurnew(idx);
    tcbdbcurjump(cur, word, wsiz);
    TCXSTR *key = tcxstrnew();
    TCXSTR *val = tcxstrnew();
    bool pchk = smode == QDBSPREFIX || smode == QDBSFULL;
    bool schk = smode == QDBSSUFFIX || smode == QDBSFULL;
    for(int i = 0; i < qdb->fwmmax && tcbdbcurrec(cur, key, val); i++){
      const char *kbuf = TCXSTRPTR(key);
      int ksiz = TCXSTRSIZE(key);
      if(ksiz < wsiz || memcmp(kbuf, word, wsiz)) break;
      if(schk && (ksiz != wsiz || memcmp(kbuf, word, wsiz))) break;
      const char *cbuf = TCXSTRPTR(val);
      int csiz = TCXSTRSIZE(val);
      while(csiz > 0){
        int64_t id = 0;
        int step;
        TDREADVNUMBUF64(cbuf, id, step);
        cbuf += step;
        csiz -= step;
        if(csiz > 0){
          int off;
          TDREADVNUMBUF(cbuf, off, step);
          cbuf += step;
          csiz -= step;
          if((!pchk || off == 0) && !tcidsetcheck(idset, id)){
            if(rnum >= ranum){
              ranum *= 2;
              res = tcrealloc(res, sizeof(*res) * ranum);
            }
            res[rnum++] = id;
            tcidsetmark(idset, id);
          }
        }
      }
      tcbdbcurnext(cur);
    }
    tcxstrdel(val);
    tcxstrdel(key);
    tcbdbcurdel(cur);
    tcidsetdel(idset);
    qsort(res, rnum, sizeof(*res), (int (*)(const void *, const void *))tccmpuint64);
    *np = rnum;
  }
  tcfree(ary);
  return res;
}


/* Compare two list elements in lexical order.
   `a' specifies the pointer to one element.
   `b' specifies the pointer to the other element.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tccmptokens(const uint16_t **a, const uint16_t **b){
  assert(a && b);
  uint32_t anum = ((*a)[0] << 16UL) + (*a)[1];
  uint32_t bnum = ((*b)[0] << 16UL) + (*b)[1];
  return (anum < bnum) ? -1 : anum > bnum;
}


/* Compare two occurrences in identical order.
   `a' specifies the pointer to one occurrence.
   `b' specifies the pointer to the other occurrence.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tccmpocrs(QDBOCR *a, QDBOCR *b){
  assert(a && b);
  if(a->id > b->id) return 1;
  if(a->id < b->id) return -1;
  return a->off - b->off;
}


/* Compare two unsigned 64-bit integers.
   `a' specifies the pointer to one number.
   `b' specifies the pointer to the other number.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
static int tccmpuint64(const uint64_t *a, const uint64_t *b){
  assert(a && b);
  return (*a < *b) ? -1 : *a > *b;
}



// END OF FILE
