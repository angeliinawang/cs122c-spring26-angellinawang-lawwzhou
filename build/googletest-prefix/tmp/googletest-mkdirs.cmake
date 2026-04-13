# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest-build"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/tmp"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest-stamp"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src"
  "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/angeliw9/cs122c/cs122c-spring26-angellinawang-lawwzhou/build/googletest-prefix/src/googletest-stamp${cfgdir}") # cfgdir has leading slash
endif()
