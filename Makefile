OUT = filterd
SRC = ${wildcard *.c}
OBJ = ${SRC:.c=.o}

CFLAGS:=-std=gnu99 \
	-Wall -Wextra -pedantic \
	-I../tunerlib/include \
	${CFLAGS}

LDFLAGS:=-ljack -lm -lpthread \
	-L../tunerlib/ -ltuner \
	${LDFLAGS}

${OUT}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	${RM} ${OUT} ${OBJ}

.PHONY: clean install uninstall
