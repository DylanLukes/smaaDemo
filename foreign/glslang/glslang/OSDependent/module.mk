sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	Unix \
	Windows \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	# empty line


ifeq ($(WIN32),y)

SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(SRC_$(d)/Windows)

else  # WIN32

SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(SRC_$(d)/Unix)

endif  # WIN32


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
