#include "miva-redis.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Function Parameter Data Structures
*/
MV_EL_FunctionParameter my_bi_function_parameters[] = {

};

MV_EL_FunctionParameter my_bi_function2_parameters[] = {
	{ "input", 5, EPF_NORMAL },
	{ "output", 6, EPF_REFERENCE },
};

void my_bi_function(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata) 
{
	mvVariable_SetValue_Integer( returnvalue, 5 );
}

void my_bi_function2(mvProgram prog, mvVariableHash parameters, mvVariable returnvalue, void ** pdata) 
{
	int input_length = 0;
	
	const char *input = mvVariable_Value( mvVariableHash_Index( parameters, 0 ), &input_length );

	mvVariable output = mvVariableHash_Index( parameters, 1 );

	mvVariable_SetValue( output, input, input_length );

	mvVariable_SetValue_Integer( returnvalue, 1 );
}


/**
* Hook the functions into Miva
*/

EXPORT MV_EL_Function_List *miva_function_table() {
	static MV_EL_Function exported_functions[] = {
		{ "my_bi_function", 14, 0, my_bi_function_parameters, my_bi_function },
		{ "my_bi_function2", 15, 2, my_bi_function2_parameters, my_bi_function2 },
		{ 0 , 0 , 0, 0 , 0 }
	};

	static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

	return &list;
}


#ifdef __cplusplus
}
#endif
