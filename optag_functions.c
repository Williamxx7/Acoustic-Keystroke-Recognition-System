#define _POSIX_C_SOURCE 200809L

#include <alloca.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <stdint.h>


struct audio_recorder {   // Structure declaration
    snd_pcm_t *pcm_handle;
    //audio buffer skal sendes til wav_prepare
    SNDFILE *wavFile;
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 50000000L };
    int record_decider;
};

int configure_pcm_settings(struct audio_recorder recorder){
    snd_pcm_t *recorder->pcm_handle;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_hw_params_t *parameters; //pointer to the configuration settings of PCM Hardware.
    unsigned int samplingRate = 48000;
    int samplingCloseness = 0;
    char *pcm_name = "plughw:0,0"; //Standard name for the first audio device connected to the pi.
    snd_pcm_sframes_t frames_read;
    int32_t audio_buffer[1024];

    if (snd_pcm_open(&recorder->pcm_handle, pcm_name, stream, 0) < 0) {
        fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
        return(-1);
    }

    //set settings on mic
    snd_pcm_hw_params_alloca(&parameters); //automatically allocate space in memory instead of static like malloc

    snd_pcm_hw_params_any(recorder->pcm_handle, parameters); //Fills hardware parameters with defualt values so the pcm will work.

    if((snd_pcm_hw_params_set_access(recorder->pcm_handle, parameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){ //using set function to set access for interleaved which takes the input from all channels one after another. Since we are mono (1 channel) it doenst affect us
        fprintf(stderr, "Cannot set interleaved access");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* Signed 32-bit little-endian format because the buffer is 24 bit on the inmp441 */
    if((snd_pcm_hw_params_set_format(recorder->pcm_handle, parameters, SND_PCM_FORMAT_S32_LE)) < 0 ){
        fprintf(stderr, "Cannot set format of buffer");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* one channel because mono mic */
    if((snd_pcm_hw_params_set_channels(recorder->pcm_handle, parameters, 1)) < 0 ){
        fprintf(stderr, "Cannot set 1 channel correctly");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* 48khz sampling rate */
    if((snd_pcm_hw_params_set_rate_near(recorder->pcm_handle, parameters, &samplingRate, &samplingCloseness)) < 0 ){
        fprintf(stderr, "Cannot set sampling rate");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* Write the parameters to the driver */
    int returnValue = snd_pcm_hw_params(recorder->pcm_handle, parameters);
    if (returnValue < 0) {
        fprintf(stderr, "Unable to set parameters: %s\n", snd_strerror(returnValue));
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    }
}

int start_recording(struct audio_recorder recorder){
    while (recorder->record_decider != 0) { //forever loop
        int ready = snd_pcm_wait(pcm_handle, 20);
        if (ready > 0) {
            frames_read = snd_pcm_readi(pcm_handle, audio_buffer, 1024);
            if (frames_read < 0) {
                frames_read = snd_pcm_recover(pcm_handle, (int)frames_read, 0); //we want to save the audio reading if the original reading fails
                if (frames_read < 0) {
                    fprintf(stderr, "Audio read failed: %s\n", snd_strerror((int)frames_read));
                    break;
                }
            } else {
                sf_count_t frames_written = sf_writef_int(wavFile, (int *)audio_buffer, frames_read);
                if (frames_written != frames_read) {
                    fprintf(stderr, "Could not write frames: %s\n", sf_strerror(wavFile));
                }
            }
        }
    }
}
int stop_recording(struct audio_recorder recorder){
    recorder->record_decider = 0;
}

int wav_prepare(struct audio_recorder recorder){
    SF_INFO info = { .samplerate = 48000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_32 };
    SNDFILE *recorder->wavFile = sf_open("audio.wav", SFM_WRITE, &info);
    if (wavFile == NULL) {
        fprintf(stderr, "Could not open WAV file: %s\n", sf_strerror(NULL));
        snd_pcm_close(pcm_handle);
        return(-1);
    }
}

int mic_prepare(struct audio_recorder recorder){
    int error = snd_pcm_prepare(recorder->pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Prepare failed: %s\n", snd_strerror(error));
        sf_close(recorder->wavFile);
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    }

    error = snd_pcm_start(recorder->pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Failed start of pcm connection: %s\n", snd_strerror(error));
        sf_close(recorder->wavFile);
        snd_pcm_close(recorder->pcm_handle);
        return(-1);
    }
}
int cleanup(struct audio_recorder recorder){
    snd_pcm_drop(recorder->pcm_handle);

    if (sf_close(recorder->wavFile) != 0) {
        fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(NULL));
    } else {
        printf("Made audio.wav.\n");
    }
}

int main(void){
    struct audio_recorder recorder = {0};

}
