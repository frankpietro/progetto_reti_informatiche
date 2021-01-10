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

//Scambio di messaggi
#include "msg.h"
//Costanti
#include "const.h"

extern int time_socket;

extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];

//Inserisce tutte le entries nel file temporaneo
void insert_temp(char* entry){
    FILE *fd1, *fd2;
    char u_time[TIME_LEN];
    char entry_type;
    int num;
    char tot_peers[MAX_CONNECTED_PEERS*6];
    char entry_in[MAX_ENTRY_UPDATE];
    
    fd1 = fopen("entries.txt", "r");
    //Se e' la prima la inserisco
    if(fd1 == NULL){
        printf("Prima entry\n");
        fd2 = fopen("entries.txt", "w");
        fprintf(fd2, "%s\n", entry);
        fclose(fd2);
        return;
    }
    //Devo controllare che non sia gia' dentro
    fd2 = fopen("temp.txt", "w");
    while(fscanf(fd1, "%s %c %d %s\n", u_time, &entry_type, &num, tot_peers) != EOF){
        sprintf(entry_in, "%s %c %d %s", u_time, entry_type, num, tot_peers);
        fprintf(fd2, "%s\n", entry_in);
        if(strcmp(entry_in, entry) == 0){
            printf("Entry gia' presente\n");
            return;
        }
    }
    //Se sono arrivato qui l'entry non era presente
    printf("Nuova entry da inserire\n");
    fprintf(fd2, "%s\n", entry);
    fclose(fd1);
    fclose(fd2);
    remove("entries.txt");
    rename("temp.txt", "entries.txt");
}

//Somma le entries
int sum_entries(char type){
    FILE *fd;
    int tot;
    char entry_type;
    char u_time[TIME_LEN];
    int num;
    char tot_peers[6*MAX_CONNECTED_PEERS];

    tot = 0;

    //printf("Filename: %s\n", filename);

    fd = fopen("entries.txt", "r");
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

//Scrive gli aggregati giornalieri su un file
void register_tot(int tests, int cases){
    FILE *fd;
    fd = fopen("total_aggr.txt", "a");
    fprintf(fd, "%s %d %d", current_d, tests, cases);
    fclose(fd);
    printf("Aggregati scritti su file\n");
}

/*
    ASSUNTO: esiste un'entrata al giorno nel file degli aggregati.
    Le date senza peer connessi sono registrate con 0 e 0.
    Questo vuol dire che tra le 17.50 e le 18.10 il server deve essere acceso.
*/
//Converte una data nella notazione yyyy:mm:dd
void convert_date(char* date){
    int d,m,y;

    sscanf(date, "%d:%d:%d", &d, &m, &y);
    printf("Data: %d, %d, %d\n", y,m,d);
    sprintf(date, "%04d:%02d:%02d", y,m,d);
}

//Conta le entrate comprese tra le date passate come argomenti
int count_aggr_entries(char* date1, char* date2){
    int entries;
    FILE *fd, *temp;
    int ok_start;
    int ok_stop;
    char ta_date[DATE_LEN];
    int t,c;

    if(strcmp(date1, "*") != 0)
        convert_date(date1);
    if(strcmp(date2, "*") != 0)
        convert_date(date2);
    
    printf("Data 1: %s; data 2: %s\n", date1, date2);

    entries = 0;
    ok_start = 0;
    ok_stop = 0;
    fd = fopen("total_aggr.txt", "r");
    if(fd == NULL)
        return 0;
    
    temp = fopen("temp_send.txt", "w");
    while(fscanf(fd,"%s %d %d", ta_date, &t, &c) != EOF){
        printf("Entro nel while\n");
        if(strcmp(ta_date, date1) == 0 || strcmp(date1, "*") == 0)
            ok_start = 1;

        if(ok_start && !ok_stop){
            entries++;
            //Salva le entries su un file per inviarle tutte insieme successivamente
            fprintf(temp, "%s %d %d\n", ta_date, t, c);
        }

        if(strcmp(ta_date, date2) == 0)
            ok_stop = 1;
    }

    fclose(fd);
    fclose(temp);

    return entries;

}

//Invia le entries richieste al peer
void send_entries(int port){
    FILE *fd;
    char d[DATE_LEN];
    int t,c;
    char mess[40]; //header, data e due numeri aggregati
    int len;

    fd = fopen("temp_send.txt", "r");
    while(fscanf(fd, "%s %d %d", d, &t, &c) != EOF){
        len = sprintf(mess, "%s %s %d %d", "AGGR_ENT", d, t, c);
        mess[len] = '\0';
        send_UDP(time_socket, mess, len, port, "AENT_ACK");
    }
    fclose(fd);
    printf("Invio terminato\n");
}
