cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable")
  endif()
endif()
project(VitaRetro LANGUAGES C CXX)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vitaretro
  src/main.cpp
  src/retroarch_manager.cpp
)

target_link_libraries(vitaretro
  SceDisplay_stub
  SceCtrl_stub
  SceSysmem_stub
  SceProcessmgr_stub
  SceAppMgr_stub
  SceIofilemgr_stub
)

vita_create_self(eboot.bin vitaretro)

set(VPK_FILES
  FILE assets/sce_sys/icon0.png sce_sys/icon0.png
  FILE assets/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)

# Keep the package small and fail at configuration when the official payload is
# absent. Vita loads each core as a separate statically linked SELF executable.
set(RETROARCH_CORE_NAMES snes9x2005_plus genesis_plus_gx fceumm gambatte gpsp pcsx_rearmed)
foreach(core_name IN LISTS RETROARCH_CORE_NAMES)
  set(core "${CMAKE_SOURCE_DIR}/vendor/retroarch/vpk/${core_name}_libretro.self")
  if(NOT EXISTS "${core}")
    message(FATAL_ERROR "Official RetroArch Vita core missing: ${core}")
  endif()
  list(APPEND VPK_FILES FILE "${core}" "retroarch/${core_name}_libretro.self")
endforeach()

file(GLOB_RECURSE RETROARCH_DATA_FILES RELATIVE "${CMAKE_SOURCE_DIR}/vendor/retroarch/data"
  "${CMAKE_SOURCE_DIR}/vendor/retroarch/data/*")
if(NOT RETROARCH_DATA_FILES)
  message(FATAL_ERROR "Official RetroArch Vita data is missing")
endif()
foreach(data_file IN LISTS RETROARCH_DATA_FILES)
  if(NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/vendor/retroarch/data/${data_file}")
    list(APPEND VPK_FILES FILE
      "${CMAKE_SOURCE_DIR}/vendor/retroarch/data/${data_file}"
      "retroarch-data/${data_file}")
  endif()
endforeach()

vita_create_vpk(VitaRetro_0.1_DEV.vpk VRET00001 eboot.bin
  VERSION 00.10
  NAME "VitaRetro 0.1 DEV"
  ${VPK_FILES}
)
