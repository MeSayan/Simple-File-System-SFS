main: main.o disk.o sfs.o
	gcc -o main main.o disk.o sfs.o -lm
main.o: main.c disk.h sfs.h
	gcc -c -g main.c
sfs.o: sfs.c sfs.h
	gcc -c -g sfs.c
disk.o: disk.c disk.h
	gcc -c -g disk.c


# Disk Test
disk_test: tests/disk_test.o disk.o sfs.o 
	gcc -o tests/disk_test.out tests/disk_test.o disk.o sfs.o -lm 
	./tests/disk_test.out > ./tests/disk_test_op
	diff ./tests/disk_test_op golden_output/disk_test_op_golden
disk_test.o: tests/disk_test.c disk.h sfs.h
	gcc -c -g tests/disk_test.c -o tests/disk_test.o
    
# SFS block level tests
sfs_test: tests/sfs_test.o disk.o sfs.o 
	gcc -o tests/sfs_test.out tests/sfs_test.o disk.o sfs.o -lm 
	./tests/sfs_test.out > ./tests/sfs_test_op
	diff ./tests/sfs_test_op golden_output/sfs_test_op_golden
sfs_test.o: tests/sfs_test.c disk.h sfs.h
	gcc -c -g tests/sfs_test.c -o tests/sfs_test.o

# SFS file and directory level testing
sfs_test2: tests/sfs_test_2.o disk.o sfs.o 
	gcc -o tests/sfs_test2.out tests/sfs_test_2.o disk.o sfs.o -lm 
	./tests/sfs_test2.out >  ./tests/sfs_test_2_op
	diff ./tests/sfs_test_2_op ./golden_output/sfs_test_2_op_golden
sfs_test2.o: tests/sfs_test_2.c disk.h sfs.h
	gcc -c -g tests/sfs_test_2.c -o tests/sfs_test_2.o

# Persistance testing


clean:
	rm -rf */*.out *.out
	rm -rf */*.o *.o
	rm -rf */*.ko *.ko