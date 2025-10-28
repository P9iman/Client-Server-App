# This is the source directory. We create two binaries, one for the client and one for the server.

# Use add_bin(name MORE_FILES) requires a file of name `src/name.c` to exist.
function(add_bin name)
  add_executable(${name} ${name}.c ${ARGN})
  target_compile_options(${name} PUBLIC -g -O2 -Wall) #-Werror -Wextra)
  if (NOT APPLE)
    target_compile_options(${name} PUBLIC
                             -fsanitize=address
                             -fno-omit-frame-pointer)
  endif()
endfunction()

add_bin(client)
add_bin(server)

# Copy Data/ into the runtime output directory of the client (and server if wanted)
add_custom_command(TARGET client POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:client>/ClientData
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/ClientData $<TARGET_FILE_DIR:client>/ClientData
  COMMENT "Copying Data/ to runtime output directory for client"
)

