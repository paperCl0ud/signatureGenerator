//! @file main.cpp
//! ����� ����� � ���������� main().

#include "signatureGenerator.h"
#include <stdint.h>

//! ������ ����� ������ ��-��������� 1��
const size_t DEFAULT_READ_BLOCK_SIZE      = 1024 * 1024;
//! ���-�� ������ � ���������
const size_t BYTES_IN_KYLOBYTE            = 1024;
//! ������������ ������� ����� ������ 
const size_t MAX_READ_BLOCK_SIZE_KB       = DEFAULT_READ_BLOCK_SIZE * 64;
//! ��� ��������� ����� ��-���������
const std::string DEFAULTOUTPUT_FILE_NAME = "./output.bin";


//! ���������� header �������� �����
//! @param inFileNamem        - [in]  ��� ��������� ����� �� argc
//! @param hInputFile         - [out] header �������� �����
//! @return true - �����, false - � ������ ������.
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


//! ���������� header ��������� �����
//! @param inFileNamem        - [in]  ��� ��������� ����� �� argc
//! @param hInputFile         - [out] header ��������� �����
//! @return true - �����, false - � ������ ������.
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


//! ���������� ������ ����� ������
//! @param blockSize        - [in/out] ������ ����� ������
//! @return true - �����, false - � ������ ������.
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


//! ����������� � ������������ ������ �����
//! @param str        - [in] ����� ���������
//! @return true - ������, false - �����.
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


//! ����� �����
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