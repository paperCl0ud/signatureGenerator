//! @file memory/MemoryPool.h
//! Объявление класса CMemoryPool
#ifndef _SIG_GEN
#define _SIG_GEN

#define INT64_MAX    _I64_MAX
#define INTMAX_MAX   INT64_MAX

#include "../common/memory/MemoryPool.h"
#include "../includes/Crc32.h"

#include <boost/atomic.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread.hpp>

#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>
#include <stdint.h>
#include <string>


//! Класс предстваляющий генератор сигнатур.
class CSignatureGenerator
{
public:
    CSignatureGenerator();
public:
    bool Init(std::ifstream&  hInnerFile, std::ofstream& hOuterFile, size_t blockSize,
              size_t threadCnt = boost::thread::hardware_concurrency());
    void DeInit();
    void StartProcessing();    
    void WaitFinished();

private:
    bool InitPool();

private:
    void ThreadProcRead();
    void ThreadProcCrcCalc();
    void ThreadProcWrite();

private:    
    struct FileDataChunk
    {
        uint32_t                     num;  //! номер куска в файле
        boost::shared_array<uint8_t> buff; //! данные
    };

private:
    typedef std::map<uint32_t, uint32_t> CrcMap;
    typedef std::queue<FileDataChunk>    DataChackQueue;

private:
    std::ifstream*               m_pHInnerFile;
    std::ofstream*               m_pHOuterFile;

    size_t                       m_blockSize;

    CMemoryPool                  m_pool;

    CrcMap                       m_crcMap;
    DataChackQueue               m_queue;

    boost::thread_group          m_crcProcessorsThreads;
    boost::thread                m_ReaderThread;
    boost::thread                m_WriterThread;

    boost::mutex                 m_readMutex;
    boost::mutex                 m_writeMutex;

    boost::condition_variable    m_condVarFreeRead;
    boost::condition_variable    m_conVarHaveDataRead;

    boost::condition_variable    m_condVarFreeWrite;
    boost::condition_variable    m_conVarHaveDataWrite;    

    boost::atomic<bool>          m_abReadFinished;
    boost::atomic<bool>          m_abError;

    size_t                       m_maxQueueSize;
    size_t                       m_currentReadBlockNum;
    size_t                       m_currentWriteBlock;
    size_t                       m_numBlocksInFile;
    size_t                       m_inFileSize;
    size_t                       m_calkCrcThreadsNum;
};

#endif// _SIG_GEN