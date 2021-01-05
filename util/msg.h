void clear_address(struct sockaddr_in*,socklen_t*,int);
int prepare(struct sockaddr_in*,socklen_t*,int);
void ack(int,char*,int,int,char*);
void send_UDP(int,char*,int,int,char*);
int recv_UDP(int,char*,int);
int s_recv_UDP(int,char*,int);