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

void clear_address(struct sockaddr_in* addr_p, socklen_t* len_p, int port){
    memset(addr_p, 0, sizeof((*addr_p)));
	addr_p->sin_family = AF_INET;
	addr_p->sin_port = htons(port);
	inet_pton(AF_INET, LOCALHOST, &addr_p->sin_addr);
    
    (*len_p) = sizeof((*addr_p));\
}

int prepare(struct sockaddr_in* addr_p, socklen_t* len_p, int port){
    int ret;
    int sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    clear_address(addr_p, len_p, port);

    ret = bind(sock, (struct sockaddr*)addr_p, (*len_p));
    if(ret<0){
        perror("Error while binding");
        exit(0);
    }

    return sock;
}