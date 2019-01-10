all=calculatorClient
SOURCE=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(SOURCE))
LIBS=-L./libjpeg_arm/lib -ljpeg -lpthread
CFLAGS=-I./libjpeg_arm/include 
CC=arm-linux-gcc

.PHONY:clean

$(all) : $(OBJS)
	$(CC) $^ -o $@ $(LIBS) $(CFLAGS)

clean:
	-rm $(all) $(OBJS) -rf



