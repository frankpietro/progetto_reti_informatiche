#include <stdio.h>
#include <time.h>

//Costanti
#include "const.h"

char current_d[DATE_LEN+1];
char current_t[TIME_LEN+1];

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
