COURSE = cs551-17s

PROJECT = prj3

#name of client executable
CLIENT = $(PROJECT)c

#name of server executable
SERVER = $(PROJECT)d

#header files giving interface specs
H_FILES = \
  common.h \
  mat_base.h \
  matmul.h \
  mat_test_data.h

#C files used to build client.
CLIENT_C_FILES = \
  common.c \
  client_main.c \
  client_matmul.c \
  mat_test_data.c 

#all C files used to build server.  
SERVER_C_FILES = \
  common.c \
  server_matmul.c

#all C files used to build modules.  
MODULES_C_FILES = \
  naive_matmul.c \
  smart_matmul.c 

#all C files to be submitted
C_FILES = \
  $(CLIENT_C_FILES) \
  $(SERVER_C_FILES)

#all source files to be submitted (after removing duplicates)
SRC_FILES = \
  $(C_FILES) \
  $(H_FILES) \
  Makefile \
  README

#this Makefile should not need changing below this line

#client objects are all client C files with .c extension replaced by .o
CLIENT_OBJS = $(CLIENT_C_FILES:.c=.o)

#server objects are all server C files with .c extension replaced by .o
SERVER_OBJS = $(SERVER_C_FILES:.c=.o)

#modules are all module C files with .c extension replaced by .mod
MODULES = $(MODULES_C_FILES:.c=.mod)

#dependency files are all C files with .c extension replaced by .depends;
#generated to contain dependencies of .c files.
DEPENDS = $(C_FILES:.c=.depends)

#all targets to be built
TARGETS = $(CLIENT) $(SERVER) $(MODULES)

#specify directory containing header files for library
INCLUDE_DIR = $(HOME)/$(COURSE)/include

#compilation options for the C preprocessor: specify the include dir used
#for searching for #include'd files.
CPPFLAGS=	-I$(INCLUDE_DIR)

#compilation options for compilation proper: -g: debugging;
#-Wall: reasonable warnings; -std=gnu11: language dialect;
#-fPIC: produce position-independent code
CFLAGS = -g -Wall -D_GNU_SOURCE -std=gnu11 -fPIC

#compilation options for libraries: -L specifies directory to be searched
#for libraries and -l specifies name of library (given -l NAME, library
#file name will be libNAME.so for dynamically-linked libraries)
LIBS = -L $(HOME)/$(COURSE)/lib -lcs551 -ldl

#this pseudo-target tells make that it should not check for the
#existence of the prerequisites files; this forces the prerequisites
#to always be remade even if there is a file having the same name
#as a prerequisite.
.PHONY:		all clean modules submit

#phony target to build all targets; since this is the first real
#target, it will be what gets built by default when make is run
#without any specified target.
all:		$(TARGETS)

#target for linking the client executable from the client object files
$(CLIENT):	$(CLIENT_OBJS)
		$(CC) $(CLIENT_OBJS) $(LIBS) -o $@

#target for linking the server executable from the server object files
$(SERVER):	$(SERVER_OBJS)
		$(CC) $(SERVER_OBJS) $(LIBS) -o $@

#phony target to build all modules
modules:	$(MODULES)

#special target to compile a test executable for matrix_test_data
test_matrix_test_data: matrix_test_data.c matrix_test_data.h matrix_mul.h
		$(CC) $(CFLAGS) $(CPPFLAGS)  \
		   -DTEST_MAT_TEST_DATA $<  $(LIBS) -o $@

#pattern rule to build a dynamically-loaded .mod module from a .c file.
%.mod:		%.c
		$(CC) $(CFLAGS) -shared $< -o $@

#phony target to clean out all generated files as well as emacs backup
#files
clean:
		rm -f *.o  *~ $(DEPENDS) $(TARGETS) $(PROJECT).tar.gz

#phony target to create compressed archive of files to be submitted
submit:
		tar -cvzf $(PROJECT).tar.gz \
                    `echo $(SRC_FILES) | perl -pe 's/\s+/\n/g' | sort -u`

##This Perl filter is used by the .depends rule below
PERL_FILTER_INCLUDE = '$$line .= $$_; \
		       if (!/\\$$/) { \
		         @a = split(/\s+/, $$line); \
		         @b = grep { $$_ !~ "$(COURSE)" } @a; \
		         print "@b\n"; \
		         $$line = ""; \
		       } \
		       else { \
		         $$line =~ s/\\$$//; \
		       }'

#This rule creates a .depends file for the .c prerequisite.  The perl
#program removes dependencies on the course library files.
%.depends:	%.c
		@$(CC) $(CPPFLAGS) -MM $< | \
		perl -ne $(PERL_FILTER_INCLUDE) > $@

#include all dependencies files.  Note that include will make them
#(using the above rule() if they don't already exist. The - before
#the include suppresses warnings generated if make finds that the
#dependencies file does not exist.
-include $(DEPENDS)
