# hungarian-ui: the interface translations (ui_language.cpp and the compiled-in
# catalogues), a library of their own so that a build with the debugger but
# without the Qt main window (ENABLE_QT_UI=OFF ENABLE_DEBUGGER=ON, a combination
# `make build-matrix` links) has them too: jnext_gui does not exist there, and
# debugger_accel_test's DACC-HU rows install the Hungarian catalogue. Added from
# the root CMakeLists whenever either Qt library is built; jnext_gui links it.
find_package(Qt${JNEXT_QT_MAJOR} REQUIRED COMPONENTS Core)

add_library(jnext_i18n STATIC ../ui_language.cpp jnext_i18n.qrc)

# --no-zstd: a cross build runs the HOST's rcc, which may compress with zstd
# while the target Qt has no zstd support (the MinGW Qt then fails to link with
# an undefined qResourceFeatureZstd); zlib works with every Qt. Qt 6 only: Qt 5's
# rcc never compresses with zstd and refuses the option ("Unknown option
# 'no-zstd'", the qt5-guard build).
set_target_properties(jnext_i18n PROPERTIES AUTORCC ON)
if(JNEXT_QT_MAJOR EQUAL 6)
    set_target_properties(jnext_i18n PROPERTIES AUTORCC_OPTIONS "--no-zstd")
endif()

target_link_libraries(jnext_i18n PUBLIC Qt${JNEXT_QT_MAJOR}::Core spdlog::spdlog)
target_include_directories(jnext_i18n PUBLIC ${CMAKE_SOURCE_DIR}/src)
