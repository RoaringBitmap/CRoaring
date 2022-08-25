# Based on github.com/simdjson/simdjson/blob/master/dependencies/import.cmocka by @friendlyanon

set(dep_root "${CMAKE_CURRENT_SOURCE_DIR}/.cache")

function(import_dependency NAME URL)
  message(STATUS "Importing ${NAME} (${URL})")
  set(target "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}")

  # If the folder exists in the cache, then we assume that everything is as
  # should be and do nothing
  if(EXISTS "${target}")
    set("${NAME}_SOURCE_DIR" "${target}" PARENT_SCOPE)
    return()
  endif()

  set(archive "${dep_root}/archive.tar.xz")
  set(dest "${dep_root}/_extract")

  file(DOWNLOAD "${URL}" "${archive}")
  file(MAKE_DIRECTORY "${dest}")
  file(GLOB dir LIST_DIRECTORIES YES "${dep_root}/*")
  execute_process(
          WORKING_DIRECTORY "${dest}"
          COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}")
  file(REMOVE "${archive}")

  # GitHub archives only ever have one folder component at the root, so this
  # will always match that single folder
  file(GLOB dir LIST_DIRECTORIES YES "${dest}/*")

  file(RENAME "${dir}" "${target}")

  set("${NAME}_SOURCE_DIR" "${target}" PARENT_SCOPE)
endfunction()
