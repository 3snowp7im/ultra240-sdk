SUBDIRS = ../ultra-sdk ../ultra-sdk-posix

bin_PROGRAMS = ultra-sdk-world
ultra_sdk_world_SOURCES = ultra-sdk-world.cc
ultra_sdk_world_CXXFLAGS = $(JSON_CFLAGS) -I$(srcdir)/../../include
ultra_sdk_world_LDADD = \
	$(JSON_LIBS) \
	$(YAML_LIBS) \
	../ultra-sdk/libultra-sdk.a \
	../ultra-sdk-posix/libultra-sdk-posix.a
