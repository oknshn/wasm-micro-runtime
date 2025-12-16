# Install script for directory: /home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/vboxuser/toolchains/riscv/bin/riscv64-unknown-elf-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/machine" TYPE FILE FILES
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/ansi.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/_arc4random.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/_default_types.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/_endian.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/endian.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/fastmath.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/fenv-softfloat.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/ieeefp.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/malloc.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/param.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/setjmp.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/stdlib.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/termios.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/_time.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/time.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/_types.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/machine/types.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/vboxuser/zephyrproject/wasm-micro-runtime/product-mini/platforms/zephyr/thesis-example-fw/qemu_rv64_nmsis_opt/modules/picolibc/newlib/libc/include/machine/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
