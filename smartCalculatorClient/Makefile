all=calculatorClient
SOURCE=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(SOURCE))
LIBS=-L./libjpeg_arm/lib -ljpeg -lpthread  -L./libopenssl_arm/lib -lcrypto
CFLAGS=-I./libjpeg_arm/include  -I./libopenssl_arm/include/ -DDISPLAY_MODULE_DEBUG 
CC=arm-linux-gcc

.PHONY:clean

$(all) : $(OBJS)
	$(CC) $^ -o $@ $(LIBS) $(CFLAGS)

clean:
	-rm $(all) $(OBJS) -rf



