package com.ssi263speech.tts

import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/**
 * Is there a newer APK than the one running?  Panthera Speech's update check (its Updates.kt), with this project's
 * releases and asset name.
 *
 * **Only ever when somebody presses the button**, as the NVDA add-on's: a pop-up steals focus, and an app that
 * contacts a server on its own tells that server when its owner picked the phone up.
 *
 * The answer stops at pointing: the APK goes to the browser, which downloads it; opening the download installs it,
 * and Android asks once to let the *browser* install apps.  The app itself asks for no install permission.
 *
 * `/releases/latest` is not consulted, and **the version does not come from the tag**.  A release carries assets
 * that do not move together (the APK, the add-on, the SAPI installer, the Linux archives), and a later release may
 * carry an unchanged APK or none, so each is versioned by its own asset's filename: `ssi263-speech-0.8.0.apk` says
 * 0.8.0 wherever it hangs.  The number is read after `ssi263-speech-`, never as the first number in the name, which
 * is the 263 of the project's own.  The comparison is pure and tested; the fetch is the smallest piece that reaches
 * the network.
 */
object Updates {
    const val RELEASES_API = "https://api.github.com/repos/tgeczy/ssi263-speech/releases?per_page=20"
    const val RELEASES_PAGE = "https://github.com/tgeczy/ssi263-speech/releases"

    /** This project's APK asset, its version the group: the add-on (`.nvda-addon`), a sibling project's APK or a
     * one-off build attached to a release is not mistaken for it. */
    val APK_ASSET = Regex("""^ssi263-speech-(\d+(?:\.\d+)*)\.apk$""", RegexOption.IGNORE_CASE)

    /** version: "0.8.0"; page: the release's page; apk: the download. */
    class Release(val version: String, val page: String, val apk: String)

    sealed class Answer {
        class UpToDate(val installed: String) : Answer()
        class Available(val release: Release, val installed: String) : Answer()
        class Failed(val reason: String) : Answer()
    }

    private val VERSION = Regex("""(\d+(?:\.\d+)*)""")

    /** -> the numbers in a version string ("0.8.0", the installed versionName), or null if there are none. */
    fun parseVersion(text: String?): List<Int>? {
        val found = text?.let { VERSION.find(it) } ?: return null
        return try { found.groupValues[1].split('.').map { it.toInt() } }
            catch (e: NumberFormatException) { null }
    }

    /** "0.8.0" out of "ssi263-speech-0.8.0.apk"; null for any other name. */
    fun assetVersion(name: String): String? = APK_ASSET.matchEntire(name)?.groupValues?.get(1)

    /** True when `latest` is strictly higher than `installed`.  Padded, so 0.8 beats 0.7.9 and 0.8 ties 0.8.0.
     * Either side unreadable is false: a check that cannot tell says nothing rather than announce an update that
     * may not exist. */
    fun isNewer(latest: String?, installed: String?): Boolean {
        val a = parseVersion(latest) ?: return false
        val b = parseVersion(installed) ?: return false
        val width = maxOf(a.size, b.size)
        val pa = a + List(width - a.size) { 0 }
        val pb = b + List(width - b.size) { 0 }
        for (i in 0 until width) if (pa[i] != pb[i]) return pa[i] > pb[i]
        return false
    }

    /** The newest APK across the published releases, by the version in its own filename, not the tag of the
     * release carrying it.  A later release carrying only desktop files does not hide an APK published before it;
     * drafts and pre-releases are passed over, so a draft under test never advertises itself.
     * -> null when no release carries an APK this recognises. */
    fun newestApk(json: String): Release? {
        val releases = org.json.JSONArray(json)
        var best: Release? = null
        for (i in 0 until releases.length()) {
            val r = releases.getJSONObject(i)
            if (r.optBoolean("draft", false) || r.optBoolean("prerelease", false)) continue
            val assets = r.optJSONArray("assets") ?: continue
            val page = if (r.has("html_url")) r.getString("html_url") else RELEASES_PAGE
            for (j in 0 until assets.length()) {
                val a = assets.getJSONObject(j)
                val version = assetVersion(a.optString("name", "")) ?: continue
                if (!a.has("browser_download_url")) continue
                // newest release first: a strict comparison keeps the page of the first release carrying an APK
                if (best == null || isNewer(version, best.version))
                    best = Release(version, page, a.getString("browser_download_url"))
                break
            }
        }
        return best
    }

    /** The one piece that reaches the network. */
    fun fetch(url: String, timeoutMs: Int = 10000): String {
        val c = URL(url).openConnection() as HttpURLConnection
        try {
            c.connectTimeout = timeoutMs
            c.readTimeout = timeoutMs
            c.setRequestProperty("User-Agent", "ssi263-speech-android")
            c.setRequestProperty("Accept", "application/vnd.github+json")
            if (c.responseCode !in 200..299) throw IOException("HTTP ${c.responseCode}")
            return c.inputStream.bufferedReader().use { it.readText() }
        } finally {
            c.disconnect()
        }
    }

    /** `opener` is for the tests: anything that takes a URL and returns the body.  The default reaches the
     * network. */
    fun check(installed: String, opener: (String) -> String = { fetch(it) }): Answer {
        val json = try { opener(RELEASES_API) } catch (e: Exception) {
            return Answer.Failed(e.message?.takeIf { it.isNotBlank() } ?: e.javaClass.simpleName)
        }
        val newest = try { newestApk(json) } catch (e: Exception) {
            return Answer.Failed("the release list could not be read")
        } ?: return Answer.Failed("no release carries an APK")
        return if (isNewer(newest.version, installed)) Answer.Available(newest, installed)
               else Answer.UpToDate(installed)
    }
}
