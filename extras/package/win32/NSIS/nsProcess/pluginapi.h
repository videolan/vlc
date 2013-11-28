#ifndef ___NSIS_PLUGIN__H___
#define ___NSIS_PLUGIN__H___

#ifdef __cplusplus
extern "C" {
#endif

#include "api.h"
#include "nsis_tchar.h"

#ifndef NSISCALL
#  define NSISCALL __stdcall
#endif

#define EXDLL_INIT()           {  \
        g_stringsize=string_size; \
        g_stacktop=stacktop;      \
        g_variables=variables; }

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[1]; // this should be the length of string_size
} stack_t;

enum
{
INST_0,         // $0
INST_1,         // $1
INST_2,         // $2
INST_3,         // $3
INST_4,         // $4
INST_5,         // $5
INST_6,         // $6
INST_7,         // $7
INST_8,         // $8
INST_9,         // $9
INST_R0,        // $R0
INST_R1,        // $R1
INST_R2,        // $R2
INST_R3,        // $R3
INST_R4,        // $R4
INST_R5,        // $R5
INST_R6,        // $R6
INST_R7,        // $R7
INST_R8,        // $R8
INST_R9,        // $R9
INST_CMDLINE,   // $CMDLINE
INST_INSTDIR,   // $INSTDIR
INST_OUTDIR,    // $OUTDIR
INST_EXEDIR,    // $EXEDIR
INST_LANG,      // $LANGUAGE
__INST_LAST
};

extern unsigned int g_stringsize;
extern stack_t **g_stacktop;
extern TCHAR *g_variables;

int NSISCALL popstring(TCHAR *str); // 0 on success, 1 on empty stack
int NSISCALL popstringn(TCHAR *str, int maxlen); // with length limit, pass 0 for g_stringsize
int NSISCALL popint(); // pops an integer
int NSISCALL popint_or(); // with support for or'ing (2|4|8)
int NSISCALL myatoi(const TCHAR *s); // converts a string to an integer
unsigned NSISCALL myatou(const TCHAR *s); // converts a string to an unsigned integer, decimal only
int NSISCALL myatoi_or(const TCHAR *s); // with support for or'ing (2|4|8)
void NSISCALL pushstring(const TCHAR *str);
void NSISCALL pushint(int value);
TCHAR * NSISCALL getuservariable(const int varnum);
void NSISCALL setuservariable(const int varnum, const TCHAR *var);

#ifdef _UNICODE
#define PopStringW(x) popstring(x)
#define PushStringW(x) pushstring(x)
#define SetUserVariableW(x,y) setuservariable(x,y)

int  NSISCALL PopStringA(char* ansiStr);
void NSISCALL PushStringA(const char* ansiStr);
void NSISCALL GetUserVariableW(const int varnum, wchar_t* wideStr);
void NSISCALL GetUserVariableA(const int varnum, char* ansiStr);
void NSISCALL SetUserVariableA(const int varnum, const char* ansiStr);

#else
// ANSI defs

#define PopStringA(x) popstring(x)
#define PushStringA(x) pushstring(x)
#define SetUserVariableA(x,y) setuservariable(x,y)

int  NSISCALL PopStringW(wchar_t* wideStr);
void NSISCALL PushStringW(wchar_t* wideStr);
void NSISCALL GetUserVariableW(const int varnum, wchar_t* wideStr);
void NSISCALL GetUserVariableA(const int varnum, char* ansiStr);
void NSISCALL SetUserVariableW(const int varnum, const wchar_t* wideStr);

#endif

#ifdef __cplusplus
}
#endif

#endif//!___NSIS_PLUGIN__H___
