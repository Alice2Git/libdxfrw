/******************************************************************************
**  libDXFrw - UTF-8 file path helper                                        **
**                                                                           **
**  Patch dxfrw_c: pe Windows, std::fstream::open(const char*) interpreteaza **
**  calea in code page-ul ANSI, deci caile cu diacritice (UTF-8) nu se pot   **
**  deschide. Helper-ul converteste calea UTF-8 in UTF-16 si foloseste       **
**  supraincarcarea wchar_t* (MSVC si MinGW libstdc++).                      **
**  Daca sirul nu e UTF-8 valid, se revine la deschiderea clasica.           **
**                                                                           **
**  This library is free software, licensed under the terms of the GNU       **
**  General Public License as published by the Free Software Foundation,     **
**  either version 2 of the License, or (at your option) any later version.  **
******************************************************************************/

#ifndef DRW_UTF8PATH_H
#define DRW_UTF8PATH_H

#include <string>
#include <ios>

#if defined(_WIN32)

/* Decodor UTF-8 -> UTF-16 strict (fara windows.h in unitatile de compilare ale bibliotecii). */
inline bool drw_utf8_to_wide(const std::string &in, std::wstring &out) {
    out.clear();
    out.reserve(in.size());
    size_t i = 0, n = in.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        unsigned long cp;
        size_t extra;
        if (c < 0x80)              { cp = c;        extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else return false;
        if (i + extra >= n) return false; /* secventa trunchiata */
        for (size_t k = 1; k <= extra; ++k) {
            unsigned char cc = static_cast<unsigned char>(in[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF))
            return false; /* overlong / surrogate / out of range */
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<wchar_t>(cp));
        }
        i += extra + 1;
    }
    return true;
}

template <class Stream>
inline void drw_open_file(Stream &stream, const std::string &utf8Name, std::ios_base::openmode mode) {
    std::wstring wide;
    if (drw_utf8_to_wide(utf8Name, wide))
        stream.open(wide.c_str(), mode);
    else
        stream.open(utf8Name.c_str(), mode);
}

#else

template <class Stream>
inline void drw_open_file(Stream &stream, const std::string &utf8Name, std::ios_base::openmode mode) {
    stream.open(utf8Name.c_str(), mode);
}

#endif

#endif // DRW_UTF8PATH_H
