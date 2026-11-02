/* Logging: stdout for the app's log sheet, a timestamped copy in ap.log.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_LOG_H
#define VAP_LOG_H

/* The app tells the levels apart by these tags, so they are the contract. */
void vap_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vap_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vap_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Where the timestamped copy goes; NULL keeps it on stdout only (tests). */
void log_set_file(const char *path);
/* One session per file: nothing reads ap.log but a person with a shell, and
 * without this it grows for as long as the phone lives. */
void log_truncate(void);

#endif
