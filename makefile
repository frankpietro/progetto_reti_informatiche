all: ./ds_dir/ds ./peer_1_dir/peer

./peer_1_dir/peer: ./peer_1_dir/peer.o
	gcc -Wall ./peer_1_dir/peer.o -o ./peer_1_dir/peer

./ds_dir/ds: ./ds_dir/ds.o ./ds_dir/peer_file.o ./ds_dir/ack.o
	gcc -Wall ./ds_dir/ds.o ./ds_dir/peer_file.o ./ds_dir/ack.o -o ./ds_dir/ds

./peer_1_dir/peer.o: ./peer_1_dir/peer.c
	gcc -Wall -c ./peer_1_dir/peer.c -o ./peer_1_dir/peer.o

./ds_dir/ds.o: ./ds_dir/ds.c
	gcc -Wall -c ./ds_dir/ds.c -o ./ds_dir/ds.o

./ds_dir/peer_file.o: ./ds_dir/peer_file.c ./ds_dir/peer_file.h
	gcc -Wall -c ./ds_dir/peer_file.c -o ./ds_dir/peer_file.o

./ds_dir/ack.o: ./ds_dir/ack.c ./ds_dir/ack.h
	gcc -Wall -c ./ds_dir/ack.c -o ./ds_dir/ack.o

clean:
	rm ./ds_dir/*.o ./ds_dir/ds ./peer_1_dir/*.o ./peer_1_dir/peer ./ds_dir/*.txt
#	rm  ./peer_1_dir/*.txt