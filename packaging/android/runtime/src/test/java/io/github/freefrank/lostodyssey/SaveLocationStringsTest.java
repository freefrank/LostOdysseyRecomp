package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;

import java.util.Locale;
import org.junit.Test;

public final class SaveLocationStringsTest {
    @Test public void followsPersistedSettingsLanguageOrder() {
        String[] expected = { "en", "zh-TW", "ja", "ko", "zh-CN", "es", "fr", "de", "it" };
        for (int language = 0; language < expected.length; ++language) {
            assertEquals("ui_language=" + language, Locale.forLanguageTag(expected[language]),
                SaveLocationStrings.localeForLanguage(language));
        }
    }

    @Test public void unknownLanguageFallsBackToEnglish() {
        for (int language : new int[] { -1, 9, 99, Integer.MIN_VALUE, Integer.MAX_VALUE }) {
            assertEquals("ui_language=" + language, Locale.ENGLISH,
                SaveLocationStrings.localeForLanguage(language));
        }
    }
}
