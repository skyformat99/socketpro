package SPA.ClientSide;

import SPA.*;
import SPA.UDB.*;

public class CCachedBaseHandler extends CAsyncServiceHandler {

    private static final int ONE_MEGA_BYTES = 0x100000;
    private static final int BLOB_LENGTH_NOT_AVAILABLE = 0xffffffe0;

    public CCachedBaseHandler(int sid) {
        super(sid);
    }

    public final tagManagementSystem getDBManagementSystem() {
        synchronized (m_csCache) {
            return m_ms;
        }
    }

    @Override
    public int CleanCallbacks() {
        synchronized (m_csCache) {
            m_mapHandler.clear();
            m_mapRowset.clear();
        }
        return super.CleanCallbacks();
    }

    public interface DRowsetHeader {

        void invoke(CDBColumnInfoArray meta);
    }

    public interface DRows {

        void invoke(CDBVariantArray vData);
    }

    public interface DResult {

        void invoke(int res, String errMsg);
    }

    public boolean GetCachedTables(String defaultDb, DResult handler, DRows row, DRowsetHeader rh, int flags) {
        long index;
        boolean rowset = (rh != null || row != null);
        CUQueue q = CScopeUQueue.Lock();
        synchronized (m_csCache) {
            ++m_nCall;
            index = m_nCall;
            if (rowset) {
                m_mapRowset.put(index, new Pair<>(rh, row));
                m_mapHandler.put(index, handler);
            }
        }
        q.Save(defaultDb).Save(flags).Save(rowset).Save(index);
        boolean ok = SendRequest(CAsyncDBHandler.idGetCachedTables, q, null, null);
        CScopeUQueue.Unlock(q);
        if (!ok) {
            synchronized (m_csCache) {
                m_mapHandler.remove(index);
                m_mapRowset.remove(index);
            }
        }
        return ok;
    }

    @Override
    protected void OnResultReturned(short reqId, CUQueue mc) {
        switch (reqId) {
            case CAsyncDBHandler.idGetCachedTables: {
                int res = mc.LoadInt();
                int dbMS = mc.LoadInt();
                String errMsg = mc.LoadString();
                DResult r = null;
                synchronized (m_csCache) {
                    m_ms = tagManagementSystem.forValue(dbMS);
                    if (m_mapHandler.containsKey(m_indexRowset)) {
                        r = m_mapHandler.get(m_indexRowset);
                        m_mapHandler.remove(m_indexRowset);
                    }
                    if (m_mapRowset.containsKey(m_indexRowset)) {
                        m_mapRowset.remove(m_indexRowset);
                    }
                }
                if (r != null) {
                    r.invoke(res, errMsg);
                }
            }
            break;
            case CAsyncDBHandler.idRowsetHeader: {
                m_Blob.SetSize(0);
                if (m_Blob.getMaxBufferSize() > ONE_MEGA_BYTES) {
                    m_Blob.Realloc(ONE_MEGA_BYTES);
                }
                m_vData.clear();
                DRowsetHeader header = null;
                CDBColumnInfoArray vColInfo = null;
                synchronized (m_csCache) {
                    vColInfo = new CDBColumnInfoArray();
                    vColInfo.LoadFrom(mc);
                    m_indexRowset = mc.LoadLong();
                    if (vColInfo.size() > 0) {
                        if (m_mapRowset.containsKey(m_indexRowset)) {
                            header = m_mapRowset.get(m_indexRowset).first;
                        }
                    }
                }
                if (header != null) {
                    header.invoke(vColInfo);
                }
            }
            break;
            case CAsyncDBHandler.idBeginRows:
                m_Blob.SetSize(0);
                m_vData.clear();
                if (mc.GetSize() > 0) {
                    synchronized (m_csCache) {
                        m_indexRowset = mc.LoadLong();
                    }
                }
                break;
            case CAsyncDBHandler.idTransferring:
                while (mc.GetSize() > 0) {
                    Object vt = mc.LoadObject();
                    m_vData.add(vt);
                }
                break;
            case CAsyncDBHandler.idEndRows:
                if (mc.GetSize() > 0 || m_vData.size() > 0) {
                    Object vt;
                    while (mc.GetSize() > 0) {
                        vt = mc.LoadObject();
                        m_vData.add(vt);
                    }
                    DRows row = null;
                    synchronized (m_csCache) {
                        if (m_mapRowset.containsKey(m_indexRowset)) {
                            row = m_mapRowset.get(m_indexRowset).second;
                        }
                    }
                    if (row != null) {
                        row.invoke(m_vData);
                    }
                }
                m_vData.clear();
                break;
            case CAsyncDBHandler.idStartBLOB:
                if (mc.GetSize() > 0) {
                    m_Blob.SetSize(0);
                    int len = mc.LoadInt();
                    if (len != -1 && len > m_Blob.getMaxBufferSize()) {
                        m_Blob.Realloc(len);
                    }
                    m_Blob.Push(mc.getIntenalBuffer(), mc.getHeadPosition(), mc.GetSize());
                    mc.SetSize(0);
                }
                break;
            case CAsyncDBHandler.idChunk:
                if (mc.GetSize() > 0) {
                    m_Blob.Push(mc.getIntenalBuffer(), mc.GetSize());
                    mc.SetSize(0);
                }
                break;
            case CAsyncDBHandler.idEndBLOB:
                if (mc.GetSize() > 0 || m_Blob.GetSize() > 0) {
                    m_Blob.Push(mc.getIntenalBuffer(), mc.GetSize());
                    mc.SetSize(0);
                    int len = m_Blob.PeekInt(m_Blob.getHeadPosition() + 2);
                    if (len < 0 && len >= BLOB_LENGTH_NOT_AVAILABLE) {
                        //length should be reset if BLOB length not available from server side at beginning
                        len = m_Blob.GetSize() - 6; //sizeof(short) - sizeof(int);
                        m_Blob.ResetInt(len, 2); //sizeof(short)
                    }
                    Object vt = m_Blob.LoadObject();
                    m_vData.add(vt);
                }
                break;
            default:
                super.OnResultReturned(reqId, mc);
                break;
        }
    }
    private final java.util.HashMap<Long, DResult> m_mapHandler = new java.util.HashMap<>();
    private final java.util.HashMap<Long, Pair<DRowsetHeader, DRows>> m_mapRowset = new java.util.HashMap<>();
    private long m_indexRowset = 0;
    private final CUQueue m_Blob = new CUQueue();
    private final CDBVariantArray m_vData = new CDBVariantArray();
    private tagManagementSystem m_ms = tagManagementSystem.msUnknown;
    private long m_nCall = 0;
    protected final Object m_csCache = new Object();
}