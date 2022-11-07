include build_config.mk

SUBDIRS=core db driver
LIBS=-pthread -lm  -lzmq -lczmq -lgdsl
.PHONY: $(SUBDIRS) all clean

all: $(SUBDIRS) bin/ycsb-tinyindex

bin/ycsb-tinyindex: db/db_factory_tinyindex.cc core/core_workload.cc driver/ycsbc.cc $(EDB_LIB)
	$(CC) $(CFLAGS) -I. db/db_factory_tinyindex.o core/core_workload.o driver/ycsbc.o $(EDB_LIB) $(LDFLAGS) -o bin/ycsb-tinyindex

$(EDB_LIB):

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	rm -rf bin/*

.PHONY: $(SUBDIRS)
