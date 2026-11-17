#ifndef STORAGE_H
#define STORAGE_H
#include <stddef.h>

int  storage_new_session_dir(char *out, size_t size, const char *session_id);
int  storage_start(const char *dir, const char *participant, const char *session_id, const char *timestamp);
void storage_add_key_event(const char *sample_id, const char *key, const char *event, const char *timestamp);
void storage_add_text(int text_id, const char *text, const char *timestamp);
void storage_stop(void);

#endif
