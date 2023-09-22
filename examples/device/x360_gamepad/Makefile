include ../../make.mk

INC += \
	src \
	src/class/x360 \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
EXAMPLE_SOURCE += $(wildcard src/class/x360/*.c)

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

CFLAGS += \
	-fshort-wchar \

include ../../rules.mk
