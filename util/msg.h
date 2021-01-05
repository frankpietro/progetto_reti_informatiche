void clear_address(struct sockaddr_in*,socklen_t*,int);
int prepare(struct sockaddr_in*,socklen_t*,int);
void ack_1(int,char*,int,struct sockaddr_in*,socklen_t,/*fd_set*,*/char*);
void ack_2(int,char*,int,struct sockaddr_in*,socklen_t,fd_set*,char*);