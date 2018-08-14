//! @file main.cpp
//! Точка входа в приложение main().

#include "signatureGenerator.h"
#include <stdint.h>

//! Размер блока чтения по-умолчанию 1Мб
const size_t DEFAULT_READ_BLOCK_SIZE      = 1024 * 1024;
//! Кол-во байтов в килобайте
const size_t BYTES_IN_KYLOBYTE            = 1024;
//! Максимальный раззмер блока чтения 
const size_t MAX_READ_BLOCK_SIZE_KB       = DEFAULT_READ_BLOCK_SIZE * 64;
//! Имя выходного файла по-умолчанию
const std::string DEFAULTOUTPUT_FILE_NAME = "./output.bin";


//! Возвращает header входного файла
//! @param inFileNamem        - [in]  имя выходного файла из argc
//! @param hInputFile         - [out] header входного файла
//! @return true - успех, false - в случае ошибки.
bool SetInputFile(std::string inFileNamem, std::ifstream& hInputFile)
{
    std::ios::iostate oldExIn = hInputFile.exceptions();
    hInputFile.exceptions(std::ios::failbit | std::ios::badbit);

    try
    {
        if (inFileNamem.empty())
        {
            std::cout << "Please enter full path for inner file:" << std::endl;
            std::cin.unsetf(std::ios::skipws);

            getline(std::cin, inFileNamem);
        }

        hInputFile.open(inFileNamem.data(), std::ios::out | std::ios::binary);
    }
    catch (std::ios::ios_base::failure &err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        std::cerr << "Unable to open Input file" << std::endl;

        hInputFile.exceptions(oldExIn);

        return false;
    }
    catch (std::length_error &)
    {
        std::cout << "File name too long" << std::endl;

        return false;
    }

    return true;
}


//! Возвращает header выходного файла
//! @param inFileNamem        - [in]  имя выходного файла из argc
//! @param hInputFile         - [out] header выходного файла
//! @return true - успех, false - в случае ошибки.
bool SetOutputFile(std::string& outFileName, std::ofstream& hOuteputFile)
{
    std::ios::iostate oldExOut = hOuteputFile.exceptions();
    hOuteputFile.exceptions(std::ios::failbit | std::ios::badbit);

    try
    {
        if (outFileName.empty())
        {
            std::cout << "Please enter the path to the output file: " << std::endl;
            getline(std::cin, outFileName);

            if (outFileName.empty())
            {
                std::cout << "The file will be created in the default directory!" << std::endl;
                std::cout << "Defailt file name is output.bin" << std::endl;
                outFileName.assign("./output.bin");
            }
        }

        hOuteputFile.open(outFileName, std::ios::in | std::ios::binary | std::ios::app);
    }
    catch (std::ios::ios_base::failure &error)
    {
        std::cerr << "Error: " << error.what() << std::endl;

        hOuteputFile.exceptions(oldExOut);

        return false;
    }

    return true;
}


//! Возвращает размер блока чтения
//! @param blockSize        - [in/out] размер блока чтения
//! @return true - успех, false - в случае ошибки.
bool SetBlockSize(size_t& blockSize)
{
    blockSize = blockSize * BYTES_IN_KYLOBYTE;

    try
    {
        if (blockSize == 0)
        {
            std::cout << "Please Enter size of block in Kb [min = 1Kb; max = 65536Kb]: ";
            std::cout << "Please Enter size of block in Kb: ";
            std::cin  >> blockSize;

            if (blockSize == 0)
            {
                blockSize = DEFAULT_READ_BLOCK_SIZE;
                std::cout << "Block size set as defailt: 1Mb" << std::endl;
            }

            if (blockSize > MAX_READ_BLOCK_SIZE_KB)
            {
                std::cout << "Block size is too big" << std::endl;
                return false;
            }

            blockSize *= BYTES_IN_KYLOBYTE;
        }
    }
    catch (std::ios::ios_base::failure &error)
    {
        std::cerr << "Error: " << error.what() << std::endl;
        return false;
    }

    return true;
}


//! Запрашивает у пользователя повтор ввода
//! @param str        - [in] текст сообщения
//! @return true - повтор, false - выход.
bool isRepeatInput(std::string str)
{
    std::cout << "Do you want to enter " << str << " again? YES/NO " << std::endl;

    std::string answer;
    getline(std::cin, answer);

    if ((answer[0] != 'Y') || (answer[0] != 'y'))
    {
        return false;
    }

    return true;
}


//! Точка входа
int main(int argc, char *argv[])
{
    std::string inputFileName;
    std::string outputFileName;
    size_t      blockSize = 0;

    if (argc > 1)
    {
        inputFileName = argv[1];
    }
    if (argc > 2)
    {        
        outputFileName = argv[2];        
    }
    if (argc > 3) 
    {        
        blockSize      = atoi(argv[3]);
    }

    std::ifstream hInFile;

    while (!SetInputFile(inputFileName, hInFile))
    {
        if (!isRepeatInput("input filename"))
        {
            return 0;
        }
    }

    std::ofstream hOutFile;

    while (!SetOutputFile(outputFileName, hOutFile))
    {
        if (!isRepeatInput("output filename"))
        {
            return 0;
        }
    }

    while (!SetBlockSize(blockSize))
    {
        if (!isRepeatInput("blockSize"))
        {
            return 0;
        }
    }

    CSignatureGenerator signGen;

    while (!signGen.Init(hInFile, hOutFile, blockSize))
    {
        return 0;     
    }

    signGen.StartProcessing();
    signGen.WaitFinished();

    hInFile.close();
    hOutFile.close();

    std::cin.ignore();

    return 0;
}