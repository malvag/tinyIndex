include ../build_config.mk

SOURCES= \
	db_factory_tinyindex.cc

OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

db_factory_tinyindex.o: db_factory_tinyindex.cc
	$(CC) $(CFLAGS) -I. -I.. -I$(EDB_INC) -c db_factory_tinyindex.cc

clean:
	$(RM) *.o

