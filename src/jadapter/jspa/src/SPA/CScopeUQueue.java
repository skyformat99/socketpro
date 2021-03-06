package SPA;

import java.util.ArrayDeque;

public final class CScopeUQueue {

    @Override
    protected void finalize() throws Throwable {
        Clean();
        super.finalize();
    }

    public static long getMemoryConsumed() {
        long mem = 0;
        synchronized (m_cs) {
            for (CUQueue q : m_sQueue) {
                mem += q.getMaxBufferSize();
            }
        }
        return mem;
    }

    public static void DestroyUQueuePool() {
        synchronized (m_cs) {
            while (m_sQueue.size() > 0) {
                CUQueue q = m_sQueue.removeLast();
                q.Empty();
            }
        }
    }

    private static int m_mem_size = 32 * 1024;

    public static int getSHARED_BUFFER_CLEAN_SIZE() {
        synchronized (m_cs) {
            return m_mem_size;
        }
    }

    public static void setSHARED_BUFFER_CLEAN_SIZE(int size) {
        synchronized (m_cs) {
            if (size <= 512) {
                size = 512;
            }
            m_mem_size = size;
        }
    }

    public static CUQueue Lock(tagOperationSystem os) {
        CUQueue UQueue = null;
        synchronized (m_cs) {
            if (m_sQueue.size() > 0) {
                UQueue = m_sQueue.removeLast();
            }
        }
        if (UQueue == null) {
            UQueue = new CUQueue();
        }
        UQueue.setOS(os);
        return UQueue;
    }

    public static CUQueue Lock() {
        return Lock(CUQueue.DEFAULT_OS);
    }

    public static void Unlock(CUQueue UQueue) {
        if (UQueue != null) {
            UQueue.SetSize(0);
            synchronized (m_cs) {
                m_sQueue.addLast(UQueue);
            }
        }
    }

    public final CUQueue Detach() {
        CUQueue q = m_UQueue;
        m_UQueue = null;
        return q;
    }

    public void Clean() {
        if (m_UQueue != null) {
            Unlock(m_UQueue);
            m_UQueue = null;
        }
    }

    public final void Attach(CUQueue q) {
        Unlock(m_UQueue);
        m_UQueue = q;
    }

    public final CUQueue getUQueue() {
        return m_UQueue;
    }

    public final CScopeUQueue Save(Byte b) {
        m_UQueue.Save(b);
        return this;
    }

    public final CScopeUQueue Save(Short s) {
        m_UQueue.Save(s);
        return this;
    }

    public final CScopeUQueue Save(Integer i) {
        m_UQueue.Save(i);
        return this;
    }

    public final CScopeUQueue Save(Long i) {
        m_UQueue.Save(i);
        return this;
    }

    public final CScopeUQueue Save(Float f) {
        m_UQueue.Save(f);
        return this;
    }

    public final CScopeUQueue Save(Double d) {
        m_UQueue.Save(d);
        return this;
    }

    public final CScopeUQueue Save(Boolean b) {
        m_UQueue.Save(b);
        return this;
    }

    public final <T extends IUSerializer> CScopeUQueue Save(T s) {
        s.SaveTo(m_UQueue);
        return this;
    }

    public final CScopeUQueue Save(int n) {
        m_UQueue.Save(n);
        return this;
    }

    public final CScopeUQueue Save(short n) {
        m_UQueue.Save(n);
        return this;
    }

    public final CScopeUQueue Save(char c) {
        m_UQueue.Save(c);
        return this;
    }

    public final CScopeUQueue Save(byte n) {
        m_UQueue.Save(n);
        return this;
    }

    public final CScopeUQueue Save(boolean b) {
        m_UQueue.Save(b);
        return this;
    }

    public final CScopeUQueue Save(long n) {
        m_UQueue.Save(n);
        return this;
    }

    public final CScopeUQueue Save(float f) {
        m_UQueue.Save(f);
        return this;
    }

    public final CScopeUQueue Save(double d) {
        m_UQueue.Save(d);
        return this;
    }

    public final CScopeUQueue Save(String s) {
        m_UQueue.Save(s);
        return this;
    }

    public final CScopeUQueue Save(byte[] bytes) {
        m_UQueue.Save(bytes);
        return this;
    }

    public final CScopeUQueue Save(java.util.UUID id) throws IllegalArgumentException {
        m_UQueue.Save(id);
        return this;
    }

    public final CScopeUQueue Save(Object obj) throws UnsupportedOperationException {
        m_UQueue.Save(obj);
        return this;
    }

    public final CScopeUQueue Save(CUQueue q) {
        m_UQueue.Save(q);
        return this;
    }

    public final CScopeUQueue Save(java.math.BigDecimal dec) throws IllegalArgumentException {
        m_UQueue.Save(dec);
        return this;
    }

    public final CScopeUQueue Save(java.util.Date dt) throws IllegalArgumentException {
        m_UQueue.Save(dt);
        return this;
    }

    private CUQueue m_UQueue = Lock();
    private final static Object m_cs = new Object();
    private final static ArrayDeque<CUQueue> m_sQueue = new ArrayDeque<>();
}
