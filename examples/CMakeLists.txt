#
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


include_directories(.)

set(DL ${CMAKE_DL_LIBS})


find_package(ct_core)
find_package(ct_optcon)
find_package(Ceres)
find_package(PkgConfig)

#find_package(tinyxml2)
if(tinyxml2_FOUND)
  message("USING system tinyxml2")
  include_directories(${tinyxml2_INCLUDE_DIRS})
else(tinyxml2_FOUND)
  message("USING third_party/tinyxml2")

  set(tinyxml2_FOUND TRUE)
  set(tinyxml2_INCLUDE_DIRS ../third_party/tinyxml2/include)
  subdirs(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tinyxml2)
endif(tinyxml2_FOUND)


find_package(crossguid)
if(crossguid_FOUND)
else()
  subdirs(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/crossguid)
  set(crossguid_INCLUDE_DIRS ../third_party/crossguid)
  include_directories(${crossguid_INCLUDE_DIRS})
endif()

#find_package(zeromq)
#find_package(cppzmq)

if(cppzmq_FOUND)
  message("USING system zmq")
else()
  message("USING third_party/zmq")
  set(cppzmq_FOUND TRUE)
  subdirs(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/zeromq)
  set(cppzmq_INCLUDE_DIRS ../third_party/zeromq/include)
  include_directories(${cppzmq_INCLUDE_DIRS})
  add_definitions(-DZMQ_STATIC)
endif()

find_package(nlohmann_json)
if(nlohmann_json_FOUND)
else()
  set(nlohmann_json_FOUND TRUE)
  set(nlohmann_json_INCLUDE_DIRS ../third_party/nlohmann/include)
  include_directories(${nlohmann_json_INCLUDE_DIRS})
endif()

#find_library(zmq_location NAMES libzmq.a)
#message(STATUS ${zmq_location})
#add_library(zmq STATIC IMPORTED)
#set_target_properties(zmq PROPERTIES IMPORTED_LOCATION ${zmq_location})

# show all cmake variables, for debugging
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()


set(MESHCAT_LIBRARIES crossguid cpp_base64 zmq)
if(WIN32)
  set(MESHCAT_LIBRARIES ${MESHCAT_LIBRARIES} ws2_32 Winmm iphlpapi)
endif()
set(HAS_MESHCAT TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if(Bullet_FOUND)
  add_executable(world_example_gui world_example_gui.cpp ${TDS_HDRS})
  target_link_libraries(world_example_gui pybullet_visualizer_api ${DL})

  if(Ceres_FOUND)
    find_package(PythonLibs REQUIRED)
    include_directories(
      ${PYTHON_INCLUDE_DIRS}
      ${PYTHON_NUMPY_INCLUDE_DIR}
    )

    add_executable(billiard_opt_gui billiard_opt_gui.cpp ${TDS_HDRS})
    target_link_libraries(billiard_opt_gui pybullet_visualizer_api ${DL} ${CERES_LIBRARIES})

    #add_executable(cartpole_opt cartpole_opt.cpp ${TDS_HDRS})
    #target_link_libraries(cartpole_opt pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})

    add_executable(ceres_estimation ceres_estimation.cpp ${TDS_HDRS})
    target_link_libraries(ceres_estimation pybullet_visualizer_api ${DL}
           ${CERES_LIBRARIES})

    set(DUSE_MATPLOTLIB true)
    #add_executable(ceres_estimation_real ceres_estimation_real.cpp ${TDS_HDRS})
    #target_link_libraries(ceres_estimation_real pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES} python2.7)

    #add_executable(neural_pendulum neural_pendulum.cpp ${TDS_HDRS})
    #target_link_libraries(neural_pendulum pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})

    #add_definitions(-DNEURAL_SIM)
    #add_executable(neural_discovery neural_discovery.cpp ${TDS_HDRS})
    #target_link_libraries(neural_discovery pybullet_visualizer_api ${DL}
    #       ${CERES_LIBRARIES})

    #add_executable(neural_actuator neural_actuator.cpp ${TDS_HDRS})
    #target_link_libraries(neural_actuator pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})

    #add_executable(neural_wtf neural_wtf.cpp ${TDS_HDRS})
    #target_link_libraries(neural_wtf pybullet_visualizer_api python2.7 ${DL}
    #        ${CERES_LIBRARIES})

    #add_executable(neural_joint neural_joint.cpp ${TDS_HDRS})
    #target_link_libraries(neural_joint pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})

    #add_executable(neural_heightfield neural_heightfield.cpp ${TDS_HDRS})
    #target_link_libraries(neural_heightfield pybullet_visualizer_api ${DL}
    #    ${PYTHON_LIBRARIES} python2.7 ${CERES_LIBRARIES})

    #add_executable(policy_optimization policy_optimization.cpp ${TDS_HDRS})
    #target_link_libraries(policy_optimization pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})

    #add_executable(contact_discovery contact_discovery.cpp ${TDS_HDRS})
    #target_link_libraries(contact_discovery pybullet_visualizer_api ${DL}
    #        ${CERES_LIBRARIES})
  endif(Ceres_FOUND)

  if(ct_core_FOUND AND ct_optcon_FOUND AND Eigen3_FOUND)
    # Activate CppAD (with CodeGen) and HPIPM for Control Toolbox
    add_definitions(-DCPPAD -DCPPADCG -DHPIPM)

    include_directories(${ct_core_INCLUDE_DIRS})
    include_directories(${ct_optcon_INCLUDE_DIRS})
    include_directories(${EIGEN3_INCLUDE_DIRS})

    #add_executable(ct_cartpole ct_cartpole.cpp ${TDS_HDRS})
    #target_link_libraries(ct_cartpole pybullet_visualizer_api ${DL}
    #        -lpthread hpipm blasfeo)

    #add_executable(ct_contact_trajopt ct_contact_trajopt.cpp ${TDS_HDRS})
    #target_link_libraries(ct_contact_trajopt pybullet_visualizer_api ${DL}
    #        -lpthread hpipm blasfeo)

    #add_executable(ct_laikago_tracking ct_laikago_tracking.cpp ${TDS_HDRS})
    #target_link_libraries(ct_laikago_tracking pybullet_visualizer_api ${DL}
    #        -lpthread hpipm blasfeo)
  endif(ct_core_FOUND AND ct_optcon_FOUND AND Eigen3_FOUND)

  # add_executable(laikago_example_gui laikago_example_gui.cpp ${TDS_HDRS})
  # target_link_libraries(laikago_example_gui pybullet_visualizer_api)

  if(Eigen3_FOUND)
    #add_executable(laikago_footstep_ik laikago_footstep_ik.cpp ${TDS_HDRS})
    #target_link_libraries(laikago_footstep_ik pybullet_visualizer_api)

    ##add_executable(pendulum_ik_gui pendulum_ik_gui.cpp)
    ##target_link_libraries(pendulum_ik_gui pybullet_visualizer_api)
  endif()


  add_executable(generic_urdf_collision generic_urdf_collision.cpp ${TDS_HDRS})
  target_link_libraries(generic_urdf_collision pybullet_visualizer_api tinyxml2)
  IF(WIN32)
   target_link_libraries(generic_urdf_collision Opengl32)
  ENDIF(WIN32)
  

  #add_executable(gravity_compensation gravity_compensation.cpp ${TDS_HDRS})
  #target_link_libraries(gravity_compensation pybullet_visualizer_api)
  #target_include_directories(gravity_compensation PUBLIC ${BULLET_INCLUDE_DIR})
  #target_compile_definitions(gravity_compensation PUBLIC ${BULLET_DEFINITIONS})
  #target_link_directories(gravity_compensation PUBLIC ${Bullet_DIR}/${BULLET_ROOT_DIR}/${BULLET_LIBRARY_DIRS})
  #target_link_libraries(gravity_compensation PUBLIC BulletDynamics BulletCollision LinearMath)


  if(HAS_MESHCAT)
    #add_executable(laikago_meshcat_example laikago_meshcat_example.cpp ${TDS_HDRS})
    #target_link_libraries(laikago_meshcat_example pybullet_visualizer_api ${MESHCAT_LIBRARIES})
  endif(HAS_MESHCAT)

endif(Bullet_FOUND)




if(HAS_MESHCAT AND tinyxml2_FOUND)

  #add_executable(tiny_urdf_parser_meshcat_example tiny_urdf_parser_meshcat_example.cpp ${TDS_HDRS})
  #target_link_libraries(tiny_urdf_parser_meshcat_example  tinyxml2 ${MESHCAT_LIBRARIES})

  # add_executable(meshcat_zmq_example meshcat_zmq_example.cpp ${TDS_HDRS})
  # target_link_libraries(meshcat_zmq_example ${MESHCAT_LIBRARIES})

endif(HAS_MESHCAT AND tinyxml2_FOUND)

if(tinyxml2_FOUND)
  # add_executable(tiny_urdf_parser_example tiny_urdf_parser_example.cpp ${TDS_HDRS})
  # target_link_libraries(tiny_urdf_parser_example tinyxml2)

  if(Ceres_FOUND)
    #add_executable(neural_contact neural_contact.cpp ${TDS_HDRS})
    #target_link_libraries(neural_contact opengl_window ${DL}
    #        ${CERES_LIBRARIES} ${MESHCAT_LIBRARIES} python2.7 tinyxml2)
  endif(Ceres_FOUND)
endif(tinyxml2_FOUND)

# add_executable(world_example world_example.cpp ${TDS_HDRS})
# add_executable(test_xarm test_xarm.cpp ${TDS_HDRS})

add_executable(pendulum_example_gui pendulum_example_gui.cpp ${TDS_HDRS})
target_link_libraries(pendulum_example_gui opengl_window ${DL})

add_executable(pendulum_example_eigen pendulum_example_eigen.cpp ${TDS_HDRS})
target_link_libraries(pendulum_example_eigen opengl_window ${DL})
if(USE_STAN)
  target_link_libraries(pendulum_example_eigen ${STAN_LIBRARIES})
endif(USE_STAN)

add_executable(opengl_test opengl_test.cpp ${TDS_HDRS})
target_link_libraries(opengl_test opengl_window ${DL})

#add_executable(tiny_urdf_parser_opengl_example tiny_urdf_parser_opengl_example.cpp ../third_party/stb_image/stb_image.cpp ../third_party/tinyobjloader/tiny_obj_loader.cc ${TDS_HDRS})
#target_link_libraries(tiny_urdf_parser_opengl_example  tinyxml2 opengl_window)

subdirs(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/dear_imgui)

add_executable(opengl_imgui_test opengl_imgui_test.cpp ../third_party/dear_imgui/opengl3/imgui_impl_opengl3.cpp  ${TDS_HDRS})
target_link_libraries(opengl_imgui_test opengl_window dear_imgui ${DL})
target_include_directories(opengl_imgui_test PRIVATE ../third_party/glad ../third_party/dear_imgui)


if(WIN32)
  target_link_libraries(pendulum_example_gui Opengl32)
  target_link_libraries(pendulum_example_eigen Opengl32)
  
  #target_link_libraries(tiny_urdf_parser_opengl_example Opengl32)
  target_link_libraries(opengl_test Opengl32)
  target_link_libraries(opengl_imgui_test Opengl32)
endif()







if(USE_ENOKI)
  if(MSVC)
    set(ENOKI_NONE_FLAGS /DENOKI_DISABLE_VECTORIZATION)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(ENOKI_SSE42_FLAGS /D__SSE4_2__)
    else()
      set(ENOKI_SSE42_FLAGS /arch:SSE2 /D__SSE4_2__)
    endif()
    set(ENOKI_AVX_FLAGS /arch:AVX)
    set(ENOKI_AVX2_FLAGS /arch:AVX2)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    add_compile_options(-wd11074 -wd11076)
    set(ENOKI_NONE_FLAGS -DENOKI_DISABLE_VECTORIZATION -ffp-contract=off)
    set(ENOKI_SSE42_FLAGS -xSSE4.2)
    set(ENOKI_AVX_FLAGS -xCORE-AVX-I)
    set(ENOKI_AVX2_FLAGS -xCORE-AVX2)
    set(ENOKI_AVX512_KNL_FLAGS -xMIC-AVX512)
    set(ENOKI_AVX512_SKX_FLAGS -xCORE-AVX512)
  else()
    set(ENOKI_NONE_FLAGS -DENOKI_DISABLE_VECTORIZATION -ffp-contract=off)
    set(ENOKI_SSE42_FLAGS -msse4.2)
    set(ENOKI_AVX_FLAGS -mavx)
    set(ENOKI_AVX2_FLAGS -mavx2 -mfma -mf16c -mbmi -mbmi2 -mlzcnt)
    if(APPLE AND ${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
      set(ENOKI_AVX512_KNL_FLAGS -march=knl -Wa,-march=knl)
      set(ENOKI_AVX512_SKX_FLAGS -march=skylake-avx512 -Wa,-march=skx)
    else()
      set(ENOKI_AVX512_KNL_FLAGS -march=knl)
      set(ENOKI_AVX512_SKX_FLAGS -march=skylake-avx512)
    endif()
    set(ENOKI_NEON_FLAGS)
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES armv7)
      set(ENOKI_NEON_FLAGS -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -mfp16-format=ieee)
    elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES aarch64)
      set(ENOKI_NEON_FLAGS -march=armv8-a+simd -mtune=cortex-a53)
    endif()

  endif()

  enoki_set_compile_flags()
endif(USE_ENOKI)

# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
#     CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
#     CMAKE_CXX_COMPILER_ID MATCHES "Intel")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
#   if (NOT ${ENOKI_U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
#     add_compile_options( -fvisibility=hidden)
#   endif()
#   add_compile_options(-Wall -Wextra -Wconversion)
# elseif(WIN32)
#   add_compile_options("/std:c++17")
#   add_compile_options("/bigobj")
# endif()

# set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# add_custom_target(check
#         ${CMAKE_COMMAND} -E echo CWD=${CMAKE_BINARY_DIR}
#         COMMAND ${CMAKE_COMMAND} -E echo CMD=${CMAKE_CTEST_COMMAND} -C $<CONFIG>
#         COMMAND ${CMAKE_COMMAND} -E echo ----------------------------------
#         COMMAND ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1
#             ${CMAKE_CTEST_COMMAND} -C $<CONFIG>
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     DEPENDS ALL_BUILD USES_TERMINAL
# )

function(enoki_test NAME SOURCE)
  add_executable(${NAME}_none ${SOURCE} ${ENOKI_HEADERS})
  add_test(${NAME}_none_test ${NAME}_none)
  set_tests_properties(${NAME}_none_test PROPERTIES LABELS "none")
  set_target_properties(${NAME}_none PROPERTIES FOLDER ${NAME})
  target_compile_options(${NAME}_none PRIVATE ${ENOKI_NONE_FLAGS})

  if(ENOKI_HOST MATCHES "INTEL")
    add_executable(${NAME}_sse42 ${SOURCE} ${ENOKI_HEADERS})
    target_compile_options(${NAME}_sse42 PRIVATE ${ENOKI_SSE42_FLAGS})
    set_target_properties(${NAME}_sse42 PROPERTIES FOLDER ${NAME})
    # if (ENOKI_TEST_SSE42)
    add_test(${NAME}_sse42_test ${NAME}_sse42)
    set_tests_properties(${NAME}_sse42_test PROPERTIES LABELS "sse42")
    # endif()

    add_executable(${NAME}_avx ${SOURCE} ${ENOKI_HEADERS})
    target_compile_options(${NAME}_avx PRIVATE ${ENOKI_AVX_FLAGS})
    set_target_properties(${NAME}_avx PROPERTIES FOLDER ${NAME})
    # if (ENOKI_TEST_AVX)
    add_test(${NAME}_avx_test ${NAME}_avx)
    set_tests_properties(${NAME}_avx_test PROPERTIES LABELS "avx")
    # endif()

    add_executable(${NAME}_avx2 ${SOURCE} ${ENOKI_HEADERS})
    target_compile_options(${NAME}_avx2 PRIVATE ${ENOKI_AVX2_FLAGS})
    set_target_properties(${NAME}_avx2 PROPERTIES FOLDER ${NAME})
    # if (ENOKI_TEST_AVX2)
    add_test(${NAME}_avx2_test ${NAME}_avx2)
    set_tests_properties(${NAME}_avx2_test PROPERTIES LABELS "avx2")
    # endif()

    if(NOT MSVC)
      add_executable(${NAME}_avx512_knl ${SOURCE} ${ENOKI_HEADERS})
      target_compile_options(${NAME}_avx512_knl PRIVATE ${ENOKI_AVX512_KNL_FLAGS})
      set_target_properties(${NAME}_avx512_knl PROPERTIES FOLDER ${NAME})
      if(ENOKI_TEST_KNL)
        add_test(${NAME}_avx512_knl_test ${NAME}_avx512_knl)
        set_tests_properties(${NAME}_avx512_knl_test PROPERTIES LABELS "avx512_knl")
      endif()

      add_executable(${NAME}_avx512_skx ${SOURCE} ${ENOKI_HEADERS})
      target_compile_options(${NAME}_avx512_skx PRIVATE ${ENOKI_AVX512_SKX_FLAGS})
      set_target_properties(${NAME}_avx512_skx PROPERTIES FOLDER ${NAME})

      if(ENOKI_TEST_SKX)
        add_test(${NAME}_avx512_skx_test ${NAME}_avx512_skx)
        set_tests_properties(${NAME}_avx512_skx_test PROPERTIES LABELS "avx512_skx")
      endif()
    endif()
  endif()

  if(ENOKI_HOST MATCHES "ARM")
    add_executable(${NAME}_neon ${SOURCE} ${ENOKI_HEADERS})
    target_compile_options(${NAME}_neon PRIVATE ${ENOKI_NEON_FLAGS})
    set_target_properties(${NAME}_neon PROPERTIES FOLDER ${NAME})
    if(ENOKI_TEST_ARM)
      add_test(${NAME}_neon_test ${NAME}_neon)
      set_tests_properties(${NAME}_neon_test PROPERTIES LABELS "neon")
    endif()
  endif()
endfunction()

if (Bullet_FOUND)
    enoki_test(enoki_hello_world enoki_hello_world.cpp)
    set_target_properties(enoki_hello_world_none PROPERTIES FOLDER autodiff)
    target_link_libraries(enoki_hello_world_none PRIVATE opengl_window tinyxml2 ${PYTHON_LIBRARIES})
    if(ENOKI_CUDA)
      target_link_libraries(enoki_hello_world_none)
    endif()
    if(USE_RBDL)
      target_link_libraries(enoki_hello_world_none PRIVATE rbdl-static)
    endif()
    if(WIN32)
      target_link_libraries(enoki_hello_world_none PRIVATE Opengl32)
    endif()

    target_include_directories(enoki_hello_world_none PUBLIC ${BULLET_INCLUDE_DIR})
    target_compile_definitions(enoki_hello_world_none PUBLIC ${BULLET_DEFINITIONS})
    target_link_directories(enoki_hello_world_none PUBLIC ${Bullet_DIR}/${BULLET_ROOT_DIR}/${BULLET_LIBRARY_DIRS})
    target_link_libraries(enoki_hello_world_none PUBLIC BulletDynamics BulletCollision LinearMath)
ELSE()
message("BULLET NOT FOUND????")
ENDIF()
