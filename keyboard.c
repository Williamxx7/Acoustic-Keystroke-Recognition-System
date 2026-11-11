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


// https://xkbcommon.org/doc/1.14.0/xkbcommon-keysyms_8h.html
// https://xkbcommon.org/doc/current/md_doc_2quick-guide.html
// https://sources.debian.org/src/libxkbcommon/1.13.1-1/tools/interactive-evdev.c?pow_referer=
// https://docs.kernel.org/input/input.html
// https://android.googlesource.com/platform/external/libxkbcommon/+/91f38399c2ea2fca0a88d624fc72d203df0e108a/xkbcommon/xkbcommon.h

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


int keyboard_open(cont char *device){

    //model skal måske ændre kan ikke huskes jonas tastatur
    struct xkb_rule_names names =
    {
        { .rules = "evdev", .model = "pc105", .layout = "dk" };
    };
    
    fd = open(device, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        perror("Kunne ikke åbne tastatur-enheden");
        return -1;
    }

    // henter kontekst fra bibloteket
    ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    keymap = ctx ? xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    state = keymap ? xkb_state_new(keymap) : NULL; 

    if (!state) {
        fprintf(stderr, "fejl i indlæsning af libxkbcommon\n");
        keyboard_close();
        return -1;
    }

    return 0;

}

int keyboard_fd(void) { return fd; }


int keyboard_process(key_callback cb){

    struct input_event ev;
    ssize_t n;
    
    while (){
    
    //filter
    if (ev.type != EV_KEY || ev.value > 1 || ev.code >= KEY_CNT) continue;

        // xkb har et offset på 8 ift linux    
        xkb_keycode_t code = ev.code + 8;
        char *name = down_names[ev.code];
        char ts[40];
        int is_down = ev.value == 1;

    
    if (is_down) {
        xkb_state_update_key(state, code, XKB_KEY_DOWN);
        key_name(code, name, sizeof(down_names[0]));
    //fejl håndtering
    } else if (!name[0]) {
        key_name(code, name, sizeof(down_names[0]));
    }

    format_time(&ev, ts, sizeof(ts));
        cb(name, is_down ? "key_down" : "key_up", ts);

    if (!is_down) {
        xkb_state_update_key(state, code, XKB_KEY_UP);
        name[0] = '\0';
    }
    
    }

    //fejlhåndtering
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
        perror("Tastaturet fejl");
        return -1;
    }
    
    return 0;
    
}
    


void keyboard_close(void){

    if (state) xkb_state_unref(state);
    
    if (keymap) xkb_keymap_unref(keymap);
    
    if (ctx) xkb_context_unref(ctx);
    state = NULL; keymap = NULL; ctx = NULL;
    
    if (fd >= 0) close(fd);
        fd = -1;

}