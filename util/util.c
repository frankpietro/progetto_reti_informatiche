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

#define LOCALHOST "127.0.0.1"
#define DATE_LEN 10
#define TIME_LEN 8
#define MAX_FILENAME_LEN 22
#define MIN_YEAR 1990

char current_d[DATE_LEN+1];
char current_t[TIME_LEN+1];

//Elenco dei comandi disponibili lato server
void comandi_server(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("showpeers --> mostra elenco peer connessi alla rete\n");
    printf("showneighbor [<peer>] --> mostra i neighbor di un peer o di tutti i peer se non viene specificato alcun parametro\n");
    printf("esc --> termina il DS\n");
}

//Elenco dei comandi disponibili lato client
void comandi_client(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("start <DS_addr> <DS_port> --> richiede al Discovery Server connessione alla rete\n");
    printf("add <type> <quantity> --> aggiunge una entry nel registro del peer\n");
    printf("get <aggr> <type> [<date1> <date2>] --> richiede un dato aggregato\n");
    printf("stop --> richiede disconnessione dalla rete\n");
}

//Recupera la data e l'ora correnti e le inserisce nelle variabili globali lato client
void retrieve_time(){
    time_t now_time;
    struct tm* now_tm;

    now_time = time(NULL);
    now_tm = gmtime(&now_time);

    if(now_tm->tm_hour < 18)
        sprintf(current_d, "%04d:%02d:%02d", now_tm->tm_year+1900, now_tm->tm_mon+1, now_tm->tm_mday);
    else {
        now_tm->tm_mday += 1;
        now_time = mktime(now_tm);
        now_tm = gmtime(&now_time);
        sprintf(current_d, "%04d:%02d:%02d", now_tm->tm_year+1900, now_tm->tm_mon+1, now_tm->tm_mday);
    }
    current_d[DATE_LEN] = '\0';
    sprintf(current_t, "%02d:%02d:%02d", now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
    current_t[TIME_LEN] = '\0';
}

//Aggiunge una entry nel file con il totale delle entries lato server
void add_entry(int type){
    FILE *fd1, *fd2;
    int entries[2];
    char filename[MAX_FILENAME_LEN+1];
    retrieve_time();
    sprintf(filename, "%s_%s", current_d, "entries.txt");
    
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
}

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