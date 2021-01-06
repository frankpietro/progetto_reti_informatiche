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

void comandi_client(){
    printf("Elenco dei comandi disponibili:\n");
    printf("help --> mostra elenco comandi e significato\n");
    printf("start <DS_addr> <DS_port> --> richiede al Discovery Server connessione alla rete\n");
    printf("add <type> <quantity> --> aggiunge una entry nel registro del peer\n");
    printf("get <aggr> <type> [<date1> [<date2>]] --> richiede un dato aggregato\n");
    printf("stop --> richiede disconnessione dalla rete\n");
}

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