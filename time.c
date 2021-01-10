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

//Gesione dei messaggi
#include "./util/msg_time.h"
#include "./util/retr_time.h"
#include "./util/peer_file.h"

#define DATE_LEN 10
#define TIME_LEN 8
#define MESS_TYPE_LEN 8
#define MAX_ENTRY_UPDATE 630 //Header, numero peer e lunghezza massima entry (lunghezza a 5 cifre di 99 peer con virgola, orario, tipo, numero)

extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];

//Variabili
int my_port;
int server_port;
int time_socket;
struct sockaddr_in time_addr;
socklen_t time_addr_len;

int daily_flag;
int i;
char* pointer;

int peer_port;

int tot[2];

char recv_buffer[MAX_ENTRY_UPDATE];
char header_buffer[MESS_TYPE_LEN];
int entries;

int main(int argc, char** argv){
    my_port = atoi(argv[1]);
    server_port = atoi(argv[2]);
    daily_flag = 0;
    //Creazione socket di ascolto
    time_socket = prepare(&time_addr, &time_addr_len, my_port);

    //Invio al server un messaggio di hello
    send_UDP(time_socket, "TM_HELLO", MESS_TYPE_LEN, server_port, "TMHL_ACK");

    while(1){
        printf("Inizio\n");
        if(daily_flag == 1){
            daily_flag = 0;
            //Fa ripartire tutti i peer
            i=0;
            peer_port = get_port(i);
            while(peer_port != -1){
                send_UDP(time_socket, "FLAG_RST", MESS_TYPE_LEN, peer_port, "FRST_ACK");
                peer_port = get_port(++i);
            }

        }

        else {
            retrieve_time();
            pointer = strstr(current_t, "17:5");
            if(pointer != NULL){
                daily_flag = 1;

                //Primo ciclo: blocca tutti i peer
                i=0;
                peer_port = get_port(i);
                while(peer_port != -1){
                    send_UDP(time_socket, "FLAG_SET", MESS_TYPE_LEN, peer_port, "FSET_ACK");
                    peer_port = get_port(++i);
                }

                //Aspetta il completamento di eventuali operazioni in corso
                sleep(2);

                //Secondo ciclo: prende le entries
                i=0;
                peer_port = get_port(i);
                while(peer_port != -1){
                    int j=0;
                    send_UDP(time_socket, "FLAG_REQ", MESS_TYPE_LEN, peer_port, "FREQ_ACK");
                    //Ricevo il numero di entries che quel peer mi deve mandare
                    recv_UDP(time_socket, recv_buffer, MAX_ENTRY_UPDATE, peer_port, "FLAG_NUM", "FNUM_ACK");
                    sscanf(recv_buffer, "%s %d", header_buffer, &entries);
                    //Ricevo ogni entry
                    while(j < entries){  
                        recv_UDP(time_socket, recv_buffer, MAX_ENTRY_UPDATE, peer_port, "FLAG_ENT", "FENT_ACK");
                        printf("Entry ricevuta: %s\n", recv_buffer+9);
                        j++;
                        insert_temp(recv_buffer+9);
                    }

                    peer_port = get_port(++i);
                }
                printf("Raccolte tutte le entries\n");

                //Calcola gli aggregati giornalieri
                tot[0] = sum_entries('t');
                tot[1] = sum_entries('n');

                printf("Totali: %d e %d\n", tot[0], tot[1]);

                i = sprintf(recv_buffer, "%s %d %d", "FLAG_TOT", tot[0], tot[1]);
                recv_buffer[i] = '\0';

                //Li invia a tutti i peer
                i=0;
                peer_port = get_port(i);
                while(peer_port != -1){
                    send_UDP(time_socket, recv_buffer, strlen(recv_buffer), peer_port, "FTOT_ACK");
                    peer_port = get_port(++i);
                }

            }

            printf("Uscito dal while\n");
        }
        
        sleep(600);
    }

    return 0;
}
