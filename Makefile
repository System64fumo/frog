EXEC = frog
PKGS = gtkmm-4.0 libmagic
SRCS +=	$(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DESTDIR = $(HOME)/.local

CXXFLAGS += -Oz -s -Wall -flto -std=c++20
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -Wl,--gc-sections

JOB_COUNT := $(EXEC) $(OBJS)
JOBS_DONE := $(shell ls -l $(JOB_COUNT) 2> /dev/null | wc -l)

define progress
	$(eval JOBS_DONE := $(shell echo $$(($(JOBS_DONE) + 1))))
	@printf "[$(JOBS_DONE)/$(shell echo $(JOB_COUNT) | wc -w)] %s %s\n" $(1) $(2)
endef

$(EXEC): $(OBJS)
	$(call progress, Linking $@)
	@$(CXX) -o $(EXEC) $(OBJS) \
	$(LDFLAGS) \
	$(CXXFLAGS)

%.o: %.cpp
	$(call progress, Compiling $@)
	@$(CXX) $(CFLAGS) -c $< -o $@ \
	$(CXXFLAGS)

install: $(EXEC)
	mkdir -p $(DESTDIR)/bin
	install $(EXEC) $(DESTDIR)/bin/$(EXEC)

clean:
	@echo "Cleaning up"
	@rm $(EXEC) $(SRCS:.cpp=.o)
