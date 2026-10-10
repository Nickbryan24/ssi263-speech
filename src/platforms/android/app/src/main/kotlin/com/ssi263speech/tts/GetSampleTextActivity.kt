// ACTION_GET_SAMPLE_TEXT: the phrase the system speaks when the user previews the engine in Settings -- in Spanish,
// German, Italian or Slovak when that is asked for and that language's unit is here.
package com.ssi263speech.tts

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.speech.tts.TextToSpeech

class GetSampleTextActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val lang = intent?.getStringExtra("language")
        val iso3 = SsiLanguages.iso3(lang)?.takeIf { lang != null && SsiEngine.speaks(this, lang) }
        val data = Intent().putExtra(
            TextToSpeech.Engine.EXTRA_SAMPLE_TEXT,
            when (iso3) {
                "spa" -> "Hola. Este es un Braille Lite, hablando en tu teléfono."
                "deu" -> "Hallo. Dies ist ein Braille Lite, der auf deinem Telefon spricht."
                "ita" -> "Ciao. Questo è un Braille Lite che parla sul tuo telefono."
                "slk" -> "Ahoj. Toto je Braille 'n Speak, ktorý hovorí na tvojom telefóne."
                else -> "Hello there. This is SSI-263 Speech, speaking on your phone."
            })
        setResult(TextToSpeech.LANG_AVAILABLE, data)
        finish()
    }
}
