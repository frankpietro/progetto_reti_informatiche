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

#define MESS_TYPE_LEN 8
#define MAX_RECV 40

#define LOCALHOST "127.0.0.1"

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

void ack_2(int socket, char* buffer, int buff_l, struct sockaddr_in* recv_addr, socklen_t recv_l, char* unacked){
    int received, ret;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[MESS_TYPE_LEN];
    struct timeval util_tv;
    fd_set readset;

    received = 0;
    ret = 0;
    util_len = sizeof(util_addr);

    printf("Inizio processo di ACK 2\n");

    while(!received){
        do {
            ret = sendto(socket, buffer, buff_l, 0, (struct sockaddr*)recv_addr, recv_l);
        } while(ret<0);

        //Attesa di un secondo
        util_tv.tv_sec = 1;
        util_tv.tv_usec = 0;
        //Mi metto per un secondo solo in ascolto sul socket
        //Solo in ascolto di un'eventuale copia del messaggio
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
            
        ret = select(socket+1, &readset, NULL, NULL, &util_tv);

        //Se arriva qualcosa
        if(FD_ISSET(socket, &readset)){
            //Leggo cosa ho ricevuto
            ret = recvfrom(socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&util_addr, &util_len);
            //Se ho ricevuto lo stesso identico messaggio
            if(util_addr.sin_port == recv_addr->sin_port && util_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr && (strcmp(recv_buffer, unacked)==0)){
                //Riinvio l'ack tornando a inizio while(!received)
                received = 0;
                break;
            }
            //Se ho ricevuto un messaggio diverso lo scarto (il peer lo rimandera') e considero arrivato correttamente l'altro
            else {
                received = 1;
                printf("Arrivato un messaggio %s inatteso, scartato\n", recv_buffer);
            }

            FD_CLR(socket, &readset);
        }
        //Se per un secondo non arriva nulla, considero terminata con successo l'operazione
        else
            received = 1;
    }

    printf("Messaggio ricevuto correttamente\n");
}

void recv_ack(int socket, char* buffer, int buff_l, struct sockaddr_in* recv_addr, socklen_t recv_l, char* acked){
    int sent,ret;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[MAX_RECV];
    char recv_buffer_h[MESS_TYPE_LEN+1];
    struct timeval util_tv;
    fd_set readset;

    sent = 0;
    ret = 0;
    util_len = sizeof(util_addr);


    while(!sent){
        //Invio lista
        do {
            ret = sendto(socket, buffer, buff_l, 0, (struct sockaddr*)recv_addr, recv_l);
        } while(ret<0);
        
        //Mi preparo a ricevere un messaggio dal peer
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
        //Imposto il timeout a un secondo
        util_tv.tv_sec = 1;
        util_tv.tv_usec = 0;
        //Aspetto la lista
        ret = select(socket+1, &readset, NULL, NULL, &util_tv);

        //Appena la ricevo
        if(FD_ISSET(socket, &readset)){
            //Ricevo ack
            ret = recvfrom(socket, recv_buffer, MESS_TYPE_LEN+1, 0, (struct sockaddr*)&util_addr, &util_len);

            ret = sscanf(recv_buffer, "%s", recv_buffer_h);
            
            //Se ho ricevuto effettivamente l'ack
            if(util_addr.sin_port == recv_addr->sin_port && util_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr && strcmp(acked, recv_buffer_h) == 0){
                //Il peer ha ricevuto sicuramente la lista
                printf("Ho ricevuto %s da %d\n", recv_buffer, ntohs(util_addr.sin_port));
                sent = 1;
            }
            //Ignoro qualunque altro messaggio
            else {
                sent = 0;
                printf("Arrivato un messaggio %s inatteso da %d mentre attendevo %s da %d, scartato\n", recv_buffer_h, ntohs(util_addr.sin_port), acked, ntohs(recv_addr->sin_port));
            }

            FD_CLR(socket, &readset);
        }
    }

    printf("Messaggio inviato correttamente\n");
}

/*
    Utilizzo: quando viene inviato il primo messaggio di uno scambio UDP

    socket: socket di chi invia
    buffer: messaggio da inviare
    buff_l: lunghezza del messaggio da inviare
    recv_port: porta del processo che deve ricevere il messaggio
    acked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che sia l'ack atteso
*/
void send_UDP(int socket, char* buffer, int buff_l, int recv_port, char* acked){
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len;

    clear_address(&recv_addr, &recv_addr_len, recv_port);

    recv_ack(socket, buffer,buff_l, &recv_addr, recv_addr_len, acked);
}

/*
    Utilizzo: quando viene inviato il secondo messaggio di uno scambio UDP

    socket: socket di chi invia
    buffer: messaggio da inviare
    buff_l: lunghezza del messaggio da inviare
    port: porta del processo a cui inviare ack
    unacked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che non ci sia un doppio invio
*/
void ack(int socket, char* buffer, int buff_l, int port, char* unacked){
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len;
    clear_address(&recv_addr, &recv_addr_len, port);
    ack_2(socket, buffer, buff_l, &recv_addr, recv_addr_len, unacked);
}

//Funzione per ricevere messaggi all'interno di una funzione
int recv_UDP(int socket, char* buffer, int buff_l){
    struct sockaddr_in send_addr;
    socklen_t send_addr_len;
    int ok;
    struct timeval util_tv;
    fd_set readset;

    ok = 0;
    send_addr_len = sizeof(send_addr);

    while(!ok){
        //Attesa di un secondo
        util_tv.tv_sec = 1;
        util_tv.tv_usec = 0;
        //Mi metto per un secondo solo in ascolto sul socket
        //Solo in ascolto di un'eventuale copia del messaggio di boot
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
            
        select(socket+1, &readset, NULL, NULL, &util_tv);

        //Se arriva qualcosa
        if(FD_ISSET(socket, &readset)){
            //Leggo cosa ho ricevuto
            recvfrom(socket, buffer, buff_l, 0, (struct sockaddr*)&send_addr, &send_addr_len);
            ok = 1;
            FD_CLR(socket, &readset);
        }
    }

    return ntohs(send_addr.sin_port);
}

int s_recv_UDP(int socket, char* buffer, int buff_l){
    struct sockaddr_in send_addr;
    socklen_t send_addr_len;

    send_addr_len = sizeof(send_addr);

    recvfrom(socket, buffer, buff_l, 0, (struct sockaddr*)&send_addr, &send_addr_len);

    return ntohs(send_addr.sin_port);
}