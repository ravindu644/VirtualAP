/* The settings: one table gives each its flag, its ap.conf key, its default
 * and its usage line, so the parser, the config file and --help never drift.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_OPTIONS_H
#define VAP_OPTIONS_H

#include <stddef.h>

/* Every value stays a string until start validates it, so a bad value in
 * ap.conf and a bad flag fail the same way. */
#define OPT_LEN 256

struct vap_opts {
  char phy_iface[OPT_LEN];
  char upstream[OPT_LEN];
  char ssid[OPT_LEN];
  char password[OPT_LEN];
  char band[OPT_LEN];
  char channel[OPT_LEN];
  char width[OPT_LEN];
  char ip_gw[OPT_LEN];
  char dns_servers[OPT_LEN];
  char hidden[OPT_LEN];
  char security[OPT_LEN];
  char pmf[OPT_LEN];
  char ttl_fix[OPT_LEN];
  char ipv6[OPT_LEN];
  char container[OPT_LEN];
  char gw_mac[OPT_LEN];
};

struct vap_key {
  char flag;           /* the start flag */
  const char *conf;    /* the ap.conf variable */
  size_t off;          /* field in struct vap_opts */
  const char *def;     /* built-in default */
  const char *arg;     /* usage: the value's name */
  const char *help;    /* usage: what it does, may span lines */
  int empty_is_default; /* -g '' means the default gateway */
};

/* In the order ap.conf lists them. */
extern const struct vap_key vap_keys[];
extern const size_t vap_nkeys;

char *opt_field(struct vap_opts *o, const struct vap_key *k);
const char *opt_cfield(const struct vap_opts *o, const struct vap_key *k);
const struct vap_key *opt_by_conf(const char *name);

/* The one way a value gets in, from a flag or from ap.conf: refuses a line
 * break (ap.conf, hostapd.conf and run.state are line based, so it would
 * become a directive of its own) and anything that does not fit, logging
 * which. 0 or -1. */
int opt_set(struct vap_opts *o, const struct vap_key *k, const char *val);

void opts_defaults(struct vap_opts *o);
/* getopt over argv (argv[0] is the command). 0, or -1 after printing what
 * was wrong. */
int opts_parse(struct vap_opts *o, int argc, char **argv);
void print_usage(const char *prog);

#endif
