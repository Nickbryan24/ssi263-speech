// SsiLanguages on the JVM: what Android -- TalkBack, the system's TTS settings -- is told about each language, with
// and without each Blazie unit's firmware imported, from SsiEngine's own voice list.  A language whose firmware is
// not here must never be offered (missing data, below zero); one no voice speaks is not supported.
package com.ssi263speech.tts

import android.speech.tts.TextToSpeech
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SsiLanguagesTest {
    private val known = SsiEngine.KNOWN

    /** The voices that can speak with nothing imported (the Accents, the Mockingboards' built-in copies) plus these. */
    private fun with(vararg imported: Int) = known.filter {
        it.index in setOf(SsiNative.ACCENT_SA, SsiNative.ACCENT_MINI, SsiNative.MOCKINGBOARD,
                          SsiNative.MOCKINGBOARD_EARLY) || it.index in imported
    }

    private fun avail(lang: String?, country: String?, voices: List<SsiEngine.VoiceInfo>) =
        SsiLanguages.availability(lang, country, voices, known)

    @Test fun nothingImportedOffersEnglishAlone() {
        val v = with()
        assertEquals(TextToSpeech.LANG_COUNTRY_AVAILABLE, avail("eng", "USA", v))
        assertEquals(TextToSpeech.LANG_AVAILABLE, avail("eng", "GBR", v))
        assertEquals(TextToSpeech.LANG_AVAILABLE, avail("en", "", v))
        for (lang in listOf("spa", "deu", "ita", "slk", "de", "it", "sk", "es", "ger", "slo"))
            assertEquals(lang, TextToSpeech.LANG_MISSING_DATA, avail(lang, "", v))
        assertEquals(TextToSpeech.LANG_NOT_SUPPORTED, avail("fra", "FRA", v))
        assertEquals(TextToSpeech.LANG_NOT_SUPPORTED, avail(null, null, v))
        assertEquals(listOf("eng-USA"), SsiLanguages.tags(v))
        assertNull(SsiLanguages.voiceFor("deu", SsiNative.GERMAN, v))
    }

    @Test fun eachBlazieLanguageOnceItsUnitIsImported() {
        // (voice, ISO 639-2/T, ISO 3166 alpha-3, and the two-letter codes some callers send)
        data class Lang(val voice: Int, val lang: String, val country: String, val lang2: String, val country2: String)
        for ((voice, lang, country, lang2, country2) in listOf(Lang(SsiNative.GERMAN, "deu", "DEU", "de", "DE"),
                Lang(SsiNative.ITALIAN, "ita", "ITA", "it", "IT"), Lang(SsiNative.SLOVAK, "slk", "SVK", "sk", "SK"),
                Lang(SsiNative.SPANISH, "spa", "ESP", "es", "ES"))) {
            val v = with(voice)
            assertEquals(lang, TextToSpeech.LANG_COUNTRY_AVAILABLE, avail(lang, country, v))
            assertEquals(lang, TextToSpeech.LANG_COUNTRY_AVAILABLE, avail(lang2, country2, v))
            assertEquals(lang, TextToSpeech.LANG_AVAILABLE, avail(lang, "", v))
            assertEquals(setOf("eng-USA", "$lang-$country"), SsiLanguages.tags(v).toSet())
            assertEquals(voice, SsiLanguages.voiceFor(lang, SsiNative.ACCENT_SA, v)?.index)
            // the others stay missing: one unit's firmware offers its language alone
            for (other in listOf("spa", "deu", "ita", "slk").filter { it != lang })
                assertEquals("$lang, then $other", TextToSpeech.LANG_MISSING_DATA, avail(other, "", v))
        }
        assertEquals(TextToSpeech.LANG_AVAILABLE, avail("deu", "AUT", with(SsiNative.GERMAN)))
        assertEquals(TextToSpeech.LANG_COUNTRY_AVAILABLE, avail("ger", "DEU", with(SsiNative.GERMAN)))
        assertEquals(TextToSpeech.LANG_COUNTRY_AVAILABLE, avail("slo", "SK", with(SsiNative.SLOVAK)))
    }

    @Test fun aRequestInABlazieLanguageGetsItsUnitAndEnglishTheChosenVoice() {
        val v = with(SsiNative.ENGLISH, SsiNative.GERMAN, SsiNative.SLOVAK)
        assertEquals(SsiNative.GERMAN, SsiLanguages.ownLanguageVoice("deu", SsiNative.ENGLISH, v)?.index)
        assertEquals(SsiNative.SLOVAK, SsiLanguages.ownLanguageVoice("sk", SsiNative.ACCENT_SA, v)?.index)
        assertNull(SsiLanguages.ownLanguageVoice("ita", SsiNative.ENGLISH, v))        // not imported
        assertNull(SsiLanguages.ownLanguageVoice("eng", SsiNative.GERMAN, v))         // English: the chosen voice
        assertNull(SsiLanguages.ownLanguageVoice(null, SsiNative.GERMAN, v))
        // English's default: the chosen voice when it is English, else the first English one -- never German
        assertEquals(SsiNative.ENGLISH, SsiLanguages.voiceFor("eng", SsiNative.ENGLISH, v)?.index)
        assertEquals(SsiNative.ACCENT_SA, SsiLanguages.voiceFor("eng", SsiNative.GERMAN, v)?.index)
    }

    @Test fun theCurrentLanguageIsTheChosenVoices() {
        fun of(index: Int) = known.first { it.index == index }
        assertArrayEquals(arrayOf("deu", "DEU", ""), SsiLanguages.language(of(SsiNative.GERMAN)))
        assertArrayEquals(arrayOf("ita", "ITA", ""), SsiLanguages.language(of(SsiNative.ITALIAN)))
        assertArrayEquals(arrayOf("slk", "SVK", ""), SsiLanguages.language(of(SsiNative.SLOVAK)))
        assertArrayEquals(arrayOf("eng", "USA", ""), SsiLanguages.language(of(SsiNative.ACCENT_SA)))
    }

    @Test fun everyVoiceIsListedOnceWithAnIndexBelowVoices() {
        assertEquals(SsiNative.VOICES, known.size)
        assertEquals((0 until SsiNative.VOICES).toSet(), known.map { it.index }.toSet())
        assertEquals(known.size, known.map { it.name }.toSet().size)
    }
}
