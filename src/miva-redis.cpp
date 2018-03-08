#include <string.h>

#include "miva-redis.h"
#include "../vendor/hiredis/hiredis.h"

extern "C" {
	/**
	* Globals
	*/
	redisContext* _connection = NULL;

	/**
	* Helpers
	*/
	void formatRedisReply(redisReply* reply, mvVariable outputVar) {
		if (reply->type == REDIS_REPLY_STATUS) {
			mvVariable_SetValue(outputVar, reply->str, reply->len);
		} else if (reply->type == REDIS_REPLY_INTEGER) {
			mvVariable_SetValue_Integer(outputVar, reply->integer);
		} else if (reply->type == REDIS_REPLY_NIL) {
			// do nothing...
		} else if (reply->type == REDIS_REPLY_STRING) {
			mvVariable_SetValue(outputVar, reply->str, reply->len);
		} else if (reply->type == REDIS_REPLY_ARRAY) {
			const char* varName = "redisreply";
			for (int i = 0; i < reply->elements; i++) {
				mvVariable arrVar = mvVariable_Allocate(varName, strlen(varName), "", 0);
				formatRedisReply(reply->element[i], arrVar);
				mvVariable_Set_Array_Element(i, outputVar, arrVar);
			}
		} else if (reply->type == REDIS_REPLY_ERROR) {
			mvVariable_SetValue(outputVar, reply->str, reply->len);
		}
	}

	/**
	* Function Parameter Data Structures
	*/
	MV_EL_FunctionParameter redis_connect_parameters[] = {
		{ "host", 4, EPF_NORMAL },
		{ "port", 4, EPF_NORMAL }
	};

	MV_EL_FunctionParameter redis_free_parameters[] = {
	};

	MV_EL_FunctionParameter redis_command_parameters[] = {
		{ "command", 7, EPF_NORMAL }
	};

	void redis_connect(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			const char* error = "Already Connected!";
			mvProgram_FatalError(program, error, strlen(error));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}
		
		int hostLength = 0;
		const char* host = mvVariable_Value(mvVariableHash_Index(parameters, 0), &hostLength);
		int port = mvVariable_Value_Integer(mvVariableHash_Index(parameters, 1));

		_connection = redisConnect(host, port);
		if (_connection != NULL && _connection->err) {
			mvProgram_FatalError(program, _connection->errstr, strlen(_connection->errstr));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	void redis_free(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			redisFree(_connection);		
			_connection = NULL;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	void redis_command(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			const char* error = "Not connected! Use redis_connect!";
			mvProgram_FatalError(program, error, strlen(error));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		int commandLength = 0;
		const char* command = mvVariable_Value(mvVariableHash_Index(parameters, 0), &commandLength);
		redisReply* reply = (redisReply*) redisCommand(_connection, command);
		if (reply == NULL) {
			mvProgram_FatalError(program, _connection->errstr, strlen(_connection->errstr));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}
		
		formatRedisReply(reply, returnValue);
		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}

	EXPORT MV_EL_Function_List* miva_function_table() {
		static MV_EL_Function exported_functions[] = {
			{ "redis_connect", 13, 2, redis_connect_parameters, redis_connect },
			{ "redis_free", 10, 0, redis_free_parameters, redis_free },
			{ "redis_command", 13, 1, redis_command_parameters, redis_command },
			{ 0 , 0 , 0, 0 , 0 }
		};

		static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

		return &list;
	}
}