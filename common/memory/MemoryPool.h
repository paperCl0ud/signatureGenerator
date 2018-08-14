//! @file memory/MemoryPool.h
//! Объявление класса CMemoryPool

#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

#include <stdint.h>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include <vector>

class CMemoryPoolImpl;

//! Класс предстваляющий пул памяти.
class CMemoryPool
{
public:
    enum EType
    {
        POOL_EXPANDABLE,        //!< Расширяемый пул
        POOL_FIXED,             //!< Фиксированный пул
        POOL_FIXED_NEW_DELETE   //!< Фиксированный пул + при полном заполнении используется
                                //!  new/delete
    };

    struct PoolParams
    {
        EType  type;
        size_t chunkSize;
        size_t capacity;
    };

    typedef std::vector<PoolParams> PoolsParams;

public:
    CMemoryPool();
    ~CMemoryPool();

    bool Init(EType type, const PoolsParams& poolParams, bool doPreallocate = true,
              size_t chunkStep = 0);

    void Clear();

    boost::shared_array<uint8_t> Get(size_t size);

    static PoolParams ExpandablePoolParams(size_t chunkSize, size_t capacity);
    static PoolParams FixedPoolParams(size_t chunkSize, size_t capacity);
    static PoolParams FixedNewDeletePoolParams(size_t chunkSize, size_t capacity);

private:
    CMemoryPool(const CMemoryPool&);
    CMemoryPool& operator=(const CMemoryPool&);

private:
    boost::shared_ptr<CMemoryPoolImpl> m_pPoolImpl;
};

#endif // _MEMORY_POOL_H