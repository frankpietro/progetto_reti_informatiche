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
#define MAX_ENTRY_LEN 50
#define DATE_LEN 10
#define MAX_FILENAME_LEN 20
#define TIME_LEN 8

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
