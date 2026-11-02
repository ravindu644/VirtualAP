package com.virtualap.app.util

import android.content.Context
import java.io.File

/**
 * How the app reaches the engine, virtualap, a static C binary deployed with the
 * rest of the payload (see VirtualAPInstaller). It is part of the payload hash,
 * so an APK with a new engine re-runs the setup flow.
 */
object Backend {
    /** Every dynamic value that reaches the root shell goes through here: POSIX single-quote wrap. */
    fun quote(value: String): String = "'" + value.replace("'", "'\\''") + "'"

    /**
     * The engine's command prefix; append a command and its options.
     *
     * Releases before the C engine extracted a script, files/backend/start-ap,
     * and ran that. After an update from one of them, until the setup flow has
     * deployed virtualap, an AP the script started is still up, and only the
     * script knows it: while that copy exists every command (the notification's
     * Stop, the tile, the installer's own stop) goes to it. The installer deletes
     * it once virtualap is in place.
     * ponytail: drop the dispatch once no release with the sh engine is out there.
     */
    lateinit var startAp: String
        private set

    fun init(context: Context) {
        val legacy = quote(File(context.filesDir, "backend/start-ap").absolutePath)
        startAp = "sh -c ${quote("if [ -f $legacy ]; then exec sh $legacy \"\$@\"; else exec ${Constants.ENGINE} \"\$@\"; fi")} virtualap"
    }
}
