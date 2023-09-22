include ../../make.mk

INC += \
	src \
	src/class/hid \
	$(TOP)/hw \

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
EXAMPLE_SOURCE += $(wildcard src/class/hid/*.c)

SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

CFLAGS += \
	-fshort-wchar \

include ../../rules.mk
