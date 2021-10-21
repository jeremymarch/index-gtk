CC = gcc
INCLUDES = `pkg-config --cflags mariadb`
LIBS = `pkg-config --libs mariadb`
GTK_INC = `pkg-config --cflags gtk+-2.0`
GTK_LIBS = `pkg-config --libs gtk+-2.0`
DEBUG = -g -Wall

all: zindex

zindex: generic_index_window.o mysql_index.o main.o index_tree.o
	$(CC) $(DEBUG) -o zindex generic_index_window.o mysql_index.o main.o index_tree.o $(GTK_LIBS) $(LIBS)

main.o: main.c generic_index_window.h
	$(CC) $(DEBUG) -c $(GTK_INC) $(INCLUDES) main.c

generic_index_window.o: generic_index_window.c mysql_index.h
	$(CC) $(DEBUG) -c $(GTK_INC) $(INCLUDES) generic_index_window.c

mysql_index.o: mysql_index.c mysql_index.h
	$(CC) $(DEBUG) -c $(GTK_INC) $(INCLUDES) mysql_index.c

index_tree.o: index_tree.c index_tree.h
	$(CC) $(DEBUG) -c $(GTK_INC) $(INCLUDES) index_tree.c

clean:
	rm -f zindex generic_index_window.o mysql_index.o main.o index_tree.o
