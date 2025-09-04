include ../makefile.inc


# build destination directories
OBJDIR = ../output/$(MACHINE)

# TARGETS
TARGET_SO = $(OBJDIR)/libwld.so.$(VERSION)

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include/libnl3)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

# files
#HEADERS = $(wildcard $(INCDIR_PUB)/$(TARGET_NAME)/*.h)
SOURCES = $(wildcard *.c */*.c */*/*.c)
OBJECTS_BASE = $(SOURCES:.c=.o)
SUBDIRS_BASE = $(shell find $(SRCDIR) -type d)

EXCLUDED_OBJECTS = Plugin/%

ifeq ($(CONFIG_SAH_SERVICES_PWHM_DISABLE_PERSIST),y)
$(info Feature persist OFF)
	EXCLUDED_OBJECTS += %/wld_persist_on.o
else
$(info Feature persist ON)
	EXCLUDED_OBJECTS += %/wld_persist_off.o
endif

OBJECTS = $(addprefix $(OBJDIR)/,$(filter-out $(EXCLUDED_OBJECTS),$(OBJECTS_BASE)))
SUBDIRS = $(addprefix $(OBJDIR),$(patsubst $(SRCDIR)%,%/,$(SUBDIRS_BASE)))

CFLAGS += -std=gnu99 -fPIC -g -Wall -Werror -Wextra  \
		-I. \
		$(addprefix -I ,$(INCDIRS)) \
		-I../include_priv/ChannelManagement/ \
		-I../include_priv/VendorModule/ \
		-I../include/wld/ \
		-I../include/ \
		-I../include_priv/nl80211/ \
		-DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=500 \
		-DROOT_OBJ_PREFIX_STR="\"$(CONFIG_SAH_WLD_ROOT_OBJ_PREFIX)\""
CFLAGS += -Wformat -Wformat-security -Wimplicit-function-declaration -Wl,--no-undefined -Wno-unused-variable
LDFLAGS += $(STAGING_LIBDIR) -shared -lsahtrace -lswlc -lswla -lcrypto -lssl -lnl-3 -lnl-genl-3 -lrt -lm -lamxo -lamxb -lamxc -lamxm -lamxp -lamxd -ldl

ifeq ($(COVERAGE),y)
CFLAGS +=-fprofile-arcs -ftest-coverage -fprofile-abs-path
LDFLAGS += -fprofile-arcs -ftest-coverage -fprofile-abs-path
endif

all: $(TARGET_SO)
	ln -sf libwld.so.$(VERSION) $(OBJDIR)/libwld.so

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,libwld.so -o $(@) $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/ $(SUBDIRS)
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

%/:
	$(MKDIR) -p $@

clean:
	rm -rf ../output

.PHONY: all clean 
