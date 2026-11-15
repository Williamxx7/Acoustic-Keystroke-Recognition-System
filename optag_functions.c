#define _POSIX_C_SOURCE 200809L
#include "optag.h"
#include <stdio.h>
#include <alloca.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

int configure_settings(struct audio_recorder *recorder){
    //PREPARE PCM
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_hw_params_t *parameters; //pointer to the configuration settings of PCM Hardware.
    unsigned int samplingRate = 48000;
    int samplingCloseness = 0;
    char *pcm_name = "plughw:0,0"; //Standard name for the first audio device connected to the pi.

    if (snd_pcm_open(&recorder->pcm_handle, pcm_name, stream, 0) < 0) {
        fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
        return(-1);
    }

    //set settings on mic
    snd_pcm_hw_params_alloca(&parameters); //automatically allocate space in memory instead of static like malloc

    snd_pcm_hw_params_any(recorder->pcm_handle, parameters); //Fills hardware parameters with defualt values so the pcm will work.

    if((snd_pcm_hw_params_set_access(recorder->pcm_handle, parameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){ //using set function to set access for interleaved which takes the input from all channels one after another. Since we are mono (1 channel) it doenst affect us
        fprintf(stderr, "Cannot set interleaved access");
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    };

    /* Signed 32-bit little-endian format because the buffer is 24 bit on the inmp441 */
    if((snd_pcm_hw_params_set_format(recorder->pcm_handle, parameters, SND_PCM_FORMAT_S32_LE)) < 0 ){
        fprintf(stderr, "Cannot set format of buffer");
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    };

    /* one channel because mono mic */
    if((snd_pcm_hw_params_set_channels(recorder->pcm_handle, parameters, 1)) < 0 ){
        fprintf(stderr, "Cannot set 1 channel correctly");
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    };

    /* 48khz sampling rate */
    if((snd_pcm_hw_params_set_rate_near(recorder->pcm_handle, parameters, &samplingRate, &samplingCloseness)) < 0 ){
        fprintf(stderr, "Cannot set sampling rate");
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    };

    /* Write the parameters to the driver */
    int returnValue = snd_pcm_hw_params(recorder->pcm_handle, parameters);
    if (returnValue < 0) {
        fprintf(stderr, "Unable to set parameters: %s\n", snd_strerror(returnValue));
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    }

    return 0;
}

int configure_wavFile(struct audio_recorder *recorder, const char *path){
     //PREPARE WAVFILE
    SF_INFO info = { .samplerate = 48000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_32 };
    recorder->wavFile = sf_open(path, SFM_WRITE, &info);
    if (recorder->wavFile == NULL) {
        fprintf(stderr, "Could not open WAV file: %s\n", sf_strerror(NULL));
        return(-1);
    }
    return 0;
}

static void *record_loop(void *recorder_data){ //has to be void, as its a parameter for the thread to call. It doesnt know our struct beforehand so we do a local variable as a pointer to get the right addresses
    struct audio_recorder *recorder = recorder_data;

    while (recorder->record_decider != 0) {
        int ready = snd_pcm_wait(recorder->pcm_handle, 20);
        if (ready > 0) {
            snd_pcm_sframes_t frames_read = snd_pcm_readi(recorder->pcm_handle, recorder->audio_buffer, 1024);
            if (frames_read < 0) {
                int recovered = snd_pcm_recover(recorder->pcm_handle, (int)frames_read, 0);
                if (recovered < 0) {
                    fprintf(stderr, "Audio read failed: %s\n", snd_strerror(recovered));
                    break;
                }
            } else {
                sf_count_t frames_written = sf_writef_int(recorder->wavFile, (int *)recorder->audio_buffer, frames_read);
                if (frames_written != frames_read) {
                    fprintf(stderr, "Could not write frames: %s\n", sf_strerror(recorder->wavFile));
                }
            }
        }
        if (ready < 0) {
            snd_pcm_recover(recorder->pcm_handle, ready, 0);
            continue;
        }
    }
    return NULL;
}

int start_recording(struct audio_recorder *recorder){
    if (recorder->thread_started || recorder->wavFile == NULL){
        return(-1);
    }

    //SETUP MIC
    int error = snd_pcm_prepare(recorder->pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Prepare failed: %s\n", snd_strerror(error));
        return(-1);
    }

    error = snd_pcm_start(recorder->pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Failed start of pcm connection: %s\n", snd_strerror(error));
        return(-1);
    }

    atomic_store(&recorder->record_decider, 1);
    if (pthread_create(&recorder->thread, NULL, record_loop, recorder) != 0) {
        fprintf(stderr, "Could not start recording thread\n");
        atomic_store(&recorder->record_decider, 0);
        return(-1);
    }
    recorder->thread_started = 1;
    return 0;
}

int stop_record(struct audio_recorder *recorder){
    if (recorder->thread_started == 0) {
        return -1;
    }


    atomic_store(&recorder->record_decider, 0);
    pthread_join(recorder->thread, NULL);
    recorder->thread_started = 0;

    snd_pcm_drop(recorder->pcm_handle);

    if (recorder->wavFile != NULL) {
        if (sf_close(recorder->wavFile) != 0)
            fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(NULL));
        recorder->wavFile = NULL;
    }
    return 0;
}

int cleanup(struct audio_recorder *recorder){
    int error = snd_pcm_drop(recorder->pcm_handle);
    if (error != 0){
        fprintf(stderr, "Could not drop the recording: %s\n", snd_strerror(error));
        recorder->pcm_handle = NULL;
        return (-1);
    };

    error = snd_pcm_close(recorder->pcm_handle);
    if (error != 0) {
        fprintf(stderr, "Could not close pcm connection: %s\n", snd_strerror(error));
        return (-1);
    }

    if (sf_close(recorder->wavFile) != 0) {
        fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(NULL));
        return (-1);
    }

    recorder->wavFile = NULL;
    recorder->pcm_handle = NULL;
    return 0;
}