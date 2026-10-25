#include "i18n.h"
#include "../utils/utils.h"

typedef struct {
    const char* key;
    const char* en;
    const char* ru;
} I18nEntry;

static I18nEntry table[] = {
    {"settings",     "Settings",           "Настройки"},
    {"language",     "Language",           "Язык"},
    {"display_bg",   "Desktop color",      "Цвет рабочего стола"},
    {"apply",        "Apply",              "Применить"},
    {"close",        "Close",              "Закрыть"},
    {"lang_en",      "English",            "Английский"},
    {"lang_ru",      "Russian",            "Русский"},
    {"open",         "Open",               "Открыть"},
    {"delete",       "Delete",             "Удалить"},
    {"cancel",       "Cancel",             "Отмена"},
    {"new_file",     "Create a file",      "Создать файл"},
    {"new_folder",   "Create directory",   "Создать папку"},
    {"save",         "Save",               "Сохранить"},
    {"clear",        "Clear",              "Очистить"},
    {"up",           "Up",                 "Вверх"},
    {"ram",          "RAM",                "ОЗУ"},
    {"disk",         "Disk",               "Диск"},
    {"time",         "Time",               "Время"},
    {"terminal",     "Terminal",           "Терминал"},
    {"filer",        "Filer",              "Файлы"},
    {"nano",         "Nano",               "Блокнот"},
    {"browser",      "Browser",            "Браузер"},
    {"graphtool",    "GraphTool",          "Рисование"},
    {"trash",        "Trash",              "Корзина"},
{"display_bg", "Desktop color", "Цвет рабочего стола"},
{"language", "Language", "Язык"},
{"lang_en", "English", "English"},
{"lang_ru", "Russian", "Русский"},
{"apply", "Apply", "Применить"},
{"settings", "Settings", "Настройки"},
    {0, 0, 0}
};

static int cur_lang = LANG_EN;

void i18n_init(void) { cur_lang = LANG_EN; }
void i18n_set_lang(int lang) {
    cur_lang = (lang == LANG_RU) ? LANG_RU : LANG_EN;
}
int i18n_get_lang(void) { return cur_lang; }

const char* _(const char* key) {
    for (int i = 0; table[i].key; i++) {
        if (strcmp(table[i].key, key) == 0)
            return (cur_lang == LANG_RU) ? table[i].ru : table[i].en;
    }
    return key;
}