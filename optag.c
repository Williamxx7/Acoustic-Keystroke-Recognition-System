#include <stdio.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>

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
  

	//record PCM sounddata to a buffer called frames
    frames_read = snd_pcm_readi(pcm_handle, audio_buffer, 1024);
	if (frames_read < 0) {
    fprintf(stderr, "Audio read failed: %s\n", snd_strerror(frames_read));
    return(-1);
	}


	//PCM TO WAV PART
	SF_INFO info = { .samplerate = 48000, .channels = 1, .format = SF_FORMAT_WAV | SF_FORMAT_PCM_32 };

	SNDFILE *wavFile = sf_open("typeRecording.wav", SFM_WRITE, &info);
	if (wavFile == NULL) {
		fprintf(stderr, "Could not open WAV file: %s\n", sf_strerror(wavFile));
		return(-1);
	}

	sf_count_t frames_written = sf_writef_int(wavFile, (int*)audio_buffer, frames_read);
	if (frames_written != frames_read) {
		fprintf(stderr, "Could not read all frames: %s\n", sf_strerror(wavFile));
	}


	if(sf_close(wavFile) != 0){
		fprintf(stderr, "Could not close WAV file: %s\n", sf_strerror(wavFile));
	}

	//stop audio device efter 
	snd_pcm_drop(pcm_handle);	
}