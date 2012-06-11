#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <alsa/asoundlib.h>
	      
	snd_pcm_t *playback_handle;
	short buf[4096];
	
	int
	playback_callback (snd_pcm_sframes_t nframes)
	{
		int err;
	
		printf ("playback callback called with %u frames\n", nframes);
	
		if ((err = snd_pcm_writei (playback_handle, buf, nframes)) < 0) {
			fprintf (stderr, "write failed (%s)\n", snd_strerror (err));
		}
	
		return err;
	}
	      
	main (int argc, char *argv[])
	{
	
		snd_pcm_hw_params_t *hw_params;
		snd_pcm_sw_params_t *sw_params;
		snd_pcm_sframes_t frames_to_deliver;
		int nfds;
		int err;
		struct pollfd *pfds;
	
		if ((err = snd_pcm_open (&playback_handle, argv[1], SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
			fprintf (stderr, "cannot open audio device %s (%s)\n", 
				 argv[1],
				 snd_strerror (err));
			exit (1);
		}
		   
		if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
			fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
				 
		if ((err = snd_pcm_hw_params_any (playback_handle, hw_params)) < 0) {
			fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
			fprintf (stderr, "cannot set access type (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_hw_params_set_format (playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
			fprintf (stderr, "cannot set sample format (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, 44100, 0)) < 0) {
			fprintf (stderr, "cannot set sample rate (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_hw_params_set_channels (playback_handle, hw_params, 2)) < 0) {
			fprintf (stderr, "cannot set channel count (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_hw_params (playback_handle, hw_params)) < 0) {
			fprintf (stderr, "cannot set parameters (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		snd_pcm_hw_params_free (hw_params);
	
		if ((err = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
			fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_current (playback_handle, sw_params)) < 0) {
			fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_set_avail_min (playback_handle, sw_params, 4096)) < 0) {
			fprintf (stderr, "cannot set minimum available count (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, 0U)) < 0) {
			fprintf (stderr, "cannot set start mode (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
		if ((err = snd_pcm_sw_params (playback_handle, sw_params)) < 0) {
			fprintf (stderr, "cannot set software parameters (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		if ((err = snd_pcm_prepare (playback_handle)) < 0) {
			fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
				 snd_strerror (err));
			exit (1);
		}
	
		while (1) {
	
			if ((err = snd_pcm_wait (playback_handle, 1000)) < 0) {
			        fprintf (stderr, "poll failed (%s)\n", strerror (errno));
			        break;
			}	           
	
			if ((frames_to_deliver = snd_pcm_avail_update (playback_handle)) < 0) {
				if (frames_to_deliver == -EPIPE) {
					fprintf (stderr, "an xrun occured\n");
					break;
				} else {
					fprintf (stderr, "unknown ALSA avail update return value (%d)\n", 
						 frames_to_deliver);
					break;
				}
			}
	
			frames_to_deliver = frames_to_deliver > 4096 ? 4096 : frames_to_deliver;
	
			if (playback_callback (frames_to_deliver) != frames_to_deliver) {
			        fprintf (stderr, "playback callback failed\n");
				break;
			}
		}
	
		snd_pcm_close (playback_handle);
		exit (0);
	}
