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

#define MAX_FILENAME_LEN 20
#define DATE_LEN 10
#define MAX_FILENAME_LEN 20
#define TIME_LEN 8
#define MAX_CONNECTED_PEERS 100

extern int my_port;
extern char current_d[DATE_LEN+1];
extern char current_t[TIME_LEN+1];

void insert_entry(char type, int quantity){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];

    sprintf(filename, "%s_%d.txt", current_d, my_port);

    printf("Filename: %s\n", filename);

    fd = fopen(filename, "a");
    fprintf(fd, "%s %c %d %d\n", current_t, type, quantity, my_port);
    fclose(fd);

    printf("Entry inserita!\n");
}

int count_entries(char type){
    FILE *fd;
    char filename[MAX_FILENAME_LEN];
    int tot;
    char entry_type;
    char u_date[DATE_LEN];
    int num;
    char tot_peers[6*MAX_CONNECTED_PEERS];

    tot = 0;

    sprintf(filename, "%s_%d.txt", current_d, my_port);

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