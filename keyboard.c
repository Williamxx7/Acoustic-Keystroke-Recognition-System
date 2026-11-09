#define _POSIX_C_SOURCE 200809L
#include "keyboard.h"
#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


struct keyboard {
    int fd;
    struct xkb_context *ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    char down_names[KEY_CNT][32];
};

static struct keyboard kbd = { .fd = -1 };

//keycode til navn 
static void key_name(xkb_keycode_t code, char *out, size_t size){
   
    // https://android.googlesource.com/platform/external/libxkbcommon/+/91f38399c2ea2fca0a88d624fc72d203df0e108a/xkbcommon/xkbcommon.h
    // 1538
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, code)
    
    // fast navn til taster i switch
    switch (sym){
        case XKB_KEY_Return: 
        case XKB_KEY_KP_Enter: snprintf(out, size, "Enter"); 
            return;
        case XKB_KEY_BackSpace: snprintf(out, size, "Backspace"); 
            return;
        case XKB_KEY_space: snprintf(out, size, " ");
            return;
        case XKB_KEY_Tab: snprintf(out, size, "Tab");
            return;
        case XKB_KEY_Escape:snprintf(out, size, "Escape");
            return;
        }
    // bruges til alle tegn 
    if (xkb_state_key_get_utf8(state, code, out, size) > 0) 
        return;
    
    // bruges til "control" taster. fx shift, ctrl osv
    if (xkb_keysym_get_name(sym, out, size) <= 0)
        snprintf(out, size, "KEY_%u", (unsigned)(code - 8));

}

// kan være en god ide når programmet startes at tjekke timedatectl på PI.
static void format_time(const struct input_event *ev, char *out, size_t size){
    
    struct tm t;
    char base[24]
    // input_event_sec er navn for pakke linux/input.h
    // ev-> gør det fungere til både 32 og 64 bit
    time_t sec = ev->input_event_sec;
    gmtime_r(&sec, &t);
    strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &t);
    // strftime kan ikke skrive millisekunder så det klare koden nedefor
    // hvis vi har brug for mere specifikke timestamps kan vi ændre dette kode
    snprintf(out, size, "%s.%03dZ", base, (int)(ev->input_event_usec / 1000) % 1000);
}


