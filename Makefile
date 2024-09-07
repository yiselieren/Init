###
### Init1
### -----
###

### Common
### ------
OBJ_DIR  = OBJS
TRG1    = init
SRCS1   =            \
	Application.cpp  \
	Log.cpp          \
	str_utils.cpp    \
	run_utils.cpp    \
	proc_utils.cpp   \
    Connection.cpp   \
	init.cpp
TRG2    = TestMem
SRCS2   = TestMem.cpp
TRG3    = TestCpu
SRCS3   = TestCpu.cpp


### General flags
ifeq ($(DEBUG),)
CFLAGS   = -O3 -Wall -W -Werror
CPPFLAGS = -std=gnu++11 -O3 -Wall -W -Werror
else
CFLAGS = -g  -W -Werror -DDEBUG
CPPFLAGS = -std=gnu++11 -g  -W -Werror -DDEBUG
endif
LIBS    = -lpthread -lutil
LFLAGS  =
CC      = gcc
CLINK   = gcc
CPP     = g++
CPPLINK = g++

all:	$(TRG1) $(TRG2) $(TRG3)
clean:
	@rm -fr $(OBJ_DIR) core* valgrind* *.gdb *.o *.d *.obj $(TRG1) $(TRG2) $(TRG3) gede*.ini *~ *.log*


### Input files
### -----------
OBJS1  = $(SRCS1:%.cpp=$(OBJ_DIR)/%.o)
OBJS2  = $(SRCS2:%.cpp=$(OBJ_DIR)/%.o)
OBJS3  = $(SRCS3:%.cpp=$(OBJ_DIR)/%.o)

### Load dependecies
### ----------------
DEPS = $(wildcard $(OBJ_DIR)/*.d)
ifneq ($(strip $(DEPS)),)
include $(DEPS)
endif

### Dependecies generation
### --------------------------------------
define DEPENDENCIES
@if [ ! -f $(@D)/$(<F:.c=.d) ]; then \
    sed 's/^$(@F):/$(@D)\/$(@F):/g' < $(<F:.c=.d) > $(@D)/$(<F:.c=.d); \
    rm -f $(<F:.c=.d); \
fi
endef
define DEPENDENCIES_CPP
@if [ ! -f $(@D)/$(<F:.cpp=.d) ]; then \
    sed 's/^$(@F):/$(@D)\/$(@F):/g' < $(<F:.cpp=.d) > $(@D)/$(<F:.cpp=.d); \
    rm -f $(<F:.cpp=.d); \
fi
endef

### Target rules
### ------------
# general compilation
$(OBJ_DIR)/%.o: %.c
		$(CC) -c -MD $(CFLAGS) $(INC) $(DEFS) -o $@ $<
		$(DEPENDENCIES)
$(OBJ_DIR)/%.o: %.cpp
		$(CPP) -c -MD $(CPPFLAGS) $(INC) $(DEFS) -o $@ $<
		$(DEPENDENCIES_CPP)

$(OBJ_DIR):
		mkdir $(OBJ_DIR)

# Targets
$(TRG1):	$(OBJ_DIR) $(OBJS1)  Makefile
			$(CPPLINK) -o $@ $(LFLAGS) $(OBJS1) $(LIBS)
$(TRG2):	$(OBJ_DIR) $(OBJS2)  Makefile
			$(CPPLINK) -o $@ $(LFLAGS) $(OBJS2) $(LIBS)
$(TRG3):	$(OBJ_DIR) $(OBJS3)  Makefile
			$(CPPLINK) -o $@ $(LFLAGS) $(OBJS3) $(LIBS)
