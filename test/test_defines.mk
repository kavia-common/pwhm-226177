ifndef STAGINGDIR
	$(error "STAGINGDIR not defined")
endif

RAW_VERSION ?= $(if $(shell git describe --tags 2> /dev/null),$(shell git describe --tags),v0.0.0)
VERSION_PREFIX ?= $(shell echo $(RAW_VERSION) | sed 's/[av][0-9]\+.*//')
VERSION = $(if $(findstring .,$(RAW_VERSION)),$(subst $(VERSION_PREFIX)v,,$(strip $(RAW_VERSION))),$(strip $(RAW_VERSION)))
MACHINE = $(shell $(CC) -dumpmachine)
PKGCONFDIR = $(STAGINGDIR)/usr/lib/pkgconfig
SRCDIR = $(realpath .) # compile only test source, plugin should be already compiled with COVERAGE=y option
OBJDIR = $(realpath ../../output/$(MACHINE))/test
TEST_COMMON_SRC_DIR = $(realpath ../testHelper/)
LIB_OBJ_DIR = $(realpath ../../output/$(MACHINE))

INCDIRS = $(realpath ../../include_priv ../../include ../../include/wld ../testHelper/)
INCDIRS += $(realpath $(STAGINGDIR)/usr/include)

LD_LIB=LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(STAGINGDIR)/usr/lib:$(STAGINGDIR)/lib:$(LIB_OBJ_DIR):$(LIB_OBJ_DIR)/Plugin

SOURCES += $(wildcard $(TEST_COMMON_SRC_DIR)/*.c)
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
TARGET = $(OBJDIR)/run_test_$(AUTO_TEST_FILE)

CFLAGS += -fprofile-arcs -ftest-coverage -fprofile-abs-path \
		  -g3 \
		  -Wextra -Wall -Werror \
		  $(addprefix -I ,$(INCDIRS)) \
		  $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --cflags sahtrace pcb cmocka swla swlc openssl test-toolbox)

LDFLAGS += -fprofile-arcs -ftest-coverage -fprofile-abs-path \
		   -L../../output/$(MACHINE) \
		   -L../../output/$(MACHINE)/Plugin \
		   -l:libwld.so.$(VERSION) \
		   -l:wld.so.$(VERSION) \
		   -L$(STAGINGDIR)/lib \
		   -L$(STAGINGDIR)/usr/lib \
		   -Wl,-rpath,$(STAGINGDIR)/lib \
		   -Wl,-rpath,$(STAGINGDIR)/usr/lib \
		   $(shell PKG_CONFIG_PATH=$(PKGCONFDIR) pkg-config --define-prefix --libs sahtrace pcb cmocka swla swlc openssl test-toolbox) \
		   -lamxb -lamxc -lamxd -lamxo -lamxp -lamxj  -lm
