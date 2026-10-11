package com.ssi263speech.tts

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** The pure half of the update check (Updates.kt): which release, and is it newer.  Panthera Speech's tests, with
 * this project's names, and one of its own: the 263 in the project's name is never read as a version. */
class UpdatesTest {

    /** This repository's shape, with its real asset names: the 0.8.1 release carries no APK, only desktop files;
     * the 0.8.0 release carries an APK newer than its own tag (0.8.0's tag, a rebuilt 0.8.2 APK); a draft and a
     * pre-release are in the way. */
    private val releases = """
        [
          {"tag_name": "v0.9.0", "draft": true, "prerelease": false, "html_url": "https://example/draft",
           "assets": [{"name": "ssi263-speech-0.9.0.apk", "browser_download_url": "https://example/draft.apk"}]},
          {"tag_name": "v0.8.5-beta", "draft": false, "prerelease": true, "html_url": "https://example/beta",
           "assets": [{"name": "ssi263-speech-0.8.5.apk", "browser_download_url": "https://example/beta.apk"}]},
          {"tag_name": "v0.8.1", "draft": false, "prerelease": false, "html_url": "https://example/0.8.1",
           "assets": [{"name": "ssi263-speech-0.8.1.nvda-addon", "browser_download_url": "https://example/0.8.1.addon"},
                      {"name": "ssi263-speech-sapi-0.8.1.exe", "browser_download_url": "https://example/0.8.1.exe"}]},
          {"tag_name": "v0.8.0", "draft": false, "prerelease": false, "html_url": "https://example/0.8.0",
           "assets": [{"name": "ssi263-speech-0.8.0.nvda-addon", "browser_download_url": "https://example/0.8.0.addon"},
                      {"name": "ssi263-speech-0.8.2.apk", "browser_download_url": "https://example/0.8.2.apk"}]},
          {"tag_name": "v0.7.7", "draft": false, "prerelease": false, "html_url": "https://example/0.7.7",
           "assets": [{"name": "ssi263-speech-0.7.7.apk", "browser_download_url": "https://example/0.7.7.apk"}]}
        ]
    """.trimIndent()

    @Test fun theVersionIsReadAfterTheProjectsName() {
        // the first number in "ssi263-speech-0.8.0.apk" is the project's 263: never the version
        assertEquals("0.8.0", Updates.assetVersion("ssi263-speech-0.8.0.apk"))
        assertEquals("0.7.7", Updates.assetVersion("SSI263-Speech-0.7.7.APK"))
        assertNull(Updates.assetVersion("ssi263-speech.apk"))
        assertNull(Updates.assetVersion("ssi263-speech-0.8.0.nvda-addon"))
        assertNull(Updates.assetVersion("panthera-android-3.3.1.apk"))
        assertFalse(Updates.isNewer(Updates.assetVersion("ssi263-speech-0.7.7.apk"), "0.8.0"))
    }

    @Test fun versionsAreReadOutOfVersionNames() {
        assertEquals(listOf(0, 8, 0), Updates.parseVersion("0.8.0"))
        assertEquals(listOf(0, 8), Updates.parseVersion("0.8"))
        assertNull(Updates.parseVersion("no numbers here"))
        assertNull(Updates.parseVersion(null))
    }

    @Test fun newerMeansStrictlyHigherWithPadding() {
        assertTrue(Updates.isNewer("0.8", "0.7.9"))
        assertTrue(Updates.isNewer("0.8.1", "0.8.0"))
        assertFalse(Updates.isNewer("0.8", "0.8.0"))
        assertFalse(Updates.isNewer("0.8.0", "0.8.0"))
        assertFalse(Updates.isNewer("0.7.7", "0.8.0"))
        assertFalse(Updates.isNewer("junk", "0.8.0"))
        assertFalse(Updates.isNewer("0.8.1", null))
    }

    @Test fun theNewestApkWinsNotTheNewestRelease() {
        val newest = Updates.newestApk(releases)
        assertNotNull(newest)
        assertEquals("0.8.2", newest!!.version)
        assertEquals("https://example/0.8.2.apk", newest.apk)
        assertEquals("https://example/0.8.0", newest.page)
    }

    @Test fun draftsAndPreReleasesAreNeverOffered() {
        val newest = Updates.newestApk(releases)!!
        assertFalse(newest.apk.contains("draft"))
        assertFalse(newest.apk.contains("beta"))
    }

    @Test fun theVersionIsTheApkFileAndNotTheTag() {
        // the 0.8.0 release carries the 0.8.2 APK: someone on 0.8.2 is not offered it again because a tag says 0.8.0,
        // and someone on 0.8.0 is offered it though no tag says 0.8.2
        val newest = Updates.newestApk(releases)!!
        assertFalse(Updates.isNewer(newest.version, "0.8.2"))
        assertTrue(Updates.isNewer(newest.version, "0.8.0"))
    }

    @Test fun anotherProjectsApkIsNotMistakenForOurs() {
        val json = """
            [{"tag_name": "v9.9.9", "draft": false, "prerelease": false, "html_url": "https://example/x",
              "assets": [{"name": "panthera-android-9.9.9.apk", "browser_download_url": "https://example/theirs.apk"},
                         {"name": "ssi263-speech-0.8.0.apk", "browser_download_url": "https://example/ours.apk"}]}]
        """.trimIndent()
        val newest = Updates.newestApk(json)!!
        assertEquals("0.8.0", newest.version)
        assertEquals("https://example/ours.apk", newest.apk)
    }

    @Test fun aListWithoutOurApkAnswersNothing() {
        assertNull(Updates.newestApk("""[{"tag_name": "v9.9", "draft": false, "prerelease": false,
            "assets": [{"name": "ssi263-speech.apk", "browser_download_url": "u"}]}]"""))
        assertNull(Updates.newestApk("""[{"tag_name": "v9.9", "assets": [{"name": "x.nvda-addon", "browser_download_url": "u"}]}]"""))
        assertNull(Updates.newestApk("[]"))
    }

    @Test fun checkSaysAvailableUpToDateOrWhyNot() {
        val available = Updates.check("0.8.0") { releases }
        assertTrue(available is Updates.Answer.Available)
        assertEquals("0.8.2", (available as Updates.Answer.Available).release.version)

        assertTrue(Updates.check("0.8.2") { releases } is Updates.Answer.UpToDate)
        assertTrue(Updates.check("0.9.5") { releases } is Updates.Answer.UpToDate)

        val offline = Updates.check("0.8.0") { throw java.io.IOException("Unable to resolve host") }
        assertTrue(offline is Updates.Answer.Failed)
        assertEquals("Unable to resolve host", (offline as Updates.Answer.Failed).reason)

        assertTrue(Updates.check("0.8.0") { "<html>rate limited</html>" } is Updates.Answer.Failed)
    }
}
