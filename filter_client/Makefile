OUT=filter_command commander

SRC=${wildcard *.c}
OBJ=${SRC:.c=.o}
PREFIX?=/usr/local

CFLAGS:=-std=gnu99 \
	-Wall -Wextra -pedantic \
	${CFLAGS}

LDFLAGS:=-ljack -lm -lpthreads ${LDFLAGS}

${OUT}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	${RM} ${OUT} ${OBJ}


.PHONY: clean install uninstall
