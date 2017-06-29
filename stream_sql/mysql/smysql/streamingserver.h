
#pragma once

#include "mysqlimpl.h"

class CStreamingServer : public SPA::ServerSide::CSocketProServer {
public:
    CStreamingServer(int nParam = 0);
    ~CStreamingServer();

protected:
    virtual void OnAccept(USocket_Server_Handle h, int errCode);
    virtual bool OnSettingServer(unsigned int listeningPort, unsigned int maxBacklog, bool v6);
    virtual bool OnIsPermitted(USocket_Server_Handle h, const wchar_t* userId, const wchar_t *password, unsigned int serviceId);
    virtual void OnClose(USocket_Server_Handle h, int errCode);
    virtual void OnIdle(SPA::INT64 milliseconds);
    virtual void OnSSLShakeCompleted(USocket_Server_Handle h, int errCode);

private:
    bool AddService();
    bool DoSQLAuthentication(USocket_Server_Handle h, const wchar_t* userId, const wchar_t *password);

private:
    SPA::ServerSide::CMysqlService m_MySql;
	bool m_bSet;
	SPA::CUCriticalSection m_cs;

private:
    CStreamingServer(const CStreamingServer &ss);
    CStreamingServer& operator=(const CStreamingServer &ss);
};

extern CStreamingServer *g_pStreamingServer;

typedef int (*pdecimal2string) (const decimal_t *from, char *to, int *to_len, int fixed_precision, int fixed_decimals, char filler);

class CSetGlobals {
private:
    FILE *m_fLog;
    SPA::CUCriticalSection m_cs;

private:
    CSetGlobals();
    void LogEntry(const char* file, int fileLineNumber, const char* szBuf);
    static unsigned int GetVersion(const char *prog);
    static void SetConfig(const std::unordered_map<std::string, std::string>& mapConfig);

#ifdef WIN32_64
    static DWORD WINAPI ThreadProc(LPVOID lpParameter);
#else

#endif

public:
    int m_nParam;
    bool DisableV6;
    unsigned int Port;
    const char *server_version;
    CHARSET_INFO *utf8_general_ci;
    pdecimal2string decimal2string;
    st_mysql_daemon async_sql_plugin;
    HINSTANCE m_hModule;
    const void *Plugin;
    std::string ssl_key;
    std::string ssl_cert;
    std::string ssl_pwd;
    std::vector<std::string> cached_tables;
    static CSetGlobals Globals;

#ifdef WIN32_64
    HANDLE m_hThread;
    DWORD m_dwThreadId;
#else

#endif

public:
    bool StartListening();
    void LogMsg(const char *file, int fileLineNumber, const char *format ...);
};

int async_sql_plugin_init(void *p);
int async_sql_plugin_deinit(void *p);