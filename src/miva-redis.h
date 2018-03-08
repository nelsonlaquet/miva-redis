#ifndef __miva_redis
#define __miva_redis

#include "include/mivaapi.h"

#ifdef WIN32
	#define IMPORT __declspec(dllimport)
	#define EXPORT __declspec(dllexport)
#else
	#define IMPORT 
	#define EXPORT 
#endif

#ifdef __cplusplus
extern "C" {
#endif

void my_bi_function(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata);
void my_bi_function2(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata);

EXPORT MV_EL_Function_List *miva_function_table();

#ifdef __cplusplus
}
#endif

#endif