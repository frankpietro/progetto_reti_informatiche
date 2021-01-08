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
#include "./util/util_s.h"
//Gestione del file con i peer
#include "./util/peer_file.h"
//Gesione dei messaggi
#include "./util/msg.h"

#define MAX_COMMAND 30
#define MESS_TYPE_LEN 8
#define LIST_MAX_LEN 21
#define LOCALHOST "127.0.0.1"
#define MAX_CONNECTED_PEERS 100
#define SOCK_MAX_LEN 30
#define MAX_ENTRY_REP 16 //Non piu' di 1M di entries distinte

//Variabili
int server_socket;  //Socket su cui il server riceve messaggi dai peer
struct sockaddr_in server_addr;     //Struttura per gestire il socket
socklen_t server_len;

char command_buffer[MAX_COMMAND];   //Buffer su cui salvare i comandi provenienti da stdin
char socket_buffer[SOCK_MAX_LEN];
char recv_buffer[MESS_TYPE_LEN+1]; //Buffer su cui ricevere messaggio di richiesta connessione

int connected_peers;    //Numero di peers connessi alla rete

//Gestione input da stdin oppure da socket
fd_set master;
fd_set readset;
int fdmax;

int main(int argc, char** argv){
    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);

    //Creazione socket di ascolto
    server_socket = prepare(&server_addr, &server_len, atoi(argv[1]));
    
    //All'inizio nessun peer connesso
    connected_peers = 0;

    //Inizializzo set di descrittori
    FD_SET(server_socket, &master);
    fdmax = server_socket;
    FD_SET(0, &master);


    comandi_server();
    
    while(1){
        //printf("Inizio\n");
        readset = master;
        //Controllo se c'e' qualcosa pronto
        select(fdmax+1, &readset, NULL, NULL, NULL);


        if(FD_ISSET(server_socket, &readset)){            
            //Variabili per salvare porta e indirizzo
            int peer_port;
            char peer_addr_buff[INET_ADDRSTRLEN] = LOCALHOST; //Assumo che i peer partano tutti sulla macchina locale
            
            //Ricezione richieste di connessione
            peer_port = s_recv_UDP(server_socket, socket_buffer, SOCK_MAX_LEN);
            sscanf(socket_buffer, "%s", recv_buffer);
            recv_buffer[MESS_TYPE_LEN] = '\0';

            printf("Arrivato messaggio %s da %d sul socket\n", socket_buffer, peer_port);

            //Messaggi che puo' ricevere il server: richiesta di connessione o di abbandono

            //Richiesta di connessione
            if(strcmp(recv_buffer, "CONN_REQ") == 0){
                int temp_port[2]; //Variabili per salvare eventuali vicini
                char list_buffer[LIST_MAX_LEN]; //Buffer per invio liste al peer                
                int n; //Variabile per la lunghezza del messaggio da inviare al peer             
                char list_update_buffer[LIST_MAX_LEN]; //Liste da inviare ai peer a cui e' cambiata la lista dei vicini
                
                //Ack dell'arrivo della richiesta
                ack_UDP(server_socket, "CONN_ACK", peer_port, socket_buffer, strlen(socket_buffer));

                //Inserisco il peer nella lista
                if(!isIn(peer_port)){
                    if(insert_peer(peer_addr_buff, peer_port, connected_peers)<0){
                        printf("Impossibile inserire il peer\n");
                        //Uscita
                        FD_CLR(server_socket, &readset);
                        continue;
                    }
                }

                //Preparazione lista
                get_neighbors(peer_port, connected_peers+1, &temp_port[0], &temp_port[1]);
                printf("Vicini di %d: %d e %d\n", peer_port, temp_port[0], temp_port[1]);

                //Compongo la lista
                if(temp_port[0] == -1 && temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s", "NBR_LIST");
                else if(temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s %d", "NBR_LIST", temp_port[0]);
                else
                    n = sprintf(list_buffer, "%s %d %d", "NBR_LIST", temp_port[0], temp_port[1]);

                // DEBUG
                printf("Lista da inviare a %d: %s (lunga %d byte)\n", peer_port, list_buffer, n);


                //Invio
                send_UDP(server_socket, list_buffer, n+1, peer_port, "LIST_ACK");

                //Invio eventuale lista aggiornata al primo peer
                if(temp_port[0] != -1){
                    //Preparo la struttura che devo tirare su
                    get_list(temp_port[0], connected_peers+1, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[0]);
                    send_UDP(server_socket, list_update_buffer, n+1, temp_port[0], "UPDT_ACK");
                }

                if(temp_port[1] != -1){
                    get_list(temp_port[1], connected_peers+1, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[1]);
                    send_UDP(server_socket, list_update_buffer, n+1, temp_port[1], "UPDT_ACK");
                }

                //Incremento il numero di peer
                connected_peers++;
            }

            //Richiesta di uscita
            if(strcmp(recv_buffer, "CLT_EXIT") == 0){
                //Variabili per salvare informazioni temporanee
                int temp_nbr_port[2];
                char list_update[LIST_MAX_LEN];
                int n;

                ack_UDP(server_socket, "C_XT_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                printf("Ricevuto messaggio di richiesta di uscita da %d\n", peer_port);

                //Se il peer per qualche motivo non e' in lista non faccio nulla
                if(!isIn(peer_port)){
                    printf("Peer %d non presente nella lista dei peer connessi. Uscita\n", peer_port);
                    FD_CLR(server_socket, &readset);
                    continue;
                }

                get_neighbors(peer_port, connected_peers, &temp_nbr_port[0], &temp_nbr_port[1]);
                
                printf("Elimino il peer dalla rete. Aggiorno la lista di vicini di %d e %d\n", temp_nbr_port[0], temp_nbr_port[1]);

                print_peers(connected_peers);

                remove_peer(peer_port);
                
                print_peers(connected_peers-1);

                if(temp_nbr_port[0] == -1){
                    printf("Eliminato l'ultimo peer connesso\n");
                }
                else {
                    get_list(temp_nbr_port[0], connected_peers-1, "NBR_UPDT", list_update, &n);
                    printf("Lista che sta per essere inviata a %d: %s\n", temp_nbr_port[0], list_update);
                    send_UDP(server_socket, list_update, n+1, temp_nbr_port[0], "UPDT_ACK");
                    
                    if(temp_nbr_port[1] != -1){
                        get_list(temp_nbr_port[1], connected_peers-1, "NBR_UPDT", list_update, &n);
                        printf("Lista che sta per essere inviata a %d: %s\n", temp_nbr_port[1], list_update);
                        send_UDP(server_socket, list_update, n+1, temp_nbr_port[1], "UPDT_ACK");
                    }

                }

                connected_peers--;
                
            }

            //Inserita nuova entry su qualche peer
            if(strcmp(recv_buffer, "NEW_ENTR") == 0){
                char type;

                ack_UDP(server_socket, "ENEW_ACK", peer_port, socket_buffer, strlen(socket_buffer));

                sscanf(socket_buffer, "%s %c", recv_buffer, &type);
                
                add_entry(type);
            }

            if(strcmp(recv_buffer, "ENTR_REQ") == 0){
                char type;
                char entr_repl[MAX_ENTRY_REP];
                int ret;
                ack_UDP(server_socket, "EREQ_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                sscanf(socket_buffer, "%s %c", recv_buffer, &type);

                ret = sprintf(entr_repl, "%s %d", "ENTR_REP", read_entries(type));
                entr_repl[ret] = '\0';

                send_UDP(server_socket, entr_repl, ret+1, peer_port, "EREP_ACK");
                

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
            
            /*
                HELP
            */
            if(strcmp(command,"help\0")==0){
                comandi_server();
            }

            /*
                SHOWPEERS
            */
            else if(strcmp(command,"showpeers\0")==0){
                print_peers(connected_peers);
            }

            /*
                SHOWNEIGHBOR
            */
            else if(strcmp(command,"showneighbor\0")==0){
                if(connected_peers == 0)
                    printf("Nessun peer connesso\n");
                else if(input_number == 2)
                    print_single_neighbor(connected_peers, neighbor_peer);
                else
                    print_all_neighbors(connected_peers);
            }
            
            /*
                ESC
            */
            else if(strcmp(command,"esc\0")==0){
                //Invia a tutti i peer un messaggio
                int i;
                //DEBUG
                printf("Invio serie di messaggi SRV_EXIT\n");
                for(i=0; i<connected_peers; i++){
                    //Invia al peer il messaggio di exit
                    printf("Invio SRV_EXIT a %d\n", get_port(i));
                    send_UDP(server_socket, "SRV_EXIT", MESS_TYPE_LEN+1, get_port(i), "S_XT_ACK");
                }
                //Aggiorno il numero di peer connessi (dovrebbe essere 0)
                connected_peers = 0;
                //Cancello il file con la lista di peer
                remove("./ds_dir/peer_addr.txt");

                close(server_socket);
                _exit(0);
            }
            
            /*
                Errore
            */
            else {
                printf("Errore, comando non esistente\n");
            }

            FD_CLR(0, &readset);
        }

    }

    return 0;
}