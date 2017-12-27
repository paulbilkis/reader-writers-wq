OBJS := main.o
OBJS_READER := reader.o methods.o #lab4_methods.o
OBJS_WRITER := writer.o methods.o #lab4_methods.o
BIN := lab4
CC := gcc
LDFLAGS := -lpthread -lrt 
CFLAGS := -m64 -Wall -ggdb
OBJ_DIR := objs

$(OBJ_DIR):
	mkdir objs

all: $(BIN)

rw: reader writer

reader: $(OBJS_READER)
	gcc -o $@ $(OBJS_READER:%.o=$(OBJ_DIR)/%.o) $(LDFLAGS)

writer: $(OBJS_WRITER)
	gcc -o $@ $(OBJS_WRITER:%.o=$(OBJ_DIR)/%.o) $(LDFLAGS)

$(BIN): $(OBJS)
	gcc -o $@ $(OBJS:%.o=$(OBJ_DIR)/%.o) $(LDFLAGS)

%.o: %.c
	gcc -o $(OBJ_DIR)/$@ -c $< $(CFLAGS) 

clean:
	rm $(OBJ_DIR)/*.o $(BIN)

run: $(BIN)
	./$(BIN)
