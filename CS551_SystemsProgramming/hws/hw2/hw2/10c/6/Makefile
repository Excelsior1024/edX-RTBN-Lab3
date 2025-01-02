
CFLAGS = -g -Wall -std=gnu11

PROJECT = myexec


H_FILES = \
  get_ids.h \

C_FILES = \
  myexec.c \
  get_ids.c \

SRC_FILES = \
  $(C_FILES) \
  $(H_FILES) \
  Makefile \

OBJS = $(C_FILES:.c=.o)
DEPENDS = $(C_FILES:.c=.depends)

TARGET = $(PROJECT)

.PHONY:		clean all

all:		$(TARGET)

$(TARGET):	$(OBJS)
		$(CC) $(OBJS) $(LIBS) -o $@

hw_num_6: excelsior.c get_ids.c get_ids.h
	$(CC) $(CFLAGS)

clean:
		rm -f *.o  *~ $(DEPENDS) $(TARGET) $(PROJECT).tar.gz
