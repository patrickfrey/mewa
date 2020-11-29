#!/bin/sh

# Builds the sources (src/memory_resource.hpp) with the flag -DMEWA_TEST_LOCAL_MEMORY_RESOURCE
# The flag switches on the usage of an allocator that always fails as upstream memory resource for the monotonic buffer 
# resource defined in src/memory_resource.hpp. The idea is to catch bad use of the monotonic buffer resource for allocation
# of structures in local (stack) memory. Bad use could be for example a configuration that leads to unexpected cases
# to the use of the fallback allocator. When switched on the test program fails with a bad alloc in such a case.
#

make clean
make -j 8 TESTLOCALMEM=YES all
make test

