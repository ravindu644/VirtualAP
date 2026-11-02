/* From USB-Arsenal src/net/iptables.c, lifted from Droidspaces by the same
 * author: the table engine, with the fingerprint parameters replaced by a
 * matcher callback and the blob arithmetic split from the socket calls.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/ipt/ipt.h"
#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct fam {
  int af, level;
  size_t entry_sz, off_target, off_next, addr_len;
};

static const struct fam v4 = {AF_INET,
                              IPPROTO_IP,
                              sizeof(struct ipt_entry),
                              offsetof(struct ipt_entry, target_offset),
                              offsetof(struct ipt_entry, next_offset),
                              4};
static const struct fam v6 = {AF_INET6,
                              IPPROTO_IPV6,
                              sizeof(struct ip6t_entry),
                              offsetof(struct ip6t_entry, target_offset),
                              offsetof(struct ip6t_entry, next_offset),
                              16};

static const struct fam *fam_of(int family) {
  return family == AF_INET6 ? &v6 : &v4;
}

/* The engine relies on the two APIs sharing these layouts and numbers. If a
 * kernel header ever breaks one, the build stops here rather than a table
 * replace corrupting someone's firewall. */
_Static_assert(sizeof(struct ipt_getinfo) == sizeof(struct ip6t_getinfo), "");
_Static_assert(sizeof(struct ipt_replace) == sizeof(struct ip6t_replace), "");
_Static_assert(offsetof(struct ipt_replace, counters) ==
                   offsetof(struct ip6t_replace, counters),
               "");
_Static_assert(offsetof(struct ipt_get_entries, entrytable) ==
                   offsetof(struct ip6t_get_entries, entrytable),
               "");
_Static_assert(IPT_SO_GET_INFO == IP6T_SO_GET_INFO &&
                   IPT_SO_GET_ENTRIES == IP6T_SO_GET_ENTRIES &&
                   IPT_SO_SET_REPLACE == IP6T_SO_SET_REPLACE,
               "");
_Static_assert(IPT_INV_DSTIP == IP6T_INV_DSTIP, "");

static unsigned u16_at(const void *e, size_t off) {
  uint16_t v;
  memcpy(&v, (const uint8_t *)e + off, sizeof(v));
  return v;
}

static unsigned ent_next(const struct fam *f, const void *e) {
  return u16_at(e, f->off_next);
}

static unsigned ent_toff(const struct fam *f, const void *e) {
  return u16_at(e, f->off_target);
}

size_t ipt_addr_len(int family) { return fam_of(family)->addr_len; }

size_t ipt_entry_size(int family) { return XT_ALIGN(fam_of(family)->entry_sz); }

void ipt_view(int family, void *e, struct ipt_view *v) {
  if (family == AF_INET6) {
    struct ip6t_ip6 *ip = &((struct ip6t_entry *)e)->ipv6;
    *v = (struct ipt_view){ip->iniface,       ip->outiface, ip->iniface_mask,
                           ip->outiface_mask, &ip->src,     &ip->smsk,
                           &ip->dst,          &ip->dmsk,    &ip->proto,
                           &ip->flags,        &ip->invflags};
  } else {
    struct ipt_ip *ip = &((struct ipt_entry *)e)->ip;
    *v = (struct ipt_view){ip->iniface,       ip->outiface, ip->iniface_mask,
                           ip->outiface_mask, &ip->src,     &ip->smsk,
                           &ip->dst,          &ip->dmsk,    &ip->proto,
                           &ip->flags,        &ip->invflags};
  }
}

const struct xt_entry_target *ipt_target(int family, const unsigned char *e) {
  const struct fam *f = fam_of(family);
  unsigned toff = ent_toff(f, e), next = ent_next(f, e);
  if (toff < f->entry_sz || toff + sizeof(struct xt_entry_target) > next)
    return NULL;
  return (const struct xt_entry_target *)(e + toff);
}

struct xt_entry_target *ipt_rule_begin(int family, unsigned char *buf,
                                       size_t target_sz) {
  const struct fam *f = fam_of(family);
  size_t hdr = XT_ALIGN(f->entry_sz);
  uint16_t toff = (uint16_t)hdr, next = (uint16_t)(hdr + target_sz);
  memset(buf, 0, hdr + target_sz);
  memcpy(buf + f->off_target, &toff, sizeof(toff));
  memcpy(buf + f->off_next, &next, sizeof(next));
  return (struct xt_entry_target *)(buf + hdr);
}

/* A standard target holding a jump (a positive verdict is the byte offset of
 * the entry jumped to), or NULL. Only these need fixing when entries move. */
static struct xt_standard_target *jump_of(const struct fam *f, unsigned char *e,
                                          unsigned next) {
  unsigned toff = ent_toff(f, e);
  if (toff + sizeof(struct xt_standard_target) > next)
    return NULL;
  struct xt_entry_target *t = (struct xt_entry_target *)(e + toff);
  if (t->u.user.name[0] ||
      t->u.target_size != XT_ALIGN(sizeof(struct xt_standard_target)))
    return NULL;
  struct xt_standard_target *st = (struct xt_standard_target *)t;
  return st->verdict >= 0 ? st : NULL;
}

static bool hook_valid(const struct ipt_getinfo *info, unsigned hook) {
  return hook < NF_INET_NUMHOOKS && (info->valid_hooks & (1u << hook)) &&
         info->hook_entry[hook] <= info->underflow[hook] &&
         info->underflow[hook] <= info->size;
}

int ipt_blob_insert(int family, const struct ipt_table *in, unsigned hook,
                    const void *rule, unsigned rule_sz, struct ipt_table *out) {
  const struct fam *f = fam_of(family);
  if (!hook_valid(&in->info, hook))
    return -EINVAL;
  unsigned at = in->info.hook_entry[hook], old = in->info.size;
  out->info = in->info;
  out->info.size = old + rule_sz;
  out->info.num_entries = in->info.num_entries + 1;
  out->blob = malloc(out->info.size);
  if (!out->blob)
    return -ENOMEM;
  for (int h = 0; h < NF_INET_NUMHOOKS; h++) {
    if (!(in->info.valid_hooks & (1u << h)))
      continue;
    /* The chain inserted into keeps its start; every later one moves, and
     * every policy entry at or after the insert point moves. */
    if (in->info.hook_entry[h] > at)
      out->info.hook_entry[h] += rule_sz;
    if (in->info.underflow[h] >= at)
      out->info.underflow[h] += rule_sz;
  }
  memcpy(out->blob, in->blob, at);
  memcpy(out->blob + at, rule, rule_sz);
  memcpy(out->blob + at + rule_sz, in->blob + at, old - at);
  /* Jumps to an entry at or past the insert point now land rule_sz later. */
  unsigned off = 0;
  while (off < out->info.size) {
    unsigned char *e = out->blob + off;
    unsigned next = ent_next(f, e);
    if (next < f->entry_sz || off + next > out->info.size)
      break;
    struct xt_standard_target *st = jump_of(f, e, next);
    if (st && st->verdict >= (int)at && off != at)
      st->verdict += (int)rule_sz;
    off += next;
  }
  /* A table we could not walk end to end is not one to hand back. */
  if (off != out->info.size) {
    ipt_free(out);
    return -EINVAL;
  }
  return 0;
}

int ipt_blob_remove(int family, const struct ipt_table *in, unsigned hook,
                    ipt_match_fn match, const void *arg,
                    struct ipt_table *out) {
  const struct fam *f = fam_of(family);
  const struct ipt_getinfo *info = &in->info;
  if (!hook_valid(info, hook))
    return -EINVAL;
  unsigned n = info->num_entries + 1;
  unsigned *old_off = calloc(n, sizeof(*old_off));
  unsigned *gone_before = calloc(n, sizeof(*gone_before));
  out->blob = malloc(info->size ? info->size : 1);
  if (!old_off || !gone_before || !out->blob) {
    free(old_off);
    free(gone_before);
    free(out->blob);
    out->blob = NULL;
    return -ENOMEM;
  }
  unsigned off = 0, ei = 0, size = 0, gone = 0, removed = 0;
  while (off + f->entry_sz <= info->size && ei < info->num_entries) {
    const unsigned char *e = in->blob + off;
    unsigned next = ent_next(f, e);
    if (next < f->entry_sz || next > info->size - off)
      break;
    old_off[ei] = off;
    gone_before[ei] = gone;
    bool policy = false;
    for (int h = 0; h < NF_INET_NUMHOOKS; h++)
      if ((info->valid_hooks & (1u << h)) && off == info->underflow[h])
        policy = true;
    if (!policy && off >= info->hook_entry[hook] &&
        off < info->underflow[hook] && match(family, e, arg)) {
      gone += next;
      removed++;
    } else {
      memcpy(out->blob + size, e, next);
      size += next;
    }
    off += next;
    ei++;
  }
  old_off[ei] = off;
  gone_before[ei] = gone;
  if (off != info->size || ei != info->num_entries) {
    free(old_off);
    free(gone_before);
    free(out->blob);
    out->blob = NULL;
    return -EINVAL;
  }
  if (removed) {
    out->info = *info;
    out->info.size = size;
    out->info.num_entries = info->num_entries - removed;
    /* Each hook start and policy entry moves back by what went before it. */
    for (int h = 0; h < NF_INET_NUMHOOKS; h++) {
      if (!(info->valid_hooks & (1u << h)))
        continue;
      for (unsigned k = 0; k <= ei; k++)
        if (old_off[k] >= info->hook_entry[h]) {
          out->info.hook_entry[h] -= gone_before[k];
          break;
        }
      for (unsigned k = 0; k <= ei; k++)
        if (old_off[k] >= info->underflow[h]) {
          out->info.underflow[h] -= gone_before[k];
          break;
        }
    }
    /* And so does every jump: the kernel's validator rejects the whole
     * replace with ELOOP on a jump into the middle of an entry. */
    for (unsigned o = 0; o < size;) {
      unsigned char *e = out->blob + o;
      unsigned next = ent_next(f, e);
      if (next < f->entry_sz || o + next > size)
        break;
      struct xt_standard_target *st = jump_of(f, e, next);
      for (unsigned k = 0; st && k <= ei; k++)
        if (old_off[k] == (unsigned)st->verdict) {
          st->verdict -= (int)gone_before[k];
          break;
        }
      o += next;
    }
  } else {
    free(out->blob);
    out->blob = NULL;
  }
  free(old_off);
  free(gone_before);
  return (int)removed;
}

bool ipt_blob_find(int family, const struct ipt_table *t, unsigned hook,
                   ipt_match_fn match, const void *arg) {
  const struct fam *f = fam_of(family);
  if (!hook_valid(&t->info, hook))
    return false;
  unsigned off = t->info.hook_entry[hook], end = t->info.underflow[hook];
  while (off + f->entry_sz <= end) {
    const unsigned char *e = t->blob + off;
    unsigned next = ent_next(f, e);
    if (next < f->entry_sz || next > end - off)
      break;
    if (match(family, e, arg))
      return true;
    off += next;
  }
  return false;
}

int ipt_open(int family) {
  int fd = socket(family, SOCK_RAW | SOCK_CLOEXEC, IPPROTO_RAW);
  return fd < 0 ? -errno : fd;
}

int ipt_get(int fd, int family, const char *table, struct ipt_table *t) {
  const struct fam *f = fam_of(family);
  memset(t, 0, sizeof(*t));
  strncpy(t->info.name, table, sizeof(t->info.name) - 1);
  socklen_t len = sizeof(t->info);
  if (getsockopt(fd, f->level, IPT_SO_GET_INFO, &t->info, &len) < 0)
    return -errno;
  size_t esz = sizeof(struct ipt_get_entries) + t->info.size;
  struct ipt_get_entries *ge = calloc(1, esz);
  if (!ge)
    return -ENOMEM;
  strncpy(ge->name, table, sizeof(ge->name) - 1);
  ge->size = t->info.size;
  len = (socklen_t)esz;
  if (getsockopt(fd, f->level, IPT_SO_GET_ENTRIES, ge, &len) < 0) {
    int e = -errno;
    free(ge);
    return e;
  }
  t->blob = malloc(t->info.size ? t->info.size : 1);
  if (t->blob)
    memcpy(t->blob, ge->entrytable, t->info.size);
  free(ge);
  return t->blob ? 0 : -ENOMEM;
}

void ipt_free(struct ipt_table *t) {
  free(t->blob);
  t->blob = NULL;
}

/* Hand the kernel a new table built from `old`. The counters buffer is the
 * kernel's to write the old entries' counters into, one per old entry. */
static int replace(int fd, int family, const struct ipt_table *old,
                   const struct ipt_table *new) {
  const struct fam *f = fam_of(family);
  size_t sz = sizeof(struct ipt_replace) + new->info.size;
  struct ipt_replace *r = calloc(1, sz);
  struct xt_counters *c =
      calloc(old->info.num_entries ? old->info.num_entries : 1, sizeof(*c));
  if (!r || !c) {
    free(r);
    free(c);
    return -ENOMEM;
  }
  memcpy(r->name, old->info.name, sizeof(r->name));
  r->valid_hooks = new->info.valid_hooks;
  r->num_entries = new->info.num_entries;
  r->size = new->info.size;
  memcpy(r->hook_entry, new->info.hook_entry, sizeof(r->hook_entry));
  memcpy(r->underflow, new->info.underflow, sizeof(r->underflow));
  r->num_counters = old->info.num_entries;
  r->counters = c;
  memcpy(r + 1, new->blob, new->info.size);
  int ret = setsockopt(fd, f->level, IPT_SO_SET_REPLACE, r, (socklen_t)sz);
  ret = ret < 0 ? -errno : 0;
  free(c);
  free(r);
  return ret;
}

static void backoff(int attempt) {
  struct timespec ts = {0, (5 + 10 * attempt) * 1000000L};
  nanosleep(&ts, NULL);
}

/* netd's iptables-restore and `iptables -w` hold this flock across their
 * whole read-modify-write. Without it our replace could land on a snapshot
 * netd has since changed and quietly revert netd's change. -1 when the lock
 * file does not exist (not Android), -EBUSY after 5 s of waiting. */
#define XT_LOCK "/system/etc/xtables.lock"

static int xt_lock(void) {
  int fd = open(XT_LOCK, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  for (int i = 0; i < 500; i++) {
    if (flock(fd, LOCK_EX | LOCK_NB) == 0)
      return fd;
    struct timespec ts = {0, 10 * 1000000L};
    nanosleep(&ts, NULL);
  }
  close(fd);
  return -EBUSY;
}

/* GET_ENTRIES answers EAGAIN when the table changed size since GET_INFO. */
static int get_retry(int fd, int family, const char *table,
                     struct ipt_table *t) {
  int ret = -EAGAIN;
  for (int attempt = 0; attempt < 5 && ret == -EAGAIN; attempt++) {
    if (attempt)
      backoff(attempt);
    ret = ipt_get(fd, family, table, t);
  }
  return ret;
}

/* One read-modify-replace under the xtables lock. edit returns > 0 to commit
 * new, 0 when there is nothing to do, or -errno. Retried on EAGAIN, which
 * netd changing the table between our read and our replace causes. */
typedef int (*edit_fn)(int family, const struct ipt_table *old,
                       struct ipt_table *new, const void *ctx);

static int edit(int fd, int family, const char *table, edit_fn fn,
                const void *ctx) {
  for (int attempt = 0; attempt < 5; attempt++) {
    if (attempt)
      backoff(attempt);
    int lock = xt_lock();
    if (lock == -EBUSY)
      return -EBUSY;
    struct ipt_table old, new = {0};
    int ret = ipt_get(fd, family, table, &old), r = 0;
    if (ret == 0) {
      r = fn(family, &old, &new, ctx);
      ret = r > 0 ? replace(fd, family, &old, &new) : r;
      ipt_free(&new);
      ipt_free(&old);
    }
    if (lock >= 0)
      close(lock);
    if (ret != -EAGAIN)
      return ret < 0 ? ret : r;
  }
  return -EAGAIN;
}

struct insert_ctx {
  unsigned hook;
  const void *rule;
  unsigned rule_sz;
  ipt_match_fn match;
  const void *arg;
};

static int insert_fn(int family, const struct ipt_table *old,
                     struct ipt_table *new, const void *c) {
  const struct insert_ctx *x = c;
  if (ipt_blob_find(family, old, x->hook, x->match, x->arg))
    return 0;
  int ret = ipt_blob_insert(family, old, x->hook, x->rule, x->rule_sz, new);
  return ret < 0 ? ret : 1;
}

int ipt_insert(int fd, int family, const char *table, unsigned hook,
               const void *rule, unsigned rule_sz, ipt_match_fn match,
               const void *arg) {
  struct insert_ctx x = {hook, rule, rule_sz, match, arg};
  int ret = edit(fd, family, table, insert_fn, &x);
  return ret < 0 ? ret : 0;
}

struct remove_ctx {
  unsigned hook;
  ipt_match_fn match;
  const void *arg;
};

static int remove_fn(int family, const struct ipt_table *old,
                     struct ipt_table *new, const void *c) {
  const struct remove_ctx *x = c;
  return ipt_blob_remove(family, old, x->hook, x->match, x->arg, new);
}

int ipt_remove(int fd, int family, const char *table, unsigned hook,
               ipt_match_fn match, const void *arg) {
  struct remove_ctx x = {hook, match, arg};
  return edit(fd, family, table, remove_fn, &x);
}

int ipt_find(int fd, int family, const char *table, unsigned hook,
             ipt_match_fn match, const void *arg) {
  struct ipt_table t;
  int ret = get_retry(fd, family, table, &t);
  if (ret < 0)
    return ret;
  ret = ipt_blob_find(family, &t, hook, match, arg);
  ipt_free(&t);
  return ret;
}

bool ipt_target_known(int fd, int family, const char *name) {
  struct xt_get_revision rev = {0};
  strncpy(rev.name, name, sizeof(rev.name) - 1);
  socklen_t len = sizeof(rev);
  int opt = family == AF_INET6 ? IP6T_SO_GET_REVISION_TARGET
                               : IPT_SO_GET_REVISION_TARGET;
  /* The answer is the best revision the kernel has, 0 or more. */
  return getsockopt(fd, fam_of(family)->level, opt, &rev, &len) >= 0;
}
