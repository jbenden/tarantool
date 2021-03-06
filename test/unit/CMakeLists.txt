add_compile_flags("C;CXX" "-Wno-unused")
if(CC_HAS_WNO_TAUTOLOGICAL_COMPARE)
    add_compile_flags("C;CXX" "-Wno-tautological-compare")
endif()
file(GLOB all_sources *.c *.cc)
set_source_files_compile_flags(${all_sources})

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/third_party)
add_executable(heap.test heap.c unit.c)
add_executable(heap_iterator.test heap_iterator.c unit.c)
add_executable(rlist.test rlist.c unit.c)
add_executable(stailq.test stailq.c unit.c)
add_executable(uri.test uri.c unit.c)
target_link_libraries(uri.test uri)
add_executable(queue.test queue.c)
add_executable(mhash.test mhash.c)
add_executable(mhash_bytemap.test mhash_bytemap.c)
add_executable(rope_basic.test rope_basic.c ${CMAKE_SOURCE_DIR}/src/lib/salad/rope.c)
add_executable(rope_avl.test rope_avl.c ${CMAKE_SOURCE_DIR}/src/lib/salad/rope.c)
add_executable(rope_stress.test rope_stress.c ${CMAKE_SOURCE_DIR}/src/lib/salad/rope.c)
add_executable(rope.test rope.c ${CMAKE_SOURCE_DIR}/src/lib/salad/rope.c)
add_executable(bit.test bit.c bit.c)
add_executable(int96.test int96.cc)
target_link_libraries(bit.test bit)
add_executable(bitset_basic.test bitset_basic.c)
target_link_libraries(bitset_basic.test bitset)
add_executable(bitset_iterator.test bitset_iterator.c)
target_link_libraries(bitset_iterator.test bitset)
add_executable(bitset_index.test bitset_index.c)
target_link_libraries(bitset_index.test bitset)
add_executable(base64.test base64.c)
target_link_libraries(base64.test misc)

add_executable(uuid.test uuid.c unit.c
        ${CMAKE_SOURCE_DIR}/src/tt_uuid.c
        ${CMAKE_SOURCE_DIR}/src/random.c)
target_link_libraries(uuid.test bit core)

add_executable(bps_tree.test bps_tree.cc)
target_link_libraries(bps_tree.test small misc)
add_executable(bps_tree_iterator.test bps_tree_iterator.cc)
target_link_libraries(bps_tree_iterator.test small misc)
add_executable(rtree.test rtree.cc)
target_link_libraries(rtree.test salad small)
add_executable(rtree_iterator.test rtree_iterator.cc)
target_link_libraries(rtree_iterator.test salad small)
add_executable(rtree_multidim.test rtree_multidim.cc)
target_link_libraries(rtree_multidim.test salad small)
add_executable(light.test light.cc)
target_link_libraries(light.test small)
add_executable(bloom.test bloom.cc)
target_link_libraries(bloom.test salad)
add_executable(vclock.test vclock.cc unit.c
    ${CMAKE_SOURCE_DIR}/src/box/vclock.c
    ${CMAKE_SOURCE_DIR}/src/box/errcode.c
    ${CMAKE_SOURCE_DIR}/src/box/error.cc
    ${CMAKE_SOURCE_DIR}/src/rmean.c
    ${CMAKE_SOURCE_DIR}/src/histogram.c)
target_link_libraries(vclock.test core salad small)
add_executable(xrow.test xrow.cc unit.c
    ${CMAKE_SOURCE_DIR}/src/box/xrow.c
    ${CMAKE_SOURCE_DIR}/src/box/vclock.c
    ${CMAKE_SOURCE_DIR}/src/box/iproto_constants.c
    ${CMAKE_SOURCE_DIR}/src/box/errcode.c
    ${CMAKE_SOURCE_DIR}/src/box/error.cc)
target_link_libraries(xrow.test server misc ${MSGPUCK_LIBRARIES})

add_executable(fiber.test fiber.cc unit.c)
set_source_files_properties(fiber.cc PROPERTIES COMPILE_FLAGS -O0)
target_link_libraries(fiber.test core)

if (NOT ENABLE_GCOV)
    # This test is known to be broken with GCOV
    add_executable(guard.test guard.cc unit.c)
    target_link_libraries(guard.test core)
endif ()

add_executable(fiber_stress.test fiber_stress.cc)
target_link_libraries(fiber_stress.test core)

add_executable(ipc.test ipc.cc unit.c ${CMAKE_SOURCE_DIR}/src/ipc.c)
target_link_libraries(ipc.test core)

add_executable(ipc_stress.test ipc_stress.cc ${CMAKE_SOURCE_DIR}/src/ipc.c)
target_link_libraries(ipc_stress.test core)

add_executable(coio.test coio.cc unit.c
        ${CMAKE_SOURCE_DIR}/src/sio.cc
        ${CMAKE_SOURCE_DIR}/src/evio.cc
        ${CMAKE_SOURCE_DIR}/src/coio.cc
        ${CMAKE_SOURCE_DIR}/src/coio_task.c
        ${CMAKE_SOURCE_DIR}/src/fio.c
        ${CMAKE_SOURCE_DIR}/src/iobuf.cc)
target_link_libraries(coio.test core eio bit uri)

if (ENABLE_BUNDLED_MSGPUCK)
    set(MSGPUCK_DIR ${PROJECT_SOURCE_DIR}/src/lib/msgpuck/)
    add_executable(msgpack.test
        ${MSGPUCK_DIR}/test/msgpuck.c
        ${MSGPUCK_DIR}/test/test.c)

    set_source_files_properties(
        ${MSGPUCK_DIR}/test/msgpuck.c
        ${MSGPUCK_DIR}/test/test.c
        PROPERTIES COMPILE_FLAGS "-I${MSGPUCK_DIR}/test")

    target_link_libraries(msgpack.test ${MSGPUCK_LIBRARIES})
endif ()

add_executable(scramble.test scramble.c
    ${CMAKE_SOURCE_DIR}/src/scramble.c
    ${CMAKE_SOURCE_DIR}/src/random.c)
target_link_libraries(scramble.test misc core)

add_executable(guava.test guava.c)
target_link_libraries(guava.test salad small)

add_executable(find_path.test find_path.c
    ${CMAKE_SOURCE_DIR}/src/find_path.c
)

add_executable(reflection_c.test reflection_c.c unit.c
    ${CMAKE_SOURCE_DIR}/src/reflection.c)
add_executable(reflection_cxx.test reflection_cxx.cc unit.c
    ${CMAKE_SOURCE_DIR}/src/reflection.c)
add_executable(csv.test csv.c
    ${CMAKE_SOURCE_DIR}/src/lib/csv/csv.c
)

add_executable(rmean.test rmean.cc unit.c
        ${CMAKE_SOURCE_DIR}/src/rmean.c)
target_link_libraries(rmean.test core)

add_executable(histogram.test histogram.c unit.c
        ${CMAKE_SOURCE_DIR}/src/histogram.c)
target_link_libraries(histogram.test core)

add_executable(say.test say.c unit.c)
target_link_libraries(say.test core)

include_directories(${PROJECT_SOURCE_DIR}/src/box)
add_executable(vy_mem.test vy_mem.c unit.c)
target_link_libraries(vy_mem.test box)

add_executable(column_mask.test column_mask.c unit.c)
target_link_libraries(column_mask.test box)
