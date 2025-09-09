#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H 1

typedef void* text_editor_t;

text_editor_t se_create_text_editor();
void se_destroy_text_editor(text_editor_t editor);
void se_display_text_editor(text_editor_t editor);
void se_set_text_editor_fonts(text_editor_t editor);

#endif