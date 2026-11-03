cmake_minimum_required(VERSION 3.24)
project(crash_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# PSXPORT_DIR: the framework, a symlink to the workspace's live checkout or a clone of its main
# (psxport tools/psxport_fetch.py); defaults to external/psxport.
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "./run.sh once to fetch it, or point PSXPORT_DIR at a psxport checkout.")
endif()
add_subdirectory("${PSXPORT_DIR}" psxport_build)

# Includes name the owning directory: "runtime/runtime.h" under game/, "crash1/entry/runtime.h" under titles/.
function(crash_include_roots target)
  target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/game" "${CMAKE_CURRENT_SOURCE_DIR}/titles")
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
          "CRASH_CD_SYNC_ENTRY;runtime;native_boot;cd_sync;entry"
          "CRASH_CD_SYNC_END;runtime;native_boot;cd_sync;end"
          "CRASH_CD_READ_ENTRY;runtime;native_boot;cd_read;entry"
          "CRASH_CD_READ_END;runtime;native_boot;cd_read;end"
          "CRASH_CD_READ_SYNC_ENTRY;runtime;native_boot;cd_read_sync;entry"
          "CRASH_CD_READ_SYNC_END;runtime;native_boot;cd_read_sync;end"
          "CRASH_CALLBACK_INITIALIZE_ENTRY;runtime;native_boot;callback_initialize;entry"
          "CRASH_CALLBACK_INITIALIZE_END;runtime;native_boot;callback_initialize;end"
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
  # A title whose watchdog check differs from the shared owner's records no watchdog.
  string(JSON watchdog ERROR_VARIABLE no_watchdog GET "${native_manifest}" runtime native_boot gpu_watchdog_start)
  if(NOT no_watchdog)
    foreach(fact
            "CRASH_GPU_WATCHDOG_START_ENTRY;gpu_watchdog_start;entry"
            "CRASH_GPU_WATCHDOG_START_END;gpu_watchdog_start;end"
            "CRASH_GPU_WATCHDOG_CHECK_ENTRY;gpu_watchdog_check;entry"
            "CRASH_GPU_WATCHDOG_CHECK_END;gpu_watchdog_check;end")
      list(POP_FRONT fact definition)
      string(JSON value GET "${native_manifest}" runtime native_boot ${fact})
      target_compile_definitions(${target} PRIVATE "${definition}=${value}u")
    endforeach()
  endif()
  # A title whose scene loader swaps buffers outside GpuUpdate records that swap.
  string(JSON loading_swap ERROR_VARIABLE no_loading_swap GET "${native_manifest}" runtime native_frame loading_swap)
  if(NOT no_loading_swap)
    foreach(fact
            "CRASH_LOADING_SWAP_ENTRY;entry"
            "CRASH_LOADING_SWAP_END;end"
            "CRASH_LOADING_SWAP_AFTER_VSYNC;after_vsync")
      list(POP_FRONT fact definition)
      string(JSON value GET "${native_manifest}" runtime native_frame loading_swap ${fact})
      target_compile_definitions(${target} PRIVATE "${definition}=${value}u")
    endforeach()
  endif()
endfunction()

# The title-independent owners: runtime, session, boot services and the frame turn.
add_library(
  crash_game STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/callback_boot.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/gpu_watchdog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/boot/libcd_init.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/frame_cut.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/debug/debug_menu.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/frame_driver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/frame/scene_warp.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/runtime/runtime.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/game/runtime/session.cpp"
)
crash_include_roots(crash_game)
target_compile_features(crash_game PUBLIC cxx_std_20)
target_link_libraries(crash_game PUBLIC psxport)

# One runtime library per title: its facts, its runtime and any title-only owners.
function(add_crash_title title)
  add_library(${title}_runtime STATIC
              "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/entry/facts.cpp"
              "${CMAKE_CURRENT_SOURCE_DIR}/titles/${title}/entry/runtime.cpp"
              ${ARGN})
  target_compile_features(${title}_runtime PUBLIC cxx_std_20)
  target_link_libraries(${title}_runtime PUBLIC crash_game)
  configure_title_vsync(${title}_runtime ${title})
  configure_title_identity(${title}_runtime ${title})
  configure_title_native_facts(${title}_runtime ${title})
endfunction()

add_crash_title(
  crash1
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/entry/session.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/frame/frame_driver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/input/bios_pad_input.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/render/block_pool.cpp"
)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crash1/executable.json" crash1_manifest_json)
string(JSON crash1_bios_pad_word GET "${crash1_manifest_json}" runtime native_frame bios_pad_read address)
target_compile_definitions(crash1_runtime PRIVATE "CRASH1_BIOS_PAD_READ_WORD_ADDRESS=${crash1_bios_pad_word}u")
add_crash_title(crash2)
add_crash_title(crash3)

add_library(crash_catalog STATIC "${CMAKE_CURRENT_SOURCE_DIR}/product/catalog.cpp")
target_include_directories(crash_catalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/product")
target_compile_features(crash_catalog PUBLIC cxx_std_20)
target_link_libraries(crash_catalog PUBLIC crash1_runtime crash2_runtime crash3_runtime)

psxport_add_product(crash_port CATALOG crash::createCatalog HEADER catalog.h LIBRARIES crash_catalog)
set_target_properties(crash_port PROPERTIES ENABLE_EXPORTS ON)
