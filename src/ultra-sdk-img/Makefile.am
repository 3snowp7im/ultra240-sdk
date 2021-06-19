bin_PROGRAMS = ultra-sdk-img
ultra_sdk_img_SOURCES = ultra-sdk-img.cc
ultra_sdk_img_CXXFLAGS = -I$(srcdir)/../../include $(PNG_CFLAGS)
ultra_sdk_img_LDADD = \
	$(PNG_LIBS) \
	../ultra-sdk/libultra-sdk.a \
	../ultra-sdk-posix/libultra-sdk-posix.a
