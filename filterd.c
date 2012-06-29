#include "filterd.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <jack/jack.h>
#include <tunerlib.h>

#define UNUSED     __attribute__((unused))
#define LOCAL      __thread
#define STREQ(x,y) (strcmp((x),(y)) == 0)

typedef jack_default_audio_sample_t sample_t;

/* Globals */
static jack_port_t *input_port;
static jack_port_t *output_port;
static biquad_t *filter;
static LOCAL jack_client_t *client;

static filter_controls_t ctrls = {
    .type = FILTER_LOW_PASS,
    .gain = 0,    // dB value
    .fc   = 100,  // Hz value
    .bw   = 0.25, // bandwidth (ocataves)
};

/* Prototypes */
static void listener(void);
static void run(int sfd);
static int parse_command(char *command, char *statusmessage);

static void *jack(void *arg);
static int jack_process(jack_nframes_t nframes, void *);
static void jack_shutdown(void *);

void
listener()
{
    int sfd;
    int len;

    struct sockaddr_un local;

    /* create a unix stream socket */
    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* assign the socket path */
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, socket_path);
    unlink(local.sun_path);

    len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(sfd, (struct sockaddr *)&local, len) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    /* qeue up to 1 connections, then reject */
    if (listen(sfd, 1) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    run(sfd);
}

void
run(int sfd)
{
    /* wait for the remote connection(s) */
    while (1) {
        struct sockaddr_un remote;
        socklen_t t;
        cmd_t command;

        printf("fclient: >> Waiting for a connection... <<\n");
        t = sizeof(remote);

        int cfd = accept(sfd, (struct sockaddr *)&remote, &t);
        if (cfd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("fclient: >> Socket Connected <<\n");

        /* loop to receive data/commands */
        while (1) {
            /* wait to receive a command */
            ssize_t ret = recv(cfd, command, sizeof(command), 0);

            if (ret == 0)
                break;
            else if (ret == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            command[ret] = '\0';
            printf("COMMAND> %s\n", command);

            /* clear last status */
            cmd_t mstatus;
            mstatus[0] = 0;
            /* strcpy(mstatus, ""); */

            /* parse the command */
            if (!parse_command(command, mstatus)) {
                printf("command was parsed with status: %s\n", mstatus);

                /* Compute new biquad (callback) */
                biquad_init(filter, &ctrls);

                printf("printing control list\n");
                printf("fc=%f, g=%f, bw=%f \n", ctrls.fc, ctrls.gain, ctrls.bw);
                /* printf("printing new coefficients:\n"); */
                /* printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n\n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2); */
            }

            /* transmit response */
            if (send(cfd, mstatus, ret, 0) < 0) {
                perror("send");
                exit(EXIT_FAILURE);
            }

            /* clear the command */
            /* for (i = 0; i < n; i++) { */
                /* command[i] = '\0'; */
            /* } */
        }

        close(cfd);
    }
}

/**
 * This is the control message parser for this filter client.
 * It is called by whenever a valid message requesting filter
 * parameter changes is received by the messenger thread
 * INPUTS:  command string, controls list, status message string
 * OUTPUTS: int
 */
int
parse_command(char *command, char *statusmessage)
{
    char *control, *value;
    char *saveptr, *endptr;
    char *type;
    smp_t fpval;

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
    if (STREQ(control, "type")) {
        type = value;
        strcpy(statusmessage, "filter type change success");

        /* determine the filter type */
        if (STREQ(type, "lpf")) {
            printf("requested an lpf\n");
            ctrls.type = FILTER_LOW_PASS;
        } else if (STREQ(type, "hpf")) {
            printf("requested an hpf\n");
            ctrls.type = FILTER_HIGH_PASS;
        } else if (STREQ(type, "bpf")) {
            printf("requested an bpf\n");
            ctrls.type = FILTER_BAND_PASS;
        } else if (STREQ(type, "notch")) {
            printf("requested a notch filter\n");
            ctrls.type = FILTER_NOTCH;
        } else if (STREQ(type, "peq")) {
            printf("requested a peaking eq\n");
            ctrls.type = FILTER_PEAKING_BAND;
        } else if (STREQ(type, "lsh")) {
            printf("requested a low shelf filter\n");
            ctrls.type = FILTER_LOW_SHELF;
        } else if (STREQ(type, "hsh")) {
            printf("requested a high shelf filter\n");
            ctrls.type = FILTER_HIGH_SHELF;
        } else {
            printf("requested an unsupported filter type\n");
            strcpy(statusmessage, "");
            strcpy(statusmessage, "filter type change failed");
        }
    } else {
        /* convert str to double */
        fpval = strtod(value, &endptr);

        /* if ((errno == ERANGE && (fpval == HUGE_VAL)) */
        /*         || (errno != 0 && fpval == 0)) { */
        /*     perror("strtod"); */
        /*     return -1; */
        /* } */

        if (endptr == value) {
            fprintf(stderr, "No digits were found\n");
            return -1;
        }

        printf("(control,value) = (%s,%f)\n",control, fpval);

        if (*endptr != '\0')        /* Not necessarily an error... */
            printf("Further characters after number: %s\n", endptr);

        /* check which control */
        if (STREQ(control, "fc")) {
            ctrls.fc = fpval;
            strcpy(statusmessage, "cut off frequency change success");
        } else if (STREQ(control, "g")) {
            ctrls.gain = fpval;
            strcpy(statusmessage, "gain change success");
        } else if (STREQ(control, "bw")) {
            ctrls.bw = fpval;
            strcpy(statusmessage, "bandwith change success");
        } else {
            printf("control parameter requested not found in list!\n");
            strcpy(statusmessage, "no control in list");
            return -1;
        }
    }
    return 0;
}

/* JACK {{{ */
void *
jack(void UNUSED *arg)
{
    const char *client_name = "filter";
    const char *server_name = NULL;
    jack_options_t options = JackNoStartServer | JackUseExactName | JackSessionID;
    jack_status_t status;

    /* open a client connection to the JACK server */
    client = jack_client_open(client_name, options, &status, server_name);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open() failed, "
                "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf (stderr, "Unable to connect to JACK server\n");
        }
        exit(EXIT_FAILURE);
    }
    if (status & JackServerStarted) {
        fprintf(stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf(stderr, "unique name `%s' assigned\n", client_name);
    }

    /* define callbacks */
    jack_set_process_callback(client, jack_process, 0);
    jack_on_shutdown(client, jack_shutdown, 0);

    /* display the current sample rate.  */
    printf("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate (client));

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

    ctrls.fs = jack_get_sample_rate(client),
    filter = biquad_new(&ctrls);

    /* printf("initial coefficients:\n"); */
    /* printf("b0=%f, b1=%f, b2=%f, a1=%f, a2=%f \n", filter->b0, filter->b1, filter->b2, filter->a1, filter->a2); */

    /* Tell the JACK server that we are ready to roll.  Our
    * process() callback will start running now. */
    if (jack_activate(client)) {
        fprintf(stderr, "cannot activate client");
        exit(EXIT_FAILURE);
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

    sleep(-1);

    /* This code should never run */
    jack_client_close(client);
    exit(EXIT_FAILURE);
}

/****************************************************
 * The process callback for this JACK application
 * (i.e. the audio workhorse function)
 * It is called by JACK at the appropriate times
 * Optimize this function for time!
****************************************************/
int
jack_process(jack_nframes_t nframes, void UNUSED *arg)
{
    /* consider passing the filter struct */
    //biquad *filter = arg[0];

    sample_t *out = (sample_t *)jack_port_get_buffer(output_port, nframes);
    sample_t *in  = (sample_t *)jack_port_get_buffer(input_port, nframes);
    smp_t outsample;

    for (size_t i = 0; i < nframes; i++) {
        /* compute the output sample */
        outsample = df1((smp_t)in[i], filter);
        out[i] = (sample_t)outsample;
    }


    /* bypass switch */
    /* if ( STATE == ON) { */
    /*     memcpy (out, in, sizeof(sample_t) * nframes); */
    /* } */

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
jack_shutdown(void UNUSED *arg)
{
    exit(EXIT_FAILURE);
}
/* }}} */

int
main(void)
{
    pthread_t audio;

    /* create the audio engine thread */
    if ((pthread_create(&audio, NULL, jack, NULL))) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    listener();

    pthread_join(audio, NULL);
    return 0;
}

// vim: et:sts=4:sw=4:cino=(0
