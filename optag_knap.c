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

    int stable_reading = 0;
    struct gpiod_chip *chip = gpiod_chip_open(GPIO_CHIP);
	struct gpiod_line_settings *settings = gpiod_line_settings_new();
	gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
	gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);
    struct gpiod_line_config *line_config = gpiod_line_config_new();
    unsigned int offset = BUTTON_GPIO; //laver den her offset, da i linjen nedenunder skal der bruges en unsigned int pointer for parameter 2
    gpiod_line_config_add_line_settings(line_config,  &offset, 1, settings);
    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, NULL, line_config);


    //time declartion of variables:
    struct timespec second, nanosecond;
    second.tv_sec = 0;
    nanosecond.tv_nsec = 50000000L; //has to be have long type
    while(stable_reading < 1) { //loop for start af recording, kører uendelig indtil man trykker
        int value1 = gpiod_line_request_get_value(request, BUTTON_GPIO);
        printf("starting loop: reading1: %d", value1);
        nanosleep(&second, &nanosecond);
        int value2 = gpiod_line_request_get_value(request, BUTTON_GPIO);
        printf("reading2: %d", value2);
        if(value1 == value2){
            stable_reading = 1;
        }
    }
    stable_reading = 0;

	if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
            fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
            return(-1);
    }

    //set settings on mic
    snd_pcm_hw_params_alloca(&parameters); //automatically allocate space in memory instead of static like malloc

    snd_pcm_hw_params_any(pcm_handle, parameters); //Fills hardware parameters with defualt values so the pcm will work.


    if((snd_pcm_hw_params_set_access(pcm_handle, parameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){ //using set function to set access for interleaved which takes the input from all channels one after another. Since we are mono (1 channel) it doenst affect us
        fprintf(stderr, "Cannot set interleaved access");
        return(-1);
    };

    /* Signed 32-bit little-endian format because the buffer is 24 bit on the inmp441 */
    if((snd_pcm_hw_params_set_format(pcm_handle, parameters, SND_PCM_FORMAT_S32_LE)) < 0 ){
        fprintf(stderr, "Cannot set format of buffer");
        return(-1);
    };

    /* one channel because mono mic */
    if((snd_pcm_hw_params_set_channels(pcm_handle, parameters, 1)) < 0 ){
        fprintf(stderr, "Cannot set 1 channel correctly");
        return(-1);
  	};

    /* 48khz sampling rate */
    if((snd_pcm_hw_params_set_rate_near(pcm_handle, parameters, &samplingRate, &samplingCloseness)) < 0 ){
        fprintf(stderr, "Cannot set sampling rate");
        return(-1);
    };

    /* Write the parameters to the driver */
    int returnValue = snd_pcm_hw_params(pcm_handle, parameters);
    if (returnValue < 0) {
        fprintf(stderr, "Unable to set parameters: %s\n", snd_strerror(returnValue));
        return(-1);
    }

	SF_INFO info = { .samplerate = 48000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_32 };
    SNDFILE *wavFile = sf_open("audio.wav", SFM_WRITE, &info);
        if (wavFile == NULL) {
                fprintf(stderr, "Could not open WAV file: %s\n", sf_strerror(wavFile));
                return(-1);
        }

    while(stable_reading < 2) {
        snd_pcm_prepare(pcm_handle);
        int error = snd_pcm_prepare(pcm_handle);
        if (error < 0){
            printf("Can't do recovery, prepare failed: %s\n", snd_strerror(error));
            return(-1);
        }

        frames_read = snd_pcm_readi(pcm_handle, audio_buffer, 1024);
        if (frames_read < 0) {
            fprintf(stderr, "Audio read failed: %s\n", snd_strerror(frames_read));
            return(-1);
        }

        sf_count_t frames_written = sf_writef_int(wavFile, (int*)audio_buffer, frames_read);
        if (frames_written != frames_read) {
            fprintf(stderr, "Could not read frames: %s\n", sf_strerror(wavFile));
        }


        int value1 = gpiod_line_request_get_value(request, BUTTON_GPIO);
        printf("recording loop: reading1: %d", value1)
        nanosleep(&second, &nanosecond); //50ms
        int value2 = gpiod_line_request_get_value(request, BUTTON_GPIO);
        printf("reading2: %d", value2)
        if(value1 == 1 && value2 == 1){
            stable_reading = 2;
        }
    }

    if(sf_close(wavFile) != 0){
            fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(wavFile));
    }
}


