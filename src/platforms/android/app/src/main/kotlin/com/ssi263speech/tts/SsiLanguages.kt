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
    /** ISO 639-2 bibliographic codes some callers send, for the terminology ones Locale gives; and Norwegian's macro
     * language (no, "nor") as the Bokmål the system's Norwegian voices speak (nb, "nob"). */
    private val BIBLIOGRAPHIC = mapOf("ger" to "deu", "slo" to "slk", "cze" to "ces", "fre" to "fra", "dut" to "nld",
                                      "gre" to "ell", "chi" to "zho", "nor" to "nob")

    /** A release voice's language with no region in the table (bl_languages.txt's tag), as the system names that
     * language's voices: its usual country -- Arabic none (it is offered for every country; the UAE's release has
     * its own tag), Norwegian as Bokmål (nb-NO), plain Portuguese Portugal's (Brazil's release has its own).  A tag
     * with a region keeps it (pt-BR, ar-AE); a language not here keeps no country, still found by its language. */
    private val REGION = mapOf("hu" to "HU", "tr" to "TR", "pl" to "PL", "cs" to "CZ", "hr" to "HR", "ru" to "RU",
                               "el" to "GR", "he" to "IL", "ko" to "KR", "da" to "DK", "nb" to "NO", "sv" to "SE",
                               "pt" to "PT", "fr" to "FR", "de" to "DE", "it" to "IT", "es" to "ES", "sk" to "SK")

    /** A BCP 47 tag from the release table as the voice's locale: "hu" Hungary's, "pt-BR" Brazil's, "no" nb-NO. */
    fun locale(tag: String): Locale {
        val parts = tag.split('-', '_')
        val lang = parts[0].lowercase().let { if (it == "no") "nb" else it }
        val region = parts.getOrNull(1)?.uppercase() ?: REGION[lang] ?: ""
        return Locale(lang, region)
    }

    /** The locale as a voice name's head: "hu-HU", "pt-BR", "ar" (Hebrew "he-IL", whatever the JVM calls it). */
    fun voiceTag(locale: Locale): String = locale.toLanguageTag()

    /** A request's language, "de" or "deu" or "de-DE" alike, as ISO 639-2/T ("deu"); null when it is none. */
    fun iso3(lang: String?): String? {
        val l = lang?.trim()?.lowercase()?.substringBefore('-')?.substringBefore('_')
        if (l.isNullOrEmpty()) return null
        if (l.length == 3) return BIBLIOGRAPHIC[l] ?: l
        return try { Locale(l).isO3Language.takeIf { it.isNotEmpty() }?.let { BIBLIOGRAPHIC[it] ?: it } }
            catch (e: MissingResourceException) { null }
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

    /** The voice for a request in that language, TGSpeechBox's order (its findLangDef): a voice of the request's
     * country first -- Brazil's Portuguese for pt-BR, the UAE's Arabic for ar-AE -- the chosen one when it is among
     * them; else the chosen voice when it speaks the language, else the first that does; null when none can. */
    fun voiceFor(lang: String?, chosen: Int, available: List<SsiEngine.VoiceInfo>,
                 country: String? = null): SsiEngine.VoiceInfo? {
        val v = voicesOf(lang, available)
        val c = iso3Country(country)
        val here = if (c == null) emptyList() else v.filter { it.locale.iso3Country() == c }
        return here.firstOrNull { it.index == chosen } ?: here.firstOrNull()
            ?: v.firstOrNull { it.index == chosen } ?: v.firstOrNull()
    }

    /** A request this app answers in a language of its own rather than the chosen voice's: one whose language is not
     * English and that a voice here speaks (today's Spanish rule, for every Blazie language). */
    fun ownLanguageVoice(lang: String?, chosen: Int, available: List<SsiEngine.VoiceInfo>,
                         country: String? = null): SsiEngine.VoiceInfo? =
        if (iso3(lang) == null || iso3(lang) == "eng") null else voiceFor(lang, chosen, available, country)

    /** The voices' languages as CheckVoiceData reports them ("eng-USA", "deu-DEU"), each once, in the voices' order. */
    fun tags(voices: List<SsiEngine.VoiceInfo>): List<String> =
        voices.map { "${it.locale.iso3()}-${it.locale.iso3Country()}" }.distinct()

    /** onGetLanguage: the chosen voice's language, as {language, country, variant}. */
    fun language(voice: SsiEngine.VoiceInfo): Array<String> =
        arrayOf(voice.locale.iso3(), voice.locale.iso3Country(), "")
}
