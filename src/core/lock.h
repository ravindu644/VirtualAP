/* One start or stop at a time.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_LOCK_H
#define VAP_LOCK_H

/* Takes an exclusive flock on run/lock, waiting up to 30 s for another start
 * or stop (the app's button, the Quick Settings tile and a shell can all fire
 * at once). 0, or -1 after logging why. Released at exit, or by vap_unlock in
 * a forked child that must not hold it. */
int vap_lock(void);
void vap_unlock(void);

#endif
