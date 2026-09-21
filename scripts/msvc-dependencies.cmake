# O MSVC pode emitir UTF-8 em pipes enquanto CMake AUTO presume a codepage OEM.
# Ler os bytes reais evita depender de um pacote de idioma ingles instalado.
if(MSVC AND CMAKE_GENERATOR MATCHES "^Ninja")
  set(_probe_dir "${CMAKE_BINARY_DIR}/CMakeFiles/DependencyPrefix")
  file(MAKE_DIRECTORY "${_probe_dir}")
  file(WRITE "${_probe_dir}/dependency.hpp" "\n")
  file(WRITE "${_probe_dir}/probe.cpp" "#include \"dependency.hpp\"\n")
  execute_process(COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /utf-8 /c probe.cpp
    WORKING_DIRECTORY "${_probe_dir}" OUTPUT_VARIABLE _output ERROR_VARIABLE _error
    RESULT_VARIABLE _result ENCODING NONE)
  file(TO_NATIVE_PATH "${_probe_dir}/dependency.hpp" _header)
  string(REGEX MATCH "[^\r\n]*dependency\\.hpp" _line "${_output}")
  string(FIND "${_line}" "${_header}" _path_start)
  if(NOT _result EQUAL 0 OR _path_start LESS 1)
    message(FATAL_ERROR "Nao foi possivel detectar dependencias MSVC: ${_error}")
  endif()
  string(SUBSTRING "${_line}" 0 ${_path_start} CMAKE_CL_SHOWINCLUDES_PREFIX)
  set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_CL_SHOWINCLUDES_PREFIX}")
endif()
