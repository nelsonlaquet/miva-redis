#include "miva-redis.h"
#include "../vendor/hiredis/hiredis.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Function Parameter Data Structures
*/
MV_EL_FunctionParameter my_bi_function_parameters[] = {
};

MV_EL_FunctionParameter my_bi_function2_parameters[] = {
};

void redis_connect(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata) 
{
}

void redis_free(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata) 
{
}

/**
* Hook the functions into Miva
*/

EXPORT MV_EL_Function_List *miva_function_table() {
	static MV_EL_Function exported_functions[] = {
		{ "redis_connect", 14, 0, my_bi_function_parameters, my_bi_function },
		{ "redis_free", 15, 2, my_bi_function2_parameters, my_bi_function2 },
		{ 0 , 0 , 0, 0 , 0 }
	};

	static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

	return &list;
}


#ifdef __cplusplus
}
#endif
