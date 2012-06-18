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

${OUT}: lib ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

client lib:
	${MAKE} -C $@

clean:
	${RM} ${OUT} ${OBJ}

.PHONY: clean lib client install uninstall
