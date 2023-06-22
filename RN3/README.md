# RN Client-server Template

This is the template for Rechnernetze exercise 3. Feel free to adjust the template to your liking but keep `-Wall -Werror` enabled as well as the address sanitizer.


## Build

To build the binaries please use the following commands:

```bash
$ mkdir build
$ cd build
$ cmake ..
$ make -j 1
```

The compiled binaries will then be located in `build/bin/`.

When handing in your homework assignement please create a zip archive of the src folder, CMakeLists.txt, the README (extended with additonal instructions on how to run your programs), as well as additional files or folders you created while programming.

## Tips

The `clang-format` tool is great to clean up your code. You can use it to format a file as follows:

```bash
$ # Format a specific file.
$ clang-format -i  --style=Google FILENAME 
$ # Format all .c files in the src folder.
$ clang-format -i  --style=Google src/*.c 
```
