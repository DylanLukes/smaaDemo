sp             := $(sp).x
dirstack_$(sp) := $(d)
d              := $(dir)


SUBDIRS:= \
	opt \
	util \
	val \
	# empty line

DIRS:=$(addprefix $(d)/,$(SUBDIRS))

$(eval $(foreach directory, $(DIRS), $(call directory-module,$(directory)) ))


FILES:= \
	assembly_grammar.cpp \
	binary.cpp \
	diagnostic.cpp \
	disassemble.cpp \
	enum_string_mapping.cpp \
	extensions.cpp \
	ext_inst.cpp \
	id_descriptor.cpp \
	libspirv.cpp \
	name_mapper.cpp \
	opcode.cpp \
	operand.cpp \
	parsed_operand.cpp \
	print.cpp \
	software_version.cpp \
	spirv_endian.cpp \
	spirv_target_env.cpp \
	spirv_validator_options.cpp \
	table.cpp \
	text.cpp \
	text_handler.cpp \
	# empty line


SRC_$(d):=$(addprefix $(d)/,$(FILES)) $(foreach directory, $(DIRS), $(SRC_$(directory)) )


d  := $(dirstack_$(sp))
sp := $(basename $(sp))
