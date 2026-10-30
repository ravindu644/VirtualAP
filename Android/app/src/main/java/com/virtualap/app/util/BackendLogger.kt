package com.virtualap.app.util

import android.util.Log

/** Where a start-ap output line lands. libsu delivers lines on the main thread. */
fun interface BackendLogger {
    fun log(level: Int, msg: String)
}

/** Level of a start-ap output line, keyed off the tags its log/warn/error helpers emit. */
fun classifyLine(line: String): Int = when {
    "[ERROR]" in line -> Log.ERROR
    "[WARN]" in line -> Log.WARN
    else -> Log.INFO
}
