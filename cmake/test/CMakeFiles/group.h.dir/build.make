# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /softwares/cmake/cmake-3.18.3/bin/cmake

# The command to remove a file.
RM = /softwares/cmake/cmake-3.18.3/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/samuelcm/lammps-dcci/cmake

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/samuelcm/lammps-dcci/cmake/test

# Utility rule file for group.h.

# Include the progress variables for this target.
include CMakeFiles/group.h.dir/progress.make

CMakeFiles/group.h: includes/lammps/group.h


includes/lammps/group.h: /home/samuelcm/lammps-dcci/src/group.h
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/samuelcm/lammps-dcci/cmake/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating includes/lammps/group.h"
	/softwares/cmake/cmake-3.18.3/bin/cmake -E copy_if_different /home/samuelcm/lammps-dcci/src/group.h /home/samuelcm/lammps-dcci/cmake/test/includes/lammps/group.h

group.h: CMakeFiles/group.h
group.h: includes/lammps/group.h
group.h: CMakeFiles/group.h.dir/build.make

.PHONY : group.h

# Rule to build all files generated by this target.
CMakeFiles/group.h.dir/build: group.h

.PHONY : CMakeFiles/group.h.dir/build

CMakeFiles/group.h.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/group.h.dir/cmake_clean.cmake
.PHONY : CMakeFiles/group.h.dir/clean

CMakeFiles/group.h.dir/depend:
	cd /home/samuelcm/lammps-dcci/cmake/test && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/samuelcm/lammps-dcci/cmake /home/samuelcm/lammps-dcci/cmake /home/samuelcm/lammps-dcci/cmake/test /home/samuelcm/lammps-dcci/cmake/test /home/samuelcm/lammps-dcci/cmake/test/CMakeFiles/group.h.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/group.h.dir/depend

