all:
	m4 -D START_ORDER=$(CONFIG_SAH_WLD_INIT_ORDER) Plugin/wld_gen-procd.sh.m4 > Plugin/wld_gen-procd.sh

clean:
	rm -f Plugin/wld_gen-procd.sh

#
# All these targets must be run regardless of the presence of a file
# named after them.
#
.PHONY: all clean

