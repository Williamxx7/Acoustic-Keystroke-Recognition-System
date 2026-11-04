#include <stdio.h>
#include <alsa/asoundlib.h>

int main(void){

        snd_pcm_t *pcm_handle;
        snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
        snd_pcm_hw_params_t *hwparams;

        char *pcm_name;
        pcm_name = strdup("plughw:0,0");
	if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
      		fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
      		return(-1);  	
	}
	fprintf("Connected to audiodevice %s\n"m pcm_name);
	
	//set settings on mic
	//record audio
	snd_pcm_readi();	
	
	//stop audio device efter 
	snd_pcm_drop(pcm_handle);	
}