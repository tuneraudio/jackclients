#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <jack/jack.h>

#include "config.h"

/* control parameters */
#define TYPE ('t' | 'y' << 8 | 'p' << 8 | 'e' << 8 )
#define CENTER_FREQUENCY ('f' | 'c' << 8)
#define BANDWIDTH ('b' | 'w' << 8)
#define GAIN ('g')

typedef jack_default_audio_sample_t sample_t;

/* Globals */
jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
/* biquad *filter; */

/* Prototypes */
void *start_jack_client(void *ptr);

void start_messenger();

int process(jack_nframes_t nframes, void *arg);
void jack_shutdown(void *arg);
int parse_command(char *command, control_list *list, char *statusmessage);


int
main(int argc, char *argv[])
{
    pthread_t audio;

    /* create the audio engine thread */
    if ((pthread_create(&audio, NULL, start_jack_client, NULL))) {
        printf("thread creation failed: %d\n", peng);
        perror("socket");
        exit(EXIT_FAILURE);
    }

    start_messager();

    pthread_join(audio, NULL);
    return 0;
}

void
run(int fd)
{
    /* wait for the remote connection(s) */
    while (1) {
        int done = 0, n;

        printf("fclient: >> Waiting for a connection... <<\n");
        t = sizeof(remote);

        if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
            perror("accept");
            _exit(errno);
        }

        printf("fclient: >> Socket Connected <<\n");

        /* loop to receive data/commands */
        do {
            /* wait to receive a command */
            n = recv(s2, command, sizeof(command), RX_FLAGS );

            /* echo back the command */
            if (command != '\0')
                printf("COMMAND> %s\n", command);

            if (n <= 0) {
                if (n < 0) {
                    perror("recv");
                }
                done = 1;
            }
            if (!done) {

                /*************************************
                  process the command
                 *************************************/

                /* clear last status */
                strcpy(mstatus, "");

                /* parse the command */
                if (!parse_command(command, &ctrls, mstatus)) {
                    printf("command was parsed with status: %s\n", mstatus);

                    /* clean up old filter */
                    free(filter);

                    /* Compute new biquad (callback) */
                    filter = compute_biquad(ctrls->ftype,
                                            ctrls->dBgain,
                                            ctrls->fc,
                                            ctrls->fs,
                                            ctrls->bw);

                    printf("printing control list\n");
                    printf("fc=%f, g=%f, bw=%f \n", ctrls->fc, ctrls->dBgain, ctrls->bw);
                    printf("printing new coefficients:\n");
                    printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n\n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2);
                }

                /* transmit response */
                if (send(s2, mstatus, n, 0) < 0) {
                    perror("send");
                    done = 1;
                }

                /* clear the command */
                for (i = 0; i < n; i++) {
                    command[i] = '\0';
                }
            }

        } while (!done);

        close(s2);
    }
}

void *
start_messenger()
{
    int sfd, cfd;
    int len, i;

    struct sockaddr_un local, remote;
    socklen_t t;

    /* create a unix stream socket */
    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* assign the socket path */
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    unlink(local.sun_path);

    len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(sfd, (struct sockaddr *)&local, len) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* qeue up to 1 connections, then reject */
    if (listen(sfd, NUM_ClIENTS) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    run();
}

/**
 * This is the control message parser for this filter client.
 * It is called by whenever a valid message requesting filter
 * parameter changes is received by the messenger thread
 * INPUTS:  command string, controls list, status message string
 * OUTPUTS: int
 */
int
parse_command(char *command, control_list *list, char *statusmessage)
{
    char *control, *value;
    char *saveptr, *endptr;
    char *type;
    double  fpval;

    /* parse the command string into tokens */
    /* control part */
    control = strtok_r(command, "=", &saveptr);
    if (control == NULL) {
        printf("fclient: invalid format -> control=value\n");
        strcpy(statusmessage, "no control specified\n");
        return -1;
    }

    /* value part */
    value = strtok_r(NULL, "\n", &saveptr);
    if (value == NULL) {
        printf("fclient: no value set!\n");
        strcpy(statusmessage, "no value specified");
        return -1;
    }

    /* filter type change request */
    if (strcmp(control, "type") == 0) {
        type = value;
        strcpy(statusmessage, "filter type change success");

        /* determine the filter type */
        if(strcmp(type, "lpf") == 0) {
            printf("requested an lpf\n");
            list->ftype = LPF;
        } else if(strcmp(type, "hpf") == 0) {
            printf("requested an hpf\n");
            list->ftype = HPF;
        } else if(strcmp(type, "bpf") == 0) {
            printf("requested an bpf\n");
            list->ftype = BPF;
        } else if(strcmp(type, "notch") == 0) {
            printf("requested a notch filter\n");
            list->ftype = NOTCH;
        } else if(strcmp(type, "peq") == 0) {
            printf("requested a peaking eq\n");
            list->ftype = PEQ;
        } else if(strcmp(type, "lsh") == 0) {
            printf("requested a low shelf filter\n");
            list->ftype = LSH;
        } else if(strcmp(type, "hsh") == 0) {
            printf("requested a high shelf filter\n");
            list->ftype = HSH;

        } else {
            printf("requested an unsupported filter type\n");
            strcpy(statusmessage, "");
            strcpy(statusmessage, "filter type change failed");

        }
    } else {

        /* convert str to double */
        fpval = strtod(value, &endptr);

        if ((errno == ERANGE && (fpval == HUGE_VAL))
                || (errno != 0 && fpval == 0)) {
            perror("strtod");
            return -1;
        }

        if (endptr == value) {
            fprintf(stderr, "No digits were found\n");
            return -1;
        }

        printf("(control,value) = (%s,%f)\n",control, fpval);

        if (*endptr != '\0')        /* Not necessarily an error... */
            printf("Further characters after number: %s\n", endptr);

        /* check which control */
        if (strcmp(control, "fc") == 0) {
            list->fc = (smp_type)fpval;
            strcpy(statusmessage, "cut off frequency change success");
        } else if (strcmp(control, "g") == 0) {
            list->dBgain = (smp_type)fpval;
            strcpy(statusmessage, "gain change success");
        } else if (strcmp(control, "bw") == 0) {
            list->bw = (smp_type)fpval;
            strcpy(statusmessage, "bandwith change success");
        } else {
            printf("control parameter requested not found in list!\n");
            strcpy(statusmessage, "no control in list");
            return -1;
        }
    }
    return 0;
}

void *
start_jack_client(void *ptr)
{
    const char *client_name = "filter";
    const char *server_name = NULL;
    jack_options_t options = JackNoStartServer|JackUseExactName|JackSessionID;
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
    printf ("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate (client));


    /* create two ports */
    input_port = jack_port_register(client, "input",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsInput, 0);

    output_port = jack_port_register(client, "output",
                                     JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsOutput, 0);

    if ((input_port == NULL) || (output_port == NULL)) {
        fprintf(stderr, "no more JACK ports available\n");
        exit (1);
    }

    /* create and initialize the control list */
    /* ctrls = malloc(sizeof(control_list)); */
    /* if (!ctrls) { */
    /*     perror("malloc"); */
    /*     exit(EXIT_FAILURE); */
    /* } */
    control_list ctrls;

    /* set with default values */
    ctrls.ftype = LPF;
    ctrls.dBgain = 0;  // dB value
    ctrls.fc = 100;    //Hz value
    ctrls.fs = (smp_type)jack_get_sample_rate(client);
    ctrls.bw = 0.25;   //bandwidth (ocataves)

    /* Compute default biquad lpf */
    filter = compute_biquad(ctrls.ftype,
                            ctrls.dBgain,
                            ctrls.fc,
                            ctrls.fs,
                            ctrls.bw);

    printf("initial coefficients:\n");
    printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2);


    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if (jack_activate(client)) {
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

    /* this is never reached, but if the program
       had some other way to exit besides being killed,
       they would be important to call.  */

    jack_client_close (client);
    exit (0);
}

/****************************************************
 * The process callback for this JACK application
 * (i.e. the audio workhorse function)
 * It is called by JACK at the appropriate times
 * Optimize this function for time!
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
        memcpy (out, in, sizeof(sample_t) * nframes);
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

