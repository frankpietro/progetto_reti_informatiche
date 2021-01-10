all: ./ds ./peer ./time

./time: ./time.o ./util/util_t.o ./util/retr_time.o ./util/peer_file.o ./util/msg.o
	gcc -Wall ./time.o ./util/util_t.o ./util/retr_time.o ./util/peer_file.o ./util/msg.o -o ./time

./peer: ./peer.o ./util/msg.o ./util/util_c.o ./util/retr_time.o
	gcc -Wall ./peer.o ./util/msg.o ./util/util_c.o ./util/retr_time.o -o ./peer

./ds: ./ds.o ./util/peer_file.o ./util/msg.o ./util/util_s.o ./util/retr_time.o
	gcc -Wall ./ds.o ./util/peer_file.o ./util/msg.o ./util/util_s.o ./util/retr_time.o -o ./ds

./time.o: ./time.c ./util/util_t.h ./util/retr_time.h ./util/peer_file.h ./util/const.h
	gcc -Wall -c ./time.c -o ./time.o

./peer.o: ./peer.c ./util/msg.h ./util/util_c.h ./util/const.h
	gcc -Wall -c ./peer.c -o ./peer.o

./ds.o: ./ds.c ./util/msg.h ./util/peer_file.h ./util/util_s.h ./util/const.h
	gcc -Wall -c ./ds.c -o ./ds.o

./util/peer_file.o: ./util/peer_file.c ./util/peer_file.h ./util/const.h
	gcc -Wall -c ./util/peer_file.c -o ./util/peer_file.o

./util/msg.o: ./util/msg.c ./util/msg.h ./util/const.h
	gcc -Wall -c ./util/msg.c -o ./util/msg.o

./util/util_t.o: ./util/util_t.c ./util/util_t.h ./util/msg.h ./util/const.h
	gcc -Wall -c ./util/util_t.c -o ./util/util_t.o

./util/util_c.o: ./util/util_c.c ./util/util_c.h ./util/retr_time.h ./util/msg.h ./util/const.h
	gcc -Wall -c ./util/util_c.c -o ./util/util_c.o

./util/util_s.o: ./util/util_s.c ./util/util_s.h ./util/retr_time.h ./util/const.h
	gcc -Wall -c ./util/util_s.c -o ./util/util_s.o

./util/retr_time.o: ./util/retr_time.c ./util/retr_time.h ./util/const.h
	gcc -Wall -c ./util/retr_time.c -o ./util/retr_time.o

clean:
	-rm ./*.o ./ds ./peer ./time ./util/*.o
	-mv ./ds_dir/2021* ./logs
	-rm ./ds_dir/*.txt
	-mv ./peer_dir/*.txt ./logs