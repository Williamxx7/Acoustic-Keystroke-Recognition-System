#ifndef OPTAG_H
#define OPTAG_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>

/*
Configure_settings(&recorder)
configure_wavFile(&recorder, path til fil-lokation)
start_recording(&recorder)
stop_record(&recorder)
cleanup(&recorder)
*/




struct audio_recorder {
    snd_pcm_t *pcm_handle;
    SNDFILE *wavFile;
    atomic_int record_decider;
    int32_t audio_buffer[1024];
    pthread_t thread;
    int thread_started;
};

int configure_settings(struct audio_recorder *recorder);

int configure_wavFile(struct audio_recorder *recorder, const char *path);

int start_recording(struct audio_recorder *recorder);

/
int stop_record(struct audio_recorder *recorder);


int cleanup(struct audio_recorder *recorder);

#endif