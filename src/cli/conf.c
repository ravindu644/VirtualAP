/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "cli/conf.h"

#include <stdio.h>
#include <string.h>

#include "core/fsutil.h"
#include "core/log.h"

/* Enough sh to read what conf_save writes: '...' verbatim (a ' is '\''), a
 * bare \x as x, and the word ends at an unquoted blank. As in sh, a # starts
 * a comment only where a word starts. A " is an ordinary character: only
 * the releases before 1.0 wrote "...", and the app passes every flag on a
 * start, so such a file is rewritten by the first one. */
void conf_unquote(const char **s, char *out, size_t size) {
  const char *p = *s;
  size_t n = 0;
#define PUT(ch)                                                                \
  do {                                                                         \
    if (n + 1 < size)                                                          \
      out[n++] = (ch);                                                         \
  } while (0)
  if (*p == '#')
    p += strlen(p);
  while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
    if (*p == '\'') {
      for (p++; *p && *p != '\''; p++)
        PUT(*p);
      if (*p)
        p++;
    } else if (*p == '\\' && p[1]) {
      PUT(p[1]);
      p += 2;
    } else {
      PUT(*p++);
    }
  }
#undef PUT
  out[n] = '\0';
  *s = p;
}

int conf_load(const char *path, struct vap_opts *o) {
  FILE *f = fopen(path, "re");
  if (!f)
    return -1;
  char line[4 * OPT_LEN + 64], val[4 * OPT_LEN];
  while (fgets(line, sizeof(line), f)) {
    /* An unterminated quote would otherwise carry the line break along. */
    line[strcspn(line, "\r\n")] = '\0';
    char *eq = strchr(line, '=');
    if (!eq || line[0] == '#')
      continue;
    *eq = '\0';
    const struct vap_key *k = opt_by_conf(line);
    if (!k)
      continue;
    const char *v = eq + 1;
    conf_unquote(&v, val, sizeof(val));
    if (opt_set(o, k, val) < 0)
      vap_warn("%s in %s is not usable, keeping the previous value.", k->conf,
               path);
  }
  fclose(f);
  return 0;
}

static void body(FILE *f, const void *arg) {
  const struct vap_opts *o = arg;
  for (size_t i = 0; i < vap_nkeys; i++) {
    fprintf(f, "%s='", vap_keys[i].conf);
    for (const char *p = opt_cfield(o, &vap_keys[i]); *p; p++) {
      if (*p == '\'')
        fputs("'\\''", f);
      else
        fputc(*p, f);
    }
    fputs("'\n", f);
  }
}

/* 0600: it holds the Wi-Fi password in the clear. */
int conf_save(const char *path, const struct vap_opts *o) {
  return write_atomic(path, 0600, body, o);
}
