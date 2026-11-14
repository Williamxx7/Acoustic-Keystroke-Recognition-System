#define _POSIX_C_SOURCE 200809L
#define GPIO_CHIP "/dev/gpiochip0"
#define BUTTON_GPIO 17

#include <alloca.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <stdint.h>
#include <gpiod.h>

int main(void){

    //PCM recording part
    snd_pcm_t *pcm_handle;
    snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_hw_params_t *parameters; //pointer to the configuration settings of PCM Hardware.
    unsigned int samplingRate = 48000;
    int samplingCloseness = 0;
    char *pcm_name = "plughw:0,0"; //Standard name for the first audio device connected to the pi.
    snd_pcm_sframes_t frames_read;
    int32_t audio_buffer[1024]; //has to be int32_t type as that is required for the buffer of PCM frames in ALSA. So its a array with 1024 spaces for 32 bit ints

    struct gpiod_chip *chip = gpiod_chip_open(GPIO_CHIP);
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    struct gpiod_line_config *line_config = gpiod_line_config_new();
    unsigned int offset = BUTTON_GPIO; //laver den her offset, da i linjen nedenunder skal der bruges en unsigned int pointer for parameter 2
    gpiod_line_config_add_line_settings(line_config, &offset, 1, settings);
    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, NULL, line_config);

    //time declartion of variables:
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 50000000L };

    printf("Ready for recording. Press button to record\n");
    while (gpiod_line_request_get_value(request, BUTTON_GPIO) != 0) { } //run forever until press to get LOW value
    nanosleep(&delay, NULL);   /* debounce */
    printf("Its hit, so we record now\n");

    while (gpiod_line_request_get_value(request, BUTTON_GPIO) == 0) { } //run forever while the button is presed down, so it has to be realeased
    nanosleep(&delay, NULL);

    if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
        fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
        return(-1);
    }

    //set settings on mic
    snd_pcm_hw_params_alloca(&parameters); //automatically allocate space in memory instead of static like malloc

    snd_pcm_hw_params_any(pcm_handle, parameters); //Fills hardware parameters with defualt values so the pcm will work.

    if((snd_pcm_hw_params_set_access(pcm_handle, parameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){ //using set function to set access for interleaved which takes the input from all channels one after another. Since we are mono (1 channel) it doenst affect us
        fprintf(stderr, "Cannot set interleaved access");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* Signed 32-bit little-endian format because the buffer is 24 bit on the inmp441 */
    if((snd_pcm_hw_params_set_format(pcm_handle, parameters, SND_PCM_FORMAT_S32_LE)) < 0 ){
        fprintf(stderr, "Cannot set format of buffer");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* one channel because mono mic */
    if((snd_pcm_hw_params_set_channels(pcm_handle, parameters, 1)) < 0 ){
        fprintf(stderr, "Cannot set 1 channel correctly");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* 48khz sampling rate */
    if((snd_pcm_hw_params_set_rate_near(pcm_handle, parameters, &samplingRate, &samplingCloseness)) < 0 ){
        fprintf(stderr, "Cannot set sampling rate");
        snd_pcm_close(pcm_handle);
        return(-1);
    };

    /* Write the parameters to the driver */
    int returnValue = snd_pcm_hw_params(pcm_handle, parameters);
    if (returnValue < 0) {
        fprintf(stderr, "Unable to set parameters: %s\n", snd_strerror(returnValue));
        snd_pcm_close(pcm_handle);
        return(-1);
    }

    SF_INFO info = { .samplerate = 48000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_32 };
    SNDFILE *wavFile = sf_open("audio.wav", SFM_WRITE, &info);
    if (wavFile == NULL) {
        fprintf(stderr, "Could not open WAV file: %s\n", sf_strerror(NULL));
        snd_pcm_close(pcm_handle);
        return(-1);
    }

    while (gpiod_line_request_get_value(request, BUTTON_GPIO) == 0) {} //Loop for waiting until button is released
    nanosleep(&delay, NULL); //give 50ms after to be stable

    //prepare mic
    int error = snd_pcm_prepare(pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Prepare fejlede: %s\n", snd_strerror(error));
        sf_close(wavFile);
        snd_pcm_close(pcm_handle);
        return(-1);
    }

    //start mic, capture streams do not start on their own after prepare
    error = snd_pcm_start(pcm_handle);
    if (error < 0) {
        fprintf(stderr, "Start fejlede: %s\n", snd_strerror(error));
        sf_close(wavFile);
        snd_pcm_close(pcm_handle);
        return(-1);
    }

    int last_reading = 1;
    printf("Recording audio now. Press to stop\n");

    while (1) { //forever loop
        /* Vent højst 20 ms på data, så knappen altid bliver tjekket */
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

        int current_reading = gpiod_line_request_get_value(request, BUTTON_GPIO);

        if (last_reading == 1 && current_reading == 0) { //if new press is here then stop.
            nanosleep(&delay, NULL); /* debounce */
            break;
        }
        last_reading = current_reading;
    }

    snd_pcm_drop(pcm_handle);

    if (sf_close(wavFile) != 0) {
        fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(NULL));
    } else {
        printf("Gemt audio.wav.\n");
    }

    snd_pcm_close(pcm_handle);
    gpiod_line_request_release(request);
    gpiod_line_config_free(line_config);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);

    return 0;
}