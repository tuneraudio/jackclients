#ifndef FILTERD_H
#define FILTERD_H

const char *socket_path = "filter";

#define BUFF_SIZE (1 << 6)
typedef char cmd_t[BUFF_SIZE];

#endif
