// The languages the engine tells Android about -- the system's text-to-speech settings, TalkBack's language list and
// its switching -- as TGSpeechBox tells its own (its TgsbTtsService.LANGUAGES, CheckVoiceData): every voice's locale,
// asked after in ISO 639-2/T and ISO 3166 alpha-3 ("deu", "DEU"), a language offered only while it can speak.  There a
// language is left out when the user turns it off; here when no voice of it can speak now: English always (the
// Accents built in; the Braille Lite, the Speak-Out and the Mockingboards with them), Spanish, German, Italian and
// Slovak each only once its Blazie unit's firmware is imported (SsiData.has).  A language this app has a voice for but
// whose firmware is not here is LANG_MISSING_DATA -- below zero, so a screen reader's switch to it fails and stays in
// the current language, and the settings' "install voice data" brings the person to the import -- and one no voice
// ever speaks LANG_NOT_SUPPORTED.  Nothing here touches a Context, so the JVM tests (src/test) run it as it is.
package com.ssi263speech.tts

import android.speech.tts.TextToSpeech
import java.util.Locale
import java.util.MissingResourceException

object SsiLanguages {
    /** ISO 639-2 bibliographic codes some callers send, for the terminology ones Locale gives. */
    private val BIBLIOGRAPHIC = mapOf("ger" to "deu", "slo" to "slk", "cze" to "ces", "fre" to "fra", "dut" to "nld")

    /** A request's language, "de" or "deu" or "de-DE" alike, as ISO 639-2/T ("deu"); null when it is none. */
    fun iso3(lang: String?): String? {
        val l = lang?.trim()?.lowercase()?.substringBefore('-')?.substringBefore('_')
        if (l.isNullOrEmpty()) return null
        if (l.length == 3) return BIBLIOGRAPHIC[l] ?: l
        return try { Locale(l).isO3Language.takeIf { it.isNotEmpty() } } catch (e: MissingResourceException) { null }
    }

    /** A request's country, "DE" or "DEU" alike, as ISO 3166 alpha-3; null when it is none. */
    fun iso3Country(country: String?): String? {
        val c = country?.trim()?.uppercase()
        if (c.isNullOrEmpty()) return null
        if (c.length == 3) return c
        return try { Locale("", c).isO3Country.takeIf { it.isNotEmpty() } } catch (e: MissingResourceException) { null }
    }

    private fun Locale.iso3(): String = isO3Language
    private fun Locale.iso3Country(): String = try { isO3Country } catch (e: MissingResourceException) { "" }

    /** The voices of a language, in their order. */
    fun voicesOf(lang: String?, voices: List<SsiEngine.VoiceInfo>): List<SsiEngine.VoiceInfo> =
        iso3(lang)?.let { l -> voices.filter { it.locale.iso3() == l } } ?: emptyList()

    /** onIsLanguageAvailable's answer: `available` the voices that can speak now, `known` every voice the app has. */
    fun availability(lang: String?, country: String?, available: List<SsiEngine.VoiceInfo>,
                     known: List<SsiEngine.VoiceInfo>): Int {
        val here = voicesOf(lang, available)
        if (here.isEmpty())
            return if (voicesOf(lang, known).isEmpty()) TextToSpeech.LANG_NOT_SUPPORTED else TextToSpeech.LANG_MISSING_DATA
        val c = iso3Country(country)
        return if (c != null && here.any { it.locale.iso3Country() == c }) TextToSpeech.LANG_COUNTRY_AVAILABLE
               else TextToSpeech.LANG_AVAILABLE
    }

    /** The voice for a request in that language: the chosen one when it speaks it, else the first that does; null
     * when none can. */
    fun voiceFor(lang: String?, chosen: Int, available: List<SsiEngine.VoiceInfo>): SsiEngine.VoiceInfo? =
        voicesOf(lang, available).let { v -> v.firstOrNull { it.index == chosen } ?: v.firstOrNull() }

    /** A request this app answers in a language of its own rather than the chosen voice's: one whose language is not
     * English and that a voice here speaks (today's Spanish rule, for every Blazie language). */
    fun ownLanguageVoice(lang: String?, chosen: Int, available: List<SsiEngine.VoiceInfo>): SsiEngine.VoiceInfo? =
        if (iso3(lang) == null || iso3(lang) == "eng") null else voiceFor(lang, chosen, available)

    /** The voices' languages as CheckVoiceData reports them ("eng-USA", "deu-DEU"), each once, in the voices' order. */
    fun tags(voices: List<SsiEngine.VoiceInfo>): List<String> =
        voices.map { "${it.locale.iso3()}-${it.locale.iso3Country()}" }.distinct()

    /** onGetLanguage: the chosen voice's language, as {language, country, variant}. */
    fun language(voice: SsiEngine.VoiceInfo): Array<String> =
        arrayOf(voice.locale.iso3(), voice.locale.iso3Country(), "")
}
