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
#include "./util/util_t.h"
#include "./util/retr_time.h"
#include "./util/peer_file.h"
#include "./util/msg.h"

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

//Salva numero di porta di ogni peer a cui scrivere
int peer_port;

//Totale degli aggregati
int tot[2];

//Gestione socket
fd_set master;
fd_set readset;
int fdmax;
struct timeval timeout;

//Buffer per scambi di messaggi
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

    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);
    FD_SET(time_socket, &master);
    FD_SET(0, &master);
    fdmax = time_socket;
     
    while(1){
        printf("Inizio\n");
        //Ogni 10 minuti se non arrivano messaggi comunque eseguo le operazioni cicliche
        timeout.tv_sec = 600;
        timeout.tv_usec = 0;
        
        readset = master;
        select(fdmax+1, &readset, NULL, NULL, &timeout);

        if(FD_ISSET(time_socket, &readset)){
            int util_port;
            char mess_type_buffer[MESS_TYPE_LEN+1];

            util_port = s_recv_UDP(time_socket, recv_buffer, MAX_ENTRY_UPDATE);
            //Leggo il tipo del messaggio
            sscanf(recv_buffer, "%s", mess_type_buffer);
            mess_type_buffer[MESS_TYPE_LEN] = '\0';
            printf("Ricevuto messaggio %s da %d sul socket\n", recv_buffer, util_port);
            
            if(util_port == server_port){
                ack_UDP(time_socket, "S_XT_ACK", server_port, recv_buffer, strlen(recv_buffer));
                printf("Chiusura time server/n");
                close(time_socket);
                _exit(0);
            }

            if(strcmp(mess_type_buffer, "AGGR_GET") == 0){
                char bound[2][DATE_LEN+1];
                ack_UDP(time_socket, "AGET_ACK", util_port, recv_buffer, strlen(recv_buffer));
                printf("Ricevuto messaggio %s\n", recv_buffer);
                sscanf(recv_buffer+9,"%s %s", bound[0], bound[1]);
                bound[0][DATE_LEN] = '\0';
                bound[1][DATE_LEN] = '\0';
                //Per i prossimi messaggi riciclo recv_buffer e uso i come variabile di servizio
                i = sprintf(recv_buffer, "%s %d", "AGGR_CNT", count_aggr_entries(bound[0], bound[1]));
                recv_buffer[i] = '\0';
                send_UDP(time_socket, recv_buffer, i, util_port, "ACNT_ACK");

                //Invio tutte le entrate al peer che le ha chieste
                send_entries(util_port);

                remove("temp_send.txt");

            }

        }


        //Dopo la ricezione del messaggio eseguo comunque il controllo
        retrieve_time();
        pointer = strstr(current_t, "17:5");
        
        //Resetto i peer se flag settato e finito periodo di chiusura
        if(daily_flag == 1 && pointer == NULL){
            daily_flag = 0;
            //Fa ripartire tutti i peer
            i=0;
            peer_port = get_port(i);
            while(peer_port != -1){
                send_UDP(time_socket, "FLAG_RST", MESS_TYPE_LEN, peer_port, "FRST_ACK");
                peer_port = get_port(++i);
            }

        }

        else if(pointer != NULL && daily_flag == 0){
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

            register_tot(tot[0], tot[1]);

            i = sprintf(recv_buffer, "%s %d %d", "FLAG_TOT", tot[0], tot[1]);
            recv_buffer[i] = '\0';

            //Li invia a tutti i peer
            i=0;
            peer_port = get_port(i);
            while(peer_port != -1){
                send_UDP(time_socket, recv_buffer, strlen(recv_buffer), peer_port, "FTOT_ACK");
                peer_port = get_port(++i);
            }

            //Cancella il file delle entries giornaliere
            remove("entries.txt");

        }

        printf("Uscito dal while\n");
    
        
    }

    return 0;
}
