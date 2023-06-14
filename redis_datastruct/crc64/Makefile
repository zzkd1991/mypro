#main : siphash.o dict.o
#	gcc -o main siphash.o dict.o
#siphash.o:siphash.c
#	gcc -c siphash.c -o siphash.o
#dict.o:dict.c
#	gcc -c dict.c -o dict.o
#clean:
#	rm -rf *.o main


#main : bucket_sort.o
#	gcc -o main bucket_sort.o
#bucket_sort.o : bucket_sort.c
#	gcc -c bucket_sort.c -o bucket_sort.o
#clean:
#	rm -rf *.o

main : crc64.o crcspeed.o 
	gcc -o main crc64.o crcspeed.o -lm
crc64.o : crc64.c
	gcc -c crc64.c -o crc64.o -lm
crcspeed.o : crcspeed.c
	gcc -c crcspeed.c -o crcspeed.o -lm
clean:
	rm -rf *.o main
