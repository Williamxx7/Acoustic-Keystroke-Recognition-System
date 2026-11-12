#include "storage.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

//https://github.com/davegamble/cjson#example

static FILE *file = NULL;

// bestemmer hvordan et objekt skrives til filen
static void write_line(cJSON *obj)
{
    char *s = cJSON_PrintUnformatted(obj);
    if (s) {
        fputs(s, file);
        fputc('\n', file);
        fflush(file);
        free(s);
    }
    cJSON_Delete(obj);
}

int storage_start(const char *participant, const char *session_id, const char *timestamp)
{
    char path[128];
    storage_stop();
    mkdir("recordings", 0755);
    snprintf(path, sizeof(path), "recordings/session_%s.jsonl", session_id);
    file = fopen(path, "w");
    if (!file) {
        perror("Kunne ikke oprette fil");
        return -1;
    }

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "session");
    cJSON_AddStringToObject(o, "participant", participant);
    cJSON_AddStringToObject(o, "session_id", session_id);
    cJSON_AddStringToObject(o, "started_at", timestamp);
    write_line(o);
    return 0;
}

void storage_add_key_event(const char *key, const char *event, const char *timestamp)
{
    if (!file) return;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "key");
    cJSON_AddStringToObject(o, "key", key);
    cJSON_AddStringToObject(o, "event", event);
    cJSON_AddStringToObject(o, "timestamp", timestamp);
    write_line(o);
}

//nok ikke nødvendig, men måske rar at have. 
void storage_add_text(int text_id, const char *text, const char *timestamp)
{
    if (!file) return;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "target_text");
    cJSON_AddNumberToObject(o, "text_id", text_id);
    cJSON_AddStringToObject(o, "text", text);
    cJSON_AddStringToObject(o, "timestamp", timestamp);
    write_line(o);
}

void storage_stop(void)
{
    if (file) {
        fclose(file);
        file = NULL;
    }
}
