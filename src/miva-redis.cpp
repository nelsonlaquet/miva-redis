#include <sstream>
#include <string>
#include <string.h>
#include <vector>

#include "miva-redis.h"
#include "../vendor/hiredis/hiredis.h"

using std::vector;
using std::string;
using std::stringstream;

vector<string> splitString(const string &s, char delim);
const char* getVariableFromProgram(mvProgram program, mvVariableList localVars, mvVariableList globalVars, const string& variableName);

inline string trim(std::string& str) {
    str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
    str.erase(str.find_last_not_of(' ')+1);         //surfixing spaces
    return str;
}

extern "C" {
	/**
	* Globals
	*/
	redisContext* _connection = NULL;

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

	/**
	 * ------------------------------
	 * Low level redis API
	 */

	MV_EL_FunctionParameter redis_connect_parameters[] = {
		{ "host", 4, EPF_NORMAL },
		{ "port", 4, EPF_NORMAL }
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

	MV_EL_FunctionParameter redis_free_parameters[] = {
	};
	void redis_free(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata)  {
		if (_connection != NULL) {
			redisFree(_connection);		
			_connection = NULL;
		}

		mvVariable_SetValue_Integer(returnValue, 1);
	}

	MV_EL_FunctionParameter redis_command_parameters[] = {
		{ "command", 7, EPF_NORMAL },
		{ "args", 4, EPF_NORMAL }
	};
	void redis_command(mvProgram program, mvVariableHash parameters, mvVariable returnValue, void** pdata) {
		stringstream ss;
		
		if (_connection == NULL) {
			const char* error = "Not connected! Use redis_connect!";
			mvProgram_FatalError(program, error, strlen(error));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		int commandLength = 0;
		const char* command = mvVariable_Value(mvVariableHash_Index(parameters, 0), &commandLength);

		int argsLength = 0;
		const char* args = mvVariable_Value(mvVariableHash_Index(parameters, 1), &argsLength);

		// first, count expected varaiables
		int variableSubCount = 0;
		for (int i = 0; i < commandLength - 1; i++) {
			if (command[i] == '%') {
				if (command[i + 1] != 's') {
					string error = "Invalid command to redis, you may only specify '%s' in the command for substitution!";
					mvProgram_FatalError(program, error.c_str(), error.size());
					mvVariable_SetValue_Integer(returnValue, 0);
					return;
				}
				
				variableSubCount++;
			}
		}

		// next, extract all variables from the args parameter
		vector<string> argVars = strlen(args) != 0 ? splitString(args, ',') : vector<string>();
		
		if (argVars.size() != variableSubCount) {
			ss << "Redis command '" << command << "' has " << variableSubCount << " variable substitutions, but you provided " << argVars.size() << " variables!";
			string error = ss.str();
			mvProgram_FatalError(program, error.c_str(), error.size());
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		if (argVars.size() > 10) {
			ss << "Command '" << command << "' has " << variableSubCount << " variable substitutions, which is more than the supported 10";
			string error = ss.str();
			mvProgram_FatalError(program, error.c_str(), error.size());
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}

		redisReply* reply;
		int len;

		mvVariableList 
			localVars = mvVariableList_Allocate(),
			globalVars = mvVariableList_Allocate();

		mvProgram_Local_Variables(program, localVars);
		mvProgram_Global_Variables(program, globalVars);

		// What we should really do is export a function per command type that redis supports, properly handling different types, optional parameters, etc...
		// However, at the moment we just need this working... SO this'll do instead (note: this is required since I do *not* want to generate va_lists at runtime).
		// Please forgive me.
		#define READ_STRING_VAR(index) getVariableFromProgram(program, localVars, globalVars, argVars[index])
		if (argVars.size() == 0)
			reply = (redisReply*) redisCommand(_connection, command);
		else if (argVars.size() == 1)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0));
		else if (argVars.size() == 2)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1));
		else if (argVars.size() == 3)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2));
		else if (argVars.size() == 4)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3));
		else if (argVars.size() == 5)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4));
		else if (argVars.size() == 6)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4), READ_STRING_VAR(5));
		else if (argVars.size() == 7)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4), READ_STRING_VAR(5), READ_STRING_VAR(6));
		else if (argVars.size() == 8)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4), READ_STRING_VAR(5), READ_STRING_VAR(6), READ_STRING_VAR(7));
		else if (argVars.size() == 9)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4), READ_STRING_VAR(5), READ_STRING_VAR(6), READ_STRING_VAR(7), READ_STRING_VAR(8));
		else if (argVars.size() == 10)
			reply = (redisReply*) redisCommand(_connection, command, READ_STRING_VAR(0), READ_STRING_VAR(1), READ_STRING_VAR(2), READ_STRING_VAR(3), READ_STRING_VAR(4), READ_STRING_VAR(5), READ_STRING_VAR(6), READ_STRING_VAR(7), READ_STRING_VAR(8), READ_STRING_VAR(9));

		mvVariableList_Free(localVars);
		mvVariableList_Free(globalVars);

		if (reply == NULL) {
			mvProgram_FatalError(program, _connection->errstr, strlen(_connection->errstr));
			mvVariable_SetValue_Integer(returnValue, 0);
			return;
		}
		
		formatRedisReply(reply, returnValue);
		freeReplyObject(reply);
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
			{ 0 , 0 , 0, 0 , 0 }
		};

		static MV_EL_Function_List list = { MV_EL_FUNCTION_VERSION, exported_functions };

		return &list;
	}
}

vector<string> splitString(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector<string> tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(trim(item));
    }
    return tokens;
}

const char* getVariableFromProgram(mvProgram program, mvVariableList localVars, mvVariableList globalVars, const string& variableName) {
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