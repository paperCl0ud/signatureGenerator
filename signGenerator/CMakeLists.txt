project(signGen)

cmake_minimum_required(VERSION 2.6)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.42.0 REQUIRED system thread)

set(HEADERS SignatureGenerator.h
			../common/memory/MemoryPool.h)

set(SOURCES main.cpp 
            SignatureGenerator.cpp
			../common/memory/MemoryPool.cpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(WIN32)
  include_directories(../common/msinttypes)
  include_directories(${Boost_INCLUDE_DIR})
	
  link_directories(${Boost_LIBRARY_DIRS})
endif(WIN32)

add_executable(signGen ${SOURCES} ${HEADERS})

if(WIN32)
  add_definitions(-DNOMINMAX -D_WIN32_WINNT=0x0501)
  set_target_properties(signGen PROPERTIES 
                        COMPILE_FLAGS "-D_SCL_SECURE_NO_WARNINGS")
  target_link_libraries(signGen)
else(WIN32)
  if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")

    if (NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
      set_target_properties(signGen PROPERTIES LINK_FLAGS "-s")

    endif()
  endif()

  target_link_libraries(signGen ${Boost_LIBRARIES})

endif(WIN32)

