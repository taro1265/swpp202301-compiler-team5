cmake_minimum_required(VERSION 3.13.0)
project(swpp-compiler VERSION 2023.1.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Specify LLVM directory to use
get_filename_component(LLVM_ROOT ~/llvm-swpp ABSOLUTE)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVM in: ${LLVM_BINARY_DIR}")

# Specify Alive2 directory to use
# Copy alive2 at home directory
get_filename_component(ALIVE2_ROOT ~/alive2 ABSOLUTE)

# Specify Interpreter directory to use
# Copy alive2 at home directory with ~/interpreter-swpp
get_filename_component(INTERPRETER_ROOT ~/interpreter-swpp ABSOLUTE)

add_compile_options(-fPIC)

add_library(Fs ${SRC_DIR}/fs.cpp)

add_library(SCArgs ${SRC_DIR}/args.cpp)

add_library(SCParser OBJECT ${SRC_DIR}/lib/parser.cpp)
target_include_directories(SCParser PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(parser_llvm_libs asmparser)
target_link_libraries(SCParser ${parser_llvm_libs})

add_library(SCPrintIR ${SRC_DIR}/lib/print_ir.cpp)
target_include_directories(SCPrintIR PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(printer_llvm_libs core support)
target_link_libraries(SCPrintIR ${printer_llvm_libs})

set(SRC_BACKEND_DIR ${SRC_DIR}/lib/backend)
add_library(SCBackendAssembly OBJECT
                ${SRC_BACKEND_DIR}/assembly/inst.cpp
                ${SRC_BACKEND_DIR}/assembly/int_t.cpp
                ${SRC_BACKEND_DIR}/assembly/register_t.cpp
                ${SRC_BACKEND_DIR}/assembly/width_t.cpp)

add_library(SCBackendAnalysis OBJECT ${SRC_BACKEND_DIR}/analysis.cpp)
target_include_directories(SCBackendAnalysis PRIVATE ${LLVM_INCLUDE_DIRS})

add_library(SCBackendEmitter OBJECT ${SRC_BACKEND_DIR}/emitter.cpp)
target_include_directories(SCBackendEmitter PRIVATE ${LLVM_INCLUDE_DIRS})

add_library(SCBackendSymbol OBJECT ${SRC_BACKEND_DIR}/symbol.cpp
                ${SRC_BACKEND_DIR}/symbol/base.cpp)
target_include_directories(SCBackendSymbol PRIVATE ${LLVM_INCLUDE_DIRS})

function(add_backend_pass pass_name file_name)
    add_library(${pass_name} OBJECT ${SRC_BACKEND_DIR}/${file_name})
    target_include_directories(${pass_name} PRIVATE ${LLVM_INCLUDE_DIRS})
endfunction()

add_backend_pass(SCBackendCheckOraclePass check_oracle.cpp)
add_backend_pass(SCBackendConstExprElimPass const_expr_eliminate.cpp)
add_backend_pass(SCBackendGEPElimPass gep_eliminate.cpp)
add_backend_pass(SCBackendGVElimPass gv_eliminate.cpp)
add_backend_pass(SCBackendAllocaElimPass alloca_eliminate.cpp)
add_backend_pass(SCBackendGEPConstCombPass gep_const_combine.cpp)
add_backend_pass(SCBackendPHIPreprocessPass phi_preprocess.cpp)
add_backend_pass(SCBackendRegAllocPass register_allocate.cpp)

add_library(SCBackend ${SRC_DIR}/lib/backend.cpp)
target_include_directories(SCBackend PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(backend_llvm_libs analysis scalaropts)
target_link_libraries(SCBackend PRIVATE
                        SCPrintIR SCBackendAssembly SCBackendAnalysis
                        SCBackendEmitter SCBackendSymbol SCBackendCheckOraclePass
                        SCBackendConstExprElimPass SCBackendGEPElimPass
                        SCBackendGVElimPass SCBackendAllocaElimPass
                        SCBackendGEPConstCombPass SCBackendPHIPreprocessPass
                        SCBackendRegAllocPass ${backend_llvm_libs})

set(SRC_OPT_DIR ${SRC_DIR}/lib/opt)
add_library(OptPasses INTERFACE)

llvm_map_components_to_libnames(pass_llvm_libs passes analysis core support)
function(add_opt_pass pass_name file_name)
    add_library(${pass_name} ${SRC_OPT_DIR}/${file_name})
    target_include_directories(${pass_name} PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(${pass_name} PRIVATE ${pass_llvm_libs})
    target_link_libraries(OptPasses INTERFACE ${pass_name})
endfunction()

# Run test for pass_name
set(FILECHECK_RUN ${TEST_DIR}/run.sh)
function(test_opt_pass pass_name do_alive2)
    # Get filechecks for pass_name opt
    file(GLOB filecheck_files ${TEST_DIR}/filecheck/${pass_name}/*)

    # Run test on each filecheck
    foreach(filecheck ${filecheck_files})
      if(${do_alive2})
        add_test(NAME ${filecheck} COMMAND bash ${FILECHECK_RUN} ${LLVM_ROOT} ${ALIVE2_ROOT} ${INTERPRETER_ROOT} ${BUILD_DIR} ${TEST_DIR} ${pass_name} ${filecheck})
      else()
        add_test(NAME ${filecheck} COMMAND bash ${FILECHECK_RUN} ${LLVM_ROOT} -1 ${INTERPRETER_ROOT} ${BUILD_DIR} ${TEST_DIR} ${pass_name} ${filecheck})
      endif()
    endforeach()
endfunction()

# /================= ADD OPT PASSES BELOW ================/
include(CTest)
add_opt_pass(O3Pass add_o3.cpp)
add_opt_pass(SimplePass simple.cpp)
test_opt_pass(SimplePass 1)
add_opt_pass(LoopsPrinterPass loop_analysis.cpp)
test_opt_pass(LoopsPrinterPass 1)
add_opt_pass(LoopBranchConditionPass loop_branch_condition.cpp)
target_link_libraries(LoopBranchConditionPass PUBLIC LoopsPrinterPass)
test_opt_pass(LoopBranchConditionPass 1)
add_opt_pass(AddSumPass add_sum.cpp)
test_opt_pass(AddSumPass 0)
add_opt_pass(ShiftConstantAddPass shift_constant_add.cpp)
test_opt_pass(ShiftConstantAddPass 0)
add_opt_pass(LoadReorderingPass load_reordering.cpp)
test_opt_pass(LoadReorderingPass 1)
add_opt_pass(LoadToAloadPass load_to_aload.cpp)
test_opt_pass(LoadToAloadPass 1)
add_opt_pass(HeapPromotionPass heap_promotion.cpp)
test_opt_pass(HeapPromotionPass 0)
add_opt_pass(MallocFreeReorderingPass malloc_free_reordering.cpp)
test_opt_pass(MallocFreeReorderingPass 0)
add_opt_pass(AloadBlockPass aload_block.cpp)
test_opt_pass(AloadBlockPass 1)
add_opt_pass(AloadReorderingPass aload_reordering.cpp)
target_link_libraries(AloadReorderingPass PUBLIC LoadToAloadPass)
test_opt_pass(AloadReorderingPass 1)
add_opt_pass(SwitchToBrPass switch_to_br.cpp)
test_opt_pass(SwitchToBrPass 1)
add_opt_pass(BrLikelyPrinterPass branch_likely_analysis.cpp)
target_link_libraries(BrLikelyPrinterPass PUBLIC LoopsPrinterPass)
test_opt_pass(BrLikelyPrinterPass 0)
add_opt_pass(BrToSwitchPass br_to_switch.cpp)
target_link_libraries(BrToSwitchPass PUBLIC BrLikelyPrinterPass)
test_opt_pass(BrToSwitchPass 1)
add_opt_pass(LikelyBranchConditionPass likely_branch_condition.cpp)
target_link_libraries(LikelyBranchConditionPass PUBLIC BrLikelyPrinterPass)
test_opt_pass(LikelyBranchConditionPass 0)
add_opt_pass(SccpPass add_sccp.cpp)
test_opt_pass(SccpPass 1)
add_opt_pass(RecursiveBranchConditionPass recursive_branch_condition.cpp)
test_opt_pass(RecursiveBranchConditionPass 0)
add_opt_pass(LicmPass add_licm.cpp)
test_opt_pass(LicmPass 0)
add_opt_pass(PreOraclePass pre_oracle.cpp)
test_opt_pass(PreOraclePass 0)
add_opt_pass(OraclePass oracle.cpp)
test_opt_pass(OraclePass 0)
add_opt_pass(DeclarePass declare.cpp)

# /================== DO NOT EDIT BELOW ==================/
add_library(SCOpt ${SRC_DIR}/lib/opt.cpp)
target_include_directories(SCOpt PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(opt_llvm_libs analysis passes scalaropts transformutils)
target_link_libraries(SCOpt PRIVATE SCPrintIR OptPasses ${opt_llvm_libs})

set(PROJECT_LIB "SComp")
add_library(${PROJECT_LIB} ${SRC_DIR}/lib.cpp)
target_include_directories(${PROJECT_LIB} PRIVATE ${LLVM_INCLUDE_DIRS})
llvm_map_components_to_libnames(comp_llvm_libs passes analysis)
target_link_libraries(${PROJECT_LIB} PRIVATE
                         Fs SCBackend SCPrintIR SCOpt SCParser ${comp_llvm_libs})

add_executable(${PROJECT_NAME} ${SRC_DIR}/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIB} SCArgs)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    add_custom_target(docs ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen should be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)

enable_testing()
# Add tests, subdirectories, etc below
