

#ifndef ___SOCKETPRO_SERVER_SIDE_CACHE_I_H__
#define ___SOCKETPRO_SERVER_SIDE_CACHE_I_H__

#include "tablecache.h"
#include "udb_client.h"

namespace SPA {

    template<typename THandler, typename TCS = ClientSide::CClientSocket>
    class CMasterSlaveBase : public ClientSide::CSocketPool < THandler, TCS> {
    public:
        typedef ClientSide::CSocketPool < THandler, TCS> CBase;

        CMasterSlaveBase(const wchar_t *defaultDb, unsigned int recvTimeout = ClientSide::DEFAULT_RECV_TIMEOUT)
        : CBase(true, recvTimeout), m_dbDefalut(defaultDb ? defaultDb : L""), m_nRecvTimeout(recvTimeout) {
        }

        typedef std::function<void() > DOnClosed;

    public:

        void Subscribe(UINT64 key, DOnClosed c) {
            CAutoLock al(this->m_cs);
            m_mapClose[key] = c;
        }

        void Remove(UINT64 key) {
            CAutoLock al(this->m_cs);
            m_mapClose.erase(key);
        }

        const wchar_t* GetDefaultDBName() const {
            return m_dbDefalut.c_str();
        }

        unsigned int GetRecvTimeout() const {
            return m_nRecvTimeout;
        }

    protected:

        virtual void OnSocketPoolEvent(ClientSide::tagSocketPoolEvent spe, const std::shared_ptr<THandler> &asyncSQL) {
            switch (spe) {
                case SPA::ClientSide::speConnected:
                    if (asyncSQL->GetAttachedClientSocket()->GetErrorCode() != 0)
                        break;
                    asyncSQL->Open(m_dbDefalut.c_str(), nullptr); //open a session to backend database by default 
                    break;
                case ClientSide::speSocketClosed:
                    if (!asyncSQL->GetAttachedClientSocket()->Sendable()) {
                        CAutoLock al(this->m_cs);
                        for (auto it = m_mapClose.begin(), end = m_mapClose.end(); it != end; ++it) {
                            DOnClosed closed = it->second;
                            if (closed)
                                closed();
                        }
                    }
                    break;
                default:
                    break;
            }
        }

    private:
        std::unordered_map<UINT64, DOnClosed> m_mapClose; //protected by base class m_cs
        std::wstring m_dbDefalut;
        unsigned int m_nRecvTimeout;
    };

    template<bool midTier, typename THandler, typename TCache = CDataSet>
    class CMasterPool : public CMasterSlaveBase < THandler > {
    public:
        typedef CMasterSlaveBase < THandler > CBase;
        
        CMasterPool(const wchar_t *defaultDb, unsigned int recvTimeout = ClientSide::DEFAULT_RECV_TIMEOUT)
        : CBase(defaultDb, recvTimeout) {
        }
        typedef TCache CDataSet;
        static TCache Cache; //real-time cache accessable from your code
        typedef ClientSide::CAsyncDBHandler<THandler::SQLStreamServiceId> CSQLHandler;
        typedef CMasterSlaveBase<THandler> CSlavePool;

    protected:

        virtual void OnSocketPoolEvent(ClientSide::tagSocketPoolEvent spe, const std::shared_ptr<THandler> &asyncSQL) {
            switch (spe) {
                case ClientSide::speConnected:
                    if (asyncSQL->GetAttachedClientSocket()->GetErrorCode() != 0)
                        break;
                    //use the first socket session for table events only, update, delete and insert
                    if (asyncSQL == this->GetAsyncHandlers()[0]) {
                        asyncSQL->GetAttachedClientSocket()->GetPush().OnPublish = [this, asyncSQL](ClientSide::CClientSocket*cs, const ClientSide::CMessageSender& sender, const unsigned int* groups, unsigned int count, const UVariant & vtMsg) {
                            assert(count == 1);
                            assert(groups != nullptr);
                            assert(groups[0] == UDB::STREAMING_SQL_CHAT_GROUP_ID || groups[0] == UDB::CACHE_UPDATE_CHAT_GROUP_ID);

                            if (groups[0] == UDB::CACHE_UPDATE_CHAT_GROUP_ID) {
                                if (midTier) {
                                    UVariant vtMessage;
                                    //notify front clients to re-initialize cache
                                    ServerSide::CSocketProServer::PushManager::Publish(vtMessage, &UDB::CACHE_UPDATE_CHAT_GROUP_ID, 1);
                                }
                                this->SetInitialCache(asyncSQL);
                                return;
                            }
                            if (midTier) {
                                //push message onto front clients which may be interested in the message
                                ServerSide::CSocketProServer::PushManager::Publish(vtMsg, &UDB::STREAMING_SQL_CHAT_GROUP_ID, 1);
                            }

                            VARIANT *vData;
                            size_t res;
                            //vData[0] == event type; vData[1] == host; vData[2] = database user; vData[3] == db name; vData[4] == table name
                            ::SafeArrayAccessData(vtMsg.parray, (void**) &vData);
                            ClientSide::tagUpdateEvent eventType = (ClientSide::tagUpdateEvent)(vData[0].intVal);

                            if (!Cache.GetDBServerName().size()) {
                                if (vData[1].vt == (VT_ARRAY | VT_I1))
                                    Cache.SetDBServerName(ToWide(vData[1]).c_str());
                                else if (vData[1].vt == VT_BSTR)
                                    Cache.SetDBServerName(vData[1].bstrVal);
                            }
                            if (vData[2].vt == (VT_ARRAY | VT_I1))
                                Cache.SetUpdater(ToWide(vData[2]).c_str());
                            else if (vData[2].vt == VT_BSTR)
                                Cache.SetUpdater(vData[2].bstrVal);
                            else
                                Cache.SetUpdater(nullptr);

                            std::wstring dbName;
                            if (vData[3].vt == (VT_I1 | VT_ARRAY)) {
                                dbName = ToWide(vData[3]);
                            } else if (vData[3].vt == VT_BSTR)
                                dbName = vData[3].bstrVal;
                            else {
                                assert(false); //shouldn't come here
                            }
                            std::wstring tblName;
                            if (vData[4].vt == (VT_I1 | VT_ARRAY)) {
                                tblName = ToWide(vData[4]);
                            } else if (vData[3].vt == VT_BSTR)
                                tblName = vData[4].bstrVal;
                            else {
                                assert(false); //shouldn't come here
                            }
                            switch (eventType) {
                                case UDB::ueInsert:
                                    res = Cache.AddRows(dbName.c_str(), tblName.c_str(), vData + 5, vtMsg.parray->rgsabound->cElements - 5);
                                    assert(res != CDataSet::INVALID_VALUE);
                                    break;
                                case UDB::ueUpdate:
                                {
                                    unsigned int count = vtMsg.parray->rgsabound->cElements - 5;
                                    res = Cache.UpdateARow(dbName.c_str(), tblName.c_str(), vData + 5, count);
                                    assert(res != CDataSet::INVALID_VALUE);
                                }
                                    break;
                                case UDB::ueDelete:
                                {
                                    unsigned int keys = vtMsg.parray->rgsabound->cElements - 5;
                                    //there must be one or two key columns. For other cases, you must implement them
                                    if (keys == 1)
                                        res = Cache.DeleteARow(dbName.c_str(), tblName.c_str(), vData[5]);
                                    else
                                        res = Cache.DeleteARow(dbName.c_str(), tblName.c_str(), vData[5], vData[6]);
                                    assert(res != CDataSet::INVALID_VALUE);
                                }
                                    break;
                                default:
                                    //not implemented
                                    assert(false);
                                    break;
                            }
                            ::SafeArrayUnaccessData(vtMsg.parray);
                        };

                        if (midTier) {
                            UVariant vtMessage;
                            ServerSide::CSocketProServer::PushManager::Publish(vtMessage, &UDB::CACHE_UPDATE_CHAT_GROUP_ID, 1);
                        }
                        SetInitialCache(asyncSQL);
                    } else {
                        asyncSQL->Open(this->GetDefaultDBName(), nullptr);
                    }
                    break;
                default:
                    CMasterSlaveBase<THandler>::OnSocketPoolEvent(spe, asyncSQL);
                    break;
            }
        }

    protected:

        static std::wstring ToWide(const VARIANT &data) {
            const char *s;
            assert(data.vt == (VT_ARRAY | VT_I1));
            ::SafeArrayAccessData(data.parray, (void**) &s);
            std::wstring ws = Utilities::ToWide(s, data.parray->rgsabound->cElements);
            ::SafeArrayUnaccessData(data.parray);
            return ws;
        }

    private:

        void SetInitialCache(const std::shared_ptr<THandler> &asyncSQL) {
            //open default database and subscribe for table update events (update, delete and insert) by setting flag UDB::ENABLE_TABLE_UPDATE_MESSAGES
            bool ok = asyncSQL->Open(this->GetDefaultDBName(), [this](CSQLHandler &h, int res, const std::wstring & errMsg) {
                this->m_cache.SetDBServerName(nullptr);
                this->m_cache.SetUpdater(nullptr);
                        this->m_cache.Empty();
                        unsigned int port;
                        std::string ip = h.GetAttachedClientSocket()->GetPeerName(&port);
                        ip += ":";
                        ip += std::to_string(port);
                        h.Utf8ToW(true);
#ifdef WIN32_64
                        h.TimeEx(true);
#endif
                        this->m_cache.Set(ip.c_str(), h.GetDBManagementSystem());
            }, UDB::ENABLE_TABLE_UPDATE_MESSAGES);

            //bring all cached table data into m_cache first for initial cache, and exchange it with Cache if there is no error
            ok = asyncSQL->Execute(L"", [this](CSQLHandler &h, int res, const std::wstring &errMsg, INT64 affected, UINT64 fail_ok, UDB::CDBVariant & vtId) {
                if (res == 0) {
                    Cache.Swap(this->m_cache); //exchange between master Cache and this m_cache
                } else {
                    std::cout << "Error code: " << res << ", error message: ";
                    std::wcout << errMsg.c_str() << std::endl;
                }
            }, [this](CSQLHandler &h, UDB::CDBVariantArray & vData) {
                auto meta = h.GetColumnInfo();
                const UDB::CDBColumnInfo &info = meta.front();
                        //populate vData into m_cache container
                        this->m_cache.AddRows(info.DBPath.c_str(), info.TablePath.c_str(), vData);
            }, [this](CSQLHandler & h) {
                //a rowset column meta comes
                this->m_cache.AddEmptyRowset(h.GetColumnInfo());
            });
        }

    protected:
        TCache m_cache;
    };

    template<bool midTier, typename THandler, typename TCache>
    TCache CMasterPool<midTier, THandler, TCache>::Cache;
}; //namespace SPA

#endif