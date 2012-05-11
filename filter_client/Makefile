########################################
CC = gcc					# compiler
CFLAGS = -Wall -pedantic -std=gnu99 -g		# compiler flags
LDFLAGS = -ljack -lm -pthread			# linker (ld) flags

PROG = filter_client 				# final executable name
PROG2 = commander
OBJ = filter_client.o biquad_df1.o 		# object/source files to be created/compiled
OBJ2 = commander.o
########################################

all: 	$(PROG) $(PROG2)

$(PROG): $(OBJ)
	@echo CC -o $@
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(PROG2): $(OBJ2)
	@echo CC -o $@
	@$(CC) -o $@ $(OBJ2) $(LDFLAGS)

%.o: 	%.c
	@echo CC $@
	@$(CC) -o $@ -c $(CFLAGS) $<

clean:
	@echo cleaning up...
	@rm *.o $(PROG) $(PROG2)

run:
	@./$(PROG) 

mem:
	@valgrind --leak-check=yes ./$(PROG)
 
	
.PHONY: all clean
