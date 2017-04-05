cmake_minimum_required(VERSION 3.3)
project(libdiscodb)

INCLUDE_DIRECTORIES(/usr/include/  src/ )
LINK_DIRECTORIES(/usr/lib64/)
set(SOURCE_FILES    src/ddb_delta.c
                    src/ddb_deltalist.c
                    src/ddb_membuffer.c
                    src/ddb_queue.c
                    src/ddb.c
                    src/ddb_cmph.c
                    src/ddb_cnf.c
                    src/ddb_cons.c
                    src/ddb_huffman.c
                    src/ddb_list.c
                    src/ddb_map.c
                    src/ddb_view.c)


add_library(libdiscodb SHARED ${SOURCE_FILES})

TARGET_LINK_LIBRARIES(libdiscodb cmph)


enable_testing()
function(ddb_add_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} libdiscodb )
endfunction()
ddb_add_test(createandquery src/tests/create_and_query.c)