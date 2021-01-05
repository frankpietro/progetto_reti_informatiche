#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LOCALHOST "127.0.0.1"

//Elenco dei comandi disponibili lato server
void comandi_server(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("showpeers --> mostra elenco peer connessi alla rete\n");
    printf("showneighbor [peer] --> mostra i neighbor di un peer o di tutti i peer se non viene specificato alcun parametro\n");
    printf("esc --> termina il DS\n");
}

void comandi_client(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("start DS_addr DS_port --> richiede al Discovery Server connessione alla rete\n");
    printf("add type quantity --> aggiunge una entry nel registro del peer\n");
    printf("get aggr type [date1 date2] --> richiede un dato aggregato\n");
    printf("stop --> richiede disconnessione dalla rete\n");
}