# European settings translations

`LostOdysseyRecomp/settings/translations_eu.h` (Spanish, French, German,
Italian) is generated from the JSON files here, one per language, keyed by the
English text passed to `settings::Translate`.

After the menu gains or changes English text:

1. `python -I extract_ui_strings.py ../../LostOdysseyRecomp ui_strings.json`
2. Add the new keys to `ui_es.json`, `ui_fr.json`, `ui_de.json` and `ui_it.json`
   (keep leading/trailing spaces, placeholders and button names).
3. `python -I make_translations_eu.py . ../../LostOdysseyRecomp/settings/translations_eu.h`

The generator stops when a language lacks a key.
