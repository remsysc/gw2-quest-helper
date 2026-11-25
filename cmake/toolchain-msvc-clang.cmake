set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)

set(XWIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../.xwin")

set(MSVC_INCLUDE "${XWIN_DIR}/crt/include")
set(UCRT_INCLUDE "${XWIN_DIR}/sdk/include/ucrt")
set(UM_INCLUDE   "${XWIN_DIR}/sdk/include/um")
set(SHARED_INC   "${XWIN_DIR}/sdk/include/shared")

set(MSVC_LIB "${XWIN_DIR}/crt/lib/x86_64")
set(UCRT_LIB "${XWIN_DIR}/sdk/lib/ucrt/x86_64")
set(UM_LIB   "${XWIN_DIR}/sdk/lib/um/x86_64")

set(ARCH_FLAGS "--target=x86_64-pc-windows-msvc -Wno-unused-command-line-argument")
set(INC_FLAGS  "/imsvc\"${MSVC_INCLUDE}\" /imsvc\"${UCRT_INCLUDE}\" /imsvc\"${UM_INCLUDE}\" /imsvc\"${SHARED_INC}\"")

set(CMAKE_C_FLAGS_INIT   "${ARCH_FLAGS} ${INC_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS} ${INC_FLAGS} /EHsc")

set(LIB_FLAGS "/libpath:\"${MSVC_LIB}\" /libpath:\"${UCRT_LIB}\" /libpath:\"${UM_LIB}\" /manifest:no")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${LIB_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LIB_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${LIB_FLAGS}")

# Statically link the MSVC runtime (/MT) so the DLL is 100% self-contained under Wine/Proton
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
