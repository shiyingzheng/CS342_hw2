TARGETS=hw2

hw2: hw2.c
	clang -pthread -o hw2 hw2.c

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

