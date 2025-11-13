# Using libavian on custom Vita homebrew

This document walks through the minimum pieces you need to glue Avian's Vita
interpreter build into an existing VPK project.  The steps assume you have
already produced `libavian.a` with `process=interpret` and compiled the Vita
shims (`avian_embed.cpp` / `vitasys.c`) from `ports/psvita/src`.

## 1. Copy the artifacts into your project

```
lib/
  libavian.a              # produced by the cross-make
  libavian_vita.a         # archive containing avian_embed + vitasys
include/
  avian/                  # copy of the upstream include tree
  ports/psvita/include/   # contains avian_vita.h and helper structs
```

Keeping the files under `lib/` and `include/` lets both the Vita CMake toolchain
and `vita-makepkg` discover them without extra boilerplate, but any layout will
work as long as your build scripts point to it.

## 2. Add the interpreter to your build (CMake example)

```cmake
# Vita CMakeLists.txt snippet
set(AVIAN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/avian)
include_directories(${AVIAN_ROOT}/include)
include_directories(${AVIAN_ROOT}/ports/psvita/include)
add_executable(myhomebrew
    src/main.c
    # ...
)
# libavian_vita pulls in the Vita-facing glue while libavian carries the VM
# itself.  Order matters on the Vita toolchain, so keep libavian last.
target_link_libraries(myhomebrew
    ${AVIAN_ROOT}/ports/psvita/lib/libavian_vita.a
    ${AVIAN_ROOT}/ports/psvita/lib/libavian.a
    pthread
    m
)
```

If you are using the `vita-makepkg` templates that rely on raw `arm-vita-eabi-`
commands, add the same include paths and libraries to your compile/link steps.

## 3. Run Java code from your entry point

```c
#include <psp2/kernel/processmgr.h>
#include "avian_vita.h"

int main(int argc, char** argv)
{
    AvianVitaConfig cfg = {0};
    cfg.main_class = "com/example/Hello";  // slash-separated name
    cfg.class_path = "app0:hello.jar";     // packaged inside your VPK
    cfg.vm_argc = 2;
    const char* vm_args[] = {"-Xms16m", "-Xmx32m"};
    cfg.vm_argv = vm_args;
    cfg.app_argc = 1;
    const char* app_args[] = {"vita"};
    cfg.app_argv = app_args;

    int rc = avian_vita_run(&cfg);
    sceKernelExitProcess(rc);
    return rc;
}
```

`avian_vita_run_jar("app0:hello.jar", 0, NULL)` is a convenience helper that
extracts the `Main-Class` value from `META-INF/MANIFEST.MF` before dispatching to
`avian_vita_run`.

## 4. Packaging your Java payload

Any `.class` or `.jar` file produced by a desktop `javac`/`jar` works.  Drop the
payload under `app0:` inside the VPK – the Vita SDK CMake toolchain exposes a
macro named `vita_add_embedded_file` that can bundle resources directly inside
your executable:

```cmake
vita_add_embedded_file(myhomebrew OUT_VAR app0_hello_jar hello.jar)
vita_create_self(myhomebrew.self myhomebrew
    CONFIG exports.yml
    UNSAFE
    STRIPPED
    EMBED ${app0_hello_jar}
)
```

The embedded file shows up at runtime as `app0:hello.jar`, so you can keep the
paths in `AvianVitaConfig` consistent between emulators and real hardware.

## 5. JNI and Android-specific code

* **Standard JNI**: Avian implements the JNI surface, so any `native` Java
  method can call back into the Vita binary as long as you provide the native
  function and register it (either via `RegisterNatives` or the traditional
  naming convention).  Compile your native code with the Vita toolchain and
  link it into the same ELF – the interpreter does not support loading `.so`
  files dynamically.
* **Android APIs**: Avian does not ship the Android framework or Dalvik/ART
  services.  Java code that imports packages such as `android.os.*` or
  `android.media.*` will only run if you bundle stubs of those classes and
  re-implement their functionality yourself.  In practice this means that
  Android applications relying on system services cannot run unmodified; only
  plain Java libraries or code that talks to your own JNI layer are realistic.

When porting Android-oriented codebases, it is common to replace the Android
entry point with a small Java shim that forwards into the classes you actually
need, while rewriting the `android.*` interactions to call Vita-specific JNI
helpers.

## 6. Troubleshooting

* Double-check that you built Avian with `process=interpret`; any JIT/AOT build
  will attempt to allocate executable heap pages and crash on Vita firmware.
* `UnsatisfiedLinkError` at runtime means a `native` method is missing – make
  sure you linked your native object file into the final Vita ELF.
* If you see `ClassNotFoundException`, log the class path using
  `avian_vita_run`'s return codes and verify that the `.jar` path uses Vita's
  `app0:`/`ux0:` prefixes instead of POSIX-style directories.

That is all the glue required to host Avian inside a custom Vita homebrew.
