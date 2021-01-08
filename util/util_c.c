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
#include <signal.h>
#include <sys/wait.h>
#include "retr_time.h"

#define DATE_LEN 10
#define TIME_LEN 8
#define MIN_YEAR 1990
#define MAX_CONNECTED_PEERS 100
#define MAX_FILENAME_LEN 31

extern int my_port;
extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];


//Elenco dei comandi disponibili lato client
void comandi_client(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("start <DS_addr> <DS_port> --> richiede al Discovery Server connessione alla rete\n");
    printf("add <type> <quantity> --> aggiunge una entry nel registro del peer\n");
    printf("get <aggr> <type> [<date1> <date2>] --> richiede un dato aggregato\n");
    printf("stop --> richiede disconnessione dalla rete\n");
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
    if(y < MIN_YEAR || m < 1 || m > 12 || d < 1 || d > 31)
        return 0;
    sscanf(current_d, "%d:%d:%d", &c_date[0], &c_date[1], &c_date[2]);
    //Sfrutto la funzione is_real_period
    if(!is_real_period(this_date, c_date, 't'))
        return 0;
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

//Inserisce entry nel database
void insert_entry(char type, int quantity){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];

    retrieve_time();

    sprintf(filename, "%s%s_%d.txt", "./peer_dir/", current_d, my_port);

    printf("Filename: %s\n", filename);

    fd = fopen(filename, "a");
    fprintf(fd, "%s %c %d %d\n", current_t, type, quantity, my_port);
    fclose(fd);

    printf("Entry inserita!\n");
}

//Conta il numero di entries presente nel proprio database
int count_entries(char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int tot;
    char entry_type;
    char u_date[DATE_LEN];
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
        while(fscanf(fd, "%s %c %d %s\n", u_date, &entry_type, &num, tot_peers) == 4)
            tot += (type == entry_type);
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
    char u_date[DATE_LEN];
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
        while(fscanf(fd, "%s %c %d %s\n", u_date, &entry_type, &num, tot_peers) == 4)
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