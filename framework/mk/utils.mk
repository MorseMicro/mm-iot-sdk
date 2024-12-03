#
# Copyright 2022-2023 Morse Micro
#

#
# Like $(dir) but remove the trailing slash, and change '.' to ''
#
# For example:
#     $(call dirname,a/b/c)		-> a/b
#     $(call dirname,a/b)		-> a
#     $(call dirname,a)			->
#
dirname = $(patsubst .,,$(patsubst %/,%,$(dir $1)))

#
# Return a space separated list of the levels up to the given file/dir name.
#
# For example:
# 		$(call dirlevel,a/b/c/d)
# 	returns:
#		a a/b a/b/c a/b/c/d
dirlevels=$(if $(call dirname,$(1)),$(call dirlevels,$(call dirname,$(1)))) $(1)

#
# Gets the file-specific CFLAGS for a given file (excluding log level CFLAGS, see loglevel_cflags).
# Note that this will include CFLAGS specific to the given file and any parent directories.
#
# CFLAGS will be ordered from least specific to most specific (i.e., those that apply to the
# top level directory first and those only applying to the specific file last).
#
# For example, if we have defined:
#     CFLAGS-a=-Werror
#     CFLAGS-a/b=-Wall
#     CFLAGS-a/b/c=-Wextra
#
# Then we would get the following results:
#     $(call file_cflags,a/b/c)		-> -Werror -Wall -Wextra
#     $(call file_cflags,a/b)		-> -Werror -Wall
#     $(call file_cflags,a)			-> -Werror
#     $(call file_cflags,a/b/x)		-> -Werror -Wall
#     $(call file_cflags,a/d)		-> -Werror
#     $(call file_cflags,z)			->
#
file_cflags=$(foreach dirlevel,$(call dirlevels,$(1)),$(CFLAGS-$(dirlevel)))

#
# Checks to see if the given files can be found. If files can't be found an error will be raised
# printing the files that could not be found.
#
# $(1) Space separated list of file paths to check.
define check_for_files =
	$(foreach FILE,$(1),$(if $(wildcard $(FILE)),,$(eval MISSING_FILES += $(FILE))))
	$(if $(MISSING_FILES), $(info Missing the following files:)\
		$(foreach file,$(MISSING_FILES),$(info $(file)))\
		$(error))
endef

#
# Hash the given input data and return the hash as a uint32.
#
# $(1) = data to hash
#
uint32_hash = 0x$(shell echo $(1) | sha256sum | cut -c1-8 -)
