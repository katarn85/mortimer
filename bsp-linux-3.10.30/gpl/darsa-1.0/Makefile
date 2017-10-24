include ../../configs.mk

BUILD  		:= daverify_arm
BUILDX86RSA := darsa
BUILDX86VER := daverify

CFLAGS   += -O6 -Wall -D_REENTRANT 
#LDFLAGS  += -static
INCLUDE  = -I./ 
LIBRARY  = -L./

COBJS	:= sha256.o da-rsa-verify.o sha1.o
SRCS 	:= $(COBJS:.o=.c)

.PHONY: all clean

all: clean $(BUILDX86RSA) $(BUILDX86VER) $(BUILD)

$(BUILDX86RSA):
	gcc -o $(BUILDX86RSA) da-rsa-sign.c da-rsa-verify.c sha256.c sha1.c -lcrypto -lssl -DRSA_FULL_INCLUDE

$(BUILDX86VER):
	gcc -o $(BUILDX86VER) da-rsa-verify.c sha256.c sha1.c

$(BUILD): depend $(COBJS)
	$(CC) $(COBJS) -o $(BUILD) $(CFLAGS) $(LIBRARY) $(LDFLAGS) 

depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
	
clean:
	rm -rf $(BUILDX86RSA) $(BUILDX86VER) $(BUILD)
	rm -f *.o *.s
	rm -f *~
	rm -f .depend
