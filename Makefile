#------constantes--------------------------------
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread  # -pthread obligatoire pour le multi-threading du serveur final
SRC = src
BIN = bin
LIB = -lws2_32 #uniquement pour windows, afin de lier la bib windows au moment de la compile

#all compile les fichiers permettant de lancer une paire client/serveur sur linux : ne compile pas le gateway ni le client windows qui doivent être compilés sur leurs architectures finales
all: directories clean serveur client_linux

#fabrication du directory s'il n'existe pas
directories:
	mkdir -p $(BIN)

#supprime tous les binaries du dossier BIN
clean:
	rm -rf $(BIN)/*

#------------- serveurs ----------------------------------
serveur: $(SRC)/serveur_main.c $(SRC)/serveur_web.c
	$(CC) $(CFLAGS) -o $(BIN)/serveur $(SRC)/serveur_main.c $(SRC)/serveur_web.c

# ------------- clients--------------------------
client_linux: $(SRC)/client_linux-v4.c $(SRC)/utils_gpu.c $(SRC)/utils_linux.c
	$(CC) $(CFLAGS) -o $(BIN)/client_linux $(SRC)/client_linux-v4.c $(SRC)/utils_gpu.c $(SRC)/utils_linux.c

client_windows : $(SRC)/client_windows-v2.c $(SRC)/utils_gpu.c $(SRC)/utils_windows.c
	$(CC) $(CFLAGS) -o $(BIN)/client_windows $(SRC)/client_windows-v2.c $(SRC)/utils_gpu.c $(SRC)/utils_windows.c $(LIB)

client_gateway : $(SRC)/client_gateway.c $(SRC)/utils_linux.c $(SRC)/utils_serial.c
	$(CC) $(CFLAGS) -o $(BIN)/client_gateway $(SRC)/client_gateway.c $(SRC)/utils_linux.c $(SRC)/utils_serial.c