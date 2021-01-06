all: ./ds_dir/ds ./peer_1_dir/peer

./peer_1_dir/peer: ./peer_1_dir/peer.o ./util/msg.o ./util/util.o ./util/entries.o
	gcc -Wall ./peer_1_dir/peer.o ./util/msg.o ./util/util.o ./util/entries.o -o ./peer_1_dir/peer

./ds_dir/ds: ./ds_dir/ds.o ./util/peer_file.o ./util/msg.o ./util/util.o
	gcc -Wall ./ds_dir/ds.o ./util/peer_file.o ./util/msg.o ./util/util.o -o ./ds_dir/ds

./peer_1_dir/peer.o: ./peer_1_dir/peer.c
	gcc -Wall -c ./peer_1_dir/peer.c -o ./peer_1_dir/peer.o

./ds_dir/ds.o: ./ds_dir/ds.c
	gcc -Wall -c ./ds_dir/ds.c -o ./ds_dir/ds.o

./util/peer_file.o: ./util/peer_file.c ./util/peer_file.h
	gcc -Wall -c ./util/peer_file.c -o ./util/peer_file.o

./util/entries.o: ./util/entries.c ./util/entries.h
	gcc -Wall -c ./util/entries.c -o ./util/entries.o

./util/msg.o: ./util/msg.c ./util/msg.h
	gcc -Wall -c ./util/msg.c -o ./util/msg.o

./util/util.o: ./util/util.c ./util/util.h
	gcc -Wall -c ./util/util.c -o ./util/util.o

clean:
	rm ./ds_dir/*.o ./ds_dir/ds ./peer_1_dir/*.o ./peer_1_dir/peer ./util/*.o
	-mv ./ds_dir/2021* ./logs
	-rm ./ds_dir/*.txt
	-mv ./peer_1_dir/*.txt ./logs