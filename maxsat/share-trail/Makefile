SIG=share-trail
TARGET=libipamir$(SIG).a

UPSTREAM=./src
PROFILE_BUILD ?= 0
JEMALLOC_ROOT ?= "./third_party/jemalloc"
JEMALLOC_LIB ?= $(JEMALLOC_ROOT)/lib/libjemalloc.a
LIBS_FILE := LIBS

all: $(TARGET) $(LIBS_FILE)

$(JEMALLOC_LIB):
	@test -d "$(JEMALLOC_ROOT)" || (echo "Missing vendored jemalloc at $(JEMALLOC_ROOT)" >&2; exit 2)
	@cd "$(JEMALLOC_ROOT)" && \
		{ \
			if test -f Makefile; then \
				true; \
			else \
				if test ! -x ./configure; then \
					test -x ./autogen.sh || { echo "Missing jemalloc configure and autogen.sh at $(JEMALLOC_ROOT)" >&2; exit 2; }; \
					./autogen.sh; \
				fi; \
				./configure --disable-shared --enable-static; \
			fi; \
		} && \
		$(MAKE) -j4
	@test -f "$(JEMALLOC_LIB)" || (echo "jemalloc build failed: $(JEMALLOC_LIB) not found" >&2; exit 2)

$(TARGET): $(JEMALLOC_LIB)
	$(MAKE) -C $(UPSTREAM) ipamir PROFILE_BUILD=$(PROFILE_BUILD)
	cp $(UPSTREAM)/lib/libipamircgss2.a $(TARGET)
	cp $(UPSTREAM)/lib/libglucose41.a libglucose41.a

$(LIBS_FILE): $(JEMALLOC_LIB)
	@echo "-L../../maxsat/share-trail/$(JEMALLOC_ROOT)/lib -ljemalloc -lpthread -ldl -lglucose41" > $(LIBS_FILE)

clean:
	$(MAKE) -C $(UPSTREAM) clean-cgss2
	rm -f $(TARGET) $(LIBS_FILE)

.PHONY: all clean
