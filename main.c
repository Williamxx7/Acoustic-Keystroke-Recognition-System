#include "keyboard.h"
#include "storage.h"
#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

https://github.com/iamscottmoyers/simple-libwebsockets-example
https://gist.github.com/zYeoman/8dffbb612d10c3a11912cb26027f4236
https://github.com/iamscottmoyers/simple-libwebsockets-example/blob/master/client.c
https://android.googlesource.com/platform/external/libwebsockets/+/33a60348/minimal-examples/raw/minimal-raw-netcat/minimal-raw-netcat.c

#define HTTP_PORT 6767
#define MAX_IN 4096
#define QUEUE_SIZE 256
#define QUEUE_MSG 512

static int recording = 0;
static char participant[128] = "";
static char session_id[64] = "";

static char queue[QUEUE_SIZE][QUEUE_MSG];
static unsigned queue_read = 0, queue_write = 0;
//websocket instans
static struct lws *browser = NULL;
//håndtere og programmet skal kører. 
//sig_atomic_t er bestemt type der sørger for læsning og skrivning sker i en process
static volatile sig_atomic_t running = 1;

struct per_session_data {char input[MAX_IN]; size_t input_len;};

static void stop_handler(int sig){
    running = 0;
}

static void timestamp(char *out, size_t size){
    struct timespec ts;
    struct tm t;
    char base[24];
    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&ts.tv_sec, &t);
    strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &t);
    snprintf(out, size, "%s.%03dZ", base, (int)(ts.tv_nsec / 1000000) % 1000);
}

static void queue_json(cJSON *json){
    char *o = cJSON_PrintUnformatted(json);
    unsigned next = next = (queue_write + 1) % QUEUE_SIZE;
    
    //Tjekker om alt er i orden inden json sendes til browser
    if (o && browser && next != queue_read && strlen(o) < QUEUE_MSG) {
        strcpy(queue[queue_write], s);
        queue_write = next;
        lws_callback_on_writable(browser);
    }
    free(s);
}

//kommunikation til browser om recordings 
static void send_state(void){

    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "type", "state");
    cJSON_AddBoolToObject(m, "recording", recording);
    if (recording) {
        cJSON_AddStringToObject(m, "session_id", session_id);
        cJSON_AddStringToObject(m, "participant", participant);
    } else {
        cJSON_AddNullToObject(m, "session_id");
        cJSON_AddNullToObject(m, "participant");
    }
    queue_json(m);
    cJSON_Delete(m);

}


static void send_error(const char *message){
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "type", "error");
    cJSON_AddStringToObject(m, "message", message);
    queue_json(m);
    cJSON_Delete(m);
}



static void stop_recording(void){
    recording = 0;
    storage_stop();
}

//henter tekstbesked fra json
static const char *get_string(const cJSON *message, const char *name, const char *fallback){
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(message, name);
    return cJSON_IsString(item) ? item->valuestring : fallback;
}

// tal fra json
static int get_int(const cJSON *message, const char *name, const char *fallback){
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(message, name);
    return cJSON_IsNumber(item) ? item->valueint : 0;
}

// komunikation med broswer til at håndtere stop, start, fejl osv
static void browser_message(const char *data, size_t length){

    cJSON *message = cJSON_ParseWithLenght(data, length);

    if(!message) return;

    const char *type = get_string(message, "type", "");
    char ts[40];

    timestamp(ts, sizeof(ts));

   if (strcmp(type, "start") == 0 && !recording) {
        snprintf(participant, sizeof(participant), "%s", get_string(msg, "participant", "Unknown"));
        snprintf(session_id, sizeof(session_id), "%.4s%.2s%.2s_%.2s%.2s%.2s",ts, ts + 5, ts + 8, ts + 11, ts + 14, ts + 17);
        
        if (storage_start(participant, session_id, ts) == 0) {
            recording = 1;
            storage_add_text(get_int(msg, "text_id"), get_string(msg, "text", ""), ts);
        
        } else {
            send_error("fejl i oprettlse af fil");
        } send_state();
    
    } else if (strcmp(type, "stop") == 0 && recording) {
        stop_recording();
        send_state();
    
    } else if (strcmp(type, "text") == 0 && recording) {
        storage_add_text(get_int(msg, "text_id"), get_string(msg, "text", ""), ts);
    }
    cJSON_Delete(msg);
}
    



//skal kunne kaldes fra keyboard.c for keypress
static void on_key(const char *key, const char *event, const char *timestamp){

    if (!recording) return;
    storage_add_key_event(key, event, timestamp);

    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "type", "key");
    cJSON_AddStringToObject(m, "key", key);
    cJSON_AddStringToObject(m, "event", event);
    cJSON_AddStringToObject(m, "timestamp", timestamp);
    queue_json(m);
    cJSON_Delete(m);

}

static int callback_http_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len){
//https://libwebsockets.org/lws-api-doc-v3.0-stable/html/md_READMEs_README_8coding.html
//https://android.googlesource.com/platform/external/libwebsockets/+/ce37ee96/plugins/generic-sessions/protocol_lws_messageboard.c
//https://android.googlesource.com/platform/external/libwebsockets/+/cbc8f0d9c/minimal-examples/ws-server/README.md
//https://android.googlesource.com/platform/external/libwebsockets/+/33a60348/minimal-examples/raw/minimal-raw-netcat/minimal-raw-netcat.c
//https://libwebsockets.org/lws-api-doc-v3.0-stable/html/group__httpft.html


    struct per_session_data *psd = user;
    
    switch (reason){
        
        //http forspørgelser
        case LWS_CALLBACK_HTTP:{
            const char *uri = in;
            if (uri && (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0))
                return lws_serve_http_file(wsi, "index.html", "text/html; charset=utf-8", NULL, 0) ? -1 : 0;
            
            lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, "Not found");
            return -1;
        }
    
        //opretter forbindelse
        case LWS_CALLBACK_ESTABLISHED:
            browser = wsi;
            psd->input_len = 0;
            queue_read = queue_write;
            send_state();
            return 0;
        
        //modtager
        case LWS_CALLBACK_RECIVE:
            if(len + psd->input_len >= sizeof(psd->input_len))
                return -1;
            
            memcpy(psd->input + psd->input_len, in, len);
            psd->input_len += len

            if (lws_is_final_fragment(wsi) && lws_remaining_packet_payload(wsi) == 0) {
                if (!lws_frame_is_binary(wsi)) handle_browser_message(pss->input, pss->input_len);
                pss->input_len = 0;
            }
            return 0;
        
        //besked til browser
        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (queue_read != queue_write){
                unsigned char buf[LWS_PRE + QUEUE_MSG];
                size_t n = strlen(queue[queue_read]);
                memcpy(buf + LWS_PRE, queue[queue_read], n);
            
                if (lws_write(wsi, buf + LWS_PRE, n, LWS_WRITE_TEXT) < 0) 
                    return -1;
                
                queue_read = (queue_read + 1) % QUEUE_SIZE;
                        
                if (queue_read != queue_write) lws_callback_on_writable(wsi);
        }
        return 0;

        case LWS_CALLBACK_RAW_RX_FILE:
            if (keyboard_process(on_key) < 0) running = 0;
            return 0;

        case LWS_CALLBACK_CLOSED:
            if (browser == wsi) {
                browser = NULL;
                stop_recording();
        }
        queue_read = queue_write;
        return 0;

    default:
        return 0;

}


int main(int argc, char **argv){

    //sæt program navn og tastaturenhed
    if (argc != 2) {
        fprintf(stderr, "Brug: %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    //åbner keyboard
    if (keyboard_open(argv[1]) != 0) return 1;

    //server konfiguration
    static const struct lws_protocols protocols[] = {
        {   "http", callback_http_ws, 
            sizeof(struct per_session_data), MAX_IN, 0, NULL, 0 
        },
        LWS_PROTOCOL_LIST_TERM
    };

    
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = HTTP_PORT;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    //opret server
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "fejl med libwebsockets\n");
        keyboard_close();
        return 1;
    }


    if (!lws_adopt_descriptor_vhost(
        //gør at websocket også holder øje med tastatur 
        lws_get_vhost_by_name(context, "default"), LWS_ADOPT_RAW_FILE_DESC,(lws_sock_file_fd_type){ .filefd = keyboard_fd() }, "http", NULL)) {
    
        //fejlhåndering 
        fprintf(stderr, "kunne ikke tilføje tastaturet i websocket\n");
        lws_context_destroy(context);
        keyboard_close();
        return 1;
    }

    // sætter ctrl c til at aflsutte
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    printf("Kører på http://<RASPBERRY_PI_IP>:%d (tastatur: %s, dansk layout)\n", HTTP_PORT, argv[1]);

    //løkke for program
    while (running && lws_service(context, 0) >= 0) {}

    stop_recording();
    lws_context_destroy(context);
    keyboard_close();
    return 0;

}