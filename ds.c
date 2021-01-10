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
//Gestione del file con i peer connessi alla rete
#include "./util/peer_file.h"
//Gesione dei messaggi
#include "./util/msg.h"
//Costanti
#include "./util/const.h"

//Variabili
int server_socket;  //Socket su cui il server riceve messaggi dai peer
struct sockaddr_in server_addr;     //Struttura per gestire il socket
socklen_t server_len;

char command_buffer[MAX_STDIN_S];   //Buffer su cui salvare i comandi provenienti da stdin
char socket_buffer[MAX_SOCKET_RECV];
char recv_buffer[MESS_TYPE_LEN+1]; //Buffer su cui ricevere messaggio di richiesta connessione

extern int connected_peers;    //Numero di peers connessi alla rete

//Gestione input da stdin oppure da socket
fd_set master;
fd_set readset;
int fdmax;

//Mutua esclusione per la get [0] e per la stop [1]
int mutex_peer[2];

int time_port;


int main(int argc, char** argv){
    mutex_peer[0] = 0;
    mutex_peer[1] = 0;
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

        printf("Digita un comando:\n");

        //Controllo se c'e' qualcosa pronto
        select(fdmax+1, &readset, NULL, NULL, NULL);


        if(FD_ISSET(server_socket, &readset)){            
            //Variabili per salvare porta e indirizzo
            int peer_port;
            char peer_addr_buff[INET_ADDRSTRLEN] = LOCALHOST; //Assumo che i peer partano tutti sulla macchina locale
            
            //Ricezione richieste di connessione
            peer_port = s_recv_UDP(server_socket, socket_buffer, MAX_SOCKET_RECV);
            sscanf(socket_buffer, "%s", recv_buffer);
            recv_buffer[MESS_TYPE_LEN] = '\0';

            printf("Arrivato messaggio %s da %d sul socket\n", socket_buffer, peer_port);

            //Messaggi che puo' ricevere il server: richiesta di connessione o di abbandono

            //Richiesta di connessione
            if(strcmp(recv_buffer, "CONN_REQ") == 0){
                int temp_port[2]; //Variabili per salvare eventuali vicini
                char list_buffer[MAX_LIST_LEN]; //Buffer per invio liste al peer                
                int n; //Variabile per la lunghezza del messaggio da inviare al peer             
                char list_update_buffer[MAX_LIST_LEN]; //Liste da inviare ai peer a cui e' cambiata la lista dei vicini
                
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

                //Invio numero di porta sfruttando list buffer
                n = sprintf(list_buffer, "%s %d", "TMR_PORT", time_port);
                list_buffer[n] = '\0';
                send_UDP(server_socket, list_buffer, n, peer_port, "TPRT_ACK");

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
                send_UDP(server_socket, list_buffer, n, peer_port, "LIST_ACK");

                //Invio eventuale lista aggiornata al primo peer
                if(temp_port[0] != -1){
                    //Preparo la struttura che devo tirare su
                    get_list(temp_port[0], connected_peers+1, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[0]);
                    send_UDP(server_socket, list_update_buffer, n, temp_port[0], "UPDT_ACK");
                }

                if(temp_port[1] != -1){
                    get_list(temp_port[1], connected_peers+1, "NBR_UPDT", list_update_buffer, &n);
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[1]);
                    send_UDP(server_socket, list_update_buffer, n, temp_port[1], "UPDT_ACK");
                }

                //Incremento il numero di peer
                connected_peers++;
                printf("Peer connessi: %d\n", connected_peers);
            }

            //Richiesta di uscita
            if(strcmp(recv_buffer, "CLT_EXIT") == 0){
                //Variabili per salvare informazioni temporanee
                int temp_nbr_port[2];
                char list_update[MAX_LIST_LEN];
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

                remove_peer(peer_port);

                if(temp_nbr_port[0] == -1){
                    printf("Eliminato l'ultimo peer connesso\n");
                }
                else {
                    get_list(temp_nbr_port[0], connected_peers-1, "NBR_UPDT", list_update, &n);
                    printf("Lista che sta per essere inviata a %d: %s\n", temp_nbr_port[0], list_update);
                    send_UDP(server_socket, list_update, n, temp_nbr_port[0], "UPDT_ACK");
                    
                    if(temp_nbr_port[1] != -1){
                        get_list(temp_nbr_port[1], connected_peers-1, "NBR_UPDT", list_update, &n);
                        printf("Lista che sta per essere inviata a %d: %s\n", temp_nbr_port[1], list_update);
                        send_UDP(server_socket, list_update, n, temp_nbr_port[1], "UPDT_ACK");
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

            //Richiesta del numero totale di entries
            if(strcmp(recv_buffer, "ENTR_REQ") == 0){
                char type;
                char entr_repl[MAX_ENTRY_REP];
                int ret;
                
                ack_UDP(server_socket, "EREQ_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                sscanf(socket_buffer, "%s %c", recv_buffer, &type);

                ret = sprintf(entr_repl, "%s %d", "ENTR_REP", read_entries(type));
                entr_repl[ret] = '\0';

                send_UDP(server_socket, entr_repl, ret, peer_port, "EREP_ACK");
            }

            //Funzioni per get e stop: duplicati facilmente eliminabili parametrizzando l'indice del lock da richiedere e inserendolo nel messaggio

            //Controllo di blocco (se qualcuno sta eseguendo una get, nessuna operazione concessa)
            if(strcmp(recv_buffer, "IS_G_LCK") == 0){

                char mutex_buffer[MAX_LOCK_LEN];
                int len;
                //Invio ack
                ack_UDP(server_socket, "ISGL_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Se qualcuno ha gia' richiesto la stessa operazione appena prima, interrompo l'esecuzione
                if(mutex_peer[0])
                    printf("Peer %d sta eseguendo la get\n", mutex_peer[0]);
                
                len = sprintf(mutex_buffer, "%s %d", "GET_MUTX", mutex_peer[0]);
                mutex_buffer[len] = '\0';

                send_UDP(server_socket, mutex_buffer, len, peer_port, "GMTX_ACK");
            }

            //Controllo di blocco (se qualcuno sta eseguendo una stop, nessuna operazione concessa)
            if(strcmp(recv_buffer, "IS_S_LCK") == 0){
                char mutex_buffer[MAX_LOCK_LEN];
                int len;
                //Invio ack
                ack_UDP(server_socket, "ISSL_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Se qualcuno ha gia' richiesto la stessa operazione appena prima, interrompo l'esecuzione
                if(mutex_peer[1])
                    printf("Peer %d sta eseguendo la get\n", mutex_peer[1]);
                
                len = sprintf(mutex_buffer, "%s %d", "STP_MUTX", mutex_peer[1]);
                mutex_buffer[len] = '\0';

                send_UDP(server_socket, mutex_buffer, len, peer_port, "SMTX_ACK");
            }

            //Richiesta di blocco (mutua esclusione per la get)
            if(strcmp(recv_buffer, "GET_LOCK") == 0){
                char mutex_buffer[MAX_LOCK_LEN];
                int len;
                //Invio ack
                ack_UDP(server_socket, "GLCK_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Se qualcuno ha gia' richiesto la stessa operazione appena prima, interrompo l'esecuzione
                if(mutex_peer[0])
                    printf("Operazione gia' richiesta dal peer %d\n", mutex_peer[0]);
                
                len = sprintf(mutex_buffer, "%s %d", "GET_MUTX", mutex_peer[0]);
                mutex_buffer[len] = '\0';

                send_UDP(server_socket, mutex_buffer, len, peer_port, "GMTX_ACK");
                
                if(!mutex_peer[0])
                    mutex_peer[0] = peer_port;
            }

            //Richiesta di blocco (mutua esclusione per la stop)
            if(strcmp(recv_buffer, "STP_LOCK") == 0){
                char mutex_buffer[MAX_LOCK_LEN];
                int len;
                //Invio ack
                ack_UDP(server_socket, "SLCK_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Se qualcuno ha gia' richiesto la stessa operazione appena prima, interrompo l'esecuzione
                if(mutex_peer[1])
                    printf("Operazione gia' richiesta dal peer %d\n", mutex_peer[1]);
                
                len = sprintf(mutex_buffer, "%s %d", "STP_MUTX", mutex_peer[1]);
                mutex_buffer[len] = '\0';

                send_UDP(server_socket, mutex_buffer, len, peer_port, "SMTX_ACK");
                
                if(!mutex_peer[1])
                    mutex_peer[1] = peer_port;
            }

            //Richiesta di sblocco (mutua esclusione per la get)
            if(strcmp(recv_buffer, "GET_UNLK") == 0){
                //Invio ack
                ack_UDP(server_socket, "GNLK_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Questa parte non dovrebbe essere mai eseguita
                if(!mutex_peer[0]){
                    printf("Errore, apparentemente nessuno sta lavorando in mutua esclusione\n");
                    continue;
                }

                mutex_peer[0] = 0;
            }

            //Richiesta di sblocco (mutua esclusione per la stop)
            if(strcmp(recv_buffer, "STP_UNLK") == 0){
                //Invio ack
                ack_UDP(server_socket, "SNLK_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Questa parte non dovrebbe essere mai eseguita
                if(!mutex_peer[1]){
                    printf("Errore, apparentemente nessuno sta lavorando in mutua esclusione\n");
                    continue;
                }

                mutex_peer[1] = 0;
            }

            //Funzioni per get e stop: duplicati facilmente eliminabili parametrizzando l'indice del lock da richiedere e inserendolo nel messaggio

            //Saluto del timer
            if(strcmp(recv_buffer, "TM_HELLO") == 0){
                //Invio ack
                ack_UDP(server_socket, "TMHL_ACK", peer_port, socket_buffer, strlen(socket_buffer));
                
                //Salvo il numero di porta del timer
                time_port = peer_port;
            }

            FD_CLR(server_socket, &readset);
            
        }

        //Gestione comandi da stdin
        if(FD_ISSET(0, &readset)) {
            //Parsing dell'input
            int neighbor_peer;
            int input_number;
            char command[MAX_COMMAND_S];
            
            fgets(command_buffer, MAX_COMMAND_S, stdin);
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
                    send_UDP(server_socket, "SRV_EXIT", MESS_TYPE_LEN, get_port(i), "S_XT_ACK");
                }
                //Aggiorno il numero di peer connessi (dovrebbe essere 0)
                connected_peers = 0;
                //Cancello il file con la lista di peer
                remove("./ds_dir/peer_addr.txt");
                //Chiudo il time server
                send_UDP(server_socket, "SRV_EXIT", MESS_TYPE_LEN, time_port, "S_XT_ACK");

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
