package com.virtualap.app.util

import android.content.Context
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat
import com.virtualap.app.R
import org.xmlpull.v1.XmlPullParser
import java.util.Locale

/**
 * The language picker, on AppCompat's per-app locales: the framework's own list from Android
 * 13, AppCompat's file (autoStoreLocales in the manifest) and a recreate on 8 to 12. Languages
 * are keyed by BCP 47 tag through Locale, so the legacy `in` of values-in and the `id` the
 * current locale reports land on the same key.
 */
object LocaleHelper {
    const val SYSTEM = "system"

    // AGP generates this file from the values-* folders, so a language merged from Weblate is
    // in the picker on the next build.
    private fun supportedLocales(context: Context): List<Locale> {
        val parser = context.resources.getXml(R.xml._generated_res_locale_config)
        val locales = mutableListOf<Locale>()
        try {
            while (parser.next() != XmlPullParser.END_DOCUMENT) {
                if (parser.eventType != XmlPullParser.START_TAG || parser.name != "locale") continue
                parser.getAttributeValue("http://schemas.android.com/apk/res/android", "name")
                    ?.let { locales += Locale.forLanguageTag(it) }
            }
        } finally {
            parser.close()
        }
        return locales
    }

    /** Tag and native name, English first and the rest by English name. */
    fun availableLanguages(context: Context): List<Pair<String, String>> =
        supportedLocales(context)
            .sortedWith(compareBy({ it.toLanguageTag() != "en" }, { it.getDisplayName(Locale.ENGLISH) }))
            .map { locale -> locale.toLanguageTag() to locale.getDisplayName(locale).replaceFirstChar { it.uppercase(locale) } }

    fun currentLanguage(): String =
        AppCompatDelegate.getApplicationLocales().get(0)?.toLanguageTag() ?: SYSTEM

    /** Saves the choice and recreates the activity; the same choice again does nothing. */
    fun changeLanguage(tag: String) {
        AppCompatDelegate.setApplicationLocales(
            if (tag == SYSTEM) LocaleListCompat.getEmptyLocaleList() else LocaleListCompat.forLanguageTags(tag)
        )
    }
}
