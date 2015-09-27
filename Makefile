TARGETS=hw2

hw2: hw2.c
	clang -g -pthread -o hw2 hw2.c

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

