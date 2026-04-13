# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog-build"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/tmp"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog-stamp"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googlelog-prefix/src/googlelog-stamp${cfgdir}") # cfgdir has leading slash
endif()
