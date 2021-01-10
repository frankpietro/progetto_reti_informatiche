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

#define MESS_TYPE_LEN 8
#define MAX_RECV 40
#define ALL_PEERS -1 //recv_UDP puo' ricevere da qualunque indirizzo
#define USEC 30000
#define TIME_LEN 8
#define MAX_CONNECTED_PEERS 100
#define MAX_ENTRY_UPDATE 630

#define LOCALHOST "127.0.0.1"

void clear_address(struct sockaddr_in* addr_p, socklen_t* len_p, int port){
    memset(addr_p, 0, sizeof((*addr_p)));
	addr_p->sin_family = AF_INET;
	addr_p->sin_port = htons(port);
	inet_pton(AF_INET, LOCALHOST, &addr_p->sin_addr);
    
    (*len_p) = sizeof((*addr_p));\
}

int prepare(struct sockaddr_in* addr_p, socklen_t* len_p, int port){
    int ret;
    int sock;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    clear_address(addr_p, len_p, port);

    ret = bind(sock, (struct sockaddr*)addr_p, (*len_p));
    if(ret<0){
        perror("Error while binding");
        exit(0);
    }

    return sock;
}

int s_recv_UDP(int socket, char* buffer, int buff_l){
    struct sockaddr_in send_addr;
    socklen_t send_addr_len;

    send_addr_len = sizeof(send_addr);

    recvfrom(socket, buffer, buff_l, 0, (struct sockaddr*)&send_addr, &send_addr_len);

    return ntohs(send_addr.sin_port);
}

/*
    Utilizzo: inviare un ACK sfruttando protocollo UDP

    socket: socket di chi invia
    buffer: ACK da inviare
    send_port: porta del mittente a cui inviare ACK
    unacked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che sia l'ack atteso
    unacked_len: lunghezza del messaggio da confrontare
*/
void ack_UDP(int socket, char* buffer, int send_port, char* unacked, int unacked_len){
    int received, ret;
    struct sockaddr_in send_addr;
    socklen_t send_addr_len;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[unacked_len+1];
    struct timeval util_tv;
    fd_set readset;

    clear_address(&send_addr, &send_addr_len, send_port);
    received = 0;
    ret = 0;
    util_len = sizeof(util_addr);

    printf("Inizio processo di ACK\n");

    while(!received){
        do {
            ret = sendto(socket, buffer, MESS_TYPE_LEN+1, 0, (struct sockaddr*)&send_addr, send_addr_len);
        } while(ret<0);

        //Attesa
        util_tv.tv_sec = 0;
        util_tv.tv_usec = USEC;
        //Mi metto per un tempo prefissato in ascolto sul socket
        //Solo in ascolto di un'eventuale copia del messaggio
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
            
        ret = select(socket+1, &readset, NULL, NULL, &util_tv);

        //Se arriva qualcosa
        if(FD_ISSET(socket, &readset)){
            //Leggo cosa ho ricevuto
            ret = recvfrom(socket, recv_buffer, MAX_RECV, 0, (struct sockaddr*)&util_addr, &util_len);
            //Se ho ricevuto lo stesso identico messaggio
            if(util_addr.sin_port == send_port && util_addr.sin_addr.s_addr == send_addr.sin_addr.s_addr && (strncmp(recv_buffer, buffer, unacked_len)==0)){
                //Riinvio l'ack tornando a inizio while(!received)
                received = 0;
                break;
            }
            //Se ho ricevuto un messaggio diverso lo scarto (il peer lo rimandera') e considero arrivato correttamente l'altro
            else 
                received = 1;
            

            FD_CLR(socket, &readset);
        }
        //Se per un secondo non arriva nulla, considero terminata con successo l'operazione
        else
            received = 1;
    }

    printf("ACK ricevuto correttamente\n");
}

/*
    Utilizzo: inviare un messaggio sfruttando protocollo UDP

    socket: socket di chi invia
    buffer: messaggio da inviare
    buff_l: lunghezza del messaggio da inviare
    recv_port: porta del destinatario del messaggio
    acked: stringa con cui confrontare eventuale tipo del messaggio inviato dal mittente per controllare che sia l'ack atteso
*/
void send_UDP(int socket, char* buffer, int buff_l, int recv_port, char* acked){
    int sent,ret;
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len;
    struct sockaddr_in util_addr;
    socklen_t util_len;
    char recv_buffer[MAX_RECV];
    struct timeval util_tv;
    fd_set readset;

    clear_address(&recv_addr, &recv_addr_len, recv_port);
    sent = 0;
    ret = 0;
    util_len = sizeof(util_addr);


    while(!sent){
        //Invio lista
        do {
            ret = sendto(socket, buffer, buff_l+1, 0, (struct sockaddr*)&recv_addr, recv_addr_len);
        } while(ret<0);
        
        //Mi preparo a ricevere un messaggio dal peer
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
        //Imposto il timeout a mezzo secondo
        util_tv.tv_sec = 0;
        util_tv.tv_usec = USEC;
        //Aspetto l'ack
        ret = select(socket+1, &readset, NULL, NULL, &util_tv);

        //Appena la ricevo
        if(FD_ISSET(socket, &readset)){
            //Ricevo ack
            ret = recvfrom(socket, recv_buffer, MAX_RECV, 0, (struct sockaddr*)&util_addr, &util_len);
            
            //Se ho ricevuto effettivamente l'ack
            if(util_addr.sin_port == recv_addr.sin_port && util_addr.sin_addr.s_addr == recv_addr.sin_addr.s_addr && strcmp(acked, recv_buffer) == 0){
                //Il messaggio e' stato sicuramente ricevuto
                printf("ACK %s ricevuto correttamente dal mittente %d\n", recv_buffer, ntohs(util_addr.sin_port));
                sent = 1;
            }
            //Ignoro qualunque altro messaggio
            else {
                printf("[S] Arrivato un messaggio %s inatteso da %d mentre attendevo %s da %d, scartato\n", recv_buffer, ntohs(util_addr.sin_port), acked, recv_port);
            }
            
            FD_CLR(socket, &readset);

        }
    }
}

/*
    Utilizzo: ricevere un messaggio sfruttando protocollo UDP

    socket: socket di chi riceve
    buffer: messaggio da ricevere
    buff_l: lunghezza del messaggio da ricevere
    send_port: porta del mittente del messaggio
    correct_header: header del messaggio che mi aspetto di ricevere
    ack_type: messaggio di ACK da inviare al mittente
*/
void recv_UDP(int socket, char* buffer, int buff_l, int send_port, char* correct_header, char* ack_type){
    struct sockaddr_in send_addr;
    socklen_t send_addr_len;
    int ok;
    char temp_buffer[MESS_TYPE_LEN];
    struct timeval util_tv;
    fd_set readset;

    ok = 0;
    send_addr_len = sizeof(send_addr);

    while(!ok){
        //Attesa di mezzo secondo
        util_tv.tv_sec = 0;
        util_tv.tv_usec = USEC;
        //Mi metto per un secondo solo in ascolto sul socket
        //Solo in ascolto di un'eventuale copia del messaggio
        FD_ZERO(&readset);
        FD_SET(socket, &readset);
            
        select(socket+1, &readset, NULL, NULL, &util_tv);

        //Se arriva qualcosa
        if(FD_ISSET(socket, &readset)){
            //Leggo cosa ho ricevuto
            recvfrom(socket, buffer, buff_l, 0, (struct sockaddr*)&send_addr, &send_addr_len);
            sscanf(buffer, "%s", temp_buffer);

            //Opzione per ricevere da tutti gli indirizzi
            if((ntohs(send_addr.sin_port) == send_port || send_port == ALL_PEERS) && strcmp(correct_header, temp_buffer) == 0){
                //Il peer ha ricevuto sicuramente la lista
                printf("Messaggio %s ricevuto correttamente dal mittente %d\n", buffer, ntohs(send_addr.sin_port));
                ok = 1;
            }
            else {
                //Stampo il messaggio di errore giusto
                printf("[R] Arrivato un messaggio %s inatteso da %d mentre attendevo %s da ", temp_buffer, ntohs(send_addr.sin_port), correct_header);
                if(send_port == ALL_PEERS)
                    printf("chiunque, scartato\n");
                else
                    printf("%d, scartato\n", send_port);
                
            }

            FD_CLR(socket, &readset);
        }
    }

    ack_UDP(socket, ack_type, ntohs(send_addr.sin_port), buffer, strlen(buffer));
}

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