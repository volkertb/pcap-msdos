/*
 * DLX Dynamic Loading and eXecution V2.91
 * Copyright (c) 1997-1998, Nanosoft, Inc.
 */
#ifndef __DLX_INCLUDED_2_
#define __DLX_INCLUDED_2_

struct dlxheader {
       long magic;
       long numimports;
       long numrelocs;
       long libmainpos;
       long extablepos;
       long libloadpos;
       long prgsize;
     };

typedef uint64 hdlx_t;

struct dlxiddesc {   /* applies only to V2.0 DLX */
       hdlx_t MFID;  /* manufacturer ID */
       hdlx_t PRID;  /* product ID */
       hdlx_t VTBL;  /* version description ID */
       hdlx_t UNID;  /* unique DLX ID */
     };

#ifdef __cplusplus
extern "C" {
#endif

  void (*dlx_first_ctor[])() __asm__("dlx_first_ctor");
  void (*dlx_last_ctor[])()  __asm__("dlx_last_ctor");
  void (*dlx_first_dtor[])() __asm__("dlx_first_dtor");
  void (*dlx_last_dtor[])()  __asm__("dlx_last_dtor");
  unsigned __djgpp_stack_limit;

#ifdef __cplusplus  /* the loader application MUST be compiled as C++ */
  extern "C++" {

    void * (*DLXOpenFile)(char*);
    void   (*DLXCloseFile)(void*);
    void   (*DLXReadFile)(void*,long,void*);

    void   (*DLXError)(long,char*);
    hdlx_t (*DLXGetID)(char*);

    void*  (*DLXMalloc)(unsigned long);
    void   (*DLXFree)(void*);
    void   (*DLXRealloc)(void*,unsigned long);

    hdlx_t  DLXLoad (char* name);
    void    DLXUnload (hdlx_t handle);
    void    DLXImport (char** symbols);
    void   *DLXGetEntry (hdlx_t target, char* name);
    void   *DLXGetMemoryBlock (hdlx_t target);
    long    DLXGetMemoryBlockLength (hdlx_t target);
  }
}
#endif /* __cplusplus */

#ifdef __cplusplus
  #define DLXUSE_BEGIN  extern "C" {
  #define DLXUSE_END    }
  #define DLX_FN        extern "C" {
  #define DLX_EF        }
  #define DLX_IMPORT    extern "C" {
  #define DLX_ENDIMPORT }
#else
  #define DLXUSE_BEGIN
  #define DLXUSE_END
  #define DLX_FN
  #define DLX_EF
  #define DLX_IMPORT
  #define DLX_ENDIMPORT
#endif

#define DLX_MAGIC         0x584c44
#define DLX2_MAGIC        0x32584c44
#define DLX_BUILD_DOS     0x534f444d4249LL

#define LIBLOADS_BEGIN    char* _LIBTOLOAD[]={
#define LIBLOADS_END      "\0\0"};
#define LIBEXPORT_BEGIN   char* _LIBEXPORTTABLE[]= {
#define LIBEXPORT_END     0, 0 };
#define LIBEXPORT(x)      "_"#x, (char*)&x,
#define LIBALIAS(x,y)     "_"#x, (char*)&y,
#define LIBENTRY(x)       (char*)0L, (char*)1L,    #x, (char*)&x,
#define LIBWEAK(x)        (char*)0L, (char*)2L, "_"#x, (char*)&x,
#define LIBDPLUG(x)       (char*)0L, (char*)3L, "_"#x, (char*)&x,  /* not impl. */

#define LIBLOAD(x)        #x,
#define LIBVERSION_BEGIN  uint64 _DLXVERSIONTABL[] = {
#define LIBVERSION_END    0,0,0,0 };
#define LIBVERSION(x)     (uint64)(x),
#define LIBMYHANDLE       _DLXVERSIONTABL[3]

#define LIBCONSTRUCT() do { \
        int i; \
        for (i = 0; i < dlx_last_ctor - dlx_first_ctor; i++) \
            dlx_first_ctor[i](); \
      } while (0)

#define LIBDESTRUCT() do { \
        int i; \
        for (i = 0; i < dlx_last_dtor - dlx_first_dtor; i++) \
            dlx_first_dtor[i](); \
      } while (0)

#endif
