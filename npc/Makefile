.PHNOY: clean

CSRC := $(shell find . -name "*.cpp")
VSRC := $(shell find . -name "*.v")


TOP = top
BUILD_DIR = obj_dir


VERILATOR = verilator
VERILATOROPT = -Wno-fatal --cc --exe --top-module $(TOP) --build


build: $(CSRC) $(VSRC)
	$(VERILATOR) $(VERILATOROPT) $^

trace: $(CSRC) $(VSRC)
	$(VERILATOR) $(VERILATOROPT) --trace -CFLAGS -DTRACE $^
	./$(BUILD_DIR)/V$(TOP)


sim: build
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	./$(BUILD_DIR)/V$(TOP)

all: sim

clean:
	rm -rf $(BUILD_DIR)

include ../Makefile
