#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Small POD configuration structure used by the Vita embedding helpers.
 */
typedef struct AvianVitaConfig {
  /** Java class containing the main entry point. When @ref jar_path is set
      this may be left null to auto-detect via the manifest. */
  const char* main_class;

  /** Optional jar path whose manifest will be scanned for the Main-Class. */
  const char* jar_path;

  /** Class path passed to the VM. Defaults to '.' when unset, but is forced
      to @ref jar_path when launching a jar. */
  const char* class_path;

  /** Command line style arguments forwarded to Java's main. */
  const char* const* app_argv;
  int app_argc;

  /** Additional -X/-D VM options. */
  const char* const* extra_vm_options;
  size_t extra_vm_option_count;
} AvianVitaConfig;

/**
 * Launch Avian with a Vita-friendly configuration.
 * Returns the exit code from the Java VM.
 */
int avian_vita_run(const AvianVitaConfig* config);

/**
 * Convenience wrapper that only requires the jar path plus the application
 * arguments. All other VM options use sensible defaults.
 */
int avian_vita_run_jar(const char* jar_path,
                       int argc,
                       const char* const* argv);

#ifdef __cplusplus
}  // extern "C"
#endif

