CC	=gcc
CFLAGS	=-c
HEAD = header.h

all: oss user

oss: oss.o $(HEAD) 
	$(CC) -o oss oss.o

user: user.o $(HEAD)
	$(CC) -o user user.o
	
oss.o: oss.c $(HEAD) 
	$(CC) $(CFLAGS) oss.c
	
user.o: user.c $(HEAD)
	$(CC) $(CFLAGS) user.c

clean:
	-rm *.o oss user
