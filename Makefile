# ============================================================
# Compiler & Flags
# ============================================================
CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread
DEBUG_FLAGS = -g -DDEBUG
LIBS = -lcurl

# ============================================================
# Directories
# ============================================================
BUILD_DIR   = build/cpp
COMMON_INC  = common/cpp/include
COMMON_SRC  = common/cpp/src
SENDER_DIR  = sender/cpp
RECEIVER_DIR = receiver/cpp

# ============================================================
# Common object files
# ============================================================
COMMON_OBJS = $(BUILD_DIR)/dns_tcp_client.o \
              $(BUILD_DIR)/extension_map.o \
              $(BUILD_DIR)/dns_https_client.o

# ============================================================
# All targets
# ============================================================
TARGETS = \
	$(BUILD_DIR)/sender.exe \
	$(BUILD_DIR)/sender_v1.exe \
	$(BUILD_DIR)/sender_v1_debug.exe \
	$(BUILD_DIR)/sender_v2.exe \
	$(BUILD_DIR)/sender_v2_debug.exe \
	$(BUILD_DIR)/sender_v3.exe \
	$(BUILD_DIR)/sender_v3_debug.exe \
	$(BUILD_DIR)/sender_v4.exe \
	$(BUILD_DIR)/sender_v4_debug.exe \
	$(BUILD_DIR)/sender_v4_demo.exe \
	$(BUILD_DIR)/receiver.exe \
	$(BUILD_DIR)/receiver_v1.exe \
	$(BUILD_DIR)/receiver_v1_debug.exe \
	$(BUILD_DIR)/receiver_v2.exe \
	$(BUILD_DIR)/receiver_v2_debug.exe \
	$(BUILD_DIR)/receiver_v3.exe \
    $(BUILD_DIR)/receiver_v3_debug.exe \
	$(BUILD_DIR)/receiver_v4.exe \
    $(BUILD_DIR)/receiver_v4_debug.exe \
	$(BUILD_DIR)/receiver_v4_demo.exe 

# ============================================================
# Default target
# ============================================================
.PHONY: all clean sender receiver

all: $(TARGETS)

sender: \
	$(BUILD_DIR)/sender_v1.exe \
	$(BUILD_DIR)/sender_v2.exe \
	$(BUILD_DIR)/sender_v3.exe \
	$(BUILD_DIR)/sender_v4.exe 

receiver: \
	$(BUILD_DIR)/receiver_v1.exe \
	$(BUILD_DIR)/receiver_v2.exe \
	$(BUILD_DIR)/receiver_v3.exe \
	$(BUILD_DIR)/receiver_v4.exe

# ============================================================
# Common objects
# ============================================================
$(BUILD_DIR)/dns_tcp_client.o: $(COMMON_SRC)/dns_tcp_client.cpp
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) -c $< -o $@

$(BUILD_DIR)/extension_map.o: $(COMMON_SRC)/extension_map.cpp
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) -c $< -o $@

$(BUILD_DIR)/dns_https_client.o: $(COMMON_SRC)/dns_https_client.cpp
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) -c $< -o $@

# ============================================================
# Sender targets
# ============================================================
$(BUILD_DIR)/sender_v1.exe: $(SENDER_DIR)/sender_v1.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v1_debug.exe: $(SENDER_DIR)/sender_v1.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v2.exe: $(SENDER_DIR)/sender_v2.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v2_debug.exe: $(SENDER_DIR)/sender_v2.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v3.exe: $(SENDER_DIR)/sender_v3.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v3_debug.exe: $(SENDER_DIR)/sender_v3.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v4.exe: $(SENDER_DIR)/sender_v4.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v4_debug.exe: $(SENDER_DIR)/sender_v4.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/sender_v4_demo.exe: $(SENDER_DIR)/sender_v4_demo.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

# sender.exe -> 指向最新版本 (v4)
$(BUILD_DIR)/sender.exe: $(BUILD_DIR)/sender_v4.exe
	cp $< $@


# ============================================================
# Receiver targets
# ============================================================
$(BUILD_DIR)/receiver_v1.exe: $(RECEIVER_DIR)/receiver_v1.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v1_debug.exe: $(RECEIVER_DIR)/receiver_v1.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v2.exe: $(RECEIVER_DIR)/receiver_v2.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v2_debug.exe: $(RECEIVER_DIR)/receiver_v2.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v3.exe: $(RECEIVER_DIR)/receiver_v3.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v3_debug.exe: $(RECEIVER_DIR)/receiver_v3.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v4.exe: $(RECEIVER_DIR)/receiver_v4.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v4_debug.exe: $(RECEIVER_DIR)/receiver_v4.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

$(BUILD_DIR)/receiver_v4_demo.exe: $(RECEIVER_DIR)/receiver_v4_demo.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -I$(COMMON_INC) $^ -o $@ $(LIBS)

# receiver.exe -> 指向最新版本 (v4)
$(BUILD_DIR)/receiver.exe: $(BUILD_DIR)/receiver_v4.exe
	cp $< $@

# ============================================================
# Clean
# ============================================================
clean:
	rm -f $(BUILD_DIR)/*.exe $(BUILD_DIR)/*.o
