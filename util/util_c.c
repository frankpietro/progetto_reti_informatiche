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

//Gestione data e ora correnti
#include "retr_time.h"
//Scambio di messaggi
#include "msg.h"
//Costanti
#include "const.h"

extern int my_port;
extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];
extern int neighbor[2];
extern int listener_socket;
extern int server_port;

//Elenco dei comandi disponibili lato client
void comandi_client(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("start <DS_addr> <DS_port> --> richiede al Discovery Server connessione alla rete\n");
    printf("add <type> <quantity> --> aggiunge una entry nel registro del peer\n");
    printf("get <aggr> <type> [<date1> <date2>] --> richiede un dato aggregato\n");
    printf("stop --> richiede disconnessione dalla rete\n");
}

//Ulteriore ausilio per comprendere i comandi
void help_client(int i){
    switch(i){
        case 0: //help
            printf("help --> mostra elenco comandi e significato\n");
            printf("Parametri: nessuno\n");
            break;
        case 1: //start
            printf("start <DS_addr> <DS_port> --> richiede al Discovery Server connessione alla rete\n");
            printf("Parametri:\n");
            printf(" - DS_addr --> stringa con IP in formato presentazione\n");
            printf(" - DS_port --> porta sulla quale gira il processo server, compresa tra 1024 e 65535\n");
            break;
        case 2:
            printf("add <type> <quantity> --> aggiunge una entry nel registro giornaliero del peer\n");
            printf("Parametri:\n");
            printf(" - type --> carattere per indicare se tamponi o nuovi casi (inserire rispettivamente 't' o 'n')\n");
            printf(" - quantity --> quantita' del dato da aggiungere (inserire intero positivo)\n");
            break;
        case 3:
            printf("get <aggr> <type> [<date1> <date2>] --> richiede un dato aggregato\n");
            printf("Parametri:\n");
            printf(" - aggr --> tipo di aggregazione, se totale o variazioni giornaliere (inserire rispettivamente 't' o 'v')\n");
            printf(" - type --> carattere per indicare se tamponi o nuovi casi (inserire rispettivamente 't' o 'n')\n");
            printf(" - date1 --> lower bound della ricerca; inserire * se non si vuole lower bound\n");
            printf(" - date2 --> upper bound della ricerca; inserire * se non si vuole lower bound\n");
            printf("Nota: se non si vuole nessun bound, non inserire * *, ma lasciare senza parametri\n");
            break;
        case 4:
            printf("stop --> richiede disconnessione dalla rete\n");
            printf("Parametri: nessuno\n");
            break;
        default:
            printf("Errore nella chiamata\n");
    }
}

/*
Validazione input get
*/

/*Controllo che la seconda non sia minore della prima in caso di totale
e sia strettamente maggiore in caso di variazione*/
int is_real_period(int *date1, int *date2, char aggr){
    return (date1[0] < date2[0] || (date1[0] == date2[0] && (date1[1] < date2[1] || (date1[1] == date2[1] && (date1[2] < date2[2] || (date1[2] == date2[2] && aggr == 't'))))));
}

//Controllo la validita' della data
int is_valid_date(int y, int m, int d){
    int c_date[3];
    int this_date[3];
    this_date[0] = y;
    this_date[1] = m;
    this_date[2] = d;
    //Basic
    if(y < MIN_YEAR || m < 1 || m > 12 || d < 1 || d > 31){
        printf("E1\n");
        return 0;
    }

    retrieve_time();
    sscanf(current_d, "%d:%d:%d", &c_date[0], &c_date[1], &c_date[2]);
    //Sfrutto la funzione is_real_period
    if(!is_real_period(this_date, c_date, 't')){
        printf("E2\n");
        return 0;
    }
    //Advanced
    if(m == 4 || m == 6 || m == 9 || m == 11)
        return (d <= 30);
    //Extreme
    if(m == 2){
        if(((y%4 == 0) && (y%100 != 0)) || (y%400 == 0))
            return (d <= 29);
        else
            return (d <= 28); 
    }

    return 1;
}

//Controllo che le date input della get siano valide
int check_dates(char *date1, char *date2, char aggr){
    int date[2][3];
    int ret[2];
    //Controllo sulla prima data
    //Conversione da dd:mm:yyyy a yyyy:mm:dd
    ret[0] = sscanf(date1, "%d:%d:%d", &date[0][2], &date[0][1], &date[0][0]);
    if(!(ret[0] == 3 || strcmp(date1, "*") == 0)){
        printf("Data 1 non conforme al tipo di input richiesto");
        return 0;
    }
    if(ret[0] == 3 && !is_valid_date(date[0][0], date[0][1], date[0][2])){
        printf("Errore nell'inserimento della prima data\n");
        return 0;
    }
    //Controllo sulla seconda data
    //Conversione da dd:mm:yyyy a yyyy:mm:dd
    ret[1] = sscanf(date2, "%d:%d:%d", &date[1][2], &date[1][1], &date[1][0]);
    if(!(ret[1] == 3 || strcmp(date2, "*") == 0)){
        printf("Data 2 non conforme al tipo di input richiesto");
        return 0;
    }
    if(ret[1] == 3 && !is_valid_date(date[1][0], date[1][1], date[1][2])){
        printf("Errore nell'inserimento della seconda data\n");
        return 0;
    }
    //Se entrambe le date sono * non va bene
    if(strcmp(date1, "*") == 0 && strcmp(date2, "*") == 0){
        printf("Non si puo' inserire due volte *.\nLasciare vuoti i campi data se si desidera l'intero periodo\n");
        return 0;
    }
    /*Se entrambe le date sono presenti,
    controllo che la seconda non sia minore della prima in caso di totale
    e sia strettamente maggiore in caso di variazione*/
    if(ret[0] == 3 && ret[1] == 3){
        if(!is_real_period(date[0], date[1], aggr)){
            printf("Periodo scelto non coerente\n");
            return 0;
        }
    }

    //Tutto ok
    return 1;
}

/*
Fine validazione input get
*/

//Inserisce entry nel file di entries
void insert_entry(char type, int quantity){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    printf("Filename: %s\n", filename);

    fd = fopen(filename, "a");
    fprintf(fd, "%s %c %d %d;\n", current_t, type, quantity, my_port);
    fclose(fd);

    printf("Entry inserita!\n");
}

//Inserisce una intera entry passata come stringa nel file di entries
void insert_entry_string(char* entry){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    fd = fopen(filename, "a");
    fprintf(fd, "%s\n", entry);
    fclose(fd);

    printf("Entry inserita!\n");
}

//Conta il numero di entries presente nel proprio database di un certo tipo ('a': le conta tutte)
int count_entries(char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int tot;
    char entry_type;
    char u_time[TIME_LEN];
    int num;
    char tot_peers[6*MAX_CONNECTED_PEERS];

    tot = 0;

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    //printf("Filename: %s\n", filename);

    fd = fopen(filename, "r");
    if(fd == NULL)
        return 0;
    else {
        while(fscanf(fd, "%s %c %d %s\n", u_time, &entry_type, &num, tot_peers) == 4)
            tot += (type == entry_type || type == 'a');
    }
    fclose(fd);
    return tot;
}

//Somma le entries in un file 
int sum_entries(char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int tot;
    char entry_type;
    char u_time[TIME_LEN];
    int num;
    char tot_peers[6*MAX_CONNECTED_PEERS];

    tot = 0;

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    //printf("Filename: %s\n", filename);

    fd = fopen(filename, "r");
    if(fd == NULL)
        return 0;
    else {
        while(fscanf(fd, "%s %c %d %s\n", u_time, &entry_type, &num, tot_peers) == 4)
            if(entry_type == type)
                tot += num;
    }
    fclose(fd);
    return tot;
}

//Scrive l'aggregato su un file
void write_aggr(int count, int sum, char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];

    printf("Numero di ");
    if(type == 't')
        printf("tamponi");
    else
        printf("nuovi casi");
    printf(" odierni: %d\n", sum);

    retrieve_time();

    sprintf(filename, "%s%c_%s_%d.txt", "./peer_dir/aggr_", type, current_d, my_port);

    //printf("Filename: %s\n", filename);

    fd = fopen(filename, "w");
    fprintf(fd, "%d %d", count, sum);
    fclose(fd);
}

//Controlla se il peer ha a disposizione un aggregato gia' pronto: se si' ritorna il dato richiesto
int check_aggr(int entries, char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int count;
    int sum;

    retrieve_time();

    sprintf(filename, "%s%c_%s_%d.txt", "./peer_dir/aggr_", type, current_d, my_port);

    //printf("Filename: %s\n", filename);

    fd = fopen(filename, "r");
    if(fd == NULL)
        return 0;

    fscanf(fd, "%d %d", &count, &sum);
    fclose(fd);
    return (count == entries) ? sum : 0;
}

//Controllo che un'entry non sia gia' nel file
int is_entry_in(char* entry){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    char tm[TIME_LEN];
    char t;
    int q;
    char p[6*MAX_CONNECTED_PEERS];
    char e[MAX_ENTRY_UPDATE];
    int ret;

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    fd = fopen(filename, "r");
    if(fd == NULL)
        return 0;
    //Scorro tutto il file per vedere se la trovo
    while(fscanf(fd, "%s %c %d %s", tm, &t, &q, p) != EOF){
        ret = sprintf(e, "%s %c %d %s", tm, t, q, p);
        e[ret] = '\0';
        if(strcmp(entry, e) == 0){
            printf("Entry gia' presente, da non inserire\n");
            return 1;
        }
    }
    //Se arrivo qui non c'e'

    return 0;
}

//Riceve tutte le entrate che mancavano al momento della chiamata della get e le aggiunge al file delle entrate
void wait_for_entries(int peer_entr, int tot_entr, char type){
    int ret;
    char get_buffer[MAX_ENTRY_UPDATE];
    char command[MESS_TYPE_LEN];
    char util_buffer[TIME_LEN];
    char r_type;
    int r_quantity;
    char entry_ins_buffer[MAX_ENTRY_UPDATE];
    char control_buffer[7];
    char* isIn;

    while(peer_entr < tot_entr){
        //Nuove entries: possono arrivare da tutti
        ret = (neighbor[1] != -1) ? 1 : 0;
        recv_UDP(listener_socket, get_buffer, MAX_ENTRY_UPDATE, ALL_PEERS, "EP2P_REP", "2REP_ACK");

        printf("Ricevuta entry %s\n", get_buffer);
        
        sscanf(get_buffer, "%s %s %c %d %s", command, util_buffer, &r_type, &r_quantity, entry_ins_buffer);
        //Controlli: non dovrebbero mai fallire
        ret = sprintf(control_buffer, "%d;", my_port);
        control_buffer[ret] = '\0';
        printf("Stringa di controllo: %s\n", control_buffer);
        isIn = strstr(entry_ins_buffer, control_buffer);
        if(!(r_type == type && isIn != NULL)){
            printf("Errore insolubile nella ricezione, arrivato pacchetto corrotto\n");
            continue;
        }
        ret = sprintf(get_buffer, "%s %c %d %s", util_buffer, r_type, r_quantity, entry_ins_buffer);
        get_buffer[ret] = 0;
        if(!is_entry_in(get_buffer)){
            insert_entry_string(get_buffer);
            peer_entr++;
        }
    }
}

//Invia tutte le entries che mancano al peer richiedente e che sono nel suo database
void send_missing_entries(int req_port, char type, char* header, char* ack){
    FILE *fd, *temp;
    char filename[MAX_FILENAME_LEN];
    char e_time[TIME_LEN];
    char e_type;
    int e_quantity;
    char e_peers[6*MAX_CONNECTED_PEERS];
    char whole_entry[MAX_ENTRY_UPDATE];
    char check[7];
    int ret;

    ret = sprintf(check, "%d;", req_port);
    check[ret] = '\0';

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    printf("Scorro tutte le entries\n");

    fd = fopen(filename, "r");
    if(fd == NULL){
        printf("Nessuna entry\n");
        return;
    }
    //Quando trovo un'entry devo aggiornarla e riscriverla
    temp = fopen("./peer_dir/temp.txt", "w");
    //Scorro tutte le entries
    while(fscanf(fd, "%s %c %d %s", e_time, &e_type, &e_quantity, e_peers) != EOF){
        //Se ne trovo una del tipo richiesto e non posseduta dal richiedente
        if((e_type == type || type == 'a') && strstr(e_peers, check) == NULL){
            printf("Trovata entry da inviare\n");
            //Aggiungo il suo numero di porta tra i peer che possiedono quella entry
            strcat(e_peers, check);
            //Gliela mando
            ret = sprintf(whole_entry, "%s %s %c %d %s", header, e_time, e_type, e_quantity, e_peers);
            whole_entry[ret] = '\0';
            send_UDP(listener_socket, whole_entry, ret, req_port, ack);
        }
        //Se quell'entry e' gia' presente la copio e basta
        else {
            ret = sprintf(whole_entry, "%s %s %c %d %s", header, e_time, e_type, e_quantity, e_peers);
            whole_entry[ret] = '\0';
        }
        fprintf(temp, "%s\n", whole_entry+9);
    }

    fclose(fd);
    fclose(temp);
    remove(filename);
    rename("./peer_dir/temp.txt", filename);
    
    printf("Fine delle entries da inviare\n");
}

//Invia tutte le entries che mancano al peer richiedente e che sono nel suo database
void send_double_missing_entries(){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    char e_time[TIME_LEN];
    char e_type;
    int e_quantity;
    char e_peers[6*MAX_CONNECTED_PEERS];
    char whole_entry[MAX_ENTRY_UPDATE];
    char check1[7];
    char check2[7];
    char check3[13];
    int ret;
    char *bool1, *bool2;

    if(neighbor[0] == -1)               
        return;                

    if(neighbor[1] == -1){
        send_missing_entries(neighbor[0], 'a', "EP2P_NEW", "2NEW_ACK");
        return;
    }

    ret = sprintf(check1, "%d;", neighbor[0]);
    check1[ret] = '\0';
    ret = sprintf(check2, "%d;", neighbor[1]);
    check2[ret] = '\0';
    ret = sprintf(check3, "%d;%d;", neighbor[0], neighbor[1]);
    check3[ret] = '\0';

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    printf("Scorro tutte le entries\n");

    fd = fopen(filename, "r");
    if(fd == NULL){
        printf("Nessuna entry\n");
        return;
    }
    //Scorro tutte le entries
    while(fscanf(fd, "%s %c %d %s", e_time, &e_type, &e_quantity, e_peers) != EOF){
            printf("Trovata entry da inviare\n");
            //Decido a quale peer va inviata
            bool1 = strstr(e_peers, check1);
            bool2 = strstr(e_peers, check2);

            //Caso 1: manca a entrambi
            if(bool1 == NULL && bool2 == NULL)
                strcat(e_peers, check3);
            //Caso 2: manca a 2
            if(bool1 != NULL && bool2 == NULL)
                strcat(e_peers, check2);
            //Caso 3: manca a 1
            if(bool1 == NULL && bool2 != NULL)
                strcat(e_peers, check1);

            //Mando la entry a chi di dovere
            ret = sprintf(whole_entry, "%s %s %c %d %s", "EP2P_NEW", e_time, e_type, e_quantity, e_peers);
            whole_entry[ret] = '\0';
            if(bool1 == NULL)
                send_UDP(listener_socket, whole_entry, ret, neighbor[0], "2NEW_ACK");
            if(bool2 == NULL)
                send_UDP(listener_socket, whole_entry, ret, neighbor[1], "2NEW_ACK");
        
    }

    fclose(fd);
    
    printf("Fine delle entries da inviare\n");
}

//Controlla se una data e' quella odierna
int is_today(char* bound_date){
    int b_date[3];
    int c_date[3];

    retrieve_time();
    sscanf(bound_date, "%d:%d:%d", &b_date[2], &b_date[1], &b_date[0]);
    sscanf(current_d, "%d:%d:%d", &c_date[0], &c_date[1], &c_date[2]);

    return (b_date[0] == c_date[0] && b_date[1] == c_date[1] && b_date[2] == c_date[2]);
}

//Controlla che nessuno stia eseguendo la get
int check_g_lock(){
    char lock_buffer[MAX_LOCK_LEN];
    char command[MESS_TYPE_LEN];
    int flag;

    //Invio richiesta al server
    send_UDP(listener_socket, "IS_G_LCK", MESS_TYPE_LEN, server_port, "ISGL_ACK");
    //Aspetto risposta con numero di flag che sta tenendo la risorsa oppure 0
    recv_UDP(listener_socket, lock_buffer, MAX_LOCK_LEN, server_port, "GET_MUTX", "GMTX_ACK");
    sscanf(lock_buffer, "%s %d", command, &flag);
    //Lo ritorno
    return flag;
}

//Controlla che nessuno stia eseguendo la get da una get --> acquisisce mutua esclusione
int get_lock(){
    char lock_buffer[MAX_LOCK_LEN];
    char command[MESS_TYPE_LEN];
    int flag;

    //Invio richiesta al server
    send_UDP(listener_socket, "GET_LOCK", MESS_TYPE_LEN, server_port, "GLCK_ACK");
    //Aspetto risposta con numero di flag che sta tenendo la risorsa oppure 0
    recv_UDP(listener_socket, lock_buffer, MAX_LOCK_LEN, server_port, "GET_MUTX", "GMTX_ACK");
    sscanf(lock_buffer, "%s %d", command, &flag);
    //Lo ritorno
    return flag;
}

//Controlla che nessuno stia eseguendo la stop
int check_s_lock(){
    char lock_buffer[MAX_LOCK_LEN];
    char command[MESS_TYPE_LEN];
    int flag;

    //Invio richiesta al server
    send_UDP(listener_socket, "IS_S_LCK", MESS_TYPE_LEN, server_port, "ISSL_ACK");
    //Aspetto risposta con numero di flag che sta tenendo la risorsa oppure 0
    recv_UDP(listener_socket, lock_buffer, MAX_LOCK_LEN, server_port, "STP_MUTX", "SMTX_ACK");
    sscanf(lock_buffer, "%s %d", command, &flag);
    //Lo ritorno
    return flag;
}

//Prova ad acquisire mutua esclusione sulla stop
int stop_lock(){
    char lock_buffer[MAX_LOCK_LEN];
    char command[MESS_TYPE_LEN];
    int flag;

    //Invio richiesta al server
    send_UDP(listener_socket, "STP_LOCK", MESS_TYPE_LEN, server_port, "SLCK_ACK");
    //Aspetto risposta con numero di flag che sta tenendo la risorsa oppure 0
    recv_UDP(listener_socket, lock_buffer, MAX_LOCK_LEN, server_port, "STP_MUTX", "SMTX_ACK");
    sscanf(lock_buffer, "%s %d", command, &flag);
    //Lo ritorno
    return flag;
}

//Registra i totali del giorno
void register_daily_tot(char* aggr){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int ret;

    ret = sprintf(filename, "%s_%d.txt", "./peer_dir/tot_aggr", my_port);
    filename[ret] = '\0';

    fd = fopen(filename, "a");
    fprintf(fd, "%s %s\n", current_d, aggr);
    fclose(fd);
    //File giornalieri: non servono piu'
    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);
    remove(filename);
    sprintf(filename, "%s_%c_%s_%d.txt", "./peer_dir/aggr", 't', current_d, my_port);
    remove(filename);
    sprintf(filename, "%s_%c_%s_%d.txt", "./peer_dir/aggr", 'n', current_d, my_port);
    remove(filename);
}

//Stampo gli aggregati giornalieri e pulisco la memoria
void print_daily_aggr(char* entr){
    int tot[2];
    char filename[MAX_FILENAME_LEN];

    sscanf(entr, "%d %d", &tot[0], &tot[1]);
    printf("Tamponi di oggi: %d;\nnuovi casi di oggi: %d\n", tot[0], tot[1]);

    //File giornalieri: non servono piu'
    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);
    remove(filename);
    sprintf(filename, "%s_%c_%s_%d.txt", "./peer_dir/aggr", 't', current_d, my_port);
    remove(filename);
    sprintf(filename, "%s_%c_%s_%d.txt", "./peer_dir/aggr", 'n', current_d, my_port);
    remove(filename);
}

//Controllo iniziale su flag del peer
int is_flag_up(){
    char *pointer;

    pointer = strstr(current_t, "17:5");
    return (pointer != NULL) ? 1 : 0;
}

//Inserisce in un file temporaneo le entries dal time server
void insert_temp(char* entry){
    FILE *fd;

    fd = fopen("temp_entries.txt", "a");
    fprintf(fd, "%s\n", entry);
    fclose(fd);
}

//Converte una data in formato dd:mm:yyyy
void convert_out_date(char* date){
    int d,m,y;

    sscanf(date, "%d:%d:%d", &y, &m, &d);
    sprintf(date, "%d:%d:%d", d,m,y);
}

//Stampa i risultati finali della get
void print_results(char aggr, char type, int sum_today, char* date1, char* date2){
    FILE *fd1;
    

    //Caso 1: richiesto totale
    if(aggr == 't'){
        int sum;
        int t,c;
        char u_date[DATE_LEN];

        date1 = (strcmp(date1, "*") == 0) ? "1:1:2021" : date1;
        date2 = (strcmp(date2, "*") == 0) ? u_date : date2;
        
        sum = 0;
        fd1 = fopen("temp_entries.txt", "r");
        //Somma il valore giusto
        while(fscanf(fd1, "%s %d %d", u_date, &t, &c) != EOF){
            if(type == 't')
                sum += t;
            else
                sum += c;
        }

        fclose(fd1);

        if(sum_today != -1)
            sum += sum_today;
        
        strcpy(u_date, current_d);
        convert_out_date(u_date);

        //Stampa
        printf("Totale ");
        if(type == 't')
            printf("tamponi ");
        else
            printf("nuovi casi ");
        printf("da %s a %s: %d\n", (strcmp(date1, "*") == 0) ? "1:1:2021" : date1, (strcmp(date2, "*") == 0) ? u_date : date2, sum);
    }

    //Caso 2: richiesta variazione
    else {
        FILE *fd2;
        //int var;
        int t[2];
        int c[2];
        char u_date[2][DATE_LEN+1];

        fd1 = fopen("temp_entries.txt", "r");
        fd2 = fopen("temp_entries.txt", "r");

        printf("Elenco variazioni di ");
        if(type == 't')
            printf("tamponi ");
        else
            printf("nuovi casi ");        
        printf("richiestse:\n");
        //Faccio scorrere i puntatori a distanza di 1
        fscanf(fd1, "%s %d %d", u_date[0], &t[0], &c[0]);
        u_date[0][DATE_LEN] = '\0';
        while(fscanf(fd1, "%s %d %d", u_date[0], &t[0], &c[0]) != EOF){
            u_date[0][DATE_LEN] = '\0';
            fscanf(fd2, "%s %d %d", u_date[1], &t[1], &c[1]);
            u_date[1][DATE_LEN] = '\0';
            convert_out_date(u_date[1]);
            convert_out_date(u_date[0]);
            printf(" - da %s a %s: %d\n", u_date[1], u_date[0], (type == 't') ? t[0] - t[1] : c[0] - c[1]);
        }

        fscanf(fd2, "%s %d %d", u_date[1], &t[1], &c[1]);
        u_date[1][DATE_LEN] = '\0';

        fclose(fd1);
        fclose(fd2);

        if(sum_today != -1){
            strcpy(u_date[0], current_d);
            convert_out_date(u_date[0]);
            convert_out_date(u_date[1]);
            printf(" - da %s a %s: %d\n", u_date[1], u_date[0], (type == 't') ? sum_today - t[1] : sum_today - c[1]);
        }


    }
    
    remove("temp_entries.txt");
}
