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

#include "./util/msg.h"
#include "./util/util_c.h"

#define MAX_IN 40   //Massima lunghezza comando da terminale
#define ADDR_LEN 15 //Massima lunghezza stringa con indirizzo IP
#define DATE_LEN 10 //Lunghezza di una stringa contenente la data in formato dd:mm:yyyy
#define TIME_LEN 8
#define DATE_UPDATE_LEN 28
#define MESS_TYPE_LEN 8 //Lunghezza tipo messaggio UDP
#define LIST_MAX_LEN 21 //Massima lunghezza lista di vicini
#define SOCK_MAX_LEN 630
#define MAX_COMMAND 6
#define MAX_FILENAME_LEN 20
#define MAX_CONNECTED_PEERS 100
#define MAX_ENTRY_REP 16 //Non piu' di 1M di entries distinte
#define ENTR_W_TYPE 10
#define MAX_ENTRY_UPDATE 630 //Header, numero peer e lunghezza massima entry (lunghezza a 5 cifre di 99 peer con virgola, orario, tipo, numero)
#define MAX_SUM_ENTRIES 19 //Massimo totale aggregato: a 10 cifre
#define ALL_PEERS -1 //recv_UDP puo' ricevere da qualunque indirizzo
#define MAX_LOCK_LEN 14
#define MAX_PAST_AGGR 30 //heeader e due date

//Variabili
int my_port;
int listener_socket;    //Descrittore del socket di ascolto
struct sockaddr_in listener_addr;   //Struttura per gestire il socket di ascolto
socklen_t listener_addr_len;

struct sockaddr_in time_addr;
socklen_t time_addr_len;

int ret;    //Variabile di servizio
char stdin_buff[MAX_IN];    //Buffer per i comandi da standard input
char socket_buffer[SOCK_MAX_LEN];

//Identifica il server
int server_port;
int time_port;

int neighbor[2];   //Da usare per salvarci i vicini

//Variabili per gestire input da socket oppure da stdin
fd_set master;
fd_set readset;
int fdmax;

//Blocco dello stdin durante operazioni lunghe
extern int flag;


int main(int argc, char** argv){
    //All'inizio controllo se e' orario di chiusura
    flag = is_flag_up();
    //Pulizia set
    FD_ZERO(&master);
    FD_ZERO(&readset);

    my_port = atoi(argv[1]);

    //Creazione socket di ascolto
    listener_socket = prepare(&listener_addr, &listener_addr_len, my_port);

    //All'inizio nessun vicino
    neighbor[0] = -1;
    neighbor[1] = -1;

    //Identifica un peer non connesso
    server_port = -1;
    time_port = -1;

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

            //Se flag settato, non sono accettati comandi da stdin (durante una get o quando si cambiano i registri)
            if(flag){
                printf("Comandi da stdin non accettati, riprovare piu' tardi\n");
                FD_CLR(0, &readset);
                continue;
            }

            /*
                HELP
            */
            if(strcmp(command,"help")==0){
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
                
                //Start: non puo' essere eseguita se in corso una get o una stop
                if(check_g_lock() || check_s_lock()){
                    printf("Comando %s non eseguibile adesso, riprova piu' tardi\n", command);
                    server_port = -1;
                    continue;
                }

                //Invio richiesta di connessione e attendo ACK
                send_UDP(listener_socket, "CONN_REQ", MESS_TYPE_LEN, server_port, "CONN_ACK");

                //Ricevo porta del time server
                recv_UDP(listener_socket, recv_buffer, LIST_MAX_LEN, server_port, "TMR_PORT", "TPRT_ACK");
                sscanf(recv_buffer, "%s %d", temp_buffer, &time_port);

                //Ricevo lista di vicini
                recv_UDP(listener_socket, recv_buffer, LIST_MAX_LEN, server_port, "NBR_LIST", "LIST_ACK");
                ret = sscanf(recv_buffer, "%s %d %d", temp_buffer, &temp_n[0], &temp_n[1]);

                switch(ret){
                    case 1:
                        printf("Lista vuota, nessun vicino\n");
                        break;
                    case 2:
                        printf("Un vicino con porta %d\n", temp_n[0]);
                        neighbor[0] = temp_n[0];
                        break;
                    case 3:
                        printf("Due vicini con porta %d e %d\n", temp_n[0], temp_n[1]);
                        neighbor[0] = temp_n[0];
                        neighbor[1] = temp_n[1];
                        break;
                    default:
                        printf("Problema nella trasmissione della lista\n");
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

                //Controllo che nessuno stia eseguendo una get
                if(check_g_lock()){
                    printf("Comando %s non eseguibile, riprova piu' tardi\n", command);
                    continue;
                }

                ret = sprintf(new_entry, "%s %c", "NEW_ENTR", type);
                new_entry[ret] = '\0';

                send_UDP(listener_socket, new_entry, ret, server_port, "ENEW_ACK");

                
                insert_entry(type, quantity);
            }

            /*
                GET
            */
            else if(strcmp(command,"get")==0){
                char aggr;
                char type;
                char bound[2][DATE_LEN+1];
                int tot_entr;
                int peer_entr;
                int sum_entr;
                char get_buffer[MAX_SUM_ENTRIES]; //Ricevere il messaggio

                //Se peer non connesso non faccio nulla
                if(server_port == -1){
                    printf("Peer non connesso\n");
                    continue;
                }

                ret = sscanf(stdin_buff,"%s %c %c %s %s", command, &aggr, &type, bound[0], bound[1]);
                //Numero di parametri
                bound[0][DATE_LEN] = '\0';
                bound[1][DATE_LEN] = '\0';

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
                    if(!check_dates(bound[0], bound[1], aggr))
                        continue;
                }

                printf("Controlli superati!\n");

                //Se c'e' qualche altro peer che sta eseguendo la get, mi fermo
                if(get_lock()){
                    printf("Comando get non eseguibile in questo momento, riprova piu' tardi\n");
                    continue;
                }

                //Riempio i buffer delle date nel caso non siano state inserite dall'utente
                if(ret == 3){
                    strcpy(bound[0], "*");
                    strcpy(bound[1], "*");
                }

                //Se la data di fine e' oggi va bene l'asterisco
                if(is_today(bound[1]))
                    strcpy(bound[1], "*");


                //TUTTA QUESTA PARTE SERVE SOLO SE ANCHE I DATI DEL GIORNO VANNO CALCOLATI
                if(ret == 3 || strcmp(bound[1], "*") == 0){
                    //Invio richiesta al server
                    ret = sprintf(get_buffer, "%s %c", "ENTR_REQ", type);
                    get_buffer[ret] = '\0';
                    send_UDP(listener_socket, get_buffer, ret, server_port, "EREQ_ACK");
                    
                    //Ricevo risposta
                    recv_UDP(listener_socket, get_buffer, MAX_ENTRY_REP, server_port, "ENTR_REP", "EREP_ACK");
                    sscanf(get_buffer, "%s %d", command, &tot_entr); //command non serve piu'

                    peer_entr = count_entries(type);

                    printf("Entries nel server: %d; entries qui: %d\n", tot_entr, peer_entr);

                    //Se nessuna entry, inutile continuare
                    if(!tot_entr)
                        printf("Nessun dato aggregato da calcolare\n");

                    //Se ho tutti i dati che servono, eseguo il calcolo e lo scrivo
                    else if(tot_entr == peer_entr){
                        sum_entr = sum_entries(type);
                        write_aggr(tot_entr, sum_entr, type);
                    }
                    
                    //Altrimenti
                    else {
                        //Se non ho vicini non posso calcolare nulla
                        if(neighbor[0] == -1 && neighbor[1] == -1)
                            printf("Errore insolubile, impossibile calcolare il dato richiesto\n");
                        
                        else {
                            printf("Devo chiedere informazioni ai miei vicini\n");
                            //Controllo se qualche peer ha il dato aggregato pronto
                            
                            //Posso riciclare il buffer get_buffer
                            ret = sprintf(get_buffer, "%s %d %d %c", "AGGR_REQ", my_port, tot_entr, type);
                            get_buffer[ret] = '\0';
                            
                            send_UDP(listener_socket, get_buffer, ret, neighbor[0], "AREQ_ACK");
                            
                            //Posso sfruttare get_buffer
                            recv_UDP(listener_socket, get_buffer, MAX_SUM_ENTRIES, ALL_PEERS, "AGGR_REP", "AREP_ACK");

                            printf("Ricevuto %s\n", get_buffer);

                            //Posso riciclare il buffer command
                            sscanf(get_buffer, "%s %d", command, &sum_entr);
                            
                            if(sum_entr != 0){
                                printf("Ho ottenuto il dato che cercavo\n");
                                write_aggr(tot_entr, sum_entr, type);
                            }
                            else{
                                printf("Nessuno ha gia' pronto il dato cercato\n");
                                //Necessario inviare a tutti la richiesta di nuove entries
                                ret = sprintf(get_buffer, "%s %d %c", "EP2P_REQ", my_port, type);
                                get_buffer[ret] = '\0';
                                printf("Faccio partire il giro di richieste di entries\n");
                                send_UDP(listener_socket, get_buffer, ret, neighbor[0], "2REQ_ACK");
                                //Aspetto tutte le entries che mi mancano
                                wait_for_entries(peer_entr, tot_entr, type);

                                printf("Tutte le entries necessarie sono arrivate a destinazione\n");

                                ret = (neighbor[1] != -1) ? 1 : 0;
                                //Ricevo HLT per verificare che sia tutto corretto
                                recv_UDP(listener_socket, get_buffer, MESS_TYPE_LEN, neighbor[ret], "EP2P_HLT", "2HLT_ACK");
                                //Eseguo il calcolo
                                sum_entr = sum_entries(type);
                                //Lo riporto sul file
                                write_aggr(tot_entr, sum_entr, type);
                            }

                        }
                    }
                }

                //Che abbia calcolato o no i dati del giorno, proseguo

                //Se la data di inizio e' oggi, ho finito perche' veniva solo richiesto il totale di oggi
                if(!is_today(bound[0]) || ret == 3){
                    char get_past_aggr[MAX_PAST_AGGR];
                    int aggr_entries;

                    printf("Mi serve lo storico dei dati\n");
                    ret = sprintf(get_past_aggr, "%s %s %s", "AGGR_GET", bound[0], bound[1]);
                    get_past_aggr[ret] = '\0';
                    send_UDP(listener_socket, get_past_aggr, ret, time_port, "AGET_ACK");
                    
                    printf("Aspetto i dati dal time server\n");
                    //Sfrutto get_past_aggr
                    recv_UDP(listener_socket, get_past_aggr, MAX_PAST_AGGR, time_port, "AGGR_CNT", "ACNT_ACK");
                    sscanf(get_past_aggr+9, "%d", &aggr_entries);
                    printf("Entrate da ricevere: %d\n", aggr_entries);

                    while(aggr_entries){
                        //Sfrutto stdin_buffer che e' della lunghezza giusta
                        recv_UDP(listener_socket, stdin_buff, 40, time_port, "AGGR_ENT", "AENT_ACK");
                        printf("Ricevuta stringa %s\n", stdin_buff+9);
                    }

                }


                //Rilascio risorsa per la get
                send_UDP(listener_socket, "GET_UNLK", MESS_TYPE_LEN, server_port, "GNLK_ACK");

            }
            
            /*
                STOP
            */
            else if(strcmp(command,"stop")==0){
                printf("Hai digitato comando di stop\n");
                //Controllo che nessuno stia eseguendo una get
                if(check_g_lock()){
                    printf("Comando %s non eseguibile, riprova piu' tardi\n", command);
                    continue;
                }

                if(stop_lock()){
                    printf("Un vicino sta uscendo dalla rete, attendi\n");
                    continue;
                }


                //Controllo che la connessione esista
                if(server_port == -1)
                    printf("Il peer non e' connesso al DS. Uscita\n");
                
                else{
                    //Invio eventuali entries mancanti agli eventuali vicini
                    send_double_missing_entries();
                        
                    send_UDP(listener_socket, "CLT_EXIT", MESS_TYPE_LEN, server_port, "C_XT_ACK");
                }

                //Rilascio risorsa per la stop
                send_UDP(listener_socket, "STP_UNLK", MESS_TYPE_LEN, server_port, "SNLK_ACK");

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
            char mess_type_buffer[MESS_TYPE_LEN+1];

            util_port = s_recv_UDP(listener_socket, socket_buffer, SOCK_MAX_LEN);
            //Leggo il tipo del messaggio
            sscanf(socket_buffer, "%s", mess_type_buffer);
            mess_type_buffer[MESS_TYPE_LEN] = '\0';

            if(util_port == server_port){
                printf("Messaggio ricevuto dal server: %s\n", socket_buffer);
                
                //Arrivo nuova lista
                if(strcmp(mess_type_buffer, "NBR_UPDT")==0){
                    //Numero di nuovi neighbor
                    int count;
                    int temp_n[2];

                    //Invio ACK
                    ack_UDP(listener_socket, "UPDT_ACK", server_port, socket_buffer, strlen(socket_buffer));
                    
                    temp_n[0] = -1;
                    temp_n[1] = -1;

                    printf("Parametri prima di sscanf: %d e %d\n", temp_n[0], temp_n[1]);

                    count = sscanf(socket_buffer, "%s %d %d", mess_type_buffer, &temp_n[0], &temp_n[1]);

                    printf("Parametri dopo sscanf: %d e %d\n", temp_n[0], temp_n[1]);
                    
                    //Modifica ai vicini
                    switch(count){
                        case 1:
                            //DEBUG
                            printf("Sono rimasto l'unico peer\n");
                            neighbor[0] = -1;
                            neighbor[1] = -1;
                            break;
                        case 2:
                            printf("Un vicino con porta %d\n", temp_n[0]);
                            neighbor[0] = temp_n[0];
                            neighbor[1] = -1;
                            break;
                        case 3:
                            printf("Due vicini con porta %d e %d\n", temp_n[0], temp_n[1]);
                            neighbor[0] = temp_n[0];
                            neighbor[1] = temp_n[1];
                            break;
                        default:
                            printf("Questa riga di codice non dovrebbe mai andare in esecuzione\n");
                            break;
                    }
                }
            
                //Notifica chiusura server
                if(strcmp(mess_type_buffer, "SRV_EXIT")==0){
                    printf("Il server sta per chiudere\n");

                    //Fa qualcosa coi dati

                    //Invia ACK
                    ack_UDP(listener_socket, "S_XT_ACK", server_port, socket_buffer, strlen(socket_buffer));

                    //Chiude
                    printf("Chiusura peer\n");
                    close(listener_socket);
                    _exit(0);
                }
            
                //FLAG_SET
                if(strcmp(mess_type_buffer, "FLAG_SET") == 0){
                    ack_UDP(listener_socket, "FSET_ACK", server_port, socket_buffer, strlen(socket_buffer));
                    printf("Standard input bloccato, non eseguire comandi per un po'\n");
                    flag = 1;
                }

                //FLAG_RST
                if(strcmp(mess_type_buffer, "FLAG_RST") == 0){
                    ack_UDP(listener_socket, "FRST_ACK", server_port, socket_buffer, strlen(socket_buffer));
                    printf("Standard input sbloccato, di nuovo possibile eseguire comandi\n");
                    flag = 0;
                }

            }
        
            else if(util_port == neighbor[0] || util_port == neighbor[1]){
                printf("Messaggio %s arrivato dal vicino %d\n", socket_buffer, util_port);
                //Vicino chiede se esiste dato aggregato
                if(strcmp(mess_type_buffer, "AGGR_REQ") == 0){
                    int req_port;
                    int act_entries;
                    char type;
                    int aggr;
                    char answer[MAX_ENTRY_REP];

                    //Invio ack
                    ack_UDP(listener_socket, "AREQ_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    
                    //Leggo il messaggio
                    sscanf(socket_buffer, "%s %d %d %c", mess_type_buffer, &req_port, &act_entries, &type);

                    aggr = check_aggr(act_entries, type);

                    if(aggr == 0){
                        printf("Non ho la risposta alla richiesta\n");
                        if(neighbor[1] == -1){
                            if(neighbor[0] != req_port)
                                printf("Errore insolubile, crash della rete\n");
                            else {
                                printf("Invio direttamente la risposta negativa a %d\n", neighbor[0]);
                                strcpy(answer, "AGGR_REP 0");
                                send_UDP(listener_socket, answer, strlen(answer), neighbor[0], "AREP_ACK");
                            }
                        }
                        else {
                            if(neighbor[0] == req_port){
                                printf("Invio risposta negativa a %d", neighbor[0]);
                                strcpy(answer, "AGGR_REP 0");
                                send_UDP(listener_socket, answer, strlen(answer), neighbor[0], "AREP_ACK");
                            }
                            else {
                                printf("Inoltro la richiesta a %d\n", neighbor[0]);
                                send_UDP(listener_socket, socket_buffer, strlen(socket_buffer), neighbor[0], "AREQ_ACK");
                            }
                        }

                    }
                    else {
                        printf("Ho la risposta che cerca il peer %d\n", req_port);
                        ret = sprintf(answer, "%s %d", "AGGR_REP", aggr);
                        answer[ret] = '\0';
                        send_UDP(listener_socket, answer, ret, req_port, "AREP_ACK");
                    }
                }
                
                //Messaggio di richiesta entries
                if(strcmp(mess_type_buffer, "EP2P_REQ") == 0){
                    char r_type;
                    //Invio ack
                    ack_UDP(listener_socket, "2REQ_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    //Leggo il numero del mittente riciclando util_port
                    sscanf(socket_buffer, "%s %d %c", mess_type_buffer, &util_port, &r_type);
                    //Invio tutte le entries mancanti
                    send_missing_entries(util_port, r_type, "EP2P_REP", "2REP_ACK");

                    //Inoltro richiesta o invio alt
                    if(neighbor[0] == util_port){
                        printf("Invio HLT al peer %d che ha fatto partire il giro di richieste\n", neighbor[0]);
                        send_UDP(listener_socket, "EP2P_HLT", MESS_TYPE_LEN, neighbor[0], "2HLT_ACK");
                    }
                    else {
                        printf("Inoltro la richiesta partita da %d al peer %d\n", util_port, neighbor[0]);
                        send_UDP(listener_socket, socket_buffer, strlen(socket_buffer), neighbor[0], "2REQ_ACK");
                    }
                }

                //Nuova entry inviata al momento della chiusura
                if(strcmp(mess_type_buffer, "EP2P_NEW") == 0){
                    ack_UDP(listener_socket, "2NEW_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    //Inserisco entry al volo
                    printf("Inserisco entry %s\n", socket_buffer+9);
                    insert_entry_string(socket_buffer+9);
                }
            
            }

            else if(util_port == time_port){
                printf("Ricevuto messaggio %s dal time server %d\n", socket_buffer, util_port);
                //Blocco stdin
                if(strcmp(mess_type_buffer, "FLAG_SET") == 0){
                    ack_UDP(listener_socket, "FSET_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    printf("Bloccati input da stdin\n");
                    flag = 1;
                }
                
                //Invio entries al time server
                if(strcmp(mess_type_buffer, "FLAG_REQ") == 0){
                    ack_UDP(listener_socket, "FREQ_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    printf("Ho ricevuto richiesta dal time server\n");
                    //Sfrutto socket buffer per inviare i prossimi messaggi
                    ret = sprintf(socket_buffer, "%s %d", "FLAG_NUM", count_entries('a'));
                    socket_buffer[ret] = '\0';
                    send_UDP(listener_socket, socket_buffer, ret, time_port, "FNUM_ACK");
                    //Invio tutte le entrate
                    send_missing_entries(time_port, 'a', "FLAG_ENT", "FENT_ACK");

                    printf("Entries inviate al time server correttamente\n");

                }

                //Ricezione del totale
                if(strcmp(mess_type_buffer, "FLAG_TOT") == 0){
                    ack_UDP(listener_socket, "FTOT_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    
                    //Mantenere lo storico degli aggregati in locale: si puo' fare, ma rende molto piu' lenta la procedura
                    //register_daily_tot(socket_buffer+9);

                    printf("Arrivati gli aggregati giornalieri\n");
                    print_daily_aggr(socket_buffer+9);
                }

                //BSlocco stdin
                if(strcmp(mess_type_buffer, "FLAG_RST") == 0){
                    ack_UDP(listener_socket, "FRST_ACK", util_port, socket_buffer, strlen(socket_buffer));
                    printf("Sbloccati input da stdin\n");
                    flag = 0;
                }
            }

            FD_CLR(listener_socket, &readset);

        }
    
    }

    return 0;
}