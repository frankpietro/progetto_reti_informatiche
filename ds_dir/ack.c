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

/*
    Utilizzo: quando viene inviato il primo messaggio di uno scambio UDP

    socket: socket di chi invia
    buffer: messaggio da inviare
    buff_l: lunghezza del messaggio da inviare
    recv_addr: puntatore a struttura con indirizzo del ricevente (passare &peer_addr)
    recv_l: lunghezza struttura con indirizzo ricevente
    readset_p: puntatore a set di descrittori in lettura (passare &readset)
    acked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che sia l'ack atteso
*/
void ack_1(int socket, char* buffer, int buff_l, struct sockaddr_in* recv_addr, socklen_t recv_l, fd_set* readset_p, char* acked){
    int sent,ret;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[MESS_TYPE_LEN+1];
    struct timeval util_tv;

    sent = 0;
    ret = 0;
    util_len = sizeof(util_addr);

    while(!sent){
        //Invio lista
        do {
            ret = sendto(socket, buffer, buff_l, 0, (struct sockaddr*)recv_addr, recv_l);
        } while(ret<0);
        
        //Mi preparo a ricevere un messaggio dal peer
        FD_ZERO(readset_p);
        FD_SET(socket, readset_p);
        //Imposto il timeout a un secondo
        util_tv.tv_sec = 1;
        util_tv.tv_usec = 0;
        //Aspetto la lista
        ret = select(socket+1, readset_p, NULL, NULL, &util_tv);

        //Appena la ricevo
        if(FD_ISSET(socket, readset_p)){
            //Ricevo ack
            ret = recvfrom(socket, recv_buffer, MESS_TYPE_LEN+1, 0, (struct sockaddr*)&util_addr, &util_len);
            
            //Se ho ricevuto effettivamente l'ack
            if(util_addr.sin_port == recv_addr->sin_port && util_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr && strcmp(acked, recv_buffer) == 0){
                //Il peer ha ricevuto sicuramente la lista
                printf("Ho ricevuto %s da %d\n", recv_buffer, ntohs(util_addr.sin_port));
                sent = 1;
            }
            //Ignoro qualunque altro messaggio
            else {
                sent = 0;
                printf("Arrivato un messaggio %s inatteso da %d mentre attendevo %s da %d, scartato\n", recv_buffer, ntohs(util_addr.sin_port), acked, ntohs(recv_addr->sin_port));
            }

            FD_CLR(socket, readset_p);
        }
    }

    printf("Messaggio inviato correttamente\n");
}

/*
    Utilizzo: quando viene inviato il secondo messaggio di uno scambio UDP

    socket: socket di chi invia
    buffer: messaggio da inviare
    buff_l: lunghezza del messaggio da inviare
    recv_addr: puntatore a struttura con indirizzo del ricevente (passare &peer_addr)
    recv_l: lunghezza struttura con indirizzo ricevente
    readset_p: puntatore a set di descrittori in lettura (passare &readset)
    unacked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che non ci sia un doppio invio
*/
void ack_2(int socket, char* buffer, int buff_l, struct sockaddr_in* recv_addr, socklen_t recv_l, fd_set* readset_p, char* unacked){
    int received, ret;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[MESS_TYPE_LEN];
    struct timeval util_tv;

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
        //Solo in ascolto di un'eventuale copia del messaggio di boot
        FD_ZERO(readset_p);
        FD_SET(socket, readset_p);
            
        ret = select(socket+1, readset_p, NULL, NULL, &util_tv);

        //Se arriva qualcosa
        if(FD_ISSET(socket, readset_p)){
            //Leggo cosa ho ricevuto
            ret = recvfrom(socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&util_addr, &util_len);
            //Se ho ricevuto lo stesso identico messaggio
            if(util_addr.sin_port == recv_addr->sin_port && util_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr && (strcmp(recv_buffer, unacked)==0)){
                //Riinvio la lista al peer tornando a inizio while(!received)
                received = 0;
                break;
            }
            //Se ho ricevuto un messaggio diverso lo scarto (il peer lo rimandera') e considero arrivato correttamente l'altro
            else {
                received = 1;
                printf("Arrivato un messaggio %s inatteso, scartato\n", recv_buffer);
            }

            FD_CLR(socket, readset_p);
        }
        //Se per due secondi non arriva nulla, considero terminata con successo l'operazione
        else
            received = 1;
    }

    printf("Messaggio ricevuto correttamente\n");
}
