#ifndef HTML_JS_H
#define HTML_JS_H

#include "../utils/utils.h"

/* ============================================
   Игрушечный JS.
   Поддерживаем:
     document.write("...")   — печатает в "консоль" (в VGA) пока
     console.log("...")      — печатает в консоль
     alert("...")            — печатает в консоль
   Реального DOM-взаимодействия нет (это уровень 5+, слишком много).
   ============================================ */

/* Пройтись по всем <script>...</script> и выполнить их. */
void html_js_run_scripts(const char* html);

/* Выполнить одну строку JS */
void html_js_exec_line(const char* line);

#endif