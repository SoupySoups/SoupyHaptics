CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

SRC_DIR  := src
BUILD    := build
TARGET   := middleware

# All source files
ALL_SRCS := $(wildcard $(SRC_DIR)/*.cpp)

# Remove transports so we can select one cleanly
SRCS_NO_TRANSPORT := $(filter-out \
    $(SRC_DIR)/usb_transport.cpp \
    $(SRC_DIR)/sim_transport.cpp, \
    $(ALL_SRCS))

# ---- detect windows ----
ifeq ($(OS),Windows_NT)
    LDFLAGS_SIM := -lws2_32
else
    LDFLAGS_SIM :=
endif

USB_CFLAGS := $(shell pkg-config --cflags libusb-1.0)
USB_LIBS   := $(shell pkg-config --libs libusb-1.0)

all: hw

# ---------------- USB build ----------------

hw: CXXFLAGS += -DUSE_USB $(USB_CFLAGS)
hw: SRCS := $(SRCS_NO_TRANSPORT) $(SRC_DIR)/usb_transport.cpp
hw: $(BUILD)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD)/$(TARGET) $(USB_LIBS)
	@echo "Built hardware mode"

# ---------------- SIM build ----------------

sim: CXXFLAGS += -DUSE_SIM
sim: SRCS := $(SRCS_NO_TRANSPORT) $(SRC_DIR)/sim_transport.cpp
sim: $(BUILD)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD)/$(TARGET) $(LDFLAGS_SIM)
	@echo "Built sim mode"

# ---------------- dir ----------------

$(BUILD):
	mkdir -p $(BUILD)

# ---------------- clean ----------------

clean:
	rm -rf $(BUILD)