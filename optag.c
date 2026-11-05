#include <stdio.h>
#include <alsa/asoundlib.h>

int main(void){

        snd_pcm_t *pcm_handle;
        snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
        snd_pcm_hw_params_t *parameters;
		unsigned int samplingRate = 48000;
		int samplingCloseness;
        char *pcm_name = "plughw:0,0";

	if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
      		fprintf(stderr, "Cannot connect to audio device %s\n", pcm_name);
      		return(-1);  	
	}
	
	//set settings on mic
  	snd_pcm_hw_params_alloca(&parameters);

  	snd_pcm_hw_params_any(pcm_handle, parameters);


  	if((snd_pcm_hw_params_set_access(pcm_handle, parameters, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0){
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
      		fprintf(stderr, "unable to set parameters: %s\n", snd_strerror(returnValue));
      		return(1);  	
	}
  

	//record audio
	snd_pcm_readi();	
	

	//stop audio device efter 
	snd_pcm_drop(pcm_handle);	
}