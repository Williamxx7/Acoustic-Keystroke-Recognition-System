#include "storage.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

//https://github.com/davegamble/cjson#example

static FILE *session_file = NULL;

// bestemmer hvordan et objekt skrives til filen
static void write_line(cJSON *obj)
{
    char *s = cJSON_PrintUnformatted(obj);
    if (s) {
        fputs(s, session_file);
        fputc('\n', session_file);
        fflush(session_file);
        free(s);
    }
    cJSON_Delete(obj);
}

int storage_new_session_dir(char *out, size_t size, const char *session_id)
{
    mkdir("recordings", 0755);

    snprintf(out, size, "recordings/session_%s", session_id);
    if (mkdir(out, 0755) != 0) {
        perror("Kunne ikke oprette sessionmappe");
        return -1;
    }
    return 0;
}

int storage_start(const char *dir, const char *participant, const char *session_id, const char *timestamp){    
    char path[256];
    storage_stop();
    snprintf(path, sizeof(path), "%s/info.jsonl", dir);
    session_file = fopen(path, "w");
    if (!session_file) {
        perror("Kunne ikke oprette fil");
        return -1;
    }

    cJSON *record = cJSON_CreateObject();
    cJSON_AddStringToObject(record, "type", "session");
    cJSON_AddStringToObject(record, "participant", participant);
    cJSON_AddStringToObject(record, "session_id", session_id);
    cJSON_AddStringToObject(record, "started_at", timestamp);
    write_line(record);
    return 0;
}

void storage_add_key_event(const char *sample_id, const char *key, const char *event, const char *timestamp)
{
    if (!session_file) return;
    cJSON *record = cJSON_CreateObject();
    cJSON_AddStringToObject(record, "type", "key");
    cJSON_AddStringToObject(record, "sample_id", sample_id);
    cJSON_AddStringToObject(record, "key", key);
    cJSON_AddStringToObject(record, "event", event);
    cJSON_AddStringToObject(record, "timestamp", timestamp);
    write_line(record);
}

//nok ikke nødvendig, men måske rar at have. 
void storage_add_text(int text_id, const char *text, const char *timestamp)
{
    if (!session_file) return;
    cJSON *record = cJSON_CreateObject();
    cJSON_AddStringToObject(record, "type", "target_text");
    cJSON_AddNumberToObject(record, "text_id", text_id);
    cJSON_AddStringToObject(record, "text", text);
    cJSON_AddStringToObject(record, "timestamp", timestamp);
    write_line(record);
}

void storage_stop(void)
{
    if (session_file) {
        fclose(session_file);
        session_file = NULL;
    }
}
