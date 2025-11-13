/* Vita focused embedding helpers that reuse the regular desktop launcher
 * logic but expose a tiny C friendly API for homebrew loaders. */

#include "../include/avian_vita.h"

#include <jni.h>

#include <avian/system/system.h>
#include "avian/finder.h"
#include <avian/util/runtime-array.h>

#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace {

const char* detectMainClass(const char* jar)
{
  if (jar == 0) {
    return 0;
  }

  using namespace vm;

  System* system = makeSystem();

  class Allocator : public avian::util::Alloc {
   public:
    explicit Allocator(System* system) : system_(system) { }

    virtual void* allocate(size_t size)
    {
      void* p = system_->tryAllocate(size);
      if (p == 0) {
        abort(system_);
      }
      return p;
    }

    virtual void free(const void* p, size_t)
    {
      system_->free(p);
    }

   private:
    System* system_;
  } allocator(system);

  Finder* finder = makeFinder(system, &allocator, jar, 0);
  char* result = 0;

  if (finder) {
    System::Region* region = finder->find("META-INF/MANIFEST.MF");
    if (region) {
      size_t start = 0;
      size_t length = 0;
      while (readLine(region->start(), region->length(), &start, &length)) {
        const unsigned kPrefix = 12;  // "Main-Class: "
        if (length >= kPrefix
            && strncasecmp("Main-Class: ",
                            reinterpret_cast<const char*>(region->start() + start),
                            kPrefix)
                   == 0) {
          result = static_cast<char*>(malloc(length + 1 - kPrefix));
          memcpy(result,
                 region->start() + start + kPrefix,
                 length - kPrefix);
          result[length - kPrefix] = 0;
          break;
        }
        start += length;
      }
      region->dispose();
    }
    finder->dispose();
  }

  system->dispose();
  return result;
}

const char* ensureClasspathProperty(const char* classpath,
                                    JavaVMOption* option)
{
  static const char kProperty[] = "-Djava.class.path=";
  const char* cp = classpath ? classpath : ".";
  const size_t propertyLen = sizeof(kProperty) - 1;
  const size_t cpLen = strlen(cp);
  char* buffer = static_cast<char*>(malloc(propertyLen + cpLen + 1));
  memcpy(buffer, kProperty, propertyLen);
  memcpy(buffer + propertyLen, cp, cpLen + 1);
  option->optionString = buffer;
  return buffer;
}

}  // namespace

extern "C" int avian_vita_run(const AvianVitaConfig* config)
{
  if (config == 0) {
    return -1;
  }

  const char* className = config->main_class;
  const char* classpath = config->class_path ? config->class_path : ".";

  char* jarProvidedClass = 0;
  if (config->jar_path) {
    classpath = config->jar_path;
    jarProvidedClass = const_cast<char*>(detectMainClass(config->jar_path));
    className = jarProvidedClass;
  }

  if (className == 0) {
    free(jarProvidedClass);
    return -1;
  }

  JavaVMInitArgs vmArgs;
  memset(&vmArgs, 0, sizeof(vmArgs));
  vmArgs.version = JNI_VERSION_1_2;
  vmArgs.ignoreUnrecognized = JNI_TRUE;

  const size_t totalOptions = 1 + config->extra_vm_option_count;
  vmArgs.nOptions = totalOptions;
  RUNTIME_ARRAY(JavaVMOption, options, totalOptions);
  vmArgs.options = RUNTIME_ARRAY_BODY(options);

  char* classpathBuffer = const_cast<char*>(
      ensureClasspathProperty(classpath, &vmArgs.options[0]));

  for (size_t i = 0; i < config->extra_vm_option_count; ++i) {
    vmArgs.options[i + 1].optionString = const_cast<char*>(
        config->extra_vm_options[i]);
  }

  JavaVM* vm = 0;
  void* env = 0;
  if (JNI_CreateJavaVM(&vm, &env, &vmArgs) != JNI_OK) {
    free(classpathBuffer);
    free(jarProvidedClass);
    return -1;
  }

  JNIEnv* e = static_cast<JNIEnv*>(env);
  jclass c = e->FindClass(className);

  if (!e->ExceptionCheck()) {
    jmethodID m = e->GetStaticMethodID(c, "main", "([Ljava/lang/String;)V");
    if (!e->ExceptionCheck()) {
      jclass stringClass = e->FindClass("java/lang/String");
      if (!e->ExceptionCheck()) {
        const int argCount
            = (config->app_argc > 0 && config->app_argv) ? config->app_argc : 0;
        const char* const* argv
            = (argCount > 0) ? config->app_argv : 0;
        jobjectArray args = e->NewObjectArray(argCount, stringClass, 0);
        if (!e->ExceptionCheck()) {
          for (int i = 0; i < argCount; ++i) {
            e->SetObjectArrayElement(args, i, e->NewStringUTF(argv[i]));
          }
          e->CallStaticVoidMethod(c, m, args);
        }
      }
    }
  }

  int exitCode = 0;
  if (e->ExceptionCheck()) {
    exitCode = -1;
    e->ExceptionDescribe();
  }

  vm->DestroyJavaVM();
  free(classpathBuffer);
  free(jarProvidedClass);
  return exitCode;
}

extern "C" int avian_vita_run_jar(const char* jar_path,
                                   int argc,
                                   const char* const* argv)
{
  AvianVitaConfig config;
  memset(&config, 0, sizeof(config));
  config.jar_path = jar_path;
  config.app_argc = argc;
  config.app_argv = argv;
  return avian_vita_run(&config);
}

