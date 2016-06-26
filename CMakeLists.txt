# Copyright (c) 2016 Fabian Schuiki
cmake_minimum_required(VERSION 3.0)
project(moore)

# Make a release build by default.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE release)
endif()
message("## moore ${CMAKE_BUILD_TYPE} build")

# Gather packages.
find_package(Threads REQUIRED)

# Versioning.
set(MOORE_VERSION_MAJOR 0)
set(MOORE_VERSION_MINOR 1)
set(MOORE_VERSION_PATCH 0)
set(MOORE_VERSION_SUFFIX "")
set(MOORE_VERSION "${MOORE_VERSION_MAJOR}.${MOORE_VERSION_MINOR}.${MOORE_VERSION_PATCH}${MOORE_VERSION_SUFFIX}")
message("## moore version ${MOORE_VERSION}")
configure_file(src/config.h.in src/config.h)

# Setup compiler flags.
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_BINARY_DIR}/src)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter")

# Setup the executable.
add_executable(moore
	src/lexer.c
	src/lexer-sv.c
	src/main.c
	src/source.c
	src/util.c
)
target_link_libraries(moore ${CMAKE_THREAD_LIBS_INIT})

# Add a documentation target if Doxygen is available.
find_package(Doxygen)
if (DOXYGEN_FOUND)
	configure_file(Doxyfile.in doc/Doxyfile)
	add_custom_target(doc
		${DOXYGEN_EXECUTABLE} Doxyfile 1> doxygen.log 2> doxygen.err
		WORKING_DIRECTORY doc
		COMMENT "Generating source code documentation" VERBATIM
	)
endif()
