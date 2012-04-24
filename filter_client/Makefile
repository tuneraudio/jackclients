########################################
CC = gcc					# compiler
CFLAGS = -Wall -pedantic -std=gnu99 -g		# compiler flags
LDFLAGS = -ljack -lm -pthread			# linker (ld) flags

PROG = filter_client				# final executable name
OBJ = filter_client.o biquad_df1.o		# object/source files to be created/compiled
########################################

all: 	$(PROG)

$(PROG): $(OBJ)
	echo CC -o $@
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

%.o: 	%.c
	@echo CC $@
	@$(CC) -o $@ -c $(CFLAGS) $<

clean:
	@echo cleaning up...
	@rm *.o $(PROG)

run:
	@./$(PROG) 

mem:
	@valgrind --leak-check=yes ./$(PROG)
 
	
.PHONY: all clean
