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

int get_port(int pos){
    FILE *fp;
    int port;
    char temp[INET_ADDRSTRLEN];

    fp = fopen("peer_addr.txt", "r");
    while(pos-- >= 0)
        fscanf(fp, "%s %d", temp, &port);
    
    return port;
}

void get_neighbors(int peer, int conn, int *nbr_1, int *nbr_2){
    FILE *fd;
    int m,f,serv;
    char temp_buff[INET_ADDRSTRLEN];

    fd = fopen("peer_addr.txt", "r");
    //Algoritmo simile a quello di inserimento

    (*nbr_1) = -1;
    (*nbr_2) = -1;

    //Se un solo peer connesso
    if(conn == 1)
        return;
    
    //Se ce ne sono due
    if(conn == 2){
        (*nbr_1) = (get_port(0) == peer) ? get_port(1) : get_port(0);
        return;
    }

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

int insert_peer(char* addr, int port, int connected){
    FILE *fp, *temp;
    int m, f, serv;
    int temp_port[2];
    char temp_buff[INET_ADDRSTRLEN];

    //Se primo peer a connettersi
    if(connected == 0){
        fp = fopen("peer_addr.txt", "w");
        fprintf(fp, "%s %d\n", addr, port);
        fclose(fp);
        return 1;
    }

    //Inserisco ordinatamente nel caso di un peer gia' presente
    if(connected == 1){
        //Non la uso
        temp_port[1] = -1;
        fp = fopen("peer_addr.txt", "r");
        temp = fopen("temp.txt", "w");

        fscanf(fp, "%s %d", temp_buff, &temp_port[0]);
        if(temp_port[0] < port){
            fprintf(temp, "%s %d\n", temp_buff, temp_port[0]);
            fprintf(temp, "%s %d\n", addr, port);
        }
        else {
            fprintf(temp, "%s %d\n", addr, port);
            fprintf(temp, "%s %d\n", temp_buff, temp_port[0]);
        }
        fclose(temp);
        fclose(fp);
        remove("peer_addr.txt");
        rename("temp.txt", "peer_addr.txt");

        return 1;
    }

    //Inserisco ordinatamente nel caso di due o piu' peer gia' presenti
    else {
        fp = fopen("peer_addr.txt", "r");
        temp = fopen("temp.txt", "w");

        temp_port[0] = -1;
        //Prelevo i primi valori
        m = fscanf(fp, "%s %d", temp_buff, &serv);
        //Salvo il primo numero
        f = serv;
        //Finche' trovo valori e non arrivo alla fine...
        while(serv < port && m == 2){
            //Salvo il precedente
            temp_port[0] = serv;
            //Lo stampo
            fprintf(temp, "%s %d\n", temp_buff, serv);
            //Provo a prenderne un altro
            m = fscanf(fp, "%s %d", temp_buff, &serv);
            //Lo metto come successivo
            temp_port[1] = serv;
        }

        //Quando esco dal while inserisco il valore
        fprintf(temp, "%s %d\n", temp_buff, port);

        //Se non sono mai entrato nel while oppure sono arrivato in fondo
        //ovvero il mio valore e' il minimo o il massimo
        if(serv == f || m != 2)
            //Il peer successivo e' il primo che ho estratto
            temp_port[1] = f;
        
        //Se non ero arrivato in fondo al file
        if(m == 2)
            //Stampo il valore successivo a quello che dovevo inserire
            fprintf(temp, "%s %d\n", temp_buff, serv);

        //Inserisco gli altri finche' non finiscono
        while(fscanf(fp, "%s %d", temp_buff, &serv) == 2)
            fprintf(temp, "%s %d\n", temp_buff, serv);

        //Se non ero mai entrato nel while
        if(temp_port[0] == -1)
            //Il mio peer precedente e' il massimo, perche' io sono il minimo
            temp_port[0] = serv;   
        
        //Fine
        fclose(temp);
        fclose(fp);
        remove("peer_addr.txt");
        rename("temp.txt", "peer_addr.txt");

        return 2;
    }

    //Questa riga non dovrebbe mai essere eseguita
    return 0;
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
