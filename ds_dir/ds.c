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

//Gestione del file con i peer
#include "peer_file.h"

//Gesione degli ack
#include "ack.h"

#define MAX_COMMAND 30
#define MESS_TYPE_LEN 8
#define LIST_MAX_LEN 21
#define LOCALHOST "127.0.0.1"

//Elenco dei comandi disponibili
void comandi(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comando e significato\n");
    printf("showpeers --> mostra elenco peer connessi alla rete\n");
    printf("showneighbor [peer] --> mostra i neighbor di un peer o di tutti i peer se non viene specificato alcun parametro\n");
    printf("esc --> termina il DS\n");
}

int main(int argc, char** argv){
    //Variabili
    int ds_port;    //Porta del server, passata come argomento del main
    
    int server_socket;  //Socket su cui il server riceve messaggi dai peer
    struct sockaddr_in server_addr;     //Struttura per gestire il socket
    
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
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ds_port);
	inet_pton(AF_INET, LOCALHOST, &server_addr.sin_addr);
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
                //int received = 0;
                int temp_port[2];
                //Buffer per invio liste al peer
                char list_buffer[LIST_MAX_LEN];
                //Variabile per la lunghezza del messaggio da inviare al peer
                int n;
                //Liste da inviare ai peer a cui e' cambiata la lista dei vicini
                char list_update_buffer[LIST_MAX_LEN];
                //Stringa che introduce la lista di peer vicini da inviare al peer richiedente
                char list_buffer_h[MESS_TYPE_LEN+1] = "NBR_LIST\0";

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

                printf("Arrivata richiesta di connessione dal peer %d\n", peer_port);
                
                get_neighbors(peer_port, connected_peers+1, &temp_port[0], &temp_port[1]);

                //Compongo la lista
                if(temp_port[0] == -1 && temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s", list_buffer_h);
                else if(temp_port[1] == -1)
                    n = sprintf(list_buffer, "%s %d", list_buffer_h, temp_port[0]);
                else
                    n = sprintf(list_buffer, "%s %d %d", list_buffer_h, temp_port[0], temp_port[1]);

                printf("Numero di byte scritti: %d\n", n);
                list_buffer[n] = '\0';

                
                // DEBUG
                printf("List buffer: %s\n", list_buffer);

                //Invio
                ack_2(server_socket, list_buffer, n+1, &network_peer, peer_addr_len, &readset, "CONN_REQ");
/*
                while(!received){
                    do {
                        ret = sendto(server_socket, list_buffer, n+1, 0, (struct sockaddr*)&network_peer, peer_addr_len);
                    } while(ret<0);

                    util_len = sizeof(util_addr);
                    //Attesa di un secondo
                    util_tv.tv_sec = 1;
                    util_tv.tv_usec = 0;
                    //Mi metto per un secondo solo in ascolto sul socket
                    //Solo in ascolto di un'eventuale copia del messaggio di boot
                    FD_ZERO(&readset);
                    FD_SET(server_socket, &readset);
                        
                    ret = select(fdmax+1, &readset, NULL, NULL, &util_tv);

                    //Se arriva qualcosa
                    if(FD_ISSET(server_socket, &readset)){
                        //Leggo cosa ho ricevuto
                        ret = recvfrom(server_socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&util_addr, &util_len);
                        //Se ho ricevuto lo stesso identico messaggio
                        if(util_addr.sin_port == network_peer.sin_port && util_addr.sin_addr.s_addr == network_peer.sin_addr.s_addr && (strcmp(recv_buffer, "CONN_REQ")==0)){
                            //Riinvio la lista al peer tornando a inizio while(!received)
                            received = 0;
                            break;
                        }
                        //Se ho ricevuto un messaggio diverso lo scarto (il peer lo rimandera')
                        else
                            received = 1;
                        
                        FD_CLR(server_socket, &readset);
                    }
                    //Se per due secondi non arriva nulla, considero terminata con successo l'operazione
                    else
                        received = 1;
                }
*/
                //Necessario inviare lista aggiornata ai peer a cui e' cambiata

                if(connected_peers == 0)
                    connected_peers++;

                //Se uno solo allora e' facile
                else if(connected_peers == 1){
                    char list_update_h[MESS_TYPE_LEN+1] = "NBR_UPDT\0";
                    int sent = 0;
                    
                    n = sprintf(list_update_buffer, "%s %d", list_update_h, peer_port);
                    list_update_buffer[n] = '\0';

                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[0]);

                    while(!sent){
                        struct sockaddr_in peer_addr_1;
                        memset(&peer_addr_1, 0, sizeof(peer_addr_1));
                        peer_addr_1.sin_family = AF_INET;
                        peer_addr_1.sin_port = htons(temp_port[0]);
                        inet_pton(AF_INET, LOCALHOST, &peer_addr_1.sin_addr);

                        peer_addr_len = sizeof(peer_addr_1);
                        //Invio lista
                        do {
                            ret = sendto(server_socket, list_update_buffer, n+1, 0, (struct sockaddr*)&peer_addr_1, peer_addr_len);
                        } while(ret<0);
                        
                        //Mi preparo a ricevere un messaggio dal peer
                        FD_ZERO(&readset);
                        FD_SET(server_socket, &readset);
                        //Imposto il timeout a un secondo
                        util_tv.tv_sec = 1;
                        util_tv.tv_usec = 0;
                        //Aspetto la lista
                        ret = select(fdmax+1, &readset, NULL, NULL, &util_tv);

                        //Appena la ricevo
                        if(FD_ISSET(server_socket, &readset)){
                            char temp_buffer[MESS_TYPE_LEN];
                            //Ricevo ack
                            ret = recvfrom(server_socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&peer_addr_1, &peer_addr_len);
                            printf("Ho ricevuto %s\n", recv_buffer);
                            
                            ret = sscanf(recv_buffer, "%s", temp_buffer);
                            
                            //Se ho ricevuto effettivamente l'ack
                            if(strcmp("CHNG_ACK", temp_buffer) == 0){
                                //Il peer ha ricevuto sicuramente la lista
                                sent = 1;
                            }

                            //Ignoro qualunque altro messaggio

                            FD_CLR(server_socket, &readset);
                        }
                    }

                    connected_peers++;

                }

                //Se sono due li cerco e invio loro la lista
                else {
                    char list_update_h[MESS_TYPE_LEN+1] = "NBR_UPDT\0";
                    int sent = 0;
                    int new_nbr[4];

                    //Connected peers: +1 perche' nel file sono gia' 3 ma quel numero vale ancora 2
                    get_neighbors(temp_port[0], connected_peers+1, &new_nbr[0], &new_nbr[1]);
                    get_neighbors(temp_port[1], connected_peers+1, &new_nbr[2], &new_nbr[3]);

                    //Invio la lista al primo
                    n = sprintf(list_update_buffer, "%s %d %d", list_update_h, new_nbr[0], new_nbr[1]);
                    list_update_buffer[n] = '\0';
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[0]);

                    while(!sent){
                        struct sockaddr_in peer_addr_1;
                        memset(&peer_addr_1, 0, sizeof(peer_addr_1));
                        peer_addr_1.sin_family = AF_INET;
                        peer_addr_1.sin_port = htons(temp_port[0]);
                        inet_pton(AF_INET, LOCALHOST, &peer_addr_1.sin_addr);

                        peer_addr_len = sizeof(peer_addr_1);
                        //Invio lista
                        do {
                            ret = sendto(server_socket, list_update_buffer, n+1, 0, (struct sockaddr*)&peer_addr_1, peer_addr_len);
                        } while(ret<0);
                        
                        //Mi preparo a ricevere un messaggio dal peer
                        FD_ZERO(&readset);
                        FD_SET(server_socket, &readset);
                        //Imposto il timeout a un secondo
                        util_tv.tv_sec = 1;
                        util_tv.tv_usec = 0;
                        //Aspetto la lista
                        ret = select(fdmax+1, &readset, NULL, NULL, &util_tv);

                        //Appena la ricevo
                        if(FD_ISSET(server_socket, &readset)){
                            char temp_buffer[MESS_TYPE_LEN];
                            //Ricevo ack
                            ret = recvfrom(server_socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&peer_addr_1, &peer_addr_len);
                            printf("Ho ricevuto %s\n", recv_buffer);
                            
                            ret = sscanf(recv_buffer, "%s", temp_buffer);
                            
                            //Se ho ricevuto effettivamente l'ack
                            if(strcmp("CHNG_ACK", temp_buffer) == 0){
                                //Il peer ha ricevuto sicuramente la lista
                                sent = 1;
                            }

                            //Ignoro qualunque altro messaggio

                            FD_CLR(server_socket, &readset);
                        }
                    }
                    
                    sent = 0;

                    //Invio la lista al secondo
                    n = sprintf(list_update_buffer, "%s %d %d", list_update_h, new_nbr[2], new_nbr[3]);
                    list_update_buffer[n] = '\0';
                    printf("Invio lista di update %s a %d\n", list_update_buffer, temp_port[1]);

                    while(!sent){
                        struct sockaddr_in peer_addr_1;
                        memset(&peer_addr_1, 0, sizeof(peer_addr_1));
                        peer_addr_1.sin_family = AF_INET;
                        peer_addr_1.sin_port = htons(temp_port[1]);
                        inet_pton(AF_INET, LOCALHOST, &peer_addr_1.sin_addr);

                        peer_addr_len = sizeof(peer_addr_1);
                        //Invio lista
                        do {
                            ret = sendto(server_socket, list_update_buffer, n+1, 0, (struct sockaddr*)&peer_addr_1, peer_addr_len);
                        } while(ret<0);
                        
                        //Mi preparo a ricevere un messaggio dal peer
                        FD_ZERO(&readset);
                        FD_SET(server_socket, &readset);
                        //Imposto il timeout a un secondo
                        util_tv.tv_sec = 1;
                        util_tv.tv_usec = 0;
                        //Aspetto la lista
                        ret = select(fdmax+1, &readset, NULL, NULL, &util_tv);

                        //Appena la ricevo
                        if(FD_ISSET(server_socket, &readset)){
                            char temp_buffer[MESS_TYPE_LEN];
                            //Ricevo ack
                            ret = recvfrom(server_socket, recv_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&peer_addr_1, &peer_addr_len);
                            printf("Ho ricevuto %s\n", recv_buffer);
                            
                            ret = sscanf(recv_buffer, "%s", temp_buffer);
                            
                            //Se ho ricevuto effettivamente l'ack
                            if(strcmp("CHNG_ACK", temp_buffer) == 0){
                                //Il peer ha ricevuto sicuramente la lista
                                sent = 1;
                            }

                            //Ignoro qualunque altro messaggio

                            FD_CLR(server_socket, &readset);
                        }
                    }

                    connected_peers++;
                }

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
            scanf("%s", command_buffer);
            if(strcmp(command_buffer,"help\0")==0){
                comandi();
            }

            else if(strcmp(command_buffer,"showpeers\0")==0){
                int i;
                printf("Peer connessi alla rete:");
                if(!connected_peers)
                    printf(" nessuno!");

                for(i=0; i<connected_peers; i++)
                    printf("\n%d", get_port(i));

                printf("\n");
            }

            else if(strcmp(command_buffer,"showneighbor\0")==0){
                int focus_peer_port;

                printf("Inserisci numero di peer di cui vuoi conoscere i vicini (0 se vuoi conoscere i vicini di tutti i peer): ");
                scanf("%d", &focus_peer_port);
                if(focus_peer_port == 0){
                    printf("Vuoi conoscere i vicini di tutti i peer\n");
                }
                else {
                    printf("Vuoi conoscere i vicini del peer %d\n", focus_peer_port);
                }
            }
            
            else if(strcmp(command_buffer,"esc\0")==0){
                
                //DEBUG
                printf("Hai digitato comando esc\n");

                //Stacca tutti i peer
                while(connected_peers){
                    char exit[MESS_TYPE_LEN+1] = "SRV_EXIT\0"; //Messaggio da inviare
                    char exit_ack_buffer[MESS_TYPE_LEN]; //Messaggio da ricevere
                    struct sockaddr_in temp_peer_addr; //Struttura in cui salvare indirizzo del peer a cui volta volta inviare o da cui ricevere
                    socklen_t temp_peer_addr_len; //Lunghezza della struttura sopra
                    struct timeval util_tv; //Attesa sul select
                    int removed = 0; //Per capire quando uscire da questo while
                    //Invia a tutti i peer un messaggio
                    int i;
                    //DEBUG
                    printf("Invio serie di messaggi SRV_EXIT\n");
                    for(i=0; i<connected_peers; i++){
                        int focus_port = get_port(i);
                        //Invia al peer il messaggio di exit
                        temp_peer_addr_len = sizeof(temp_peer_addr);

                        printf("Inviato SRV_EXIT a %d\n", focus_port);
                        temp_peer_addr.sin_port = htons(focus_port);
                        temp_peer_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, LOCALHOST, &temp_peer_addr.sin_addr);

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
                        
                        }

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