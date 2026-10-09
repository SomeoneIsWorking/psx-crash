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
    disc
    entry
    frame
    render/widescreen
)
set(CRASH_TITLE_DIRS
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

# The serial-identified executable a title's catalog entry is built from.
function(configure_title_identity target title)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/executable.json" identity_manifest)
  foreach(fact
          "CRASH_EXE_ENTRY;header;entry"
          "CRASH_EXE_GP;header;gp"
          "CRASH_EXE_TEXT_ADDRESS;header;text_address"
          "CRASH_EXE_TEXT_SIZE;header;text_size"
          "CRASH_EXE_STACK_ADDRESS;header;stack_address"
          "CRASH_EXE_STACK_OFFSET;header;stack_offset")
    list(POP_FRONT fact definition)
    string(JSON value GET "${identity_manifest}" ${fact})
    target_compile_definitions(${target} PRIVATE "${definition}=${value}u")
  endforeach()
  string(JSON expected_size GET "${identity_manifest}" file_size)
  string(JSON exe_sha256 GET "${identity_manifest}" sha256)
  string(JSON exe_name GET "${identity_manifest}" executable)
  string(JSON exe_title GET "${identity_manifest}" title)
  target_compile_definitions(${target} PRIVATE
    "CRASH_EXE_EXPECTED_SIZE=${expected_size}u"
    "CRASH_EXE_SHA256=\"${exe_sha256}\""
    "CRASH_EXE_NAME=\"${exe_name}\""
    "CRASH_EXE_TITLE=\"${exe_title}\"")
endfunction()

# The native boot and frame facts every Crash title measures; the title's facts file consumes them.
function(configure_title_native_facts target title)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/executable.json" native_manifest)
  foreach(fact
          "CRASH_STATIC_CONSTRUCTORS_ENTRY;runtime;native_boot;static_constructors;entry"
          "CRASH_USE_CD_ADDRESS;runtime;native_boot;use_cd_address"
          "CRASH_INIT_ENTRY;runtime;native_boot;init;entry"
          "CRASH_CD_INITIALIZE_ENTRY;runtime;native_boot;cd_initialize;entry"
          "CRASH_CD_INITIALIZE_END;runtime;native_boot;cd_initialize;end"
          "CRASH_CD_CONTROL_ENTRY;runtime;native_boot;cd_control;entry"
          "CRASH_CD_CONTROL_END;runtime;native_boot;cd_control;end"
          "CRASH_CD_CONTROL_F_ENTRY;runtime;native_boot;cd_control_f;entry"
          "CRASH_CD_CONTROL_F_END;runtime;native_boot;cd_control_f;end"
          "CRASH_CD_SYNC_WRAPPER_ENTRY;runtime;native_boot;cd_sync_wrapper;entry"
          "CRASH_CD_SYNC_WRAPPER_END;runtime;native_boot;cd_sync_wrapper;end"
          "CRASH_CD_SYNC_ENTRY;runtime;native_boot;cd_sync;entry"
          "CRASH_CD_SYNC_END;runtime;native_boot;cd_sync;end"
          "CRASH_CD_READ_ENTRY;runtime;native_boot;cd_read;entry"
          "CRASH_CD_READ_END;runtime;native_boot;cd_read;end"
          "CRASH_CD_READ_SYNC_ENTRY;runtime;native_boot;cd_read_sync;entry"
          "CRASH_CD_READ_SYNC_END;runtime;native_boot;cd_read_sync;end"
          "CRASH_CALLBACK_INITIALIZE_ENTRY;runtime;native_boot;callback_initialize;entry"
          "CRASH_CALLBACK_INITIALIZE_END;runtime;native_boot;callback_initialize;end"
          "CRASH_GPU_WATCHDOG_START_ENTRY;runtime;native_boot;gpu_watchdog_start;entry"
          "CRASH_GPU_WATCHDOG_START_END;runtime;native_boot;gpu_watchdog_start;end"
          "CRASH_GPU_WATCHDOG_CHECK_ENTRY;runtime;native_boot;gpu_watchdog_check;entry"
          "CRASH_GPU_WATCHDOG_CHECK_END;runtime;native_boot;gpu_watchdog_check;end"
          "CRASH_CORE_LOOP_ENTRY;runtime;native_frame;core_loop;entry"
          "CRASH_CORE_LOOP_END;runtime;native_frame;core_loop;end"
          "CRASH_INITIAL_SCENE;runtime;native_frame;initial_scene"
          "CRASH_FRAME_ITERATION_ENTRY;runtime;native_frame;iteration;entry"
          "CRASH_FRAME_ITERATION_END;runtime;native_frame;iteration;end"
          "CRASH_FRAME_TRANSITION_ENTRY;runtime;native_frame;transition;entry"
          "CRASH_FRAME_TRANSITION_END;runtime;native_frame;transition;end"
          "CRASH_FRAME_TRANSITION_RETURN;runtime;native_frame;transition;return_address"
          "CRASH_GPU_UPDATE_ENTRY;runtime;native_frame;gpu_update;entry"
          "CRASH_GPU_UPDATE_END;runtime;native_frame;gpu_update;end"
          "CRASH_FIRST_VSYNC;runtime;native_frame;first_vsync"
          "CRASH_AFTER_FIRST_VSYNC;runtime;native_frame;after_first_vsync"
          "CRASH_SECOND_VSYNC;runtime;native_frame;second_vsync"
          "CRASH_AFTER_VSYNC;runtime;native_frame;after_vsync"
          "CRASH_DONE_ADDRESS;runtime;native_frame;done;address"
          "CRASH_SCENE_ID_ADDRESS;runtime;native_frame;scene;id_address"
          "CRASH_SCENE_REQUEST_ADDRESS;runtime;native_frame;scene;request_address"
          "CRASH_ROOT_COUNTER_INCREMENT;runtime;native_frame;root_counter_increment"
          "CRASH_SET_ROOT_COUNTER;runtime;native_frame;set_root_counter"
          "CRASH_START_ROOT_COUNTER;runtime;native_frame;start_root_counter"
          "CRASH_STOP_ROOT_COUNTER;runtime;native_frame;stop_root_counter")
    list(POP_FRONT fact definition)
    string(JSON value GET "${native_manifest}" ${fact})
    target_compile_definitions(${target} PRIVATE "${definition}=${value}u")
  endforeach()
endfunction()

# The title-independent owners: boot services, the frame turn, the projection rule, the refusing runtime.
add_library(
  crash_game STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/boundary_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/callback_boot.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/gpu_watchdog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/libcd_init.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/disc/stock_libcd.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/entry/crash_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/crash_frame_driver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/frame_cut.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/scene_warp.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/native_frame_loop_contract.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/render/widescreen/guest_projection_publication.cpp"
)
crash_game_include_dirs(crash_game)
target_compile_features(crash_game PUBLIC cxx_std_20)
target_link_libraries(crash_game PUBLIC psxport)

add_library(
  crash1_runtime STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/entry/crash1_facts.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/entry/crash1_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/frame/crash1_frame_driver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/input/crash1_bios_pad_input.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/crash1_block_pool.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/widescreen/crash1_horizontal_bound.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/widescreen/crash1_widescreen.cpp"
)
crash_title_include_dirs(crash1_runtime crash1)
target_compile_features(crash1_runtime PUBLIC cxx_std_20)
target_link_libraries(crash1_runtime PUBLIC crash_game)
configure_title_vsync(crash1_runtime crash1)
configure_title_identity(crash1_runtime crash1)
configure_title_native_facts(crash1_runtime crash1)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json" crash1_manifest_json)
string(JSON crash1_bios_pad_word GET "${crash1_manifest_json}" runtime native_frame bios_pad_read address)
target_compile_definitions(crash1_runtime PRIVATE "CRASH1_BIOS_PAD_READ_WORD_ADDRESS=${crash1_bios_pad_word}u")

add_library(
  crash2_runtime STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash2/entry/crash2_facts.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash2/entry/crash2_runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash2/render/widescreen/crash2_widescreen.cpp"
)
crash_title_include_dirs(crash2_runtime crash2)
target_compile_features(crash2_runtime PUBLIC cxx_std_20)
target_link_libraries(crash2_runtime PUBLIC crash_game)
configure_title_vsync(crash2_runtime crash2)
configure_title_identity(crash2_runtime crash2)
configure_title_native_facts(crash2_runtime crash2)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/bios_contract.json" crash1_bios_contract_json)
string(JSON crash1_get_c0_table_function
       GET "${crash1_bios_contract_json}" get_c0_table function)
string(JSON crash1_c0_exception_handler_slot
       GET "${crash1_bios_contract_json}" exception_handler table_slot)
string(JSON crash1_c0_exception_handler_address
       GET "${crash1_bios_contract_json}" exception_handler retail_address)

if(BUILD_TESTING)
  # Shared owners, run over every title that ships the Crash runtime.
  foreach(shared_test
          crash_runtime crash_code_module crash_scene_warp crash_libcd_init crash_callback_boot crash_gpu_watchdog crash_frame_cut crash_frame_turn)
    add_executable(${shared_test}_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/${shared_test}.cpp")
    target_include_directories(${shared_test}_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_compile_features(${shared_test}_test PRIVATE cxx_std_20)
    target_link_libraries(${shared_test}_test PRIVATE crash1_runtime crash2_runtime)
    add_test(NAME ${shared_test} COMMAND ${shared_test}_test)
  endforeach()

  add_executable(crash_stock_libcd_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash_stock_libcd.cpp")
  target_include_directories(crash_stock_libcd_test PRIVATE "${PSXPORT_DIR}/tests")
  target_compile_features(crash_stock_libcd_test PRIVATE cxx_std_20)
  target_link_libraries(crash_stock_libcd_test PRIVATE crash_game)
  add_test(NAME crash_stock_libcd COMMAND crash_stock_libcd_test)

  add_executable(crash_catalog_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash_catalog.cpp")
  target_include_directories(crash_catalog_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/product")
  target_compile_features(crash_catalog_test PRIVATE cxx_std_20)
  target_link_libraries(crash_catalog_test PRIVATE crash_catalog)
  add_test(NAME crash_catalog COMMAND crash_catalog_test
           "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json"
           "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash2/executable.json")

  add_executable(crash1_bios_pad_input_test
                 "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash1_bios_pad_input.cpp")
  target_compile_features(crash1_bios_pad_input_test PRIVATE cxx_std_20)
  target_link_libraries(crash1_bios_pad_input_test PRIVATE crash1_runtime)
  add_test(NAME crash1_bios_pad_input COMMAND crash1_bios_pad_input_test)

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


  add_executable(crash2_widescreen_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/crash2_widescreen.cpp")
  target_compile_features(crash2_widescreen_test PRIVATE cxx_std_20)
  target_link_libraries(crash2_widescreen_test PRIVATE crash2_runtime)
  add_test(NAME crash2_widescreen COMMAND crash2_widescreen_test)

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
  target_link_libraries(${title}_runtime PUBLIC crash_game)
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

add_crt0_boundary_title(crash3)

add_library(crash_catalog STATIC "${CMAKE_CURRENT_SOURCE_DIR}/product/crash_catalog.cpp")
target_include_directories(crash_catalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/product")
target_compile_features(crash_catalog PUBLIC cxx_std_20)
target_link_libraries(crash_catalog PUBLIC crash1_runtime crash2_runtime)

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
  "${CMAKE_CURRENT_SOURCE_DIR}/product/main.cpp"
)
crash_game_include_dirs(crash1_port)
target_compile_features(crash1_port PRIVATE cxx_std_20)
target_link_libraries(crash1_port PRIVATE crash_catalog)
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
