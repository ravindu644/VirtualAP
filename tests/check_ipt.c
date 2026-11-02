/* The iptables engine's table surgery on synthetic tables, no kernel needed.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <linux/netfilter.h>
#include <string.h>
#include <sys/socket.h>

#include "check.h"
#include "net/ipt.h"
#include "net/ipt_rules.h"

/* A filter table as the kernel hands one out: INPUT and OUTPUT with only
 * their policies, FORWARD with a jump into user chain X ahead of its policy,
 * X holding "-i foo -j ACCEPT" and a RETURN, and the closing ERROR entry. */
struct tbl {
  struct ipt_table t;
  unsigned char buf[4096];
  unsigned jump_at, x_rule;
};

static unsigned put_std(int fam, struct tbl *b, int verdict) {
  unsigned off = b->t.info.size;
  size_t tsz = XT_ALIGN(sizeof(struct xt_standard_target));
  struct xt_standard_target *st =
      (struct xt_standard_target *)ipt_rule_begin(fam, b->buf + off, tsz);
  st->target.u.target_size = (uint16_t)tsz;
  st->verdict = verdict;
  b->t.info.size += (unsigned)(ipt_entry_size(fam) + tsz);
  b->t.info.num_entries++;
  return off;
}

static unsigned put_error(int fam, struct tbl *b, const char *name) {
  unsigned off = b->t.info.size;
  size_t tsz = XT_ALIGN(sizeof(struct xt_error_target));
  struct xt_error_target *et =
      (struct xt_error_target *)ipt_rule_begin(fam, b->buf + off, tsz);
  et->target.u.target_size = (uint16_t)tsz;
  strcpy(et->target.u.user.name, "ERROR");
  strcpy(et->errorname, name);
  b->t.info.size += (unsigned)(ipt_entry_size(fam) + tsz);
  b->t.info.num_entries++;
  return off;
}

static void make_table(int fam, struct tbl *b) {
  memset(b, 0, sizeof(*b));
  b->t.blob = b->buf;
  b->t.info.valid_hooks = (1 << NF_INET_LOCAL_IN) | (1 << NF_INET_FORWARD) |
                          (1 << NF_INET_LOCAL_OUT);
  const int accept = -NF_ACCEPT - 1;
  b->t.info.hook_entry[NF_INET_LOCAL_IN] =
      b->t.info.underflow[NF_INET_LOCAL_IN] = put_std(fam, b, accept);
  b->t.info.hook_entry[NF_INET_FORWARD] = b->jump_at = put_std(fam, b, 0);
  b->t.info.underflow[NF_INET_FORWARD] = put_std(fam, b, accept);
  b->t.info.hook_entry[NF_INET_LOCAL_OUT] =
      b->t.info.underflow[NF_INET_LOCAL_OUT] = put_std(fam, b, accept);
  put_error(fam, b, "X");
  b->x_rule = b->t.info.size;
  struct ipt_iface_arg foo = {"foo", false};
  int sz = ipt_build_accept(fam, &foo, b->buf + b->x_rule);
  b->t.info.size += (unsigned)sz;
  b->t.info.num_entries++;
  put_std(fam, b, XT_RETURN);
  put_error(fam, b, "ERROR");
  /* The FORWARD jump lands on X's first rule. */
  struct xt_standard_target *st =
      (struct xt_standard_target *)(void *)(b->buf + b->jump_at +
                                            ipt_entry_size(fam));
  st->verdict = (int)b->x_rule;
}

static void test_ipt_family(int fam) {
  struct tbl b;
  make_table(fam, &b);
  struct ipt_iface_arg ap_in = {"ap0", false}, ap_out = {"ap0", true};
  struct ipt_iface_arg foo = {"foo", false};
  unsigned char rule[256];
  int sz = ipt_build_accept(fam, &ap_in, rule);
  CHECK(sz > 0);
  CHECK(!ipt_blob_find(fam, &b.t, NF_INET_FORWARD, ipt_match_accept, &ap_in));

  struct ipt_table ins;
  CHECK(ipt_blob_insert(fam, &b.t, NF_INET_FORWARD, rule, (unsigned)sz, &ins) ==
        0);
  CHECK(ins.info.size == b.t.info.size + (unsigned)sz);
  CHECK(ins.info.num_entries == b.t.info.num_entries + 1);
  CHECK(ins.info.hook_entry[NF_INET_FORWARD] ==
        b.t.info.hook_entry[NF_INET_FORWARD]);
  CHECK(ins.info.underflow[NF_INET_FORWARD] ==
        b.t.info.underflow[NF_INET_FORWARD] + (unsigned)sz);
  CHECK(ins.info.hook_entry[NF_INET_LOCAL_OUT] ==
        b.t.info.hook_entry[NF_INET_LOCAL_OUT] + (unsigned)sz);
  CHECK(ins.info.hook_entry[NF_INET_LOCAL_IN] == 0);
  /* Ours is first in FORWARD, and the jump behind it still lands on X. */
  CHECK(!memcmp(ins.blob + b.jump_at, rule, (size_t)sz));
  const struct xt_standard_target *jump =
      (const void *)(ins.blob + b.jump_at + (unsigned)sz + ipt_entry_size(fam));
  CHECK(jump->verdict == (int)(b.x_rule + (unsigned)sz));
  CHECK(ipt_blob_find(fam, &ins, NF_INET_FORWARD, ipt_match_accept, &ap_in));
  CHECK(!ipt_blob_find(fam, &ins, NF_INET_FORWARD, ipt_match_accept, &ap_out));
  /* X's -i foo rule is not in FORWARD's range. */
  CHECK(!ipt_blob_find(fam, &ins, NF_INET_FORWARD, ipt_match_accept, &foo));

  /* Removing ours gives back the very table we started from. */
  struct ipt_table rem = {0};
  CHECK(ipt_blob_remove(fam, &ins, NF_INET_FORWARD, ipt_match_accept, &ap_in,
                        &rem) == 1);
  CHECK(rem.info.size == b.t.info.size &&
        !memcmp(rem.blob, b.t.blob, b.t.info.size));
  CHECK(!memcmp(rem.info.hook_entry, b.t.info.hook_entry,
                sizeof(b.t.info.hook_entry)));
  CHECK(!memcmp(rem.info.underflow, b.t.info.underflow,
                sizeof(b.t.info.underflow)));
  ipt_free(&rem);
  /* Nothing of ours left, nothing else taken. */
  struct ipt_table none = {0};
  CHECK(ipt_blob_remove(fam, &b.t, NF_INET_FORWARD, ipt_match_accept, &foo,
                        &none) == 0 &&
        !none.blob);
  ipt_free(&ins);

  /* MASQUERADE: ours matches, another subnet's does not. */
  struct ipt_net_arg net, other;
  const char *cidr = fam == AF_INET6 ? "fd76:6170:2a::/64" : "192.168.42.0/24";
  const char *cidr2 = fam == AF_INET6 ? "fd76:6170:2b::/64" : "192.168.43.0/24";
  CHECK(ipt_net_parse(fam, cidr, &net) == 0);
  CHECK(ipt_net_parse(fam, cidr2, &other) == 0);
  sz = ipt_build_masq(fam, &net, rule);
  CHECK(sz > 0 && ipt_match_masq(fam, rule, &net) &&
        !ipt_match_masq(fam, rule, &other));
  CHECK(!ipt_match_accept(fam, rule, &ap_in));
  /* Someone else's -p tcp MASQUERADE for our subnet is not ours. */
  struct ipt_view v;
  ipt_view(fam, rule, &v);
  *v.proto = 6;
  CHECK(!ipt_match_masq(fam, rule, &net));

  /* A table that cannot be walked to its end is refused, never truncated. */
  make_table(fam, &b);
  b.t.info.size += 8;
  struct ipt_table bad = {0};
  CHECK(ipt_blob_remove(fam, &b.t, NF_INET_FORWARD, ipt_match_accept, &foo,
                        &bad) == -EINVAL &&
        !bad.blob);
}

void test_ipt(void) {
  test_ipt_family(AF_INET);
  test_ipt_family(AF_INET6);
  struct ipt_net_arg n;
  CHECK(ipt_net_parse(AF_INET, "192.168.42.7/24", &n) == 0 && n.net[3] == 0 &&
        n.mask[2] == 0xff && n.mask[3] == 0);
  CHECK(ipt_net_parse(AF_INET, "192.168.42.0/", &n) < 0);
}
