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

int main(){
    /*int ds_port;    //Porta del server, passata come argomento del main
    int server_socket;  //Socket su cui il server riceve messaggi dai peer
    struct sockaddr_in server_addr;     //Struttura per gestire il socket
    int ret;    //Variabile di servizio per funzioni che ritornano un intero di controllo
*/

    time_t now;
    struct tm* now_tm;

    now = time(NULL);
    now_tm = gmtime(&now);
    printf("Data: %d-%02d-%02d", now_tm->tm_year+1900, now_tm->tm_mon+1, now_tm->tm_mday);

    return 0;
}