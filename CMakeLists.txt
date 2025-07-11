cmake_minimum_required(VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("push2_resolume")

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}")

# Add executable
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")
add_executable(push2_resolume ${SOURCES} ${HEADERS})

# Add RtMidi as a static library
add_library(rtmidi_static STATIC 
    "include/RtMidi.cpp"
    "include/RtMidi.h"
)

# separate versions of NetworkingUtils.cpp and UdpSocket.cpp are provided for Win32 and POSIX
# the IpSystemTypePath selects the correct ones based on the current platform

IF(WIN32)
    set(IpSystemTypePath ip/win32)
    set(LIBS ${LIBS} Ws2_32 winmm)
    message(STATUS ">>> Windows")
    
    # Configure RtMidi for Windows
    target_compile_definitions(rtmidi_static PRIVATE __WINDOWS_MM__)
    
    # Link libraries
    target_link_libraries(push2_resolume ${CMAKE_CURRENT_SOURCE_DIR}/lib/libusb-1.0.lib)
    target_link_libraries(push2_resolume rtmidi_static winmm)
    
ELSE(WIN32)
    set(IpSystemTypePath ip/posix)
    message(STATUS ">>> Linux")
    
    # Configure RtMidi for Linux (ALSA)
    target_compile_definitions(rtmidi_static PRIVATE __LINUX_ALSA__)
    
    add_library( libusb SHARED IMPORTED )
    set_target_properties( libusb PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libusb-1.0.so )
    target_link_libraries(push2_resolume libusb)
    target_link_libraries(push2_resolume rtmidi_static asound pthread)
ENDIF(WIN32)

ADD_LIBRARY(oscpack 
    ip/IpEndpointName.h
    ip/IpEndpointName.cpp
    
    ip/NetworkingUtils.h
    ${IpSystemTypePath}/NetworkingUtils.cpp
    
    ip/UdpSocket.h
    ${IpSystemTypePath}/UdpSocket.cpp

    ip/PacketListener.h
    ip/TimerListener.h

    osc/OscTypes.h
    osc/OscTypes.cpp 
    osc/OscHostEndianness.h
    osc/OscException.h
    osc/OscPacketListener.h
    osc/MessageMappingOscPacketListener.h
    osc/OscReceivedElements.h
    osc/OscReceivedElements.cpp
    osc/OscPrintReceivedElements.h
    osc/OscPrintReceivedElements.cpp
    osc/OscOutboundPacketStream.h
    osc/OscOutboundPacketStream.cpp
)

TARGET_LINK_LIBRARIES(push2_resolume oscpack ${LIBS})

# Set C++ standard
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET push2_resolume PROPERTY CXX_STANDARD 20)
endif()

# Include directories
target_include_directories(push2_resolume PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/ip"
    "${CMAKE_SOURCE_DIR}/include/osc"
)

# Include directories for RtMidi
target_include_directories(rtmidi_static PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic")
endif()