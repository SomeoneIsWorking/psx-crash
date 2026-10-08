cmake_minimum_required(VERSION 3.24)
project(crash_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR: the framework, a symlink to the workspace's live checkout or a clone of its main
# (tools/psxport_fetch.py --auto); defaults to external/psxport.
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "`python3 tools/psxport_fetch.py --auto`, or point PSXPORT_DIR at a psxport checkout.")
endif()
option(PSXPORT_BUILD_SMOKE "Build the framework agnosticism smoke" ON)
add_subdirectory("${PSXPORT_DIR}" psxport_build)
if(PSXPORT_BUILD_SMOKE)
  add_custom_target(crash_scaffold DEPENDS psxport_smoke)
endif()

# First-party directories by concept; a target includes only the concepts it links.
set(CRASH_GAME_DIRS
    boot
    frame
    render/widescreen
)
set(CRASH_TITLE_DIRS
    boot
    disc
    entry
    frame
    input
    render
    render/widescreen
)

function(crash_game_include_dirs target)
  foreach(dir IN LISTS CRASH_GAME_DIRS)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/game/${dir}")
  endforeach()
endfunction()

function(crash_title_include_dirs target title)
  foreach(dir IN LISTS CRASH_TITLE_DIRS)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/${dir}")
  endforeach()
endfunction()

function(configure_title_vsync target title)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/executable.json" title_vsync_manifest)
  string(JSON title_vsync_entry GET "${title_vsync_manifest}" runtime vsync entry)
  string(JSON title_vsync_end GET "${title_vsync_manifest}" runtime vsync end)
  string(JSON title_vsync_query_counter GET "${title_vsync_manifest}" runtime vsync query_counter)
  target_compile_definitions(
    ${target}
    PRIVATE
      "CRASH_TITLE_VSYNC_ENTRY=${title_vsync_entry}u"
      "CRASH_TITLE_VSYNC_END=${title_vsync_end}u"
      "CRASH_TITLE_VSYNC_QUERY_COUNTER=${title_vsync_query_counter}u"
  )
endfunction()

function(configure_crash1_native_contract target)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json" crash1_contract_manifest)
  foreach(fact
          "CRASH1_STATIC_CONSTRUCTORS_ENTRY;runtime;native_boot;static_constructors;entry"
          "CRASH1_USE_CD_ADDRESS;runtime;native_boot;use_cd_address"
          "CRASH1_INIT_ENTRY;runtime;native_boot;init;entry"
          "CRASH1_CD_INITIALIZE_ENTRY;runtime;native_boot;cd_initialize;entry"
          "CRASH1_CD_INITIALIZE_END;runtime;native_boot;cd_initialize;end"
          "CRASH1_DISC_INDEX_ENTRY;runtime;native_boot;disc_index;entry"
          "CRASH1_DISC_INDEX_END;runtime;native_boot;disc_index;end"
          "CRASH1_CD_CONTROL_ENTRY;runtime;native_boot;cd_control;entry"
          "CRASH1_CD_CONTROL_END;runtime;native_boot;cd_control;end"
          "CRASH1_CD_CONTROL_F_ENTRY;runtime;native_boot;cd_control_f;entry"
          "CRASH1_CD_CONTROL_F_END;runtime;native_boot;cd_control_f;end"
          "CRASH1_CD_SYNC_WRAPPER_ENTRY;runtime;native_boot;cd_sync_wrapper;entry"
          "CRASH1_CD_SYNC_WRAPPER_END;runtime;native_boot;cd_sync_wrapper;end"
          "CRASH1_CD_SYNC_ENTRY;runtime;native_boot;cd_sync;entry"
          "CRASH1_CD_SYNC_END;runtime;native_boot;cd_sync;end"
          "CRASH1_CD_READ_ENTRY;runtime;native_boot;cd_read;entry"
          "CRASH1_CD_READ_END;runtime;native_boot;cd_read;end"
          "CRASH1_CD_READ_SYNC_ENTRY;runtime;native_boot;cd_read_sync;entry"
          "CRASH1_CD_READ_SYNC_END;runtime;native_boot;cd_read_sync;end"
          "CRASH1_CALLBACK_INITIALIZE_ENTRY;runtime;native_boot;callback_initialize;entry"
          "CRASH1_CALLBACK_INITIALIZE_END;runtime;native_boot;callback_initialize;end"
          "CRASH1_GPU_WATCHDOG_START_ENTRY;runtime;native_boot;gpu_watchdog_start;entry"
          "CRASH1_GPU_WATCHDOG_START_END;runtime;native_boot;gpu_watchdog_start;end"
          "CRASH1_GPU_WATCHDOG_CHECK_ENTRY;runtime;native_boot;gpu_watchdog_check;entry"
          "CRASH1_GPU_WATCHDOG_CHECK_END;runtime;native_boot;gpu_watchdog_check;end"
          "CRASH1_CORE_LOOP_ENTRY;runtime;native_frame;core_loop;entry"
          "CRASH1_CORE_LOOP_END;runtime;native_frame;core_loop;end"
          "CRASH1_FRAME_ITERATION_ENTRY;runtime;native_frame;iteration;entry"
          "CRASH1_FRAME_ITERATION_END;runtime;native_frame;iteration;end"
          "CRASH1_FRAME_TRANSITION_ENTRY;runtime;native_frame;transition;entry"
          "CRASH1_FRAME_TRANSITION_END;runtime;native_frame;transition;end"
          "CRASH1_BIOS_PAD_READ_WORD_ADDRESS;runtime;native_frame;bios_pad_read;address"
          "CRASH1_GPU_UPDATE_ENTRY;runtime;native_frame;gpu_update;entry"
          "CRASH1_GPU_UPDATE_END;runtime;native_frame;gpu_update;end"
          "CRASH1_FIRST_VSYNC;runtime;native_frame;first_vsync"
          "CRASH1_AFTER_FIRST_VSYNC;runtime;native_frame;after_first_vsync"
          "CRASH1_SECOND_VSYNC;runtime;native_frame;second_vsync"
          "CRASH1_AFTER_VSYNC;runtime;native_frame;after_vsync"
          "CRASH1_DONE_ADDRESS;runtime;native_frame;done;address"
          "CRASH1_SCENE_ID_ADDRESS;runtime;native_frame;scene;id_address"
          "CRASH1_SCENE_REQUEST_ADDRESS;runtime;native_frame;scene;request_address"
          "CRASH1_TICKS_ELAPSED_ADDRESS;runtime;native_frame;ticks_elapsed_address"
          "CRASH1_DISPLAY_CONTEXT_ADDRESS;runtime;native_frame;display_context_address"
          "CRASH1_ROOT_COUNTER_INCREMENT;runtime;native_frame;root_counter_increment"
          "CRASH1_SET_ROOT_COUNTER;runtime;native_frame;set_root_counter"
          "CRASH1_START_ROOT_COUNTER;runtime;native_frame;start_root_counter"
          "CRASH1_STOP_ROOT_COUNTER;runtime;native_frame;stop_root_counter"
          "CRASH1_HORIZONTAL_BOUND;runtime;projection;horizontal_bound;global"
          "CRASH1_HORIZONTAL_BOUND_WRITER;runtime;projection;horizontal_bound;writer;address"
          "CRASH1_HORIZONTAL_BOUND_RESEND;runtime;projection;horizontal_bound;resend_call_site;address"
          "CRASH1_HORIZONTAL_SUBMITTER;runtime;projection;horizontal_bound;submitter_consumer;entry"
          "CRASH1_HORIZONTAL_SUBMITTER_END;runtime;projection;horizontal_bound;submitter_consumer;end"
          "CRASH1_HORIZONTAL_NEAR_PLANE;runtime;projection;horizontal_bound;near_plane_consumer;entry"
          "CRASH1_HORIZONTAL_NEAR_PLANE_END;runtime;projection;horizontal_bound;near_plane_consumer;end"
          "CRASH1_HORIZONTAL_FAR_LIMIT;runtime;projection;horizontal_bound;near_plane_consumer;far_limit")
    list(POP_FRONT fact definition)
    string(JSON value GET "${crash1_contract_manifest}" ${fact})
    target_compile_definitions(${target} PRIVATE "${definition}=${value}u")
  endforeach()
endfunction()

add_library(
  crash_boundary_runtime STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/boundary_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/render/widescreen/guest_projection_publication.cpp"
)
crash_game_include_dirs(crash_boundary_runtime)
target_compile_features(crash_boundary_runtime PUBLIC cxx_std_20)
target_link_libraries(crash_boundary_runtime PUBLIC psxport)

add_library(
  crash_native_frame_contract STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/native_frame_loop_contract.cpp"
)
crash_game_include_dirs(crash_native_frame_contract)
target_compile_features(crash_native_frame_contract PUBLIC cxx_std_20)
target_link_libraries(crash_native_frame_contract PUBLIC psxport)

add_library(
  crash1_runtime STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/boot/crash1_callback_boot.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/boot/crash1_cd_boot.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/boot/crash1_gpu_watchdog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/disc/crash1_disc_index_io.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/entry/crash1_catalog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/entry/crash1_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/frame/crash1_frame_cut.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/frame/crash1_frame_driver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/input/crash1_bios_pad_input.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/crash1_block_pool.cpp"
  
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/widescreen/crash1_horizontal_bound.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/widescreen/crash1_widescreen.cpp"
)
crash_title_include_dirs(crash1_runtime crash1)
target_compile_features(crash1_runtime PUBLIC cxx_std_20)
target_link_libraries(crash1_runtime PUBLIC crash_boundary_runtime crash_native_frame_contract)
configure_title_vsync(crash1_runtime crash1)
configure_crash1_native_contract(crash1_runtime)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json" crash1_manifest_json)
foreach(fact
        "CRASH1_EXE_ENTRY;header;entry"
        "CRASH1_EXE_GP;header;gp"
        "CRASH1_EXE_TEXT_ADDRESS;header;text_address"
        "CRASH1_EXE_TEXT_SIZE;header;text_size"
        "CRASH1_EXE_STACK_ADDRESS;header;stack_address"
        "CRASH1_EXE_STACK_OFFSET;header;stack_offset")
  list(POP_FRONT fact definition)
  string(JSON value GET "${crash1_manifest_json}" ${fact})
  target_compile_definitions(crash1_runtime PRIVATE "${definition}=${value}u")
endforeach()
string(JSON crash1_exe_expected_size GET "${crash1_manifest_json}" file_size)
string(JSON crash1_exe_sha256 GET "${crash1_manifest_json}" sha256)
string(JSON crash1_exe_name GET "${crash1_manifest_json}" executable)
string(JSON crash1_exe_title GET "${crash1_manifest_json}" title)
target_compile_definitions(crash1_runtime PRIVATE
  "CRASH1_EXE_EXPECTED_SIZE=${crash1_exe_expected_size}u"
  "CRASH1_EXE_SHA256=\"${crash1_exe_sha256}\""
  "CRASH1_EXE_NAME=\"${crash1_exe_name}\""
  "CRASH1_EXE_TITLE=\"${crash1_exe_title}\"")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/bios_contract.json" crash1_bios_contract_json)
string(JSON crash1_get_c0_table_function
       GET "${crash1_bios_contract_json}" get_c0_table function)
string(JSON crash1_c0_exception_handler_slot
       GET "${crash1_bios_contract_json}" exception_handler table_slot)
string(JSON crash1_c0_exception_handler_address
       GET "${crash1_bios_contract_json}" exception_handler retail_address)

if(BUILD_TESTING)
  add_executable(crash1_runtime_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_runtime.cpp")
  target_include_directories(crash1_runtime_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_compile_features(crash1_runtime_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_runtime_test PRIVATE crash1_runtime)
  add_test(NAME crash1_runtime_inheritance COMMAND crash1_runtime_test)

  add_executable(crash1_frame_turn_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_frame_turn.cpp")
  target_compile_features(crash1_frame_turn_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_frame_turn_test PRIVATE crash1_runtime)
  add_test(NAME crash1_frame_turn COMMAND crash1_frame_turn_test)

  add_executable(crash1_frame_cut_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_frame_cut.cpp")
  target_compile_features(crash1_frame_cut_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_frame_cut_test PRIVATE crash1_runtime)
  add_test(NAME crash1_frame_cut COMMAND crash1_frame_cut_test)

  add_executable(crash1_catalog_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_catalog.cpp")
  target_compile_features(crash1_catalog_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_catalog_test PRIVATE crash1_runtime)
  add_test(NAME crash1_catalog COMMAND crash1_catalog_test
           "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json")

  add_executable(crash1_cd_boot_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_cd_boot.cpp")
  target_compile_features(crash1_cd_boot_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_cd_boot_test PRIVATE crash1_runtime)
  add_test(NAME crash1_cd_boot COMMAND crash1_cd_boot_test)

  add_executable(crash1_bios_pad_input_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_bios_pad_input.cpp")
  target_compile_features(crash1_bios_pad_input_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_bios_pad_input_test PRIVATE crash1_runtime)
  add_test(NAME crash1_bios_pad_input COMMAND crash1_bios_pad_input_test)

  add_executable(crash1_disc_index_io_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_disc_index_io.cpp")
  target_include_directories(crash1_disc_index_io_test PRIVATE "${PSXPORT_DIR}/tests")
  target_compile_features(crash1_disc_index_io_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_disc_index_io_test PRIVATE crash1_runtime)
  add_test(NAME crash1_disc_index_io COMMAND crash1_disc_index_io_test)

  add_executable(crash1_callback_boot_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_callback_boot.cpp")
  target_compile_features(crash1_callback_boot_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_callback_boot_test PRIVATE crash1_runtime)
  add_test(NAME crash1_callback_boot COMMAND crash1_callback_boot_test)

  add_executable(crash1_gpu_watchdog_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_gpu_watchdog.cpp")
  target_compile_features(crash1_gpu_watchdog_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_gpu_watchdog_test PRIVATE crash1_runtime)
  add_test(NAME crash1_gpu_watchdog COMMAND crash1_gpu_watchdog_test)

  add_executable(crash1_widescreen_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_widescreen.cpp")
  target_compile_features(crash1_widescreen_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_widescreen_test PRIVATE crash1_runtime)
  add_test(NAME crash1_widescreen COMMAND crash1_widescreen_test
           "${CMAKE_CURRENT_SOURCE_DIR}/scratch/executable-loader")

  add_executable(crash1_horizontal_bound_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_horizontal_bound.cpp")
  target_compile_features(crash1_horizontal_bound_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_horizontal_bound_test PRIVATE crash1_runtime)
  add_test(NAME crash1_horizontal_bound COMMAND crash1_horizontal_bound_test)

  # Block-pool owner: the recovered walk against a fixture, and the install/lookup through the real runtime.
  add_executable(crash1_block_pool_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_block_pool.cpp")
  target_compile_features(crash1_block_pool_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_block_pool_test PRIVATE crash1_runtime)
  add_test(NAME crash1_block_pool COMMAND crash1_block_pool_test)
  

  add_executable(crash1_c0_exception_contract_probe
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_c0_exception_contract.cpp")
  target_compile_definitions(
    crash1_c0_exception_contract_probe
    PRIVATE
      "CRASH1_GET_C0_TABLE_FUNCTION=${crash1_get_c0_table_function}u"
      "CRASH1_C0_EXCEPTION_HANDLER_SLOT=${crash1_c0_exception_handler_slot}u"
      "CRASH1_C0_EXCEPTION_HANDLER_ADDRESS=${crash1_c0_exception_handler_address}u"
  )
  target_compile_features(crash1_c0_exception_contract_probe PRIVATE cxx_std_20)
  target_link_libraries(crash1_c0_exception_contract_probe PRIVATE crash1_runtime)
  add_test(NAME crash1_c0_exception_contract COMMAND crash1_c0_exception_contract_probe)

  endif()

function(add_crt0_boundary_title title)
  set(title_cache "${CMAKE_CURRENT_SOURCE_DIR}/scratch/bin/${title}")

  add_library(
    ${title}_runtime STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/entry/${title}_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/frame/${title}_frame_driver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/render/widescreen/${title}_widescreen.cpp"
  )
  crash_title_include_dirs(${title}_runtime ${title})
  target_compile_features(${title}_runtime PUBLIC cxx_std_20)
  target_link_libraries(${title}_runtime PUBLIC crash_boundary_runtime crash_native_frame_contract)
  configure_title_vsync(${title}_runtime ${title})
  set_target_properties(${title}_runtime PROPERTIES CRASH_TITLE_CACHE "${title_cache}")

  if(BUILD_TESTING)
    add_executable(${title}_runtime_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/${title}_runtime.cpp")
    target_include_directories(${title}_runtime_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(${title}_runtime_test PRIVATE cxx_std_20)
    target_link_libraries(${title}_runtime_test PRIVATE ${title}_runtime)
    add_test(NAME ${title}_runtime_inheritance COMMAND ${title}_runtime_test)

    # Guest-widescreen owner: 4:3 identity, widening, call-site policy, install proof with no HLE plan. Needs no disc.
    add_executable(${title}_widescreen_test
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/${title}_widescreen.cpp")
    target_compile_features(${title}_widescreen_test PRIVATE cxx_std_20)
    target_link_libraries(${title}_widescreen_test PRIVATE ${title}_runtime)
    add_test(NAME ${title}_widescreen COMMAND ${title}_widescreen_test)
  endif()
endfunction()

add_crt0_boundary_title(crash2)
add_crt0_boundary_title(crash3)

if(BUILD_TESTING)
  add_executable(crash_native_frame_contract_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_frame_loop_contract.cpp")
  target_compile_features(crash_native_frame_contract_test PRIVATE cxx_std_20)
  target_link_libraries(crash_native_frame_contract_test
                        PRIVATE crash1_runtime crash2_runtime crash3_runtime)
  add_test(NAME crash_native_frame_contract COMMAND crash_native_frame_contract_test)

  add_test(
    NAME crash_cpp_policy
    COMMAND
      "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
      --root "${CMAKE_CURRENT_SOURCE_DIR}" --compile-commands "${CMAKE_BINARY_DIR}"
  )
  add_test(
    NAME crash_title_provision_tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_provision_titles.py"
  )
  set_tests_properties(crash_title_provision_tests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
  add_test(
    NAME crash_launcher_tests
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_run.py"
  )
  set_tests_properties(crash_launcher_tests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
add_executable(
  crash1_port
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/main.cpp"
)
crash_game_include_dirs(crash1_port)
target_compile_features(crash1_port PRIVATE cxx_std_20)
target_link_libraries(crash1_port PRIVATE crash1_runtime)
set_target_properties(crash1_port PROPERTIES ENABLE_EXPORTS ON)
if(BUILD_TESTING)
  add_test(NAME crash1_product_help COMMAND $<TARGET_FILE:crash1_port> --help)
  set_tests_properties(crash1_product_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")
  add_test(NAME crash1_product_help_short COMMAND $<TARGET_FILE:crash1_port> -h)
  set_tests_properties(crash1_product_help_short PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")
  add_test(
    NAME crash1_execution_boundary
    COMMAND
      "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_execution_boundary.py"
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --cmake "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
      --binary "$<TARGET_FILE:crash1_port>"
  )
endif()
