BINS = frog
PKGS = gtkmm-4.0 gtk4-layer-shell-0 blkid libudev
SRCS +=	$(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
SHAREDIR ?= $(PREFIX)/share

CXXFLAGS += -Oz -s -Wall -flto -std=c++20
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -Wl,--gc-sections

JOB_COUNT := $(BINS) $(OBJS)
JOBS_DONE := $(shell ls -l $(JOB_COUNT) 2> /dev/null | wc -l)

define progress
	$(eval JOBS_DONE := $(shell echo $$(($(JOBS_DONE) + 1))))
	@printf "[$(JOBS_DONE)/$(shell echo $(JOB_COUNT) | wc -w)] %s %s\n" $(1) $(2)
endef

$(BINS): $(OBJS)
	$(call progress, Linking $@)
	@$(CXX) -o $(BINS) $(OBJS) \
	$(LDFLAGS) \
	$(CXXFLAGS)

%.o: %.cpp
	$(call progress, Compiling $@)
	@$(CXX) $(CFLAGS) -c $< -o $@ \
	$(CXXFLAGS)

install: $(BINS)
	@echo "Installing..."
	@install -D -t $(DESTDIR)$(BINDIR) $(BINS)
	@install -D -t $(DESTDIR)$(SHAREDIR)/applications data/frog.desktop
	@install -D -t $(DESTDIR)$(SHAREDIR)/pixmaps data/frog.png

clean:
	@echo "Cleaning up"
	@rm $(BINS) $(SRCS:.cpp=.o)
