/*This is very simple biquad filter client that processes a single
 * channel of audio with a selected filter parameter set.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include "biquad_df1.h"

/* GLOBALS */
jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
typedef jack_default_audio_sample_t sample_t;
biquad *filter;

/**
 * The process callback for this JACK application 
 * (i.e. the audio workhorse function)
 * It is called by JACK at the appropriate times.
 */
int
process (jack_nframes_t nframes, void *arg)
{
    //biquad *filter = arg[0];

    sample_t *out = (sample_t *) jack_port_get_buffer (output_port, nframes);
    sample_t *in = (sample_t *) jack_port_get_buffer (input_port, nframes);

    for(int i = 0; i < nframes; i++) {

	/* cast to the current sample format */
	smp_type insample = (smp_type)in[i];
	smp_type outsample;

	/* compute the output sample */
	outsample = df1(insample, filter);

	/*
	out[i] = (sample_t)outsample;
    }
	

    /* bypass switch */
    //memcpy (out, in, sizeof (sample_t) * nframes);

    //printf("%d = %f \n", i, out[i]);
//   printf("%f \n", out[10]);
    return 0;      
}

/**
 * This is the shutdown callback for this JACK application.
 * It is called by JACK if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
    exit (1);
}

int
main (int argc, char *argv[])
{
    const char **ports;
    const char *client_name = "filter";
    const char *server_name = NULL;
    jack_options_t options = (JackNoStartServer|JackUseExactName|JackSessionID);
    jack_status_t status;

    /* open a client connection to the JACK server */
    client = jack_client_open (client_name, options, &status, server_name);
    if (client == NULL) {
	fprintf (stderr, "jack_client_open() failed, "
		"status = 0x%2.0x\n", status);
	if (status & JackServerFailed) {
	    fprintf (stderr, "Unable to connect to JACK server\n");
	}
	exit (1);
    }
    if (status & JackServerStarted) {
	fprintf (stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
	client_name = jack_get_client_name(client);
	fprintf (stderr, "unique name `%s' assigned\n", client_name);
    }

    /* tell the JACK server to call `process()' whenever
       there is work to be done.  */

    jack_set_process_callback (client, process, 0);

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.  */

    jack_on_shutdown (client, jack_shutdown, 0);

    /* display the current sample rate.  */

    printf ("engine sample rate: %" PRIu32 "\n",
	    jack_get_sample_rate (client));

    /* create two ports */

    input_port = jack_port_register (client, "input",
	    JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    output_port = jack_port_register (client, "output",
	    JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if ((input_port == NULL) || (output_port == NULL)) {
	fprintf(stderr, "no more JACK ports available\n");
	exit (1);
    }

    /* Compute biquad filter */
    
    smp_type gain = 0;	// dB value
    smp_type fc = 100;	// Hz value
    smp_type Fs = (smp_type)jack_get_sample_rate(client);
    smp_type bw = 0.25;	// bandwith (octaves)

    filter = BiQuad_new(LPF, gain, fc, Fs, bw); 
    printf("b0= %f, b1= %f, b2= %f, a1=%f, a2=%f \n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2);

    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if (jack_activate (client)) {
	fprintf (stderr, "cannot activate client");
	exit (1);
    }


    /* Connect the ports.  You can't do this before the client is
     * activated, because we can't make connections to clients
     * that aren't running.  Note the confusing (but necessary)
     * orientation of the driver backend ports: playback ports are
     * "input" to the backend, and capture ports are "output" from
     * it.  */

    /**********************************************************
     Automatic Port Connections! -> Not usually recommended!
    ***********************************************************
    ports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
    if (ports == NULL) {
	fprintf(stderr, "no physical capture ports\n");
	exit (1);
    }

    if (jack_connect (client, ports[0], jack_port_name (input_port))) {
	fprintf (stderr, "cannot connect input ports\n");
    }

    free (ports);

    ports = jack_get_ports (client, NULL, NULL, 
			    JackPortIsPhysical|JackPortIsInput);

    if (ports == NULL) {
	fprintf(stderr, "no physical playback ports\n");
	exit (1);
    }

    if (jack_connect (client, jack_port_name (output_port), ports[0])) {
	fprintf (stderr, "cannot connect output ports\n");
    }

    free (ports);
*/
    /* keep running until stopped by the user */

    sleep (-1);

    /* this is never reached but if the program
       had some other way to exit besides being killed,
       they would be important to call.  */

    jack_client_close (client);
    exit (0);
}
