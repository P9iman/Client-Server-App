# RN Client-server

## Build

To build the binaries please use the following commands:

```bash
mkdir build
cd build
cmake ..
make -j 1
```
The compiled binaries will then be located in `build/bin/`.

## Overview

This repository contains a small TCP client–server file service.

- Server: a file-service module that accepts client connections and exposes basic file operations. It tracks connected clients and makes a server-side storage location available to clients. See `src/server.c` for function-level descriptions (`@brief` comments).
- Client: a CLI client that connects to the server and issues textual commands to request file lists, download or upload files, and query connected clients. See `src/client.c` for function-level descriptions (`@brief` comments).

Commands (names only)
- Get
- Put
- Files
- List
- Quit




Notes
- The source files (`src/server.c`, `src/client.c`) contain concise `@brief` comments describing the intended behavior of the main components and functions.