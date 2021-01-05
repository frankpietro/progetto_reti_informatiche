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

#define MAX_IN 40   //Massima lunghezza comando da terminale
#define ADDR_LEN 15 //Massima lunghezza stringa con indirizzo IP
#define DATE_LEN 10 //Lunghezza di una stringa contenente la data in formato dd:mm:yyyy
#define MESS_TYPE_LEN 8 //Lunghezza tipo messaggio UDP
#define LIST_MAX_LEN 21 //Massima lunghezza lista di vicini
#define SOCK_MAX_LEN 30
#define MAX_COMMAND 6

//Variabili
int listener_socket;    //Descrittore del socket di ascolto
struct sockaddr_in listener_addr;   //Struttura per gestire il socket di ascolto
socklen_t listener_addr_len;

int ret;    //Variabile di servizio
char stdin_buff[MAX_IN];    //Buffer per i comandi da standard input
char sock_buff[SOCK_MAX_LEN];

//Variabili per collegarsi al server
struct sockaddr_in server_addr;
socklen_t server_addr_len;

int started = 0; //Controlla se il peer e' gia' connesso al DS o no
int neighbors[2];   //Da usare per salvarci i vicini

//Variabili per gestire input da socket oppure da stdin
fd_set master;
fd_set readset;
int fdmax;


int main(int argc, char** argv){
    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);

    //Creazione socket di ascolto
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
            char command[MAX_COMMAND];

            fgets(stdin_buff, MAX_IN, stdin);
            ret = sscanf(stdin_buff, "%s", command);

            //Stampa comandi
            if(strcmp(command,"help\0")==0){
                comandi_client();
            }

            //Connessione al server
            if(strcmp(command,"start\0")==0){
                //Variabili da prendere da stdin
                char DS_addr[ADDR_LEN]; //Indirizzo IP del server
                int DS_port; //Porta del server
                //Gestione ricezione lista
                char recv_buffer[LIST_MAX_LEN];
                char temp_buffer[MESS_TYPE_LEN];
                int temp_n[2];
                int sscanf_ret;
                int is_list;

                //Richiesta dati DS
                ret = sscanf(stdin_buff, "%s %s %d", command, DS_addr, &DS_port);
                if(ret != 3){
                    printf("Errore nel passaggio degli input alla chiamata di start\n");
                    comandi_client();
                    continue;
                }
                
                //Controllo che la connessione non esista gia'
                if(started == 1){
                    printf("Il peer e' gia' connesso al DS. Il comando non ha effetto\n");
                    continue;
                }
                
                //Configurazione socket lato server (do per scontato che indirizzo sia 127.0.0.1)
                clear_address(&server_addr, &server_addr_len, DS_port);

                //Invio richiesta di connessione e attendo ACK
                ack_1(listener_socket, "CONN_REQ", MESS_TYPE_LEN+1, &server_addr, server_addr_len, &readset, "CONN_ACK");

                is_list = 0;
                while(!is_list){
                    struct sockaddr_in util_addr;
                    socklen_t util_len;
                    //Ricevo la lista e invio ACK
                    ret = recvfrom(listener_socket, recv_buffer, LIST_MAX_LEN, 0, (struct sockaddr*)&util_addr, &util_len);
                    ret = sscanf(recv_buffer, "%s", temp_buffer);
                    if(util_addr.sin_port == server_addr.sin_port && util_addr.sin_addr.s_addr == server_addr.sin_addr.s_addr && strcmp("NBR_LIST", temp_buffer) == 0){
                        //Il peer ha ricevuto sicuramente la lista
                        printf("Ho ricevuto %s da %d\n", recv_buffer, ntohs(util_addr.sin_port));
                        is_list = 1;
                    }                        
                }

                ack_2(listener_socket, "LIST_ACK", MESS_TYPE_LEN+1, &server_addr, server_addr_len, &readset, "NBR_LIST");

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
            }

            else if(strcmp(command,"add\0")==0){
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

            else if(strcmp(command,"get\0")==0){
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
            
            else if(strcmp(command,"stop\0")==0){
                printf("Hai digitato comando di stop\n");

                //Controllo che la connessione esista
                if(!started){
                    printf("Il peer non e' connesso al DS. Uscita\n");
                    close(listener_socket);
                    _exit(0);
                }

                //Gestisce i propri dati

                ack_1(listener_socket, "CLT_EXIT", MESS_TYPE_LEN+1, &server_addr, server_addr_len, &readset, "ACK_C_XT");

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

                    printf("Parametri prima di sscanf: %d e %d\n", temp_n[0], temp_n[1]);

                    count = sscanf(sock_buff, "%s %d %d", mess_type_buff, &temp_n[0], &temp_n[1]);

                    printf("Parametri dopo sscanf: %d e %d\n", temp_n[0], temp_n[1]);
                    
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