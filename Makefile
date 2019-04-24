# Include path where lua.h, luaconf.h and lauxlib.h reside:
LUA_INCLUDE_DIR= /usr/include/lua5.1
INCLUDES= -I$(PWD) -I$(LUA_INCLUDE_DIR)

# Lua executable name. Used to find the install path and for testing.
LUA= lua

CC= gcc
CCOPT= -O2 -fomit-frame-pointer
CCWARN= -Wall
SOCC= $(CC) -shared
SOCFLAGS= -fPIC $(CCOPT) $(CCWARN) $(DEFINES) $(INCLUDES) $(CFLAGS)
SOLDFLAGS= -fPIC $(LDFLAGS)
RM= rm -rf

DEP= skiplist
MODNAME= lzset
MODSO= $(MODNAME).so

all: $(MODSO)

# Alternative target for compiling on Mac OS X:
macosx:
	$(MAKE) all "SOCC=MACOSX_DEPLOYMENT_TARGET=10.4 $(CC) -dynamiclib -single_module -undefined dynamic_lookup"

$(DEP).o: $(DEP).c
	$(CC) $(SOCFLAGS) -c -o $@ $<

$(MODNAME).o: $(MODNAME).c
	$(CC) $(SOCFLAGS) -c -o $@ $<

$(MODSO): $(MODNAME).o $(DEP).o
	$(SOCC) $(SOLDFLAGS) -o $(MODSO) $^

clean:
	$(RM) *.o *.so *.dylib *.out build

test:
	luajit test.lua
	luajit test_zset.lua


.PHONY: all macosx test clean
