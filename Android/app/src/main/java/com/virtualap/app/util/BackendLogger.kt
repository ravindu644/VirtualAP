package com.virtualap.app.util

import android.util.Log

/** Where an engine output line lands. libsu delivers lines on the main thread. */
fun interface BackendLogger {
    fun log(level: Int, msg: String)
}

/** Level of an engine output line, keyed off its [WARN] and [ERROR] tags. */
fun classifyLine(line: String): Int = when {
    "[ERROR]" in line -> Log.ERROR
    "[WARN]" in line -> Log.WARN
    else -> Log.INFO
}
