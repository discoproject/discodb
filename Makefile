export

DISCODB_VERSION = 0.2
DISCODB_RELEASE = 0.2

prefix     = /usr/local

CC         = gcc
CFLAGS     = -O3
REBAR      = rebar
PYTHON     = python
SPHINXOPTS = "-D version=$(DISCODB_VERSION) -D release=$(DISCODB_RELEASE)"

ifeq ($(shell uname), Darwin)
	CFLAGS = -O3 -fnested-functions
endif

.PHONY: build clean doc doc-clean erlang python

build: $(patsubst %.c,%.o,$(wildcard src/*.c))
utils: create query
create query: build
	$(CC) $(CFLAGS) -Isrc -lcmph -o $@ src/util/$@.c src/*.o

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

clean:
	rm -rf `find . -name \*.o`
	rm -rf create query
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

python: CFLAGS =
python: CMD = build
python:
	(cd python && $(PYTHON) setup.py $(CMD))
