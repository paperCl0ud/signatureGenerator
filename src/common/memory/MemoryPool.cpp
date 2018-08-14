//! @file MemoryPool.cpp
//! Реализация класса CMemoryPool

#include "MemoryPool.h"

#include <boost/thread.hpp>
#include <boost/pool/pool.hpp>
#include <boost/shared_array.hpp>

//! Шаг увеличения размера блока пула (значение по умолчанию).
const size_t DEFAULT_CHUNK_STEP = 1024 * 1024;

//! Закрытый класс для реализации пула памяти.
class CMemoryPoolImpl
{
public:
    CMemoryPoolImpl();
    ~CMemoryPoolImpl();
    bool Init(CMemoryPool::EType type, const CMemoryPool::PoolsParams& poolParams,
              bool doPreallocate, size_t chunkStep);

    void Clear();

    boost::shared_array<uint8_t> Get(size_t size);

private:
    class CReleaser;

    class CPool
    {
        friend class CReleaser;
    private:
        struct Data
        {
            Data(size_t sizeChunk) : pool(sizeChunk), cntAllocated(0) {};

            size_t          cntAllocated;
            boost::mutex    lock;
            boost::pool<>   pool;
        };

    public:
        //! Конструктор.
        //! @param type      - [in] тип пула;
        //! @param sizeChunk - [in] размер блоков памяти в пуле;
        //! @param capacity  - [in] исходное кол-во блоков памяти в пуле.
        CPool(CMemoryPool::EType type, size_t sizeChunk, size_t capacity) :
            m_type(type), m_sizeChunk(sizeChunk), m_capacity(capacity)
        {
        }

        //! Создает пул.
        //! @param doPreallocate - [in] true  - выделить память под пул сразу,
        //!                             false - выделять память только когда она понадобится.
        //! @return true - пул успешно создан, false - в случае ошибки.
        bool Create(bool doPreallocate)
        {
            m_pData.reset(new Data(m_sizeChunk));

            if (m_capacity != 0)
            {
                m_pData->pool.set_next_size(m_capacity);

                // Выделяем память из пула и сразу освобождаем, таким образом выделяя память
                if (doPreallocate)
                {
                    void* p = m_pData->pool.malloc();

                    if (!p)
                    {
                        return false;
                    }

                    m_pData->pool.free(p);
                }
            }

            return true;
        }
        
        //! Возвращает блок памяти из пула.
        //! @return блок памяти из пула или NULL - если выделить память не удалось.
        boost::shared_array<uint8_t> Malloc(size_t size)
        {
            if (size > m_sizeChunk)
            {
                return boost::shared_array<uint8_t>();
            }

            assert(m_pData.get() != NULL);

            boost::unique_lock<boost::mutex> lock(m_pData->lock);

            if ((m_type == CMemoryPool::POOL_EXPANDABLE) || 
                (m_pData->cntAllocated + 1 <= m_capacity))
            {
                boost::shared_array<uint8_t> p(static_cast<uint8_t*>(m_pData->pool.malloc()),
                                               CReleaser(*this));

                if (p.get())
                {
                    ++m_pData->cntAllocated;
                }

                return p;
            }
            else if (m_type == CMemoryPool::POOL_FIXED_NEW_DELETE)
            {
                return boost::shared_array<uint8_t>(new (std::nothrow) uint8_t[size]);
            }

            return boost::shared_array<uint8_t>();
        }

    private:
        //! Возвращает блок памяти в пул.
        //! @param chunk - [in] указатель на блок памяти.
        void Release(void* chunk)
        {
            assert(m_pData.get() != NULL);

            boost::unique_lock<boost::mutex> lock(m_pData->lock);
            m_pData->pool.free(chunk);
            --m_pData->cntAllocated;
        }

    private:
        const CMemoryPool::EType            m_type;
        const size_t                        m_sizeChunk;
        const size_t                        m_capacity;
        boost::shared_ptr<Data>             m_pData;
    };

    class CReleaser
    {
    public:
        CReleaser(CPool pool) :
            m_pool(pool)
        {
        }

        void operator()(void* p)
        {
            if (p)
            {
                m_pool.Release(p);
            }
        }

    private:
        CPool m_pool;
    };

    typedef std::map< size_t, CPool > CPoolMap;

private:
    bool AddPool(CMemoryPool::EType type, size_t chunkSize, size_t capacity, bool doPreallocate);

private:
    CMemoryPool::EType  m_type;
    CPoolMap            m_pools;
    boost::mutex        m_lockPools;
    size_t              m_chunkStep;
};


//! Констуктор.
CMemoryPool::CMemoryPool()
{

}

//! Конструктор.
CMemoryPoolImpl::CMemoryPoolImpl() :
    m_chunkStep(DEFAULT_CHUNK_STEP)
{
}

//! Деструктор.
CMemoryPool::~CMemoryPool()
{
}

//! Деструктор.
CMemoryPoolImpl::~CMemoryPoolImpl()
{
}

//! Инициализирует пул.
//! @param type          - [in] тип пула;
//! @param poolParams    - [in] контейнер задает параметры пулов;
//! @param doPreallocate - [in] true  - выделить память под заданые пулы сразу,
//!                             false - выделять память только когда она понадобится;
//! @param chunkStep     - [in] шаг увеличения размера блока для пулов с рамером блоков более тех,
//!                             что заданы в poolParams.
//! @return true - если удалось инициализировать пул, false - в случае ошибки.
bool CMemoryPool::Init(EType type, const PoolsParams& poolParams,
                       bool doPreallocate/* = true*/,
                       size_t chunkStep/* = 0*/)
{
    if (!m_pPoolImpl)
    {
        boost::shared_ptr<CMemoryPoolImpl> pPoolImpl(new CMemoryPoolImpl());

        if (!pPoolImpl->Init(type, poolParams, doPreallocate, chunkStep))
        {
            return false;
        }

        m_pPoolImpl = pPoolImpl;
    }

    return true;
}

//! Инициализирует пул
//! @param type          - [in] тип пула;
//! @param poolParams    - [in] контейнер задает параметры пулов;
//! @param doPreallocate - [in] true  - выделить память под заданые пулы сразу,
//!                             false - выделять память только когда она понадобится;
//! @param chunkStep     - [in] шаг увеличения размера блока для пулов с рамером блоков более тех,
//!                             что заданы в poolParams.
//! @return true - если удалось инициализировать пул, false - в случае ошибки.
bool CMemoryPoolImpl::Init(CMemoryPool::EType type, const CMemoryPool::PoolsParams& poolParams,
                           bool doPreallocate, size_t chunkStep)
{
    boost::unique_lock<boost::mutex> lock(m_lockPools);
    
    if (!m_pools.empty())
    {
        return true;
    }

    for (size_t i = 0; i < poolParams.size(); ++i)
    {
        if (!AddPool(poolParams[i].type, poolParams[i].chunkSize, poolParams[i].capacity,
                     doPreallocate))
        {
            return false;
        }
    }

    m_type      = type;
    m_chunkStep = (chunkStep != 0) ? chunkStep : DEFAULT_CHUNK_STEP;

    return true;
}

//! Освобождает ссылки на пулы.
//! Реальное освобождение пулов произойдет только тогда,
//! когда будут возвращены все выделенныее блоки памяти.
void CMemoryPool::Clear()
{
    if (m_pPoolImpl)
    {
        m_pPoolImpl->Clear();
    }
}

//! Освобождает ссылки на пулы.
//! Реальное освобождение пулов произойдет только тогда,
//! когда будут возвращены все выделенныее блоки памяти.
void CMemoryPoolImpl::Clear()
{
    boost::unique_lock<boost::mutex> lock(m_lockPools);

    m_pools.clear();
}

//! Возвращает блок памяти из пула.
//! Для того чтобы вернуть блок памяти обратно в пул,
//! нужно просто уменьшить счетчик ссылок блока памяти (используется делетер).
//! @param size - [in] размер требуемого блока
//! @return блок памяти из пула
boost::shared_array<uint8_t> CMemoryPool::Get(size_t size)
{
    if (!m_pPoolImpl)
    {
        return boost::shared_array<uint8_t>();
    }

    return m_pPoolImpl->Get(size);
}

//! Возвращает блок памяти из пула.
//! Для того чтобы вернуть блок памяти обратно в пул,
//! нужно просто уменьшить счетчик ссылок блока памяти (используется делетер).
//! @param size - [in] размер требуемого блока
//! @return блок памяти из пула
boost::shared_array<uint8_t> CMemoryPoolImpl::Get(size_t size)
{
    boost::unique_lock<boost::mutex> lock(m_lockPools);
    CPoolMap::iterator itMap = m_pools.lower_bound(size);

    if ((itMap == m_pools.end()) && (m_type == CMemoryPool::POOL_EXPANDABLE))
    {
        size_t s = 0;

        while (s < size)
        {
            s += m_chunkStep;

            itMap = m_pools.lower_bound(s);

            if (itMap == m_pools.end())
            {
                AddPool(CMemoryPool::POOL_EXPANDABLE, s, 0, false);
            }
        }

        itMap = m_pools.lower_bound(size);
    }

    if (itMap == m_pools.end())
    {
        if (m_type == CMemoryPool::POOL_FIXED_NEW_DELETE)
        {
            return boost::shared_array<uint8_t>(new (std::nothrow) uint8_t[size]);
        }
        else
        {
            return boost::shared_array<uint8_t>();
        }
    }

    CPool& pool = itMap->second;

    lock.unlock();

    return pool.Malloc(size);    
}

//! Добавляет пул.
//! @param type          - [in] тип пула;
//! @param chunkSize     - [in] размер блоков памяти в пуле;
//! @param capacity      - [in] исходное кол-во блоков памяти в пуле;
//! @param doPreallocate - [in] true  - выделить память под заданый пул сразу,
//!                             false - выделять память только когда она понадобится.
//! @return true - пул добален успешно, false - в случае ошибки.
bool CMemoryPoolImpl::AddPool(CMemoryPool::EType type, size_t chunkSize, size_t capacity,
                              bool doPreallocate)
{
    CPool pool(type, chunkSize, capacity);

    if (!pool.Create(doPreallocate))
    {
        return false;
    }

    m_pools.insert(CPoolMap::value_type(chunkSize, pool));

    return true;
}

//! Формирует структуру настроек для расширяемого пула.
//! @param chunkSize     - [in] размер блоков памяти в пуле;
//! @param capacity      - [in] исходное кол-во блоков памяти в пуле;
//! @return настройки пула.
CMemoryPool::PoolParams CMemoryPool::ExpandablePoolParams(size_t chunkSize, size_t capacity)
{
    PoolParams params;
    params.type      = POOL_EXPANDABLE;
    params.chunkSize = chunkSize;
    params.capacity  = capacity;
    return params;
}

//! Формирует структуру настроек для фиксированного пула.
//! @param chunkSize     - [in] размер блоков памяти в пуле;
//! @param capacity      - [in] исходное кол-во блоков памяти в пуле;
//! @return настройки пула.
CMemoryPool::PoolParams CMemoryPool::FixedPoolParams(size_t chunkSize, size_t capacity)
{
    PoolParams params;
    params.type      = POOL_FIXED;
    params.chunkSize = chunkSize;
    params.capacity  = capacity;
    return params;
}

//! Формирует структуру настроек для фиксированного пула расширяемого с помощью new/delete.
//! @param chunkSize     - [in] размер блоков памяти в пуле;
//! @param capacity      - [in] исходное кол-во блоков памяти в пуле;
//! @return настройки пула.
CMemoryPool::PoolParams CMemoryPool::FixedNewDeletePoolParams(size_t chunkSize, size_t capacity)
{
    PoolParams params;
    params.type      = POOL_FIXED_NEW_DELETE;
    params.chunkSize = chunkSize;
    params.capacity  = capacity;
    return params;
}
