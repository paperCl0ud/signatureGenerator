//! @file MemoryPool.cpp
//! Реализация класса CMemoryPool
#include "SignatureGenerator.h"

//! Максимальный размер карты рассчитанных значний CRC
const uint32_t MAX_MAP_SIZE = 50;

//! Конструктор.
CSignatureGenerator::CSignatureGenerator() : 
            m_currentReadBlockNum(0), m_abReadFinished(0),    m_abError(0), 
            m_calkCrcThreadsNum(0),   m_inFileSize(0),        m_blockSize(0),
            m_maxQueueSize(0),        m_currentWriteBlock(0), m_numBlocksInFile(0)
{
}

//! Инициализация CSignatureGenerator
//! @param hInnerFile        - [in] входной файл
//! @param hOuterFile        - [in] выходной файл
//! @param blockSize         - [in] размер блока чтения 
//! @param numCrcCalcThreads - [in] кол-во потоков для рассчета CRC
//! @return true - инициализация успешна, false - в случае ошибки.
bool CSignatureGenerator::Init(std::ifstream& hInnerFile, std::ofstream& hOuterFile, 
                               size_t blockSize, size_t numCrcCalcThreads)
{  
    m_pHInnerFile = &hInnerFile;
    m_pHOuterFile = &hOuterFile;
    m_blockSize   = blockSize;

    m_pHInnerFile->seekg(0, m_pHInnerFile->end);
    m_inFileSize = static_cast<size_t>(m_pHInnerFile->tellg());
    m_pHInnerFile->seekg(0, m_pHInnerFile->beg);

    m_numBlocksInFile = m_inFileSize / m_blockSize;
    if (m_inFileSize % m_blockSize)
    {
        ++m_numBlocksInFile;
    }

    m_calkCrcThreadsNum = numCrcCalcThreads;

    m_maxQueueSize = m_calkCrcThreadsNum * 2;

    if (!InitPool())
    {
        return false;
    }    

    return true;
}

//! Запуск потоков обработки, рассчета сигнатуры
void CSignatureGenerator::StartProcessing()
{
    boost::thread threadRead(boost::bind(&CSignatureGenerator::ThreadProcRead, this));
    m_ReaderThread.swap(threadRead);

    boost::thread threadWrite(boost::bind(&CSignatureGenerator::ThreadProcWrite, this));
    m_WriterThread.swap(threadWrite);

    for (size_t i = 0; i < m_calkCrcThreadsNum; ++i)
    {
        m_crcProcessorsThreads.create_thread(boost::bind(&CSignatureGenerator::ThreadProcCrcCalc, this));
    }
}

//! Завершение потоков обработки
void CSignatureGenerator::DeInit()
{
    if (m_ReaderThread.get_id() != boost::thread::id())
    {
        m_ReaderThread.interrupt();
        m_ReaderThread.join();
    }

    if (m_WriterThread.get_id() != boost::thread::id())
    {
        m_WriterThread.interrupt();
        m_WriterThread.join();
    }

    m_crcProcessorsThreads.interrupt_all();
    m_crcProcessorsThreads.join_all();
}

//! Ожидание завершения потоков обработки
void CSignatureGenerator::WaitFinished()
{
    while (!m_abError && !m_abReadFinished)
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(50));
    }

    if (!m_abError && m_abReadFinished)
    {
        boost::unique_lock<boost::mutex> lockReadQueue(m_readMutex);

        while (!m_queue.empty() && !m_abError)
        {
            m_condVarFreeRead.wait(lockReadQueue);
        }

        lockReadQueue.unlock();

        boost::unique_lock<boost::mutex> lockWriteMap(m_writeMutex);

        while (!m_crcMap.empty() && !m_abError)
        {
            m_condVarFreeWrite.wait(lockWriteMap); 
        }

        lockWriteMap.unlock();

        std::cout << "Signatures file generation complited" << std::endl;
    }

    DeInit();
}

//! Инициализация пула пямяти
//! @return true - если удалось инициализировать пул, false - в случае ошибки.
bool CSignatureGenerator::InitPool()
{
    CMemoryPool::PoolsParams poolParams;
    poolParams.push_back(CMemoryPool::FixedPoolParams(m_blockSize,
                                                      m_maxQueueSize * 2));

    if (!m_pool.Init(CMemoryPool::POOL_FIXED, poolParams))
    {
        poolParams.clear();

        if (!m_pool.Init(CMemoryPool::POOL_FIXED_NEW_DELETE, poolParams))
        {
            return false;
        }
    }

    return true;
}

//! Тело потока чтения из файла
void CSignatureGenerator::ThreadProcRead()
{
    if (!m_pHInnerFile->is_open())
    {
        std::cerr << "Input file is close" << std::endl;
    }
    else
    {
        try
        {
            while ((m_currentReadBlockNum + 1) <= m_numBlocksInFile)
            {
                boost::this_thread::interruption_point();

                boost::shared_array<uint8_t> readBuff = m_pool.Get(m_blockSize);

                if (!readBuff.get())
                {
                    std::cerr << "Unable allocate memory " << std::endl;
                    throw "Unable allocate memory ";
                }

                uint32_t readblockSize = m_blockSize;

                if ((m_currentReadBlockNum + 1) == m_numBlocksInFile)
                {
                    if (m_inFileSize % m_blockSize)
                    {
                        readblockSize = m_inFileSize - (m_numBlocksInFile - 1) *  m_blockSize;
                    }
                }

                m_pHInnerFile->read(reinterpret_cast<char*>(readBuff.get()), readblockSize);

                if (readblockSize < m_blockSize)
                {
                    memset((readBuff.get() + (m_blockSize - readblockSize)),
                           0,
                          (m_blockSize - readblockSize));
                }

                boost::unique_lock<boost::mutex> lock(m_readMutex);

                while (m_queue.size() > m_maxQueueSize)
                {
                    m_condVarFreeRead.wait(lock);
                }

                FileDataChunk chunk;
                chunk.num = m_currentReadBlockNum++;
                chunk.buff = readBuff;

                m_queue.push(chunk);

                m_conVarHaveDataRead.notify_all();
            }

            m_abReadFinished = true;
            return;
        }
        catch (boost::thread_interrupted&)
        {
        }
        catch (std::ios::ios_base::failure &)
        {
            std::cerr << "File read error" << std::endl;
        }
        catch (...)
        {            
        }
    }

    m_abError = true;
}

//! Тело потока рассчета CRC
void CSignatureGenerator::ThreadProcCrcCalc()
{
    try
    {
        while (true)
        {
            boost::this_thread::interruption_point();

            boost::unique_lock<boost::mutex> lockReadQueue(m_readMutex);       

            while (m_queue.empty())
            {                
                m_conVarHaveDataRead.wait(lockReadQueue);
            }

            FileDataChunk chunk = m_queue.front();

            m_queue.pop();

            m_condVarFreeRead.notify_all();

            lockReadQueue.unlock();

            uint32_t crc32 = CalcCrc32(chunk.buff.get(), m_blockSize);

            boost::unique_lock<boost::mutex> lockWriteMap(m_writeMutex);

            while (m_crcMap.size() > MAX_MAP_SIZE)
            {
                m_condVarFreeWrite.wait(lockWriteMap);
            }

            m_crcMap[chunk.num] = crc32;
            m_conVarHaveDataWrite.notify_all();

            lockWriteMap.unlock();
        }

        return;
    }
    catch (...)
    {
        m_abError = true;
        m_condVarFreeRead.notify_all();
    }    
}

//! Тело потока записи в файл
void CSignatureGenerator::ThreadProcWrite()
{
    if (!m_pHOuterFile->is_open())
    {
        std::cerr << "Output file is close" << std::endl;
    }
    else
    {
        try
        {
            while (true)
            {
                boost::this_thread::interruption_point();

                boost::unique_lock<boost::mutex> lockWriteMap(m_writeMutex);

                while (m_crcMap.empty())
                {
                    m_conVarHaveDataWrite.wait(lockWriteMap);
                }

                if (m_crcMap.begin()->first != m_currentWriteBlock)
                {
                    boost::this_thread::sleep(boost::posix_time::milliseconds(20));
                    continue;
                }

                uint32_t crc32 = m_crcMap.begin()->second;    

                m_crcMap.erase(m_crcMap.begin());

                m_condVarFreeWrite.notify_all();

                m_currentWriteBlock++;

                lockWriteMap.unlock();

                m_pHOuterFile->write(reinterpret_cast<char*>(&crc32), sizeof(uint32_t));

                if (m_currentWriteBlock == m_numBlocksInFile) 
                {
                    break;
                }
            }           

            return;
        }
        catch (boost::thread_interrupted&)
        {
            
        }
        catch (std::ios::ios_base::failure&)
        {
            std::cerr << "File write error" << std::endl;    
        }
        catch (...)
        {            
        }
    }

    m_condVarFreeWrite.notify_all();
    m_abError = true;
}