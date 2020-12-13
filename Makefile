#rbtree:rbtree.o 
#rbtree.o:rbtree.c
#    rbtree -c rbtree.c:
#radix:radix.o
#radix.o:radix.c
#    radix -c radix.c:


#list:list.o
#list.o:list.c
#    list -c list.c:

#heap_sort:heap_sort.o
#heap_sort.o:heap_sort.c
#    heap_sort -c heap_sort.c:

#dict:dict.o
#dict.o:dict.c
#    dict -c dict.c:
#siphash:siphash.o
#siphash.o:siphash.c
#    siphash -c siphash.c:


#OBJ=siphash.o dict.o
#main:$(OBJ)
#    gcc $(OBJ) -o main
main : siphash.o dict.o
    gcc -o main siphash.o dict.o
siphash.o:siphash.c
    gcc -c siphash.c -o siphash.o
dict.o:dict.c
    gcc -c dict.c -o dict.o
#clean:
#    rm -rf *.o main

