OBJLIBS	= libxscontroller libxscommon libxstypes

all : $(OBJLIBS)

libxscontroller : libxscommon
	$(MAKE) -C xscontroller

libxscommon : libxstypes
	$(MAKE) -C xscommon

libxstypes :
	$(MAKE) -C xstypes libxstypes.a

clean :
	-$(MAKE) -C xscontroller clean
	-$(MAKE) -C xscommon clean
	-$(MAKE) -C xstypes clean
