CC      ?= gcc
CFLAGS  ?= -O3 -march=native -std=c11 -fopenmp -Wall -Wextra
# GMP: system package (Ubuntu/Debian: sudo apt install libgmp-dev), or GMP_DIR=/path/to/gmp
GMP_DIR ?=
GMP_INC  = $(if $(GMP_DIR),-I$(GMP_DIR)/include)
GMP_LIB  = $(if $(GMP_DIR),$(GMP_DIR)/lib/libgmp.a,-lgmp)

SRC = main.c my_bigint.c reference.c baseline_gmp.c

bigint: $(SRC) bigint.h
	$(CC) $(CFLAGS) $(GMP_INC) $(SRC) $(GMP_LIB) -o $@

clean:
	rm -f bigint

.PHONY: clean
