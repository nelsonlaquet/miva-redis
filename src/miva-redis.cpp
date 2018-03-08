#include <string.h>

#include "miva-redis.h"
#include "../vendor/hiredis/hiredis.h"

extern "C" {
	/**
	* Globals
	*/
	redisContext* _connection = NULL;

	/**
	* Function Parameter Data Structures
	*/
	MV_EL_FunctionParameter redis_connect_parameters[] = {
	};

	MV_EL_FunctionParameter redis_free_parameters[] = {
	};

	void redis_connect(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			const char* error = "";
			mvProgram_FatalError(program, error, strlen(error));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}
		
		int inputLength = 0;
		const char* host = mvVariable_Value(mvVariableHash_Index(parameters, 0), &inputLength);
		int port = mvVariable_Value_Integer(mvVariableHash_Index(parameters, 1));

		_connection = redisConnect(host, port);
		if (_connection != NULL && _connection->errstr) {
			mvProgram_FatalError(program, _connection->errstr, strlen(_connection->errstr));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	void redis_free(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
	}

	EXPORT MV_EL_Function_List* miva_function_table() {
		static MV_EL_Function exported_functions[] = {
			{ "redis_connect", 13, 2, redis_connect_parameters, redis_connect },
			{ "redis_free", 10, 0, redis_free_parameters, redis_free },
			{ 0 , 0 , 0, 0 , 0 }
		};

		static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

		return &list;
	}
}