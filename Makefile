CC=gcc
BIN=./bin
CFLAGS=-g -Wall -Wextra -Wshadow -Wconversion -Wunreachable-code
LDLIBS=-lncurses -lpthread -lrt

PROG=nave estacion servidor

# Genera la lista: ./bin/nave ./bin/estacion ./bin/servidor
LIST=$(addprefix $(BIN)/, $(PROG))

.PHONY: all
all: $(LIST)

# REGLAS EXPLICITAS PARA EVITAR QUE MAKE IGNORE LAS LIBRERIAS
$(BIN)/nave: nave.c comun.h
	$(CC) -o $@ nave.c $(CFLAGS) $(LDLIBS)

$(BIN)/estacion: estacion.c comun.h
	$(CC) -o $@ estacion.c $(CFLAGS) $(LDLIBS)

$(BIN)/servidor: servidor.c comun.h
	$(CC) -o $@ servidor.c $(CFLAGS) $(LDLIBS)

test:
	@./test.sh ||:

.PHONY: clean
clean:
	rm -f $(LIST)

zip:
	git archive --format zip --output ${USER}-lab04.zip HEAD

html:
	pandoc -o README.html -f gfm README.md