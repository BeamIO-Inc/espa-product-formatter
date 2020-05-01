#-----------------------------------------------------------------------------
# Makefile
#
# Project Name: product formatter
#-----------------------------------------------------------------------------
.PHONY: check-environment all install clean all-raw-binary install-raw-binary clean-raw-binary rpms schema-rpm build test deploy login

include make.config

DIR_RAW_BINARY = raw_binary
DIR_PYTHON = py_modules
DIR_SCHEMA = schema

BRANCH     := $(or $(CI_COMMIT_REF_NAME), $(shell git rev-parse --abbrev-ref HEAD))
BRANCH     := $(shell echo $(BRANCH) | tr / -)
SHORT_HASH := `git rev-parse --short HEAD`
VERSION    := $(shell cat version.txt)
REPO_TAG   := $(VERSION)-$(SHORT_HASH)

#-----------------------------------------------------------------------------
# ESPA Standard Makefile targets
#
#-----------------------------------------------------------------------------
build:

tests:

deploy: login
ifeq ("$(BRANCH)", "master")
	@echo "master branch update, creating tag: $(REPO_TAG)"
	git tag $(REPO_TAG)
	git push origin $(REPO_TAG)
else
	@echo "not the master branch, no new tag will be created"
endif

#-----------------------------------------------------------------------------
all: all-raw-binary

install: check-environment install-raw-binary install-python install-schema

clean: clean-raw-binary

#-----------------------------------------------------------------------------
all-raw-binary:
	echo "make all in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) all);

install-raw-binary:
	echo "make install in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) install);

clean-raw-binary:
	echo "make clean in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) clean);

#-----------------------------------------------------------------------------
install-python:
	echo "make install in $(DIR_PYTHON)"; \
        (cd $(DIR_PYTHON); $(MAKE) install);

#-----------------------------------------------------------------------------
install-schema:
	echo "make install in $(DIR_SCHEMA)"; \
        (cd $(DIR_SCHEMA); $(MAKE) install);

#-----------------------------------------------------------------------------
rpms:
	rpmbuild -bb --clean RPM_spec_files/RPM.spec

schema-rpm:
	rpmbuild -bb --clean RPM_spec_files/RPM-SCHEMAS.spec

#-----------------------------------------------------------------------------
check-environment:
ifndef PREFIX
    $(error Environment variable PREFIX is not defined)
endif

#-----------------------------------------------------------------------------
login:
	@$(if $(and $(CI_REGISTRY_USER), $(CI_REGISTRY_PASSWORD)), \
          docker login  -u $(CI_REGISTRY_USER) \
                        -p $(CI_REGISTRY_PASSWORD) \
                         $(CI_REGISTRY), \
          docker login)