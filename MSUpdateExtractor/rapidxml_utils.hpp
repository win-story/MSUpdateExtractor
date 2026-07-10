#ifndef RAPIDXML_UTILS_HPP_INCLUDED
#define RAPIDXML_UTILS_HPP_INCLUDED

// Copyright (C) 2006, 2009 Marcin Kalicinski
// Version 1.13
// Revision $DateTime: 2009/05/13 01:46:17 $
//! \file rapidxml_utils.hpp This file contains high-level rapidxml utilities that can be useful
//! in certain simple scenarios. They should probably not be used if maximizing performance is the main objective.

#include "rapidxml.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

namespace rapidxml
{

    //! Represents data loaded from a file
    template<class Ch = char>
    class file
    {
        
    public:
        
        //! Loads file into the memory. Data will be automatically destroyed by the destructor.
        //! \param filename Filename to load.
        file(const char *filename)
        {
            using namespace std;

            // Open stream
            basic_ifstream<Ch> stream(filename, ios::binary);
            if (!stream)
                throw runtime_error(string("cannot open file ") + filename);
            stream.unsetf(ios::skipws);
            
            // Determine stream size
            stream.seekg(0, ios::end);
            size_t size = stream.tellg();
            stream.seekg(0);   
            
            // Load data and add terminating 0
            m_data.resize(size + 1);
            stream.read(&m_data.front(), static_cast<streamsize>(size));
            m_data[size] = 0;
        }

        file(const Ch* filename)
        {
            // Get the file size, so we can pre-allocate the string. HUGE speed impact.
            long length = 0;
            FILE* file;
            _wfopen_s(&file,filename, L"rb");
            fseek(file, 0, SEEK_END);
            length = ftell(file);
            fseek(file, 0, SEEK_SET);

            char* tempBuffer = new char[length + 1];
            tempBuffer[0] = 0;

            wchar_t* tempBuffer2 = new wchar_t[length + 1];
            tempBuffer2[0] = 0;
            if (fread(tempBuffer, length, 1, file) != 1) {
                delete[] tempBuffer;
                return;
            }

            fclose(file);
            if (tempBuffer[0] == (char)0xEF && tempBuffer[1] == (char)0xBB && tempBuffer[2] == (char)0xBF) {
                tempBuffer += 3;
                length -= 3;
                int unicodeSize = MultiByteToWideChar(0, 0, (LPCSTR)tempBuffer, length + 1, NULL, 0);
                MultiByteToWideChar(0, 0, (LPCSTR)tempBuffer, length + 1, tempBuffer2, sizeof(wchar_t) * unicodeSize);
                delete[] (tempBuffer-3);
            }
            else {
                int unicodeSize = MultiByteToWideChar(0, 0, (LPCSTR)tempBuffer, length + 1, NULL, 0);
                MultiByteToWideChar(0, 0, (LPCSTR)tempBuffer, length + 1, tempBuffer2, sizeof(wchar_t) * unicodeSize);
                delete[] tempBuffer;
            }
            Ch* a;
            m_data.resize(length);
            memset(&m_data[0], 0, (length+1) * sizeof(wchar_t));
            memcpy(&m_data[0], tempBuffer2, length * sizeof(wchar_t));
            delete[]tempBuffer2;
        }

        //! Loads file into the memory. Data will be automatically destroyed by the destructor
        //! \param stream Stream to load from
        file(std::basic_istream<Ch> &stream)
        {
            using namespace std;

            // Load data and add terminating 0
            stream.unsetf(ios::skipws);
            m_data.assign(istreambuf_iterator<Ch>(stream), istreambuf_iterator<Ch>());
            if (stream.fail() || stream.bad())
                throw runtime_error("error reading stream");
            m_data.push_back(0);
        }
        
        //! Gets file data.
        //! \return Pointer to data of file.
        Ch *data()
        {
            return &m_data.front();
        }

        //! Gets file data.
        //! \return Pointer to data of file.
        const Ch *data() const
        {
            return &m_data.front();
        }

        //! Gets file data size.
        //! \return Size of file data, in characters.
        std::size_t size() const
        {
            return m_data.size();
        }

    private:

        std::vector<Ch> m_data;   // File data

    };

    //! Counts children of node. Time complexity is O(n).
    //! \return Number of children of node
    template<class Ch>
    inline std::size_t count_children(xml_node<Ch> *node)
    {
        xml_node<Ch> *child = node->first_node();
        std::size_t count = 0;
        while (child)
        {
            ++count;
            child = child->next_sibling();
        }
        return count;
    }

    //! Counts attributes of node. Time complexity is O(n).
    //! \return Number of attributes of node
    template<class Ch>
    inline std::size_t count_attributes(xml_node<Ch> *node)
    {
        xml_attribute<Ch> *attr = node->first_attribute();
        std::size_t count = 0;
        while (attr)
        {
            ++count;
            attr = attr->next_attribute();
        }
        return count;
    }

}

#endif
