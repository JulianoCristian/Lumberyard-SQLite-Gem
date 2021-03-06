
#include "StdAfx.h"
#include "SQLiteSystemComponent.h"

namespace SQLite {
	////////////////////////////////////////////////////////////////////////
	// Component Implementation
	void SQLiteSystemComponent::Reflect(AZ::ReflectContext* context) {
		if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context)) {
			serialize->Class<SQLiteSystemComponent, AZ::Component>()
				->Version(1)
				->Field("DBPath", &SQLiteSystemComponent::m_dbPath);
			//->SerializerForEmptyClass();

			if (AZ::EditContext* ec = serialize->GetEditContext()) {
				ec->Class<SQLiteSystemComponent>("SQLiteLY", "SQLite database component.")
					->ClassElement(AZ::Edit::ClassElements::EditorData, "")
					->Attribute(AZ::Edit::Attributes::Category, "Database")
					->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System"))
					->Attribute(AZ::Edit::Attributes::AutoExpand, true)
					->DataElement(0, &SQLiteSystemComponent::m_dbPath, "Database", "Database Path");
			}
		}

		AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
		if (behaviorContext) {
		#define SQLITE_EVENT(name) ->Event(#name, &SQLiteRequestBus::Events::##name##)
		#define SQLITE_EVENTLUA(name) ->Event(#name, &SQLiteRequestBus::Events::##name##Lua)
			behaviorContext->EBus<SQLiteRequestBus>("SQLiteLY")
				->Handler<LuaHandler>()
				SQLITE_EVENTLUA(Exec)
				SQLITE_EVENTLUA(ExecTo)
				SQLITE_EVENTLUA(GetConnection)
				SQLITE_EVENT(Open)
				SQLITE_EVENT(Open16)
				SQLITE_EVENT(Open_v2)
				SQLITE_EVENT(ErrCode)
				SQLITE_EVENT(ExtErrCode)
				SQLITE_EVENT(ErrMsg)
				SQLITE_EVENT(ErrMsg16)
				;
		#undef SQLITE_EVENTLUA
		#undef SQLITE_EVENT

			//behaviorContext->EBus<SQLiteLuaRequests>("");
		}
	}

	void SQLiteSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided) {
		provided.push_back(AZ_CRC("SQLiteLYService"));
	}

	void SQLiteSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible) {
		incompatible.push_back(AZ_CRC("SQLiteLYService"));
	}

	void SQLiteSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required) {
		(void)required;// .push_back(AZ_CRC("SQLite3Service"));
	}

	void SQLiteSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent) {
		//(void)dependent;
		dependent.push_back(AZ_CRC("SQLite3Service"));
	}

	void SQLiteSystemComponent::Init() {
		this->m_dbPath = ":memory:";
		this->m_OpenType = CLOSED;
	}

	void SQLiteSystemComponent::Activate() {
		AZ_Printf("SQLiteLY", "%s - Opening Database: %s", this->GetEntityId().ToString().c_str(), this->m_dbPath);
		sqlite3_open(this->m_dbPath, &(this->m_pDB));
		this->m_OpenType = OPEN;
		SQLiteRequestBus::Handler::BusConnect(this->GetEntityId());
	}

	void SQLiteSystemComponent::Deactivate() {
		SQLiteRequestBus::Handler::BusDisconnect();
		AZ_Printf("SQLiteLY", "%s - Closing Database", this->GetEntityId().ToString().c_str());
		sqlite3_close(this->m_pDB);
		this->m_OpenType = CLOSED;
	}

	int SQLiteSystemComponent::Open(const char * path) {
		AZ_Printf("SQLiteLY", "%s - Closing Database", this->GetEntityId().ToString().c_str());
		int ret;
		switch (this->m_OpenType) {
		case OPEN:
		case OPEN16:
			ret = sqlite3_close(this->m_pDB);
			break;
		case OPENV2:
			ret = sqlite3_close_v2(this->m_pDB);
			break;
		}

		if (ret != SQLITE_OK) return ret;

		this->m_dbPath = path;
		AZ_Printf("SQLiteLY", "%s - Opening Database: %s", this->GetEntityId().ToString().c_str(), this->m_dbPath);
		return sqlite3_open(path, &(this->m_pDB));
		this->m_OpenType = OPEN;
	}

	int SQLiteSystemComponent::Open16(const char * path) {
		AZ_Printf("SQLiteLY", "%s - Closing Database", this->GetEntityId().ToString().c_str());
		int ret;
		switch (this->m_OpenType) {
		case OPEN:
		case OPEN16:
			ret = sqlite3_close(this->m_pDB);
			break;
		case OPENV2:
			ret = sqlite3_close_v2(this->m_pDB);
			break;
		}

		if (ret != SQLITE_OK) return ret;

		this->m_dbPath = path;
		AZ_Printf("SQLiteLY", "%s - Opening Database: %s", this->GetEntityId().ToString().c_str(), this->m_dbPath);
		return sqlite3_open16(path, &(this->m_pDB));
		this->m_OpenType = OPEN16;
	}

	int SQLiteSystemComponent::Open_v2(const char * path, int flags, const char *zVfs) {
		AZ_Printf("SQLiteLY", "%s - Closing Database", this->GetEntityId().ToString().c_str());
		int ret;
		switch (this->m_OpenType) {
		case OPEN:
		case OPEN16:
			ret = sqlite3_close(this->m_pDB);
			break;
		case OPENV2:
			ret = sqlite3_close_v2(this->m_pDB);
			break;
		}
		this->m_dbPath = path;

		if (ret != SQLITE_OK) return ret;

		AZ_Printf("SQLiteLY", "%s - Opening Database: %s", this->GetEntityId().ToString().c_str(), this->m_dbPath);
		return sqlite3_open_v2(path, &(this->m_pDB), flags, zVfs);
		this->m_OpenType = OPENV2;
	}
	////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////
	// Lua Implementation
	int SQLiteSystemComponent::ExecLua(const char *sql, void * cbarg) {
		typedef struct {
			void* cbarg;
			AZ::EntityId id;
		} ARGS;

		ARGS args;
		args.cbarg = cbarg;
		args.id = this->GetEntityId();

		auto callback = [](void* cbarg, int argc, char **argv, char **azColName) -> int {
			int ret;
			ARGS* args = (ARGS*)cbarg;

			//////////////////////////
			for (int i = 0; i < argc; i++) {
				AZ_Printf("SQLiteLY", "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
			}

			AZStd::vector<AZStd::string> argvVec(argv, argc + argv);
			AZStd::vector<AZStd::string> azColNameVec(azColName, azColName+argc);
			//////////////////////////

			//SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argv, azColName);
			SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argvVec, azColNameVec);
			
			return ret;
		};

		//char * errmsg = 0;

		return sqlite3_exec(
			this->m_pDB,
			sql,
			callback,
			&args,
			nullptr
		);
	}

	int SQLiteSystemComponent::ExecToLua(AZ::EntityId id, const char *sql, void * cbarg) {
		typedef struct {
			void* cbarg;
			AZ::EntityId id;
		} ARGS;

		ARGS args;
		args.cbarg = cbarg;
		args.id = id;

		auto callback = [](void* cbarg, int argc, char **argv, char **azColName) -> int {
			int ret;
			ARGS* args = (ARGS*)cbarg;

			//////////////////////////
			for (int i = 0; i < argc; i++) {
				AZ_Printf("SQLiteLY", "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
			}

			AZStd::vector<AZStd::string> argvVec(argv, argc + argv);
			AZStd::vector<AZStd::string> azColNameVec(azColName, azColName + argc);
			//////////////////////////

			//SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argv, azColName);
			SQLite::SQLiteLuaBus::EventResult(ret, args->id, &SQLite::SQLiteLuaBus::Events::SqlCallback, args->cbarg, argc, argvVec, azColNameVec);

			return ret;
		};

		//char * errmsg = 0;

		return sqlite3_exec(
			this->m_pDB,
			sql,
			callback,
			&args,
			nullptr
		);
	}

	int SQLiteSystemComponent::LuaHandler::SqlCallback(void * cbargs, int argc, AZStd::vector<AZStd::string> argv, AZStd::vector<AZStd::string> azColName){
	//int SQLiteSystemComponent::LuaHandler::SqlCallback(void * cbargs, int argc, char **argv, char **azColName) {
		int ret;
		CallResult(ret, FN_SqlCallback, cbargs, argc, argv, azColName);
		return ret;
	}

	/*namespace SQLite3 {
		void SQLite3Constructor(SQLite * thisPtr, AZ::ScriptDataContext& dc) {
			if (dc.GetNumArguments() == 1) {
				const char * path;
				dc.ReadArg(0, path);
				sqlite3_open(path, &thisPtr->ptr);
			}
		}
	}*/
	////////////////////////////////////////////////////////////////////////
}
