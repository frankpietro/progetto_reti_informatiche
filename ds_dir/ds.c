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

//Funzioni di utilita'
#include "util.h"
//Gestione del file con i peer
#include "peer_file.h"
//Gesione degli ack
#include "ack.h"

#define MAX_COMMAND 30
#define MESS_TYPE_LEN 8
#define LIST_MAX_LEN 21
#define LOCALHOST "127.0.0.1"

int main(int argc, char** argv){
    //Variabili
    int ds_port;    //Porta del server, passata come argomento del main
    
    int server_socket;  //Socket su cui il server riceve messaggi dai peer
    struct sockaddr_in server_addr;     //Struttura per gestire il socket
    socklen_t server_len;
    
    int ret;    //Variabile di servizio per funzioni che ritornano un intero di controllo
    char command_buffer[MAX_COMMAND];   //Buffer su cui salvare i comandi provenienti da stdin
    
    int connected_peers;    //Numero di peers connessi alla rete
    
    //Variabili per gestire comunicazione coi peer
    struct sockaddr_in network_peer;
    socklen_t peer_addr_len;

    char recv_buffer[MESS_TYPE_LEN+1]; //Buffer su cui ricevere messaggio di richiesta connessione

    //Gestione input da stdin oppure da socket
    fd_set master;
    fd_set readset;
    int fdmax;
    
    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);

    //Creazione socket di ascolto
    ds_port = atoi(argv[1]);
    server_socket = prepare(&server_addr, &server_len, ds_port);/*socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ds_port);
	inet_pton(AF_INET, LOCALHOST, &server_addr.sin_addr);*/
    ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret<0){
        perror("Error while binding");
        exit(0);
    }
    
    //All'inizio nessun peer connesso
    connected_peers = 0;

    //Inizializzo set di descrittori
    FD_SET(server_socket, &master);
    fdmax = server_socket;
    FD_SET(0, &master);


    comandi();
    
    while(1){
        //printf("Inizio\n");
        readset = master;
        //Controllo se c'e' qualcosa pronto
        ret = select(fdmax+1, &readset, NULL, NULL, NULL);


        if(FD_ISSET(server_socket, &readset)){
            printf("Arrivato messaggio sul socket\n");
            //Lunghezza della struttura in cui salvare le info del peer
            peer_addr_len = sizeof(network_peer);
            
            //Variabili per salvare porta e indirizzo
            char peer_addr_buff[INET_ADDRSTRLEN];
            int peer_port;
            
            //Variabili per gestire la consistenza della comunicazione UDP
            struct sockaddr_in util_addr;
            socklen_t util_len;
            struct timeval util_tv;

            util_len = sizeof(util_addr);
            
            //Ricezione richieste di connessione
            ret = recvfrom(server_socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&network_peer, &peer_addr_len);
            

            if(ret<0){
                perror("Errore di ricezione");
                //Uscita
                FD_CLR(server_socket, &readset);
                continue;
            }

            //Porta scritta in formato normale
            peer_port = ntohs(network_peer.sin_port);
            //IP scritto come stringa
            inet_ntop(AF_INET, &network_peer.sin_addr, peer_addr_buff, INET_ADDRSTRLEN);

            recv_buffer[MESS_TYPE_LEN] = '\0';

            //Messaggi che puo' ricevere il server: richiesta di connessione o di abbandono

            //Richiesta di connessione
            if(strcmp(recv_buffer, "CONN_REQ") == 0){
                int temp_port[2]; //Variabili per salvare eventuali vicini
                char list_buffer[LIST_MAX_LEN]; //Buffer per invio liste al peer                
                int n; //Variabile per la lunghezza del messaggio da inviare al peer             
                char list_update_buffer[LIST_MAX_LEN]; //Liste da inviare ai peer a cui e' cambiata la lista dei vicini            
                struct sockaddr_in temp_peer_addr; //Struttura di supporto per inviare liste nuove
                
                printf("Arrivata richiesta di connessione dal peer %d\n", peer_port); 
                
                //Inserisco il peer nella lista
                if(!isIn(peer_port)){
                    ret = insert_peer(peer_addr_buff, peer_port, connected_peers);
                    if(ret<0){
                        printf("Impossibile inserire il peer\n");
                        //Uscita
                        FD_CLR(server_socket, &readset);
                        continue;
                    }
                }

                get_neighbors(peer_port, connected_peers+1, &temp_port[0], &temp_port[1]);
                printf("Vicini: %d e %d\n", temp_port[0], temp_port[1]);
                //Compongo la lista
                if(temp_port[0] == -1 && temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s", "NBR_LIST");
                else if(temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s %d", "NBR_LIST", temp_port[0]);
                else
                    n = sprintf(list_buffer, "%s %d %d", "NBR_LIST", temp_port[0], temp_port[1]);

                // DEBUG
                printf("List buffer: %s (lungo %d byte)\n", list_buffer, n);
                //Invio
                ack_2(server_socket, list_buffer, n+1, &network_peer, peer_addr_len, &readset, "CONN_REQ");

                //Invio eventuale lista aggiornata al primo peer
                if(temp_port[0] != -1){
                    //Preparo la struttura che devo tirare su
                    clear_address(&temp_peer_addr, &peer_addr_len, temp_port[0]);
                    get_list(temp_port[0], connected_peers, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[0]);
                    ack_1(server_socket, list_update_buffer, n+1, &temp_peer_addr, peer_addr_len, &readset, "CHNG_ACK");
                }

                if(temp_port[1] != -1){
                    clear_address(&temp_peer_addr, &peer_addr_len, temp_port[1]);
                    get_list(temp_port[1], connected_peers, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[1]);
                    ack_1(server_socket, list_update_buffer, n+1, &temp_peer_addr, peer_addr_len, &readset, "CHNG_ACK");
                }

                //Incremento il numero di peer
                connected_peers++;
            }

            if(strcmp(recv_buffer, "CLT_EXIT") == 0){
                //Vettore per salvare porte temporanee
                int temp_nbr_port[4];
                char list_update_h[MESS_TYPE_LEN+1] = "NBR_UPDT\0";
                char list_update[LIST_MAX_LEN];
                int n;
                int received = 0;
                
                //Per inviare le liste aggiornate ai peer
                struct sockaddr_in temp_peer_addr;
                socklen_t temp_peer_addr_len;

                memset(&temp_peer_addr, 0, sizeof(temp_peer_addr));
                temp_peer_addr.sin_family = AF_INET;
                inet_pton(AF_INET, LOCALHOST, &temp_peer_addr.sin_addr);
                temp_peer_addr_len = sizeof(temp_peer_addr);

                printf("Ricevuto messaggio di richiesta di uscita da %d\n", peer_port);
                //Se il peer per qualche motivo non e' in lista non faccio nulla
                if(!isIn(peer_port)){
                    printf("Peer %d non presente nella lista dei peer connessi. Uscita\n", peer_port);
                    FD_CLR(server_socket, &readset);
                    continue;
                }

                //4 casi da gestire (si suppone che i dati a questo punto siano consistenti)
                switch(connected_peers){
                    case 1:
                        //Se l'ultimo peer chiede di uscire, cancello il file e via
                        printf("Elimino l'ultimo peer dalla rete\n");
                        remove("peer_addr.txt");
                        break;
                    case 2:
                        printf("Elimino il penultimo peer dalla rete. Aggiorno la lista di vicini dell'ultimo\n");
                        remove_peer(peer_port);
                        
                        //Prendo numero di porta dell'ultimo peer e gli invio la lista vuota
                        temp_nbr_port[0] = get_port(0);
                        

                        break;

                    case 3:
                        break;

                    default:

                        break;
                }

                
            }

            FD_CLR(server_socket, &readset);
            
        }

        //Gestione comandi da stdin
        if(FD_ISSET(0, &readset)) {
            //Parsing dell'input
            int neighbor_peer;
            int input_number;
            char command[MAX_COMMAND];
            
            fgets(command_buffer, MAX_COMMAND, stdin);
            input_number = sscanf(command_buffer, "%s %d", command, &neighbor_peer);
            if(strcmp(command,"help\0")==0){
                comandi();
            }

            else if(strcmp(command,"showpeers\0")==0){
                print_peers(connected_peers);
            }

            else if(strcmp(command,"showneighbor\0")==0){
                if(connected_peers == 0)
                    printf("Nessun peer connesso\n");
                else if(input_number == 2)
                    print_single_neighbor(connected_peers, neighbor_peer);
                else
                    print_all_neighbors(connected_peers);
            }
            
            else if(strcmp(command,"esc\0")==0){
                
                //DEBUG
                printf("Hai digitato comando esc\n");

                //Stacca tutti i peer
                while(connected_peers){
                    char exit[MESS_TYPE_LEN+1] = "SRV_EXIT\0"; //Messaggio da inviare
                    char exit_ack_buffer[MESS_TYPE_LEN]; //Messaggio da ricevere
                    struct sockaddr_in temp_peer_addr; //Struttura in cui salvare indirizzo del peer a cui volta volta inviare o da cui ricevere
                    socklen_t temp_peer_addr_len; //Lunghezza della struttura sopra
                    int removed = 0;
                    //Invia a tutti i peer un messaggio
                    int i;
                    //DEBUG
                    printf("Invio serie di messaggi SRV_EXIT\n");
                    for(i=0; i<connected_peers; i++){
                        //Invia al peer il messaggio di exit
                        clear_address(&temp_peer_addr, &temp_peer_addr_len, get_port(i));
                        printf("Invio SRV_EXIT a %d\n", get_port(i));

                        ack_1(server_socket, exit, MESS_TYPE_LEN+1, &temp_peer_addr, &temp_peer_addr_len, &readset, "ACK_S_XT");
/*
                        do {
                            ret = sendto(server_socket, exit, MESS_TYPE_LEN+1, 0, (struct sockaddr*)&temp_peer_addr, temp_peer_addr_len);
                        } while(ret<0);

                        FD_ZERO(&readset);
                        FD_SET(server_socket, &readset);
                        //Attesa: un secondo
                        util_tv.tv_sec = 1;
                        util_tv.tv_usec = 0;

                        ret = select(server_socket+1,&readset, NULL, NULL, &util_tv);

                        //Se arriva un ACK della chiusura del server
                        if(FD_ISSET(server_socket, &readset)){
                            ret = recvfrom(server_socket, exit_ack_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&temp_peer_addr, &temp_peer_addr_len);
                            int del_port = ntohs(temp_peer_addr.sin_port);
                            printf("Messaggio dal peer %d: %s\n", del_port, exit_ack_buffer);
                            if(strcmp(exit_ack_buffer, "ACK_S_XT")==0 && del_port == focus_port){
                                //Elimino quel peer e ricomincio da capo
                                //DEBUG
                                printf("ACK_S_XT ricevuto da %d\n", del_port);

                                if(isIn(del_port)){
                                    removed++;
                                    printf("Peer %d rimosso dalla rete\n", del_port);
                                }
                                //Se arriva ack duplicato, lo ignoro
                                else
                                    printf("Rimozione del peer %d fallita", del_port);
                            }
                            else {
                                printf("Arrivato pacchetto non di ack di chiusura, ignorato\n");
                            }

                            FD_CLR(server_socket, &readset);
                        
                        }*/

                        removed++;

                    }
                    
                    //Aggiorno il numero di peer connessi (dovrebbe essere 0)
                    connected_peers -= removed;

                }

                //Cancello il file con la lista di peer
                remove("peer_addr.txt");

                close(server_socket);
                _exit(0);
            }
            
            else {
                printf("Errore, comando non esistente\n");
            }

            FD_CLR(0, &readset);
        }

    }

    return 0;
}