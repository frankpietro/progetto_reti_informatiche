all: ./ds_dir/ds ./peer_1_dir/peer

./peer_1_dir/peer: ./peer_1_dir/peer.o ./util/msg.o ./util/util_c.o ./util/retr_time.o
	gcc -Wall ./peer_1_dir/peer.o ./util/msg.o ./util/util_c.o ./util/retr_time.o -o ./peer_1_dir/peer

./ds_dir/ds: ./ds_dir/ds.o ./util/peer_file.o ./util/msg.o ./util/util_s.o ./util/retr_time.o
	gcc -Wall ./ds_dir/ds.o ./util/peer_file.o ./util/msg.o ./util/util_s.o ./util/retr_time.o -o ./ds_dir/ds

./peer_1_dir/peer.o: ./peer_1_dir/peer.c ./util/msg.h ./util/util_c.h
	gcc -Wall -c ./peer_1_dir/peer.c -o ./peer_1_dir/peer.o

./ds_dir/ds.o: ./ds_dir/ds.c ./util/msg.h ./util/peer_file.h ./util/util_s.h
	gcc -Wall -c ./ds_dir/ds.c -o ./ds_dir/ds.o

./util/peer_file.o: ./util/peer_file.c ./util/peer_file.h
	gcc -Wall -c ./util/peer_file.c -o ./util/peer_file.o

./util/msg.o: ./util/msg.c ./util/msg.h
	gcc -Wall -c ./util/msg.c -o ./util/msg.o

./util/util_c.o: ./util/util_c.c ./util/util_c.h ./util/retr_time.h
	gcc -Wall -c ./util/util_c.c -o ./util/util_c.o

./util/util_s.o: ./util/util_s.c ./util/util_s.h ./util/retr_time.h
	gcc -Wall -c ./util/util_s.c -o ./util/util_s.o

./util/retr_time.o: ./util/retr_time.c ./util/retr_time.h
	gcc -Wall -c ./util/retr_time.c -o ./util/retr_time.o

clean:
	rm ./ds_dir/*.o ./ds_dir/ds ./peer_1_dir/*.o ./peer_1_dir/peer ./util/*.o
	-mv ./ds_dir/2021* ./logs
	-rm ./ds_dir/*.txt
	-mv ./peer_1_dir/*.txt ./logs