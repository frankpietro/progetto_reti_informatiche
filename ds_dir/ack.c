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

//int ack_1(){}

void ack_2(int socket, char* buffer, int buff_l, struct sockaddr_in* recv_addr, socklen_t recv_l, fd_set* readset_p, char* unacked){
    int received, ret;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[8];
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
            ret = recvfrom(socket, recv_buffer, 8, 0, (struct sockaddr*)&util_addr, &util_len);
            //Se ho ricevuto lo stesso identico messaggio
            if(util_addr.sin_port == recv_addr->sin_port && util_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr && (strcmp(recv_buffer, unacked)==0)){
                //Riinvio la lista al peer tornando a inizio while(!received)
                received = 0;
                break;
            }
            //Se ho ricevuto un messaggio diverso lo scarto (il peer lo rimandera')
            else
                received = 1;
            
            FD_CLR(socket, readset_p);
        }
        //Se per due secondi non arriva nulla, considero terminata con successo l'operazione
        else
            received = 1;
    }

    printf("Messaggio ricevuto correttamente\n");
}