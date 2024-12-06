BINS = frog
PKGS = gtkmm-4.0 gtk4-layer-shell-0 blkid libudev gstreamer-1.0 gstreamer-video-1.0
SRCS +=	$(wildcard src/*.cpp)
OBJS = $(patsubst src/%,$(BUILDDIR)/%,$(SRCS:.cpp=.o))

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
BUILDDIR = build

CXXFLAGS += -Oz -s -Wall -flto -std=c++20
CXXFLAGS += $(shell pkg-config --cflags $(PKGS)) -I "include"
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -Wl,--gc-sections

$(shell mkdir -p $(BUILDDIR))
JOB_COUNT := $(BINS) $(OBJS)
JOBS_DONE := $(shell ls -l $(JOB_COUNT) 2> /dev/null | wc -l)

define progress
	$(eval JOBS_DONE := $(shell echo $$(($(JOBS_DONE) + 1))))
	@printf "[$(JOBS_DONE)/$(shell echo $(JOB_COUNT) | wc -w)] %s %s\n" $(1) $(2)
endef

$(BINS): $(OBJS)
	$(call progress, Linking $@)
	@$(CXX) -o \
	$(BUILDDIR)/$(BINS) \
	$(OBJS) \
	$(LDFLAGS) \
	$(CXXFLAGS)

$(BUILDDIR)/%.o: src/%.cpp
	$(call progress, Compiling $@)
	@$(CXX) -c $< -o $@ $(CXXFLAGS)

install: $(BINS)
	@echo "Installing..."
	@install -D -t $(DESTDIR)$(BINDIR) $(BUILDDIR)/$(BINS)
	@install -D -t $(DESTDIR)$(DATADIR)/sys64/frog config.conf style.css
	@install -D -t $(DESTDIR)$(DATADIR)/applications data/frog.desktop
	@install -D -t $(DESTDIR)$(DATADIR)/pixmaps data/frog.png

clean:
	@echo "Cleaning up"
	@rm -r $(BUILDDIR)
