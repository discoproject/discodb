DISCODB_VERSION = 0.2
DISCODB_RELEASE = 0.2

prefix     = /usr/local

PYTHON     = python
PYTHONENVS = DISCODB_VERSION=$(DISCODB_VERSION) DISCODB_RELEASE=$(DISCODB_RELEASE)
SPHINXOPTS = "-D version=$(DISCODB_VERSION) -D release=$(DISCODB_RELEASE)"

.PHONY: build clean install doc doc-clean

build:
	$(PYTHONENVS) $(PYTHON) setup.py build

clean:
	rm -rf build

install:
	$(PYTHONENVS) $(PYTHON) setup.py install --root=$(DESTDIR)/ --prefix=$(prefix)

doc:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) html)

doc-clean:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) clean)

doc-test:
	(cd doc && $(MAKE) SPHINXOPTS=$(SPHINXOPTS) doctest)