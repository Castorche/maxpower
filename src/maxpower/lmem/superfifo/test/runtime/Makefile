NAME=superFifoTest
DESIGNNAME=SuperFifoTest_HW
CC=gcc

USE_SLIC = 1
include $(MAXCOMPILERDIR)/lib/Makefile.include

CFLAGS += -std=gnu99 -Wall -Wextra -DDESIGN_NAME=$(DESIGNNAME) 
CFLAGS += $(MAXCOMPILER_INC) 
LDFLAGS += -ggdb $(MAXCOMPILER_LIBS)

%.o: %.max
	$(SLICCOMPILE) $^ $@ 

TARGETS := $(NAME) 

all: $(TARGETS)

$(NAME): $(NAME).o \
	$(DESIGNNAME).o 
	$(CC) $^ $(LDFLAGS) -o $@

restart_sim:
	$(MAXCOMPILERDIR)/bin/maxcompilersim -c ISCA restart


clean:
	$(RM) *.o $(TARGETS)
