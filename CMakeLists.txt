cmake_minimum_required(VERSION 3.14)

# Company release version (keep in sync with VERSION / CHANGELOG)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" AETHER_VERSION_RAW)
string(STRIP "${AETHER_VERSION_RAW}" AETHER_VERSION)
if(NOT AETHER_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    set(AETHER_VERSION "1.1.0")
endif()

project(AetherResearch VERSION ${AETHER_VERSION} LANGUAGES C CXX)
message(STATUS "Aether ${PROJECT_VERSION} — company/lab research build")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Public headers: include/   Private impl: lib/aether/

# ---------------------------------------------------------------------------
# Zydis — real x86-64 disassembler
# Air-gap: -DAETHER_ZYDIS_ROOT=/path/to/zydis/source (local tree, no fetch)
# ---------------------------------------------------------------------------
set(AETHER_ZYDIS_ROOT "" CACHE PATH "Local Zydis source for air-gapped builds")
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYAN_NO_LIBC OFF CACHE BOOL "" FORCE)
if(AETHER_ZYDIS_ROOT AND EXISTS "${AETHER_ZYDIS_ROOT}/CMakeLists.txt")
    message(STATUS "Aether air-gap: Zydis from ${AETHER_ZYDIS_ROOT}")
    add_subdirectory("${AETHER_ZYDIS_ROOT}" "${CMAKE_BINARY_DIR}/_zydis_local")
else()
    include(FetchContent)
    FetchContent_Declare(
        zydis
        GIT_REPOSITORY https://github.com/zyantific/zydis.git
        GIT_TAG v4.1.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(zydis)
endif()

# ---------------------------------------------------------------------------
# Shared library sources (metamorphic engine + helpers)
# ---------------------------------------------------------------------------
set(AETHER_LIB_SOURCES
    lib/aether/common/rng.cpp
    lib/aether/noise/noise.cpp
    lib/aether/mutate/mutate.cpp
    lib/aether/meta/catalogue.cpp
    lib/aether/meta/decode.cpp
    lib/aether/meta/decode_real.cpp
    lib/aether/meta/elf_view.cpp
    lib/aether/meta/pe_view.cpp
    lib/aether/meta/equiv.cpp
    lib/aether/meta/morph_real.cpp
    lib/aether/meta/morph_engine.cpp
    lib/aether/meta/binary_rewrite.cpp
    lib/aether/meta/x64_sim.cpp
    lib/aether/meta/real_analysis.cpp
    lib/aether/meta/ssa_analysis.cpp
    lib/aether/meta/real_func_extract.cpp
    lib/aether/meta/bench.cpp
    lib/aether/meta/transforms.cpp
    lib/aether/meta/assemble.cpp
    lib/aether/meta/stages.cpp
    lib/aether/meta/crypto_cascade.cpp
    lib/aether/meta/pipeline.cpp
    lib/aether/infect/infect.cpp
    lib/aether/runtime/lazy_jsr.cpp
    lib/aether/api/aether_api.cpp
)

# Extras (infect demo, lazy JSR, cascade demos) always linked for research CLIs.
# Pure library consumers should use only aether.h morph/bench/rewrite APIs — see docs/LIBRARY.md
option(AETHER_BUILD_EXTRAS "Build research demo binaries (shock/next/stubs)" ON)

add_library(aether_core STATIC ${AETHER_LIB_SOURCES})
target_link_libraries(aether_core PUBLIC Zydis)
# PUBLIC: consumers get include/aether/aether.h
# PRIVATE: build sees lib/ for aether/meta/*.hpp etc.
target_include_directories(aether_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_SOURCE_DIR}/lib
)
# Tests and apps need private headers too when linking aether_core only PUBLIC
# Export lib/ as INTERFACE for in-tree apps/tests (research build)
target_include_directories(aether_core INTERFACE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib>)

if(AETHER_BUILD_EXTRAS)
  add_executable(aether_next apps/aether_next_main.cpp)
  target_link_libraries(aether_next PRIVATE aether_core)
  add_executable(aether_stubs apps/aether_stubs_main.cpp)
  target_include_directories(aether_stubs PRIVATE ${CMAKE_SOURCE_DIR}/stubs)
  add_executable(aether_shock apps/aether_shock_main.cpp)
  target_link_libraries(aether_shock PRIVATE aether_core)
endif()

# Step 1: real Zydis disasm of ELF .text / raw blobs
add_executable(aether_disasm apps/aether_disasm_main.cpp)
target_link_libraries(aether_disasm PRIVATE aether_core)

# Morph benchmark (≥1k corpus, break-rate, JSON) — public/internal bench entry
add_executable(aether_bench apps/aether_bench_main.cpp)
target_link_libraries(aether_bench PRIVATE aether_core)

# Industry-style morph CLI
add_executable(aether_morph apps/aether_morph_main.cpp)
target_link_libraries(aether_morph PRIVATE aether_core)

# ---------------------------------------------------------------------------
# Lightweight, dependency-free regression tests
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    set(AETHER_TEST_SOURCES
        tests/aether_core_tests.cpp
        tests/test_rng.cpp
        tests/test_noise.cpp
        tests/test_mutate.cpp
        tests/test_catalogue.cpp
        tests/test_decode_assemble.cpp
        tests/test_decode_real.cpp
        tests/test_equivalence.cpp
        tests/test_morph_real.cpp
        tests/test_morph_engine.cpp
        tests/test_binary_rewrite.cpp
        tests/test_ssa_analysis.cpp
        tests/test_bench.cpp
        tests/test_transforms.cpp
        tests/test_stages.cpp
        tests/test_pipeline.cpp
        tests/test_crypto_cascade.cpp
        tests/test_uniqueness.cpp
        tests/test_lazy_jsr.cpp
        tests/test_infect.cpp
        tests/test_stubs.cpp
    )
    add_executable(aether_core_tests ${AETHER_TEST_SOURCES})
    target_link_libraries(aether_core_tests PRIVATE aether_core)
    target_include_directories(aether_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/stubs)
    # Run ELF test with repo root as cwd so victim_clean resolves
    set(AETHER_TEST_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set(AETHER_TEST_ENV "AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE")
    foreach(AETHER_TEST_CASE
            rng
            noise
            mutate
            catalogue
            decode_assemble
            decode_real
            equivalence
            morph_real
            morph_engine
            binary_rewrite
            ssa_analysis
            bench
            transforms
            stages
            pipeline
            crypto_cascade
            uniqueness
            lazy_jsr
            infect
            stubs)
        add_test(NAME unit_${AETHER_TEST_CASE}
                 COMMAND aether_core_tests ${AETHER_TEST_CASE}
                 WORKING_DIRECTORY ${AETHER_TEST_WORKING_DIRECTORY})
        set_tests_properties(unit_${AETHER_TEST_CASE} PROPERTIES
            ENVIRONMENT "AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE")
    endforeach()
endif()

# Convenience: run full uniqueness proof (200 morph + 200 cascade + permute)
add_custom_target(prove-unique
    COMMAND $<TARGET_FILE:aether_core_tests> uniqueness
    DEPENDS aether_core_tests
    COMMENT "Prove always-different generations (200-run harness)"
)

# Professional equivalence oracle (Step 2) — 0 semantic breaks required
add_custom_target(prove-equiv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/artifacts
    COMMAND $<TARGET_FILE:aether_core_tests> equivalence
    DEPENDS aether_core_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Prove morph semantic equivalence (0 breaks on pure-function corpus)"
)

# Full shock proof: uniqueness + equivalence (Edu + Real 1B)
add_custom_target(prove-shock
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/artifacts
    COMMAND $<TARGET_FILE:aether_core_tests> uniqueness
    COMMAND $<TARGET_FILE:aether_core_tests> equivalence
    COMMAND $<TARGET_FILE:aether_core_tests> morph_real
    DEPENDS aether_core_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Uniqueness + equivalence (Edu/Real 1B) + morph_real — professional gate"
)

# Final gate: ≥1k total, ≥200 ELF-extracted, break-rate 0, JSON regenerable
add_custom_target(prove-bench
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/artifacts
    COMMAND $<TARGET_FILE:aether_bench> --count 1000
            --json ${CMAKE_SOURCE_DIR}/artifacts/bench_report.json --rounds 2
    DEPENDS aether_bench
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Morph bench ≥1k + elf≥200, break-rate 0 → artifacts/bench_report.json"
)

# Industry morph framework gate (MorphEngine multi-policy self-test)
add_custom_target(prove-industry
    COMMAND ${CMAKE_COMMAND} -E env AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
            $<TARGET_FILE:aether_core_tests> morph_engine
    COMMAND ${CMAKE_COMMAND} -E env AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
            $<TARGET_FILE:aether_core_tests> binary_rewrite
    COMMAND ${CMAKE_COMMAND} -E env AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
            $<TARGET_FILE:aether_core_tests> ssa_analysis
    COMMAND ${CMAKE_COMMAND} -E env AETHER_LICENSE_ACCEPTED=I_ACCEPT_AETHER_LICENSE
            $<TARGET_FILE:aether_bench> --count 1000
            --json ${CMAKE_SOURCE_DIR}/artifacts/bench_report.json --rounds 2
    DEPENDS aether_core_tests aether_bench
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Industry MorphEngine + rewrite + SSA + full bench"
)

# ---------------------------------------------------------------------------
# Code style — clang-format (C++ analogue of Spotless apply / check)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE AETHER_FORMAT_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/*.cpp
    ${CMAKE_SOURCE_DIR}/lib/*.hpp
    ${CMAKE_SOURCE_DIR}/include/*.h
    ${CMAKE_SOURCE_DIR}/apps/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.hpp
)

find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${AETHER_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Applying clang-format (Spotless-style rewrite)"
        VERBATIM
    )

    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --style=file ${AETHER_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking clang-format (fail on drift)"
        VERBATIM
    )

    message(STATUS "clang-format: ${CLANG_FORMAT_EXE} (targets: format, format-check)")
else()
    message(STATUS "clang-format not found — install it to use format / format-check targets")
endif()

# ---------------------------------------------------------------------------
# Install (company prefix e.g. /opt/aether)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
set(AETHER_INSTALL_APPS aether_disasm aether_bench aether_morph)
if(AETHER_BUILD_EXTRAS)
    list(APPEND AETHER_INSTALL_APPS aether_next aether_shock aether_stubs)
endif()
install(TARGETS ${AETHER_INSTALL_APPS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        OPTIONAL)
install(TARGETS aether_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/aether
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
install(FILES
        README.md LICENSE SECURITY.md CHANGELOG.md VERSION SUPPORT.md
        docs/PRODUCT.md docs/LAB_INSTALL.md docs/SCORECARD.md docs/SCOPE.md
        docs/LIBRARY.md docs/LICENSE_ACCEPTANCE.md docs/REPOSITORY_LAYOUT.md
        docs/IMPLEMENTATION_PLAN.md docs/RELEASE.md docs/AIRGAP.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES corpus/real_corpus.elf DESTINATION ${CMAKE_INSTALL_DATADIR}/aether/corpus)
