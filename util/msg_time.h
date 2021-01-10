int prepare(struct sockaddr_in*,socklen_t*,int);
int s_recv_UDP(int,char*,int);
void ack_UDP(int,char*,int,char*,int);
void send_UDP(int,char*,int,int,char*);
void recv_UDP(int,char*,int,int,char*,char*);
void insert_temp(char*);
int sum_entries(char);