#include "translation.h"

#include "core/encoding.h"
#include "core/log.h"
#include "core/string.h"

#include <string.h>

#define BUFFER_SIZE 100000

static struct {
    uint8_t *strings[TRANSLATION_MAX_KEY];
    uint8_t buffer[BUFFER_SIZE];
    int buf_index;
} data;

static void set_strings(const translation_string *strings, int num_strings, int is_default)
{
    for (int i = 0; i < num_strings; i++) {
        const translation_string *string = &strings[i];
        if (data.strings[string->key]) {
            continue;
        }
        if (is_default) {
            log_info("Translation key not found:", string->string, string->key);
        }
        int length_left = BUFFER_SIZE - data.buf_index;
        encoding_from_utf8(string->string, &data.buffer[data.buf_index], length_left);
        data.strings[string->key] = &data.buffer[data.buf_index];
        data.buf_index += 1 + string_length(&data.buffer[data.buf_index]);
    }
}

void translation_load(language_type language)
{
    const translation_string *strings = NULL;
    int num_strings = 0;
    const translation_string *default_strings = NULL;
    int num_default_strings = 0;
    translation_english(&default_strings, &num_default_strings);

    switch (language) {
        case LANGUAGE_ENGLISH:
            translation_english(&strings, &num_strings);
            break;
        case LANGUAGE_FRENCH:
            translation_french(&strings, &num_strings);
            break;
        case LANGUAGE_GERMAN:
            translation_german(&strings, &num_strings);
            break;
        case LANGUAGE_GREEK:
            translation_greek(&strings, &num_strings);
            break;
        case LANGUAGE_ITALIAN:
            translation_italian(&strings, &num_strings);
            break;
        case LANGUAGE_JAPANESE:
            translation_japanese(&strings, &num_strings);
            break;
        case LANGUAGE_KOREAN:
            translation_korean(&strings, &num_strings);
            break;
        case LANGUAGE_POLISH:
            translation_polish(&strings, &num_strings);
            break;
        case LANGUAGE_PORTUGUESE:
            translation_portuguese(&strings, &num_strings);
            break;
        case LANGUAGE_RUSSIAN:
            translation_russian(&strings, &num_strings);
            break;
        case LANGUAGE_SPANISH:
            translation_spanish(&strings, &num_strings);
            break;
        case LANGUAGE_SWEDISH:
            translation_swedish(&strings, &num_strings);
            break;
        case LANGUAGE_SIMPLIFIED_CHINESE:
            translation_simplified_chinese(&strings, &num_strings);
            break;
        case LANGUAGE_TRADITIONAL_CHINESE:
            translation_traditional_chinese(&strings, &num_strings);
            break;
        case LANGUAGE_CZECH:
            translation_czech(&strings, &num_strings);
            break;
        default:
            log_error("Invalid translation selected", 0, 0);
    }

    memset(data.strings, 0, sizeof(data.strings));
    data.buf_index = 0;
    set_strings(strings, num_strings, 0);
    set_strings(default_strings, num_default_strings, 1);
}

const uint8_t *translation_for(translation_key key)
{
    return data.strings[key];
}
