# This is the source directory. We create two binaries, one for the client and one for the server.

# Use add_bin(name MORE_FILES) requires a file of name `src/name.c` to exist.
function(add_bin name)
  add_executable(${name} ${name}.c ${ARGN})
  target_compile_options(${name} PUBLIC -g -O2 -Werror -Wall -Wextra)
  if (NOT APPLE)
    target_compile_options(${name} PUBLIC
                             -fsanitize=address
                             -fno-omit-frame-pointer)
  endif()
endfunction()

add_bin(client)
add_bin(server)

