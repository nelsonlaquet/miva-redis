#include <sstream>
#include <string>
#include <string.h>
#include <vector>

#include "miva-redis.h"

using std::vector;
using std::string;
using std::stringstream;

const int ERROR_ALREADY_CONNECTED = 1;
const int ERROR_CONNECT_ERROR = 2;
const int ERROR_NOT_CONNECTED = 3;
const int ERROR_MALFORMED_COMMAND = 4;
const int ERROR_COMMAND = 5;

const char* getVariableFromLists(mvVariableList localVars, mvVariableList globalVars, const string& variableName);

extern "C" {
	#include "../vendor/hiredis/hiredis.h"
	#include "../vendor/hiredis/sds.h"

	/**
	* Globals
	*/
	redisContext* _connection = NULL;
	string _lastRedisError;
	int _lastRedisErrorCode = 0;
	int _redisAppendStackSize = 0;

	/**
	* Helpers
	*/
	void formatRedisReply(redisReply* reply, mvVariable outputVar) {
		mvVariable typeVar = mvVariable_Allocate("type", 4, "", 0);
		mvVariable_SetValue_Integer(typeVar, reply->type);
		mvVariable_Set_Struct_Member("type", 4, typeVar, outputVar);

		if (reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_ERROR) {
			mvVariable stringVar = mvVariable_Allocate("string", 6, reply->str, reply->len);
			mvVariable_Set_Struct_Member("string", 6, stringVar, outputVar);
		} else if (reply->type == REDIS_REPLY_INTEGER) {
			mvVariable intVar = mvVariable_Allocate("string", 6, reply->str, reply->len);
			mvVariable_SetValue_Integer(intVar, reply->integer);
			mvVariable_Set_Struct_Member("string", 6, intVar, outputVar);			
		} else if (reply->type == REDIS_REPLY_ARRAY) {
			const char* varName = "redisreply";
			for (int i = 0; i < reply->elements; i++) {
				mvVariable arrVar = mvVariable_Allocate(varName, strlen(varName), "", 0);
				formatRedisReply(reply->element[i], arrVar);
				mvVariable_Set_Array_Element(i, outputVar, arrVar);
			}
		}
	}

	void setRedisError(int code, const string& error, mvProgram program, mvVariable returnValue) {
		_lastRedisError = error;
		_lastRedisErrorCode = code;
		mvVariable_SetValue_Integer(returnValue, 0);
		return;
	}

	bool parseRedisArgs(mvProgram program, mvVariable returnValue, const char* command, int commandLength, const char* args, int argsLength, vector<const char*>& argv) {
		if (commandLength == 0) {
			setRedisError(ERROR_MALFORMED_COMMAND, "Command must be specified!", program, returnValue);
			return false;
		}

		// Extract all parts of the command into an array
		int commandPartCount;
		sds* commandParts = sdssplitlen(command, commandLength, " ", 1, &commandPartCount);
		for (int i = 0; i < commandPartCount; i++)
			sdstrim(commandParts[i], " ");

		// Count substitions...
		int variableSubCount = 0;
		for (int i = 0; i < commandPartCount; i++) {
			if (strcmp(commandParts[i], "?") == 0)
				variableSubCount++;
		}

		// extract all variables from the args parameter
		int varNameCount;
		sds* varNames = sdssplitlen(args, argsLength, ",", 1, &varNameCount);
		for (int i = 0; i < varNameCount; i++)
			sdstrim(varNames[i], " ");
		
		if (varNameCount != variableSubCount) {
			stringstream ss;
			ss << "Redis command '" << command << "' has " << variableSubCount << " variable substitutions, but you provided " << varNameCount << " variables!";
			setRedisError(ERROR_MALFORMED_COMMAND, ss.str(), program, returnValue);
			return false;
		}

		// Load in all varaibles
		mvVariableList 
			localVars = mvVariableList_Allocate(),
			globalVars = mvVariableList_Allocate();

		mvProgram_Local_Variables(program, localVars);
		mvProgram_Global_Variables(program, globalVars);

		// Combine the var parts with the command parts
		for (int i = 0, varI = 0; i < commandPartCount; i++) {
			sds commandPart = commandParts[i];
			if (strcmp(commandPart, "?") != 0) {
				argv.push_back(commandPart);
			} else {
				argv.push_back(getVariableFromLists(localVars, globalVars, varNames[varI++]));
			}
		}

		mvVariableList_Free(localVars);
		mvVariableList_Free(globalVars);

		return true;
	}

	/**
	 * -----------------------------------------
	 * redis_last_error
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_error_parameters[] = {
		{ "message", 7, EPF_REFERENCE }
	};
	void redis_error(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		mvVariable_SetValue_Integer(returnValue, _lastRedisErrorCode);
		
		if (_lastRedisErrorCode != 0) {
			mvVariable_SetValue(mvVariableHash_Index(parameters, 0), _lastRedisError.c_str(), _lastRedisError.size());
		}
	}

	/**
	 * -----------------------------------------
	 * redis_clear_error
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_error_clear_parameters[] = {
	};
	void redis_error_clear(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		_lastRedisError = "";
		_lastRedisErrorCode = 0;
	}

	/**
	 * -----------------------------------------
	 * redis_connect
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_connect_parameters[] = {
		{ "host", 4, EPF_NORMAL },
		{ "port", 4, EPF_NORMAL }
	};
	void redis_connect(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			setRedisError(ERROR_ALREADY_CONNECTED, "You are already connected!", program, returnValue);
			return;
		}
		
		int hostLength = 0;
		const char* host = mvVariable_Value(mvVariableHash_Index(parameters, 0), &hostLength);
		int port = mvVariable_Value_Integer(mvVariableHash_Index(parameters, 1));

		_connection = redisConnect(host, port);
		if (_connection != NULL && _connection->err) {
			setRedisError(ERROR_CONNECT_ERROR, _connection->errstr, program, returnValue);
			return;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	/**
	 * -----------------------------------------
	 * redis_free
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_free_parameters[] = {
	};
	void redis_free(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			redisFree(_connection);		
			_connection = NULL;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	/**
	 * -----------------------------------------
	 * redis
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_command_parameters[] = {
		{ "command", 7, EPF_NORMAL },
		{ "args", 4, EPF_NORMAL }
	};
	void redis_command(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int commandLength = 0;
		const char* command = mvVariable_Value(mvVariableHash_Index(parameters, 0), &commandLength);

		int argsLength = 0;
		const char* args = mvVariable_Value(mvVariableHash_Index(parameters, 1), &argsLength);

		vector<const char*> argv;
		if (!parseRedisArgs(program, returnValue, command, commandLength, args, argsLength, argv)) {
			return;
		}
		
		if (argv.size() == 0) {
			setRedisError(ERROR_MALFORMED_COMMAND, "Blank command?!", program, returnValue);
			return;
		}
		
		// invoke command!
		redisReply* reply = (redisReply*)redisCommandArgv(_connection, argv.size(), &argv[0], NULL);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}
		
		formatRedisReply(reply, returnValue);
		freeReplyObject(reply);
	}

	/**
	 * -----------------------------------------
	 * redis_append
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_command_append_parameters[] = {
		{ "command", 7, EPF_NORMAL },
		{ "args", 4, EPF_NORMAL }
	};
	void redis_command_append(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int commandLength = 0;
		const char* command = mvVariable_Value(mvVariableHash_Index(parameters, 0), &commandLength);

		int argsLength = 0;
		const char* args = mvVariable_Value(mvVariableHash_Index(parameters, 1), &argsLength);

		vector<const char*> argv;
		if (!parseRedisArgs(program, returnValue, command, commandLength, args, argsLength, argv)) {
			return;
		}
		
		// Append command to be invoked...
		redisAppendCommandArgv(_connection, argv.size(), &argv[0], NULL);
		_redisAppendStackSize++;
	}
	
	/**
	 * -----------------------------------------
	 * redis_get_reply
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_get_reply_parameters[] = {
		{ "reply", 5, EPF_REFERENCE }
	};
	void redis_get_reply(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		if (_redisAppendStackSize == 0) {
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		_redisAppendStackSize--;

		redisReply* reply;
		if (redisGetReply(_connection, (void**)&reply) != REDIS_OK) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply) {
			formatRedisReply(reply, mvVariableHash_Index(parameters, 0));
			freeReplyObject(reply);
		}

		mvVariable_SetValue_Integer(returnValue, _redisAppendStackSize + 1);
	}
	
	/**
	 * -----------------------------------------
	 * Redis Command: GET
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_get_parameters[] = { { "key", 3, EPF_NORMAL }, { "ret", 3, EPF_REFERENCE } };
	void redis_get(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int keyLength = 0;
		const char* key = mvVariable_Value(mvVariableHash_Index(parameters, 0), &keyLength);
		redisReply* reply = (redisReply*)redisCommand(_connection, "GET %s", key);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}

		if (reply->type == REDIS_REPLY_NIL) {
			mvVariable_SetValue_Integer(returnValue, -1);
			return;
		}

		if (reply->type != REDIS_REPLY_STRING) {
			setRedisError(ERROR_COMMAND, "Redis did not return with the proper type REDIS_REPLY_STRING", program, returnValue);
			freeReplyObject(reply);
			return;
		}

		mvVariable_SetValue(mvVariableHash_Index(parameters, 1), reply->str, reply->len);
		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}
	
	/**
	 * -----------------------------------------
	 * Redis Command: DEL
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_del_parameters[] = { { "key", 3, EPF_NORMAL } };
	void redis_del(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int keyLength = 0;
		const char* key = mvVariable_Value(mvVariableHash_Index(parameters, 0), &keyLength);
		redisReply* reply = (redisReply*)redisCommand(_connection, "DEL %s", key);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}

		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}
	
	/**
	 * -----------------------------------------
	 * Redis Command: SET
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_set_parameters[] = { { "key", 3, EPF_NORMAL }, { "value", 5, EPF_REFERENCE } };
	void redis_set(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int keyLength = 0;
		const char* key = mvVariable_Value(mvVariableHash_Index(parameters, 0), &keyLength);
		
		int valueLength = 0;
		const char* value = mvVariable_Value(mvVariableHash_Index(parameters, 1), &valueLength);

		redisReply* reply = (redisReply*)redisCommand(_connection, "SET %s %s", key, value);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}

		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}
	
	/**
	 * -----------------------------------------
	 * Redis Command: APPEND
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_append_parameters[] = { { "key", 3, EPF_NORMAL }, { "value", 5, EPF_REFERENCE } };
	void redis_append(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int keyLength = 0;
		const char* key = mvVariable_Value(mvVariableHash_Index(parameters, 0), &keyLength);
		
		int valueLength = 0;
		const char* value = mvVariable_Value(mvVariableHash_Index(parameters, 1), &valueLength);

		redisReply* reply = (redisReply*)redisCommand(_connection, "APPEND %s %s", key, value);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}

		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}
	
	/**
	 * -----------------------------------------
	 * Redis Command: SETEX
	 * -----------------------------------------
	 */
	MV_EL_FunctionParameter redis_setex_parameters[] = { { "key", 3, EPF_NORMAL }, { "value", 5, EPF_REFERENCE }, { "expires", 7, EPF_NORMAL } };
	void redis_setex(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		if (_connection == NULL) {
			setRedisError(ERROR_NOT_CONNECTED, "Not connected! Use redis_connect!", program, returnValue);
			return;
		}

		int keyLength = 0;
		const char* key = mvVariable_Value(mvVariableHash_Index(parameters, 0), &keyLength);
		
		int valueLength = 0;
		const char* value = mvVariable_Value(mvVariableHash_Index(parameters, 1), &valueLength);

		int expires = mvVariable_Value_Integer(mvVariableHash_Index(parameters, 2));

		redisReply* reply = (redisReply*)redisCommand(_connection, "SETEX %s %s %s", key, value, expires);

		if (reply == NULL) {
			setRedisError(ERROR_COMMAND, _connection->errstr, program, returnValue);
			return;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			setRedisError(ERROR_COMMAND, reply->str, program, returnValue);
			freeReplyObject(reply);
			return;
		}

		freeReplyObject(reply);
		mvVariable_SetValue_Integer(returnValue, 1);
	}

	/**
	 * ------------------------------
	 * Function Export
	 */
	EXPORT MV_EL_Function_List* miva_function_table() {
		static MV_EL_Function exported_functions[] = {
			{ "redis_connect", 13, 2, redis_connect_parameters, redis_connect },
			{ "redis_free", 10, 0, redis_free_parameters, redis_free },
			{ "redis_command", 13, 2, redis_command_parameters, redis_command },
			{ "redis_command_append", 20, 2, redis_command_append_parameters, redis_command_append },
			{ "redis_error", 11, 1, redis_error_parameters, redis_error },
			{ "redis_error_clear", 17, 1, redis_error_clear_parameters, redis_error_clear },
			{ "redis_get_reply", 15, 1, redis_get_reply_parameters, redis_get_reply },

			{ "redis_get", 9, 2, redis_get_parameters, redis_get},
			{ "redis_set", 9, 2, redis_set_parameters, redis_set},
			{ "redis_setex", 9, 3, redis_setex_parameters, redis_setex},
			{ "redis_del", 9, 1, redis_del_parameters, redis_del},
			{ "redis_append", 12, 2, redis_append_parameters, redis_append},

			{ 0 , 0 , 0, 0 , 0 }
		};

		static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

		return &list;
	}
}

const char* getVariableFromLists(mvVariableList localVars, mvVariableList globalVars, const string& variableName) {
	if (variableName.size() < 3)
		return "";

	mvVariable var = NULL;
	if (variableName[0] == 'l' && variableName[1] == '.')
		var = mvVariableList_Find(localVars, variableName.c_str() + 2, variableName.size() - 2);
	else if (variableName[0] == 'g' && variableName[1] == '.')
		var = mvVariableList_Find(globalVars, variableName.c_str() + 2, variableName.size() - 2);
	else
		return "";

	if (var == NULL)
		return "";
	
	int len;
	return mvVariable_Value(var, &len);
}