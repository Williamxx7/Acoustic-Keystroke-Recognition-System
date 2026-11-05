#ifndef STORAGE_H
#define STORAGE_H

int storage_start(const char *participant, const char *session_id);
void storage_add_event(const char *key, const char *timestamp);
void storage_stop(void);

#endif