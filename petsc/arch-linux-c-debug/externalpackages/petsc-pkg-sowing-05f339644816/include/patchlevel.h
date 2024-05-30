#define PATCHLEVEL 1
#define PATCHLEVEL_MINOR 26
#define PATCHLEVEL_SUBMINOR 10
#define PATCHLEVEL_RELEASE_KIND "alpha"
#ifndef PATCHLEVEL_RELEASE_DATE
#ifdef RELEASE_DATE
#define PATCHLEVEL_RELEASE_DATE RELEASE_DATE
#else
#define PATCHLEVEL_RELEASE_DATE "Date unknown"
#endif
#endif
#ifndef CONFIGURE_OPTIONS
#define CONFIGURE_OPTIONS " '--prefix=/test/toolkit/NLA/petsc/arch-linux-c-debug' 'CPPFLAGS=-O2'"
#endif
