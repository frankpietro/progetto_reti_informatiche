int prepare(struct sockaddr_in*,socklen_t*,int);
int s_recv_UDP(int,char*,int);
void ack_UDP(int,char*,int,char*);
void send_UDP(int,char*,int,int,char*);
void recv_UDP(int,char*,int,int,char*,char*);