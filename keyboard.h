#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef void (*key_callback)(const char *key, const char *event, const char *timestamp);

int  keyboard_open(const char *device);
int  keyboard_fd(void);
int  keyboard_process(key_callback cb);
void keyboard_close(void);

#endif
