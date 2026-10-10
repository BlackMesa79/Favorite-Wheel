# Favorite Wheel language files

The current 0.5.2 development build includes English (`en.ini`), Simplified Chinese (`zh_CN.ini`), French (`fr.ini`), Brazilian Portuguese (`pt_BR.ini`), Russian (`ru.ini`), Japanese (`ja.ini`), Korean (`ko.ini`), and German (`de.ini`). Its default language mode follows your **Windows display language**, independently of Skyrim's language or the active keyboard/IME. The French base was supplied by a community user; the other new translations and French additions are initial translations awaiting native-speaker feedback.

## Automatic selection

`Language=auto` in `Data/SKSE/Plugins/FavoriteWheel.ini` selects the system language at game startup. The F2 settings page shows **System (resolved language)**; this option remains automatic when saved. Restart the game after changing Windows display language or adding translation files.

Installation ZIPs include language/theme INI resources but omit the main settings INI. If it is absent, automatic language selection works from built-in defaults; the first successful Apply creates the main INI. Existing files are read without being rewritten at startup.

Language codes ignore case and treat `_` and `-` equally. The resolver first looks for the full locale, then removes trailing subtags to look for a more general translation, then falls back to English:

| Windows display language | Installed translation | Result |
| --- | --- | --- |
| `zh-CN` | `zh_CN.ini` | Simplified Chinese |
| `fr-FR` | `fr.ini` | French |
| `fr-CA` | `fr-CA.ini` and `fr.ini` | Canadian French |
| `pt-BR` | bundled `pt_BR.ini` | Brazilian Portuguese |
| `pt-PT` | only bundled `pt_BR.ini` | English |
| `ru-RU` | bundled `ru.ini` | Russian |
| `de-DE` | bundled `de.ini` | German |
| `ja-JP` | bundled `ja.ini` | Japanese |
| `ko-KR` | bundled `ko.ini` | Korean |
| `pt-BR` | `pt.ini` | Portuguese |
| `ja-JP` | no `ja-JP.ini` or `ja.ini` | English |
| `zh-TW` | only `zh_CN.ini` | English |

A different region is not substituted automatically: for example, `pt-PT.ini` alone does not match `pt-BR`. Use a generic `pt.ini` when the translation is intended for all Portuguese locales, or supply regional files separately.

Select an explicit language in F2 settings to override automatic selection, or set its filename without `.ini` in the main INI. Existing explicit `Language` values are preserved when upgrading. Legacy configurations containing only `Chinese=0` or `Chinese=1` retain their former English or Simplified Chinese selection. To use the new behavior on those installations, choose System in F2 or set `Language=auto` yourself. New installations and missing language settings default to automatic selection.

## Create a translation

1. Copy `SKSE/Plugins/FavoriteWheel/Languages/en.ini` to a new filename in the same folder, for example `fr.ini`, `de.ini`, `ja.ini`, or `pt-BR.ini`.
2. Save as **UTF-8**, with or without BOM. Language IDs contain ASCII letters/digits separated by `-` or `_`, up to 63 characters. `auto` is reserved. Use lowercase `.ini` as the extension.
3. Set `Name` to the language's display name, such as `Français`. Translate values after `=`, keeping every key unchanged.
4. Keep `Font` empty to inherit the theme or main configuration font, use `Font=auto` to choose an installed Windows font for the language, or provide a local font path covering the language's characters.
5. Restart the game, open the wheel, press F2, select your translation, and apply. Test all wheel categories, item details, outfit dialogs, and notifications. Also test with System selected when Windows uses a matching locale.

The files use the existing flat `key=value` format, not the Face Lighting project's sectioned format. Example:

```ini
; UTF-8 translation
Name=Français
Font=

settings=PARAMÈTRES
language=Langue
languageAuto=Système
apply=APPLIQUER
cancel=ANNULER
```

Start from the full English file so all keys are available. An incomplete translation is accepted: missing or empty strings fall back to the English file, then to the built-in English strings. Unknown keys are ignored by the UI. Files with invalid UTF-8, embedded NUL characters, or a size over 256 KiB are skipped. A missing language folder still leaves the built-in English UI available.

Use short labels, particularly for category titles, key hints, and setting names. Long strings may be shortened to fit. Keep strings on one line; literal `\n` is not converted into a line break. Lines beginning with `;` or `#` are comments. Do not translate `Name` or `Font` as key names, or any other key before `=`.

The language's `Font` takes precedence over the theme's font and the main INI's font. Do not include fonts you lack permission to redistribute; the mod itself does not distribute Windows fonts. Glyphs are prepared for the resolved language, including when automatic selection is used.

## What a translation changes

Translation files cover the wheel UI, action labels, notifications, settings, and item-information labels and units. Item, spell, effect, actor, and user-created preset names come from the game or the player. They are not renamed by the UI translation.

Japanese and Korean use `Font=auto`. Windows' Fonts directory is resolved at resource load, rather than assuming Windows is installed on C:. Japanese tries Yu Gothic, Meiryo, then MS Gothic; Korean tries Malgun Gothic, then Gulim. Other automatic fonts try Segoe UI/Arial, or appropriate Chinese system fonts for Chinese locales. If no matching primary font is installed, the configured/theme font remains the primary choice.

When building an atlas, the renderer merges only requested characters missing from the primary font, using available Windows fonts. This also covers language-selector names and inventory text in another script. It caches loaded fonts and retains the existing atlas reuse path; it does not rasterize complete CJK character sets or rebuild the atlas every frame. Windows fonts are never bundled. If the needed script's fonts are absent, install the corresponding Windows language fonts or set a suitable local font path; fallback cannot supply characters absent from every installed candidate.

The renderer currently supports common left-to-right text covered by installed fonts. Complex-script shaping and right-to-left layout are not implemented.

Keep technical names, filenames and paths unchanged in translated messages: for example `FavoriteWheel/Outfits/Exports`, `Imports`, `.fwo`, and `FavoriteWheel.log`.

## Share a translation

Distribute only the translation INI in this folder structure:

```text
SKSE/Plugins/FavoriteWheel/Languages/fr.ini
```

Do not bundle the plugin DLL, main INI, saves, or unrelated themes. Translation-only downloads are additive; the original mod must also be installed. Users can select the language manually, or use automatic selection with a matching Windows display language.

For a GitHub contribution, submit the language file under `assets/Languages/`. Include the locale and a brief description of which UI pages you tested. No recompilation is required to install or test a translation.

## 0.4.8 category labels

Translate `spells`, `shouts`, and `powers` for the new top-level magic categories. `powers` means active greater/lesser/voice powers, not passive abilities. The old `magic` key is retained but no longer labels a current category. Missing new keys fall back to English.
