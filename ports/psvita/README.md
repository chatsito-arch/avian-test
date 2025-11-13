# PlayStation Vita port

The Vita does not support executable heap pages which makes Avian's JIT and
AOT modes a non-starter.  The new `ports/psvita` helpers configure Avian in an
interpreter-only mode, build it with the Vita SDK toolchain and provide two
small glue layers:

* `src/vitasys.c` contains the POSIX shim that keeps the interpreter happy.
* `src/avian_embed.cpp` wraps the traditional `src/main.cpp` launcher in a
  Vita friendly `avian_vita_run`/`avian_vita_run_jar` C API.

The intent is to let you link `libavian.a` into an existing Vita homebrew
without having to fork Avian's primary build logic.

## 1. Prerequisites

```text
brew tap vitasdk/vitasdk
brew install vitasdk
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

Verify that the toolchain is on the path:

```text
arm-vita-eabi-gcc --version
arm-vita-eabi-ar --version
```

## 2. Build Avian for Vita

The Vita build simply reuses the regular `make` build while overriding the
compilers and adding a few `CFLAGS`.  The helper script below is intentionally
minimal – copy it somewhere on your system and adjust the paths if your setup
is non-standard.

```bash
#!/usr/bin/env bash
set -euo pipefail
: "${VITASDK:?please export VITASDK=/path/to/vitasdk}"
ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
export CC=arm-vita-eabi-gcc
export CXX=arm-vita-eabi-g++
export AR=arm-vita-eabi-ar
export RANLIB=arm-vita-eabi-ranlib
export STRIP=arm-vita-eabi-strip
VITA_FLAGS="-march=armv7-a -mfpu=neon -fPIC -O2 -DAVIAN_NO_JIT -DNO_POSIX_SIGNALS"
make -C "$ROOT_DIR" clean
make -C "$ROOT_DIR" arch=arm platform=linux process=interpret \
  extra-cflags="$VITA_FLAGS" extra-cxxflags="$VITA_FLAGS" \
  extra-ldflags="-L$VITASDK/arm-vita-eabi/lib" \
  extra-libraries="-lpthread"
```

The `process=interpret` flag keeps the runtime away from `mprotect` and other
APIs that are unavailable on Vita.

After the build finishes grab the resulting static archive plus headers:

```text
cp build/linux-arm-interpret-fast/libavian.a ports/psvita/lib/libavian.a
cp -R include ports/psvita/include/avian
```

(You may keep the library anywhere; the path above simply mirrors this guide.)

## 3. Compile the Vita shim

The POSIX shim and embedding helpers are just two files and compile quickly.
Add them to your Vita CMake project or use the provided snippet:

```text
arm-vita-eabi-g++ -I../../include -I../../ports/psvita/include \
  -DAVIAN_NO_JIT -DNO_POSIX_SIGNALS -c ports/psvita/src/avian_embed.cpp -o avian_embed.o
arm-vita-eabi-gcc -I../../include -c ports/psvita/src/vitasys.c -o vitasys.o
arm-vita-eabi-ar rcs libavian_vita.a avian_embed.o vitasys.o
```

Link `libavian_vita.a` together with the Avian archive produced earlier.

## 4. Minimal usage example

```c
#include <psp2/kernel/processmgr.h>
#include "ports/psvita/include/avian_vita.h"

int main(int argc, char* argv[])
{
    const char* javaArgs[] = {"vita", "port"};
    AvianVitaConfig config = {0};
    config.main_class = "Hello";
    config.class_path = "app0:hello.jar"; // packaged inside the VPK
    config.app_argc = 2;
    config.app_argv = javaArgs;
    return avian_vita_run(&config);
}
```

If you prefer packaging a jar, call `avian_vita_run_jar("app0:hello.jar", 0,
NULL)` and the helper will parse `META-INF/MANIFEST.MF` to determine the entry
point automatically.

The `sample` directory contains a minimal `Hello.java` you can use while
bringing up the Vita port:

```text
cd ports/psvita/sample
javac Hello.java
jar cf hello.jar Hello.class
```

Drop the generated `hello.jar` into your VPK under `app0:` and reference it
from the example program above.

## 5. Debugging tips

* Keep the heap size small (`-Xmx32m`) when running on real hardware.
* The stubbed `mmap` implementation only supports RW data; enabling the JIT
  requires a Vita-specific executable memory allocator.
* Use `psp2shell` or a UART adapter for logs – `printf` works fine from the
  interpreter build.
* JNI is supported, but every native method must be linked into your Vita
  binary.  The interpreter build is easiest when you rely on Java code only.

