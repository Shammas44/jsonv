#ifndef MACRO_H
#define MACRO_H

#ifndef JSONV_API
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define JSONV_API __attribute__((visibility("default")))
  #else
    #define JSONV_API
  #endif
#endif

#endif
