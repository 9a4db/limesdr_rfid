# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/sg723/LimeSuite

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/sg723/LimeSuite/udev-rules

# Include any dependencies generated for this target.
include src/examples/CMakeFiles/singleRX.dir/depend.make

# Include the progress variables for this target.
include src/examples/CMakeFiles/singleRX.dir/progress.make

# Include the compile flags for this target's objects.
include src/examples/CMakeFiles/singleRX.dir/flags.make

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o: src/examples/CMakeFiles/singleRX.dir/flags.make
src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o: ../src/examples/singleRX.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/sg723/LimeSuite/udev-rules/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o"
	cd /home/sg723/LimeSuite/udev-rules/src/examples && /usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/singleRX.dir/singleRX.cpp.o -c /home/sg723/LimeSuite/src/examples/singleRX.cpp

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/singleRX.dir/singleRX.cpp.i"
	cd /home/sg723/LimeSuite/udev-rules/src/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/sg723/LimeSuite/src/examples/singleRX.cpp > CMakeFiles/singleRX.dir/singleRX.cpp.i

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/singleRX.dir/singleRX.cpp.s"
	cd /home/sg723/LimeSuite/udev-rules/src/examples && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/sg723/LimeSuite/src/examples/singleRX.cpp -o CMakeFiles/singleRX.dir/singleRX.cpp.s

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.requires:

.PHONY : src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.requires

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.provides: src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.requires
	$(MAKE) -f src/examples/CMakeFiles/singleRX.dir/build.make src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.provides.build
.PHONY : src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.provides

src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.provides.build: src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o


# Object files for target singleRX
singleRX_OBJECTS = \
"CMakeFiles/singleRX.dir/singleRX.cpp.o"

# External object files for target singleRX
singleRX_EXTERNAL_OBJECTS =

bin/singleRX: src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o
bin/singleRX: src/examples/CMakeFiles/singleRX.dir/build.make
bin/singleRX: src/libLimeSuite.so.17.12.0
bin/singleRX: /usr/lib/x86_64-linux-gnu/libsqlite3.so
bin/singleRX: /usr/lib/x86_64-linux-gnu/libusb-1.0.so
bin/singleRX: src/examples/CMakeFiles/singleRX.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/sg723/LimeSuite/udev-rules/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ../../bin/singleRX"
	cd /home/sg723/LimeSuite/udev-rules/src/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/singleRX.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/examples/CMakeFiles/singleRX.dir/build: bin/singleRX

.PHONY : src/examples/CMakeFiles/singleRX.dir/build

src/examples/CMakeFiles/singleRX.dir/requires: src/examples/CMakeFiles/singleRX.dir/singleRX.cpp.o.requires

.PHONY : src/examples/CMakeFiles/singleRX.dir/requires

src/examples/CMakeFiles/singleRX.dir/clean:
	cd /home/sg723/LimeSuite/udev-rules/src/examples && $(CMAKE_COMMAND) -P CMakeFiles/singleRX.dir/cmake_clean.cmake
.PHONY : src/examples/CMakeFiles/singleRX.dir/clean

src/examples/CMakeFiles/singleRX.dir/depend:
	cd /home/sg723/LimeSuite/udev-rules && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sg723/LimeSuite /home/sg723/LimeSuite/src/examples /home/sg723/LimeSuite/udev-rules /home/sg723/LimeSuite/udev-rules/src/examples /home/sg723/LimeSuite/udev-rules/src/examples/CMakeFiles/singleRX.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/examples/CMakeFiles/singleRX.dir/depend

