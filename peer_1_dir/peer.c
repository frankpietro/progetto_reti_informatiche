#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "../util/ack.h"
#include "../util/util.h"

#define MAX_IN 20   //Massima lunghezza comando da terminale
#define ADDR_LEN 15 //Massima lunghezza stringa con indirizzo IP
#define DATE_LEN 10 //Lunghezza di una stringa contenente la data in formato dd:mm:yyyy
#define MESS_TYPE_LEN 8 //Lunghezza tipo messaggio UDP
#define LIST_MAX_LEN 21 //Massima lunghezza lista di vicini
#define SOCK_MAX_LEN 30

int main(int argc, char** argv){
    //Variabili
    //int port;   //Porta del socket di ascolto
    int listener_socket;    //Descrittore del socket di ascolto
    struct sockaddr_in listener_addr;   //Struttura per gestire il socket di ascolto
    socklen_t listener_addr_len;
    int ret;    //Variabile di servizio
    char stdin_buff[MAX_IN];    //Buffer per i comandi da standard input
    char sock_buff[SOCK_MAX_LEN];
    int started = 0; //Controlla se il peer e' gia' connesso al DS o no
    int neighbors[2];   //Da usare per salvarci i vicini
    //Variabili per gestire input da socket oppure da stdin
    fd_set master;
    fd_set readset;
    int fdmax;
    //Variabili per collegarsi al server
    struct sockaddr_in server_addr;
    socklen_t addrlen;
    
    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);


    //Creazione socket di ascolto
    //port = atoi(argv[1]);
    listener_socket = prepare(&listener_addr, &listener_addr_len, atoi(argv[1]));

    //All'inizio nessun vicino
    neighbors[0] = 0;
    neighbors[1] = 0;

    //Stampa elenco comandi
    comandi_client();

    FD_SET(listener_socket, &master);
    FD_SET(0, &master);
    fdmax = listener_socket;

    //Ciclo infinito di funzionamento peer
    while(1){
        //Controllo con la select cosa e' pronto
        readset = master;
        ret = select(fdmax+1, &readset, NULL, NULL, NULL);

        //Messaggio da stdin
        if(FD_ISSET(0, &readset)){
            //Richiede comando
            scanf("%s", stdin_buff);

            if(strcmp(stdin_buff,"help\0")==0){
                comandi_client();
            }


            //Connessione al server
            if(strcmp(stdin_buff,"start\0")==0){
                //Variabili da prendere da stdin
                char DS_addr[ADDR_LEN]; //Indirizzo IP del server
                int DS_port; //Porta del server
                //Gestione ricezione lista
                char recv_buffer[LIST_MAX_LEN];
                char temp_buffer[MESS_TYPE_LEN];
                int temp_n[2];
                int sscanf_ret;
                struct timeval util_tv;
                //Messaggio da inviare per collegarsi
                char conn_req_buffer[MESS_TYPE_LEN+1] = "CONN_REQ\0";


                //Richiesta dati DS
                scanf("%s", DS_addr);
                scanf("%d", &DS_port);
                //Controllo che la connessione non esista gia'
                if(started == 1){
                    printf("Il peer e' gia' connesso al DS. Il comando non ha effetto\n");
                    continue;
                }
                
                //Configurazione socket lato server
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(DS_port);
                inet_pton(AF_INET, DS_addr, &server_addr.sin_addr);

                //Peer: prova a connettersi finche' non ci riesce
                while(!started){
                    addrlen = sizeof(server_addr);
                    //Invio boot message
                    do {
                        ret = sendto(listener_socket, conn_req_buffer, MESS_TYPE_LEN+1, 0, (struct sockaddr*)&server_addr, addrlen);
                        printf("Invio richiesta di connessione\n");
                    } while(ret<0);
                    
                    //Mi preparo a ricevere un messaggio dal server
                    FD_ZERO(&readset);
                    FD_SET(listener_socket, &readset);
                    //Imposto il timeout a 1 secondo
                    util_tv.tv_sec = 1;
                    util_tv.tv_usec = 0;

                    printf("Sto aspettando la lista\n");
                    //Aspetto la lista
                    ret = select(listener_socket+1, &readset, NULL, NULL, &util_tv);
                    //Appena la ricevo
                    if(FD_ISSET(listener_socket, &readset)){
                        //Ricevo lista di peer
                        ret = recvfrom(listener_socket, recv_buffer, LIST_MAX_LEN, 0, (struct sockaddr*)&server_addr, &addrlen);
                        
                        printf("Ricevuta lista di peer %s\n", recv_buffer);

                        sscanf_ret = sscanf(recv_buffer, "%s %d %d", temp_buffer, &temp_n[0], &temp_n[1]);
                        
                        //Se ho ricevuto effettivamente la lista
                        if(strcmp("NBR_LIST", temp_buffer) == 0){
                            //Controllo quanti vicini ho
                            switch(sscanf_ret){
                                case 1:
                                    printf("Lista vuota, nessun vicino\n");
                                    started = 1;
                                    break;
                                case 2:
                                    printf("Un vicino con porta %d\n", temp_n[0]);
                                    neighbors[0] = temp_n[0];
                                    started = 1;
                                    break;
                                case 3:
                                    printf("Due vicini con porta %d e %d\n", temp_n[0], temp_n[1]);
                                    neighbors[0] = temp_n[0];
                                    neighbors[1] = temp_n[1];
                                    started = 1;
                                    break;
                                default:
                                    printf("Problema nella trasmissione della lista\n");
                            }
                        }
                        //Se arriva qualcosa di diverso dalla lista semplicemente si ignora

                        FD_CLR(listener_socket, &readset);
                    }
                }
            }

            else if(strcmp(stdin_buff,"add\0")==0){
                //Dati da richiedere
                int type;
                int quantity;
                //Richiesta dati
                printf("Tipo di dato (0: tamponi; 1: nuovi casi): ");

                do{
                    scanf("%d", &type);
                    if(type != 0 && type != 1)
                        printf("Input non valido, inserisci di nuovo: ");
                } while (type != 0 && type != 1);

                printf("Quantita': ");
                scanf("%d", &quantity);

                printf("Tipo: %d; quantita': %d\n", type, quantity);
                
                //Elaborazione...

            }

            else if(strcmp(stdin_buff,"get\0")==0){
                //Dati da richiedere
                int aggr;
                int type;
                char period1[DATE_LEN];
                char period2[DATE_LEN];

                printf("Tipo di aggregazione (0: totale; 1: variazione): ");
                do{
                    scanf("%d", &aggr);
                    if(aggr != 0 && aggr != 1)
                        printf("Input non valido, inserisci di nuovo: ");
                } while (aggr != 0 && aggr != 1);

                printf("Tipo di dato (0: tamponi; 1: nuovi casi): ");
                do{
                    scanf("%d", &type);
                    if(type != 0 && type != 1)
                        printf("Input non valido, inserisci di nuovo: ");
                } while (type != 0 && type != 1);

                printf("Data di inizio in formato dd:mm:yyyy\n(inserire 'a' se si vuole tutto il periodo di monitoraggio, inserire * se non si vuole lower bound): ");
                scanf("%s", period1);
                if(strcmp(period1, "a\0") == 0){
                    //Elaborazione senza limiti temporali
                }
                else {
                    printf("Data di fine in formato dd:mm:yyyy\n(inserire * se non si vuole upper bound): ");
                    scanf("%s", period2);
                }
            }
            
            else if(strcmp(stdin_buff,"stop\0")==0){
                char exit_buffer[MESS_TYPE_LEN] = "CLT_EXIT";
                int exit_acked = 0;
                printf("Hai digitato comando di stop\n");

                //Controllo che la connessione esista
                if(!started){
                    printf("Il peer non e' connesso al DS. Uscita\n");
                    close(listener_socket);
                    _exit(0);
                }

                //Gestisce i propri dati

                //Esce
                while(!exit_acked){
                    struct timeval util_tv;
                    char recv_buffer[MESS_TYPE_LEN];
                    addrlen = sizeof(server_addr);
                    //Invio exit message
                    do {
                        ret = sendto(listener_socket, exit_buffer, MESS_TYPE_LEN, 0, (struct sockaddr*)&server_addr, addrlen);
                    } while(ret<0);
                    
                    //Mi preparo a ricevere un ack dal server
                    FD_ZERO(&readset);
                    FD_SET(listener_socket, &readset);
                    //Imposto il timeout a 1 secondo
                    util_tv.tv_sec = 1;
                    util_tv.tv_usec = 0;
                    //Aspetto l'ack
                    ret = select(fdmax+1, &readset, NULL, NULL, &util_tv);
                    //Appena arriva qualcosa dal server
                    if(FD_ISSET(listener_socket, &readset)){
                        //Ricevo messaggio
                        ret = recvfrom(listener_socket, recv_buffer, LIST_MAX_LEN, 0, (struct sockaddr*)&server_addr, &addrlen);
                        //Se ho ricevuto effettivamente la lista
                        if(strcmp("ACK_C_XT", recv_buffer) == 0){
                            printf("Il server ha ricevuto correttamente l'informazione\n");
                            exit_acked = 1;
                        }

                        //Se messaggio diverso, ignorato
                    }

                    //Se non arriva niente, si ricomincia il ciclo

                }

                close(listener_socket);
                _exit(0);
                
            }
            
            else {
                printf("Errore, comando non esistente\n");
            }
            memset(stdin_buff, '\0', sizeof(stdin_buff));
            FD_CLR(0, &readset);
        }

        //Messaggio dal server
        if(FD_ISSET(listener_socket, &readset)){
            struct sockaddr_in util_addr;
            socklen_t util_len;
            util_len = sizeof(util_addr);
            //Messaggi su socket UDP: solo ricevibili dal server
            ret = recvfrom(listener_socket, sock_buff, SOCK_MAX_LEN, 0, (struct sockaddr*)&util_addr, &util_len);
            if(util_addr.sin_port == server_addr.sin_port && util_addr.sin_addr.s_addr == server_addr.sin_addr.s_addr){
                char mess_type_buff[MESS_TYPE_LEN];
                printf("Messaggio ricevuto dal server: %s\n", sock_buff);
                //Leggo il tipo del messaggio
                sscanf(sock_buff, "%s", mess_type_buff);
                
                //Arrivo nuova lista
                if(strcmp(mess_type_buff, "NBR_UPDT")==0){
                    //Numero di nuovi neighbor
                    int count;
                    int temp_n[2];
                    //Variabile di controllo

                    count = sscanf(sock_buff, "%s %d %d", mess_type_buff, &temp_n[0], &temp_n[1]);
                    
                    //Modifica ai vicini
                    switch(count){
                        case 1:
                            //DEBUG
                            printf("Sono rimasto l'unico peer\n");
                            neighbors[0] = 0;
                            neighbors[1] = 0;
                            break;
                        case 2:
                            printf("Un vicino con porta %d\n", temp_n[0]);
                            neighbors[0] = temp_n[0];
                            neighbors[1] = 0;
                            break;
                        case 3:
                            printf("Due vicini con porta %d e %d\n", temp_n[0], temp_n[1]);
                            neighbors[0] = temp_n[0];
                            neighbors[1] = temp_n[1];
                            break;
                        default:
                            printf("Questa riga di codice non dovrebbe mai andare in esecuzione\n");
                            break;
                    }

                    //Invio ACK
                    ack_2(listener_socket, "CHNG_ACK", MESS_TYPE_LEN+1, &server_addr, sizeof(server_addr), &readset, "NBR_UPDT");
                }
            
                //Notifica chiusura server
                if(strcmp(mess_type_buff, "SRV_EXIT")==0){
                    printf("Il server sta per chiudere\n");

                    //Fa qualcosa coi dati

                    //Invia ACK
                    ack_2(listener_socket, "ACK_S_XT", MESS_TYPE_LEN+1, &server_addr, sizeof(server_addr), &readset, "SRV_EXIT");

                    //Chiude
                    printf("Chiusura peer\n");
                    close(listener_socket);
                    _exit(0);
                }
            }
        
        }

        FD_CLR(listener_socket, &readset);

    
    }

    return 0;
}