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
#include <signal.h>
#include <sys/wait.h>

#include "../util/msg.h"
#include "../util/util.h"
//Gestione del file con le entries
#include "../util/entries.h"

#define MAX_IN 40   //Massima lunghezza comando da terminale
#define ADDR_LEN 15 //Massima lunghezza stringa con indirizzo IP
#define DATE_LEN 10 //Lunghezza di una stringa contenente la data in formato dd:mm:yyyy
#define TIME_LEN 8
#define DATE_UPDATE_LEN 28
#define MESS_TYPE_LEN 8 //Lunghezza tipo messaggio UDP
#define LIST_MAX_LEN 21 //Massima lunghezza lista di vicini
#define SOCK_MAX_LEN 30
#define MAX_COMMAND 6
#define MAX_FILENAME_LEN 20
#define MAX_CONNECTED_PEERS 100
#define MAX_ENTRY_REP 16 //Non piu' di 1M di entries distinte
#define ENTR_W_TYPE 10

//Variabili
int my_port;
int listener_socket;    //Descrittore del socket di ascolto
struct sockaddr_in listener_addr;   //Struttura per gestire il socket di ascolto
socklen_t listener_addr_len;
int time_socket;    //Descrittore del socket di ascolto
struct sockaddr_in time_addr;   //Struttura per gestire il socket di ascolto
socklen_t time_addr_len;

int ret;    //Variabile di servizio
char stdin_buff[MAX_IN];    //Buffer per i comandi da standard input
char socket_buffer[SOCK_MAX_LEN];

//Identifica il server
int server_port;

int neighbors[2];   //Da usare per salvarci i vicini

//Variabili per gestire input da socket oppure da stdin
fd_set master;
fd_set readset;
int fdmax;

//Variabili temporali
extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];
pid_t pid;


int main(int argc, char** argv){
    /*pid = fork();

    if(pid != 0){
        while(1){
            sleep(1);
            kill(pid,SIGUSR1);
            //printf("SIGUSR1 inviato\n");
            for(ret = 0; ret < 59; ret++){
                if(waitpid(pid, NULL, WNOHANG) == 0)
                    sleep(1);
                else
                    _exit(0);
            }
        }
    }

    else {*/
        retrieve_time();
        //Pulizia set
        FD_ZERO(&master);
        FD_ZERO(&readset);

        my_port = atoi(argv[1]);

        //Creazione socket di ascolto
        listener_socket = prepare(&listener_addr, &listener_addr_len, my_port);

        //All'inizio nessun vicino
        neighbors[0] = 0;
        neighbors[1] = 0;

        //Identifica un peer non connesso
        server_port = -1;

        //Stampa elenco comandi
        comandi_client();

        FD_SET(listener_socket, &master);
        FD_SET(0, &master);
        fdmax = listener_socket;

        //Ciclo infinito di funzionamento peer
        while(1){
            //Controllo con la select cosa e' pronto
            readset = master;
            select(fdmax+1, &readset, NULL, NULL, NULL);

            //Messaggio da stdin
            if(FD_ISSET(0, &readset)){
                //Richiede comando
                char command[MAX_COMMAND];

                fgets(stdin_buff, MAX_IN, stdin);
                sscanf(stdin_buff, "%s", command);

                /*
                    HELP
                */
                if(strcmp(command,"help")==0){
                    printf("Data: %s; ora: %s\n", current_d, current_t);
                    comandi_client();
                }

                /*
                    START
                */
                else if(strcmp(command,"start")==0){
                    //Variabili da prendere da stdin
                    char DS_addr[ADDR_LEN]; //Indirizzo IP del server
                    //Gestione ricezione lista
                    char recv_buffer[LIST_MAX_LEN];
                    char temp_buffer[MESS_TYPE_LEN];
                    int temp_n[2];
                    
                    retrieve_time();
                    //Controllo che la connessione non esista gia'
                    if(server_port != -1){
                        printf("Il peer e' gia' connesso al DS. Il comando non ha effetto\n");
                        continue;
                    }

                    //Richiesta dati DS
                    ret = sscanf(stdin_buff, "%s %s %d", command, DS_addr, &server_port);
                    if(ret != 3){
                        printf("Errore nel passaggio degli input alla chiamata di start\n");
                        comandi_client();
                        continue;
                    }

                    server_port %= 65536;
                    
                    //Invio richiesta di connessione e attendo ACK
                    send_UDP(listener_socket, "CONN_REQ", MESS_TYPE_LEN+1, server_port, "CONN_ACK");

                    recv_UDP(listener_socket, recv_buffer, LIST_MAX_LEN, server_port, "NBR_LIST", "LIST_ACK");

                    ret = sscanf(recv_buffer, "%s %d %d", temp_buffer, &temp_n[0], &temp_n[1]);
                            
                    //Se ho ricevuto effettivamente la lista
                    if(strcmp("NBR_LIST", temp_buffer) == 0){
                        //Controllo quanti vicini ho
                        switch(ret){
                            case 1:
                                printf("Lista vuota, nessun vicino\n");
                                break;
                            case 2:
                                printf("Un vicino con porta %d\n", temp_n[0]);
                                neighbors[0] = temp_n[0];
                                break;
                            case 3:
                                printf("Due vicini con porta %d e %d\n", temp_n[0], temp_n[1]);
                                neighbors[0] = temp_n[0];
                                neighbors[1] = temp_n[1];
                                break;
                            default:
                                printf("Problema nella trasmissione della lista\n");
                        }
                    }
                }

                /*
                    ADD
                */
                else if(strcmp(command,"add")==0){
                    char type;
                    int quantity;
                    char new_entry[ENTR_W_TYPE+1];

                    //Se peer non connesso non faccio nulla
                    if(server_port == -1){
                        printf("Peer non connesso\n");
                        continue;
                    }

                    ret = sscanf(stdin_buff, "%s %c %d", command, &type, &quantity);
                    if(!(type == 't' || type == 'n') || ret != 3 || quantity < 1){
                        printf("Errore nell'inserimento dei dati\n");
                        continue;
                    }
                    
                    retrieve_time();
                    insert_entry(type, quantity);

                    sprintf(new_entry, "%s %c", "NEW_ENTR", type);

                    send_UDP(listener_socket, new_entry, ENTR_W_TYPE+1, server_port, "ENEW_ACK");

                }

                /*
                    GET
                */
                else if(strcmp(command,"get")==0){
                    //int date[2][3];
                    char aggr;
                    char type;
                    char bounds[2][DATE_LEN];
                    int tot_entr;
                    char entry_buffer[MAX_ENTRY_REP];
                    
                    ret = sscanf(stdin_buff,"%s %c %c %s %s", command, &aggr, &type, bounds[0], bounds[1]);
                    //Numero di parametri
                    if(!(ret == 3 || ret == 5)){
                        printf("Errore nel numero di parametri passati\n");
                        continue;
                    }
                    //Controllo su aggr e type
                    if(!((aggr == 't' || aggr == 'v') && (type == 't' || type == 'n'))){
                        printf("Errore nell'inserimento della richiesta\n");
                        continue;
                    }
                    //Controllo sulle date
                    if(ret == 5){
                        if(!check_dates(bounds[0], bounds[1], aggr))
                            continue;
                    }

                    printf("Controlli superati!\n");

                    //Invio richiesta al server
                    sprintf(entry_buffer, "%s %c", "ENTR_REQ", type);
                    entry_buffer[ENTR_W_TYPE] = '\0';
                    send_UDP(listener_socket, entry_buffer, ENTR_W_TYPE+1, server_port, "EREQ_ACK");
                    
                    //Ricevo risposta
                    recv_UDP(listener_socket, entry_buffer, MAX_ENTRY_REP, server_port, "ENTR_REP", "EREP_ACK");
                    sscanf(entry_buffer, "%s %d", command, &tot_entr); //command non serve piu'

                    printf("Entries nel server: %d; entries qui: %d\n", tot_entr, count_entries(type));
                }
                
                /*
                    STOP
                */
                else if(strcmp(command,"stop")==0){
                    printf("Hai digitato comando di stop\n");

                    //Controllo che la connessione esista
                    if(server_port == -1){
                        printf("Il peer non e' connesso al DS. Uscita\n");
                        close(listener_socket);
                        _exit(0);
                    }

                    //Gestisce i propri dati

                    send_UDP(listener_socket, "CLT_EXIT", MESS_TYPE_LEN+1, server_port, "C_XT_ACK");

                    close(listener_socket);
                    _exit(0);
                    
                }
                
                /*
                    Errore
                */
                else
                    printf("Errore, comando non esistente\n");
                
                
                FD_CLR(0, &readset);
            }

            //Messaggio sul socket
            if(FD_ISSET(listener_socket, &readset)){
                int util_port;
                char mess_type_buff[MESS_TYPE_LEN+1];

                util_port = s_recv_UDP(listener_socket, socket_buffer, SOCK_MAX_LEN);
                //Leggo il tipo del messaggio
                sscanf(socket_buffer, "%s", mess_type_buff);
                mess_type_buff[MESS_TYPE_LEN] = '\0';

                if(util_port == server_port){
                    printf("Messaggio ricevuto dal server: %s\n", socket_buffer);
                    
                    //Arrivo nuova lista
                    if(strcmp(mess_type_buff, "NBR_UPDT")==0){
                        //Numero di nuovi neighbor
                        int count;
                        int temp_n[2];
                        
                        temp_n[0] = -1;
                        temp_n[1] = -1;

                        printf("Parametri prima di sscanf: %d e %d\n", temp_n[0], temp_n[1]);

                        count = sscanf(socket_buffer, "%s %d %d", mess_type_buff, &temp_n[0], &temp_n[1]);

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
                        ack_UDP(listener_socket, "UPDT_ACK", server_port, "NBR_UPDT");
                    }
                
                    //Notifica chiusura server
                    if(strcmp(mess_type_buff, "SRV_EXIT")==0){
                        printf("Il server sta per chiudere\n");

                        //Fa qualcosa coi dati

                        //Invia ACK
                        ack_UDP(listener_socket, "S_XT_ACK", server_port, "SRV_EXIT");

                        //Chiude
                        printf("Chiusura peer\n");
                        close(listener_socket);
                        _exit(0);
                    }
                }
            
            }

            FD_CLR(listener_socket, &readset);
        
        }

    //}

    return 0;
}