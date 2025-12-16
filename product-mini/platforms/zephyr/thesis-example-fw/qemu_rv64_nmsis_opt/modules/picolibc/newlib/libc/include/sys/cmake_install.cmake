# Install script for directory: /home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/sys" TYPE FILE FILES
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/auxv.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/cdefs.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/config.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/custom_file.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_default_fcntl.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/dirent.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/dir.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/errno.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/fcntl.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/features.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/file.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/iconvnls.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_initfini.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_intsup.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_locale.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/lock.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/param.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/queue.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/resource.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/sched.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/select.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_select.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_sigset.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/stat.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_stdint.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/string.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/syslimits.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/timeb.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/time.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/times.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_timespec.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/timespec.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_timeval.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/tree.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_types.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/types.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_tz_structs.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/unistd.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/utime.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/wait.h"
    "/home/vboxuser/zephyrproject/modules/lib/picolibc/newlib/libc/include/sys/_wait.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/vboxuser/zephyrproject/wasm-micro-runtime/product-mini/platforms/zephyr/thesis-example-fw/qemu_rv64_nmsis_opt/modules/picolibc/newlib/libc/include/sys/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
