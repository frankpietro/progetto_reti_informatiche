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

#define LOCALHOST "127.0.0.1"
#define DATE_LEN 10
#define TIME_LEN 8
#define MAX_FILENAME_LEN 31
#define MIN_YEAR 1990

extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];

//Elenco dei comandi disponibili lato server
void comandi_server(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("showpeers --> mostra elenco peer connessi alla rete\n");
    printf("showneighbor [<peer>] --> mostra i neighbor di un peer o di tutti i peer se non viene specificato alcun parametro\n");
    printf("esc --> termina il DS\n");
}

//Aggiunge una entry nel file con il totale delle entries lato server
void add_entry(char t){
    FILE *fd1, *fd2;
    int entries[2];
    char filename[MAX_FILENAME_LEN];
    int type;
    
    retrieve_time();
    sprintf(filename, "%s%s_%s", "./ds_dir/", current_d, "entries.txt");

    type = (t == 't') ? 0 : 1;
    
    fd1 = fopen(filename, "r");
    if(fd1 == NULL){
        fd2 = fopen(filename, "w");
        entries[type] = 1;
        entries[(type+1)%2] = 0;
        fprintf(fd2, "%d %d", entries[0], entries[1]);
        fclose(fd2);
        return;
    }
    else {
        fd2 = fopen("temp_entries.txt", "w");
        fscanf(fd1, "%d %d", &entries[0], &entries[1]);
        entries[type]++;
        fprintf(fd2, "%d %d", entries[0], entries[1]);
        fclose(fd1);
        fclose(fd2);
        remove(filename);
        rename("temp_entries.txt", filename);
    }

    printf("Aggiunta entry di tipo %c\n", t);
}

//Legge una entry richiesta da un peer sul file lato server
int read_entries(char type){
    FILE *fd;
    int entries[2];
    char filename[MAX_FILENAME_LEN];

    retrieve_time();
    sprintf(filename, "%s%s_%s", "./ds_dir/", current_d, "entries.txt");
    
    fd = fopen(filename, "r");
    if(fd == NULL)
        return 0;
    fscanf(fd, "%d %d", &entries[0], &entries[1]);
    return (type == 't') ? entries[0] : entries[1];
}