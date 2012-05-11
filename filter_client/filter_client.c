/*This is very simple biquad filter client that processes a single
 * channel of audio with a selected filter parameter set.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <jack/jack.h>
#include "biquad_df1.h"

#define SOCK_PATH "command_socket"
#define RX_FLAGS 0 //MSG_WAITALL? -> wtf should this be anyways?
#define NUM_ClIENTS 1

/* GLOBALS */
jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
typedef jack_default_audio_sample_t sample_t;
biquad *filter;
control_list *ctrls;

/* passthrough switch state */
enum {
    ON,
    OFF,
};
int STATE = OFF;

/* function prototypes */
void * start_jack_client(void *ptr);
void * start_messenger(void *ptr);
int process (jack_nframes_t nframes, void *arg);
void jack_shutdown (void *arg);
char *parse_command(char *command, control_list **list);


int
main (int argc, char *argv[])
{
    int peng, pmssg;
    pthread_t aengine, messg; 

    /* create two threads: audio engine, and messenger */
    if((peng = pthread_create(&aengine, NULL, start_jack_client, NULL))) {
	printf("thread creation failed: %d\n", peng);
    }

    if((pmssg = pthread_create(&messg, NULL, start_messenger, NULL))) {
	printf("thread creation failed: %d\n", pmssg);
    } 
    
    /* wait until threads complete before main continues */
    pthread_join(aengine,NULL);
    //pthread_join(messg,NULL); 

    /* shouldn't get here but if we do shut down safely */
    jack_client_close (client);
    exit (0);
}

void *
start_messenger(void *ptr)
{

    int s, s2, len;
    socklen_t t;
    struct sockaddr_un local, remote;
    char command[32];
    char *mstatus;
    control_list *ctrls;

    /* create a unix stream socket */
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	perror("socket");
	exit(1);
    }

    /* assign the socket path */
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    /* unlink if socket already exists */
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(s, (struct sockaddr *)&local, len) == -1) {
	perror("bind");
	pthread_exit((void *)errno);
    }

    /* qeue up to 1 connections, then reject */
    if (listen(s, NUM_ClIENTS) == -1) {
	perror("listen");
	exit(1);
    }

    /* wait for the remote connection */
    for(;;) {
	int done, n;
	printf("Waiting for a connection...\n");
	t = sizeof(remote);
	if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
	    perror("accept");
	    pthread_exit((void *)errno);
	}

	printf("Socket Connected.\n");

	done = 0;
	do {
	    n = recv(s2, command, sizeof(command), RX_FLAGS );
	    printf("echo> %s", command);
	    if (n <= 0) { 
		if (n < 0) {
		    perror("recv");
		}
		/* code to process command */
		mstatus = parse_command(command, &ctrls);
		


		done = 1;
	    }

	    if (!done) 
		/* code to generate response */
		if (send(s2, command, n, 0) < 0) {
		    perror("send");
		    done = 1;
		}
	} while (!done);

	close(s2);
    }
    pthread_exit(NULL);
    //return 0;
}

void *
start_jack_client(void *ptr)
{
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

    /* Compute default biquad lpf */
    
    smp_type gain = 0;	// dB value
    smp_type fc = 100;	// Hz value
    smp_type Fs = (smp_type)jack_get_sample_rate(client);
    smp_type bw = 0.25;	// bandwith (octaves)

    filter = compute_biquad(LPF, gain, fc, Fs, bw); 

    printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2);


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
    const char **ports;

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

/****************************************************
 * The process callback for this JACK application 
 * (i.e. the audio workhorse function)
 * It is called by JACK at the appropriate times.
****************************************************/
int
process (jack_nframes_t nframes, void *arg)
{
    /* consider passing the filter struct */
    //biquad *filter = arg[0];

    sample_t *out = (sample_t *) jack_port_get_buffer (output_port, nframes);
    sample_t *in = (sample_t *) jack_port_get_buffer (input_port, nframes);
    smp_type outsample;

    for(int i = 0; i < nframes; i++) {

	/* compute the output sample */
	outsample = df1((smp_type)in[i], filter);
	out[i] = (sample_t)outsample;
    }
	

    /* bypass switch */
    if( STATE == ON) { 
	memcpy (out, in, sizeof (sample_t) * nframes);
    }

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

/**
 * This is the control message parser for this filter client.
 * It is called by whenever a valid message requesting filter 
 * tap changes is received by the messenger thread
 * INPUTS:  command string
 * OUTPUTS: controls struct
 */
char *
parse_command(char *command, control_list **list)
{    
    char *control;
    char *str;
    char *mstatus 
    smp_type decimal;
    int i;

    for (i = 0; strncmp(command + i, "=", 1); i++) {
	*(control + i) = command[i];
    }
    i+=1;    // skip '='
    while (!strncmp(command + i,"\0", 1)) {
	str[i] = command[i];
    }
    /* if control is the filter "type" */
    if (strcmp(control,"type")) {
	*list.type = *str;
    } else {
	decimal = strtod(str, NULL);
    }

    /* determine which filter control */
    switch (command[0]) {
	case "fc": 
	    *list->fc = decimal;
	default:
	    mstatus = "invalid";
	    printf("invalid command");
    }
		
    smp_type gain = 0;	// dB value
    smp_type fc = 100;	// Hz value
    smp_type Fs = (smp_type)jack_get_sample_rate(client);
    smp_type bw = 0.25;	// bandwith (octaves)

    /* Compute biquad filter */
    filter = compute_biquad(LPF, gain, fc, Fs, bw); 
    printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2);

    return ctrl;
    }
}
/* this holds the control data instructions to compute a 
 * biquad filter 
typedef struct {
    int type;	    // see filter types below 
    smp_type dBgain;// gain in dB 
    smp_type fc;    // cut off / center frequency 
    smp_type fs;    // sample rate (not actual control data?) 
    smp_type bw;    // bandwidth in octaves 
}control_list; */
