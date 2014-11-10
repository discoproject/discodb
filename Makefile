export

DISCODB_VERSION = 0.2
DISCODB_RELEASE = 0.2

prefix     = /usr/local

CFLAGS     = -O3
CSRCS      = $(wildcard src/*.c)
COBJS      = $(patsubst %.c,%.o,$(CSRCS))
REBAR      = rebar
PYTHON     = python
SPHINXOPTS = "-D version=$(DISCODB_VERSION) -D release=$(DISCODB_RELEASE)"

.PHONY: build clean doc doc-clean erlang python

build: $(COBJS)
utils: create query
create query: build
	$(CC) $(CFLAGS) -Isrc -o $@ src/util/$@.c src/*.o -lcmph

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

libdiscodb.a: $(COBJS)
	$(AR) -ruvs $@ $^

libdiscodb.so: $(COBJS)
	$(CC) $(CFLAGS) -Isrc -shared -o $@ $^ -lcmph

clean:
	rm -rf `find . -name \*.o`
	rm -rf create query *.dSYM
	rm -rf python/build
	rm -rf erlang/ebin erlang/priv

doc:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) html)

doc-clean:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) clean)

doc-test:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) doctest)

erlang: CMD = compile
erlang:
	(cd erlang && $(REBAR) $(CMD))

python: CMD = build
python:
	(cd python && $(PYTHON) setup.py $(CMD))
