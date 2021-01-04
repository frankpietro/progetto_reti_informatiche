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

//Elenco dei comandi disponibili
void comandi(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comando e significato\n");
    printf("showpeers --> mostra elenco peer connessi alla rete\n");
    printf("showneighbor [peer] --> mostra i neighbor di un peer o di tutti i peer se non viene specificato alcun parametro\n");
    printf("esc --> termina il DS\n");
}

int prepare(struct sockaddr_in* addr_p, socklen_t* len_p, int port){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    memset(addr_p, 0, sizeof((*addr_p)));
	addr_p->sin_family = AF_INET;
	addr_p->sin_port = htons(port);
	inet_pton(AF_INET, LOCALHOST, &addr_p->sin_addr);
    
    (*len_p) = sizeof((*addr_p));

    return sock;
}