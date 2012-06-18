OUT = filterd
SRC = ${wildcard *.c}
OBJ = ${SRC:.c=.o}

CFLAGS:=-std=gnu99 \
	-Wall -Wextra -pedantic \
	-Ilib/include \
	${CFLAGS}

LDFLAGS:=-ljack -lm -lpthread \
	-Llib -ltuner \
	${LDFLAGS}

${OUT}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	${RM} ${OUT} ${OBJ}

.PHONY: clean install uninstall
