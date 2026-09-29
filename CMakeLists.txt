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
find_package(OpenSSL REQUIRED)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${OPENSSL_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/vendor/retroarch")

add_executable(vitaretro
  src/main.cpp
  src/retroarch_manager.cpp
  src/retroarch_deployer.cpp
  src/source_pairing.cpp
  src/vendor/qrcodegen.c
)

target_link_libraries(vitaretro
  SceDisplay_stub
  SceCtrl_stub
  SceSysmem_stub
  SceProcessmgr_stub
  SceAppMgr_stub
  SceIofilemgr_stub
  SceNet_stub
  SceNetCtl_stub
  SceSysmodule_stub
  SceHttp_stub
  SceSsl_stub
  crypto
  z
)

vita_create_self(eboot.bin vitaretro)

set(VPK_FILES
  FILE assets/sce_sys/icon0.png sce_sys/icon0.png
  FILE assets/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/retroarch/payload_manifest.hpp")
  message(FATAL_ERROR "Prepare the RetroArch payload before configuring")
endif()

vita_create_vpk(VitaRetro_0.1_DEV.vpk VRET00001 eboot.bin
  VERSION 00.10
  NAME "VitaRetro 0.1 DEV"
  ${VPK_FILES}
)
