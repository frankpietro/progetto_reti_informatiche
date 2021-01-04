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

void get_neighbors(int peer, int *nbr_1, int *nbr_2){
    FILE *fd;
    int m,f,serv;
    char temp_buff[INET_ADDRSTRLEN];

    fd = fopen("peer_addr.txt", "r");
    //Algoritmo simile a quello di inserimento

    (*nbr_1) = -1;
    (*nbr_2) = -1;
    //Prelevo i primi valori
    m = fscanf(fd, "%s %d", temp_buff, &serv);
    //Salvo il primo numero
    f = serv;
    //Finche' trovo valori e non arrivo alla fine...
    while(serv < peer && m == 2){
        //Salvo il precedente
        (*nbr_1) = serv;
        //Provo a prenderne un altro
        m = fscanf(fd, "%s %d", temp_buff, &serv);
        //Se ho trovato il peer di cui devo cercare i vicini metto come successivo il successivo o il primo, nel caso il mio peer sia il massimo
        if(serv == peer){
            //Provo a estrarre un altro peer
            if(fscanf(fd, "%s %d", temp_buff, &serv) == 2)
                //Lo metto come successivo
                (*nbr_2) = serv;
            //Altrimenti metto il primo (il peer in esame e' il massimo
            else
                (*nbr_2) = f;
        }
    }

    //Se non ero mai entrato nel while
    if((*nbr_1) == -1 && (*nbr_2) == -1){
        //Il successivo e' il secondo della lista
        m = fscanf(fd, "%s %d", temp_buff, &serv);
        //Salvo il primo numero
        (*nbr_2) = serv;
        //Il precedente e' l'ultimo della lista
        while(fscanf(fd, "%s %d", temp_buff, &serv) == 2)
            (*nbr_1) = serv; 
    }
}

int get_port(int pos){
    FILE *fp;
    int port;
    char temp[INET_ADDRSTRLEN];

    fp = fopen("peer_addr.txt", "r");
    while(pos-- >= 0)
        fscanf(fp, "%s %d", temp, &port);
    
    return port;
}

int isIn(int port){
    FILE *fd;
    char temp_buffer[INET_ADDRSTRLEN];
    int serv;
    
    fd = fopen("peer_addr.txt", "r");

    while(fscanf(fd, "%s %d", temp_buffer, &serv)==2){
        if(serv == port)
            return 1;
    }

    return 0;
}

void remove_peer(int port){
    FILE *fd, *temp;
    char temp_buffer[INET_ADDRSTRLEN];
    int serv;
    
    fd = fopen("peer_addr.txt", "r");
    temp = fopen("temp.txt", "w");
    
    fscanf(fd, "%s %d", temp_buffer, &serv);

    while(serv < port){
        fprintf(temp, "%s %d", temp_buffer, serv);
        fscanf(fd, "%s %d", temp_buffer, &serv);
    }

    while(fscanf(fd, "%s %d", temp_buffer, &serv)==2)
        fprintf(temp, "%s %d", temp_buffer, serv);
    
    remove("peer_addr.txt");
    rename("temp.txt", "peer_addr.txt");
}
