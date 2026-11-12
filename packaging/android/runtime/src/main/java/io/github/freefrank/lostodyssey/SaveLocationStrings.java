package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.content.res.Configuration;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Locale;

/** Uses Settings language, including when it differs from the Android language. */
final class SaveLocationStrings {
    // Stable settings::UiLanguageNames indices (settings/config.h).
    private static final String[] LANGUAGE_TAGS = {
        "en", "zh-TW", "ja", "ko", "zh-CN", "es", "fr", "de", "it"
    };

    private SaveLocationStrings() {}

    static Locale localeForLanguage(int language) {
        int index = language >= 0 && language < LANGUAGE_TAGS.length ? language : 0;
        return Locale.forLanguageTag(LANGUAGE_TAGS[index]);
    }

    static Context context(Context context) {
        int language = 0;
        try {
            File settings = new File(context.getFilesDir(), "config/settings.ini");
            for (String line : Files.readAllLines(settings.toPath(), StandardCharsets.UTF_8)) {
                String[] pair = line.split("=", 2);
                if (pair.length == 2 && pair[0].trim().equals("ui_language")) {
                    language = Integer.parseInt(pair[1].trim());
                    break;
                }
            }
        } catch (IOException | NumberFormatException ignored) { }
        Configuration configuration = new Configuration(context.getResources().getConfiguration());
        configuration.setLocale(localeForLanguage(language));
        return context.createConfigurationContext(configuration);
    }
}
