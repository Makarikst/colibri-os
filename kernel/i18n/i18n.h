//
// Created by  W26 on 10/21/26.
//

#ifndef I18N_H
#define I18N_H

enum { LANG_EN = 0, LANG_RU = 1 };

void i18n_init(void);
void i18n_set_lang(int lang);
int  i18n_get_lang(void);
const char* _(const char* key);

#endif
//COLIBRI_I18N_H
