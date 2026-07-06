#include "MatpowerCase.h"
#include <fo/FileOperations.h>
#include <cnt/PushBackVector.h>
#include <td/StringUtils.h>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    // Strips a trailing MATLAB-style comment (starting at '%').
    td::String stripCommentAndSemicolon(const td::String& line)
    {
        std::string tmp(line.c_str());
        auto pos = tmp.find('%');
        if (pos != std::string::npos)
            tmp = tmp.substr(0, pos);
        return td::String(tmp.c_str());
    }

    // Reads all whitespace/comma separated numeric tokens of one logical row into values.
    void tokenizeRow(const td::String& row, cnt::PushBackVector<double>& values)
    {
        cnt::PushBackVector<td::String> tokens;
        tokens.reserve(32);
        row.split(" ,;\t", tokens);
        auto nToks = tokens.size();
        for (td::UINT4 i = 0; i < nToks; ++i)
        {
            const td::String& tok = tokens[i];
            if (tok.isEmpty())
                continue;
            values.push_back() = std::atof(tok.c_str());
        }
    }

    // Reads one MATPOWER "mpc.<name> = [ ... ];" block (may span several lines) starting
    // right after the opening line has already been consumed, and fills pMat with nCols
    // columns (determined by the first non-empty row of the block).
    bool readMatrixBlock(fo::InFile& inFile, dense::DblMatrix& mat, td::String& status)
    {
        fo::LineNormal buffer;
        cnt::PushBackVector<double> allValues;
        allValues.reserve(4096);
        td::UINT4 nCols = 0;
        td::UINT4 nRows = 0;

        while (fo::getLine(inFile, buffer))
        {
            td::String line(buffer.c_str());
            td::String content = stripCommentAndSemicolon(line);
            bool isEnd = (strchr(content.c_str(), ']') != nullptr);
            content = content.replace("]", " ");

            cnt::PushBackVector<double> rowValues;
            rowValues.reserve(32);
            tokenizeRow(content, rowValues);

            if (rowValues.size() > 0)
            {
                if (nCols == 0)
                    nCols = rowValues.size();
                else if (rowValues.size() != nCols)
                {
                    status = "ERROR! Inconsistent number of columns in MATPOWER data block";
                    return false;
                }
                for (td::UINT4 i = 0; i < nCols; ++i)
                    allValues.push_back() = rowValues[i];
                ++nRows;
            }

            if (isEnd)
                break;
        }

        if (nRows == 0 || nCols == 0)
        {
            status = "ERROR! Empty MATPOWER data block";
            return false;
        }

        mat.reserve(nRows, nCols);
        auto m = mat.getManipulator();
        td::UINT4 k = 0;
        for (td::UINT4 r = 0; r < nRows; ++r)
            for (td::UINT4 c = 0; c < nCols; ++c)
                m(r, c) = allValues[k++];

        return true;
    }
}

bool MatpowerCase::loadFromFile(const td::String& fileName, td::String& status)
{
    fo::InFile inFile;
    if (!fo::openExistingBinaryFile(inFile, fileName))
    {
        status = "ERROR! Cannot open MATPOWER case file!";
        return false;
    }

    bool gotBaseMVA = false, gotBus = false, gotGen = false, gotBranch = false;

    fo::LineNormal buffer;
    while (fo::getLine(inFile, buffer))
    {
        td::String line(buffer.c_str());
        const char* pBuff = td::findFirstNonWhiteSpace(line.c_str());
        if (!pBuff || *pBuff == 0 || *pBuff == '%')
            continue;

        if (strstr(pBuff, "mpc.baseMVA"))
        {
            const char* pEq = strchr(pBuff, '=');
            if (pEq)
            {
                _baseMVA = std::atof(pEq + 1);
                gotBaseMVA = true;
            }
            continue;
        }
        if (strstr(pBuff, "mpc.bus") && strstr(pBuff, "[") && !strstr(pBuff, "mpc.branch"))
        {
            if (!readMatrixBlock(inFile, _bus, status))
                return false;
            gotBus = true;
            continue;
        }
        if (strstr(pBuff, "mpc.gen") && strstr(pBuff, "[") && !strstr(pBuff, "mpc.gencost"))
        {
            if (!readMatrixBlock(inFile, _gen, status))
                return false;
            gotGen = true;
            continue;
        }
        if (strstr(pBuff, "mpc.branch") && strstr(pBuff, "["))
        {
            if (!readMatrixBlock(inFile, _branch, status))
                return false;
            gotBranch = true;
            continue;
        }
        // mpc.gencost and any other block: not needed for power-flow/SVC conversion, skip.
    }

    if (!gotBaseMVA)
    {
        status = "ERROR! mpc.baseMVA not found in the input file!";
        return false;
    }
    if (!gotBus)
    {
        status = "ERROR! mpc.bus data not found in the input file!";
        return false;
    }
    if (!gotGen)
    {
        status = "ERROR! mpc.gen data not found in the input file!";
        return false;
    }
    if (!gotBranch)
    {
        status = "ERROR! mpc.branch data not found in the input file!";
        return false;
    }

    return true;
}

int MatpowerCase::findBusRow(td::UINT4 busNumber) const
{
    auto nRows = _bus.getNoOfRows();
    auto b = _bus.getManipulator();
    for (td::UINT4 r = 0; r < nRows; ++r)
    {
        if (td::UINT4(b(r, busCol::busI)) == busNumber)
            return int(r);
    }
    return -1;
}
