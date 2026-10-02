/******************************************************************************
**  libDXFrw - Library to read/write DXF files (ascii & binary)              **
**                                                                           **
**  Copyright (C) 2011-2015 José F. Soriano, rallazz@gmail.com               **
**                                                                           **
**  This library is free software, licensed under the terms of the GNU       **
**  General Public License as published by the Free Software Foundation,     **
**  either version 2 of the License, or (at your option) any later version.  **
**  You should have received a copy of the GNU General Public License        **
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.    **
******************************************************************************/

#include <cstdlib>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstring>
#include <sstream>
#include "dxfwriter.h"

//RLZ TODO change std::endl to x0D x0A (13 10)
/*bool dxfWriter::readRec(int *codeData, bool skip) {
//    std::string text;
    int code;

#ifdef DRW_DBG
    count = count+2; //DBG
#endif

    if (!readCode(&code))
        return false;
    *codeData = code;

    if (code < 10)
        readString();
    else if (code < 60)
        readDouble();
    else if (code < 80)
        readInt();
    else if (code > 89 && code < 100) //TODO this is an int 32b
        readInt32();
    else if (code == 100 || code == 102 || code == 105)
        readString();
    else if (code > 109 && code < 150) //skip not used at the v2012
        readDouble();
    else if (code > 159 && code < 170) //skip not used at the v2012
        readInt64();
    else if (code < 180)
        readInt();
    else if (code > 209 && code < 240) //skip not used at the v2012
        readDouble();
    else if (code > 269 && code < 290) //skip not used at the v2012
        readInt();
    else if (code < 300) //TODO this is a boolean indicator, int in Binary?
        readBool();
    else if (code < 370)
        readString();
    else if (code < 390)
        readInt();
    else if (code < 400)
        readString();
    else if (code < 410)
        readInt();
    else if (code < 420)
        readString();
    else if (code < 430) //TODO this is an int 32b
        readInt32();
    else if (code < 440)
        readString();
    else if (code < 450) //TODO this is an int 32b
        readInt32();
    else if (code < 460) //TODO this is long??
        readInt();
    else if (code < 470) //TODO this is a floating point double precision??
        readDouble();
    else if (code < 481)
        readString();
    else if (code > 998 && code < 1009) //skip not used at the v2012
        readString();
    else if (code < 1060) //TODO this is a floating point double precision??
        readDouble();
    else if (code < 1071)
        readInt();
    else if (code == 1071) //TODO this is an int 32b
        readInt32();
    else if (skip)
        //skip safely this dxf entry ( ok for ascii dxf)
        readString();
    else
        //break in binary files because the conduct is unpredictable
        return false;

    return (filestr->good());
}*/

bool dxfWriter::writeUtf8String(int code, std::string text) {
    std::string t = encoder.fromUtf8(text);
    return writeString(code, t);
}

bool dxfWriter::writeSymbolName(int code, std::string text) {
    /* patch dxfrw_c: in R12 numele admit doar litere, cifre, "$", "_", "-"; "*" doar ca prefix de bloc
       anonim (*U, *D, *X). Caracterele non-ASCII raman si sunt codificate ca \\U+XXXX de encoder. */
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0x80) continue;
        if (c >= 'a' && c <= 'z') { text[i] = static_cast<char>(c - 32); continue; }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '$' || c == '_' || c == '-') continue;
        if (i == 0 && c == '*' && text.size() > 1) {
            char n = text[1];
            if (n == 'U' || n == 'u' || n == 'D' || n == 'd' || n == 'X' || n == 'x') continue;
        }
        text[i] = '_';
    }
    std::string t = encoder.fromUtf8(text);
    return writeString(code, t);
}

bool dxfWriter::writeUtf8Caps(int code, std::string text) {
    std::string strname = text;
    std::transform(strname.begin(), strname.end(), strname.begin(),::toupper);
    std::string t = encoder.fromUtf8(strname);
    return writeString(code, t);
}

bool dxfWriterBinary::writeString(int code, std::string text) {
    char bufcode[2];
    bufcode[0] =code & 0xFF;
    bufcode[1] =code  >> 8;
    filestr->write(bufcode, 2);
    *filestr << text << '\0';
    return (filestr->good());
}

/*bool dxfWriterBinary::readCode(int *code) {
    unsigned short *int16p;
    char buffer[2];
    filestr->read(buffer,2);
    int16p = (unsigned short *) buffer;
//exist a 32bits int (code 90) with 2 bytes???
    if ((*code == 90) && (*int16p>2000)){
        DBG(*code); DBG(" de 16bits\n");
        filestr->seekg(-4, std::ios_base::cur);
        filestr->read(buffer,2);
        int16p = (unsigned short *) buffer;
    }
    *code = *int16p;
    DBG(*code); DBG("\n");

    return (filestr->good());
}*/

/*bool dxfWriterBinary::readString() {
    std::getline(*filestr, strData, '\0');
    DBG(strData); DBG("\n");
    return (filestr->good());
}*/

/*bool dxfWriterBinary::readString(std::string *text) {
    std::getline(*filestr, *text, '\0');
    DBG(*text); DBG("\n");
    return (filestr->good());
}*/

/* patch dxfrw_c: in DXF binar marimea fiecarei valori este data de CODUL de grup, nu de functia apelata.
   Biblioteca apela uneori functia gresita (de ex. writeInt16 pentru 91, un intreg pe 32 de biti, la HATCH;
   writeDouble pentru 76 la LEADER); in ASCII nu conteaza, in binar tot ce urma era citit decalat.
   Scrierea binara alege acum formatul dupa tabelul de tipuri din specificatia DXF. */
namespace {
enum DrwBinType { BT_UNKNOWN, BT_STRING, BT_DOUBLE, BT_INT16, BT_INT32, BT_INT64, BT_BOOL };

DrwBinType drwBinTypeOf(int c) {
    if (c >= 0 && c <= 9) return BT_STRING;
    if (c >= 10 && c <= 59) return BT_DOUBLE;
    if (c >= 60 && c <= 79) return BT_INT16;
    if (c >= 90 && c <= 99) return BT_INT32;
    if (c == 100 || c == 102 || c == 105) return BT_STRING;
    if (c >= 110 && c <= 149) return BT_DOUBLE;
    if (c >= 160 && c <= 169) return BT_INT64;
    if (c >= 170 && c <= 179) return BT_INT16;
    if (c >= 210 && c <= 239) return BT_DOUBLE;
    if (c >= 270 && c <= 289) return BT_INT16;
    if (c >= 290 && c <= 299) return BT_BOOL;
    if (c >= 300 && c <= 369) return BT_STRING;
    if (c >= 370 && c <= 389) return BT_INT16;
    if (c >= 390 && c <= 399) return BT_STRING;
    if (c >= 400 && c <= 409) return BT_INT16;
    if (c >= 410 && c <= 419) return BT_STRING;
    if (c >= 420 && c <= 429) return BT_INT32;
    if (c >= 430 && c <= 439) return BT_STRING;
    if (c >= 440 && c <= 459) return BT_INT32;
    if (c >= 460 && c <= 469) return BT_DOUBLE;
    if (c >= 470 && c <= 481) return BT_STRING;
    if (c == 999) return BT_STRING;
    if (c >= 1000 && c <= 1009) return BT_STRING;
    if (c >= 1010 && c <= 1059) return BT_DOUBLE;
    if (c >= 1060 && c <= 1070) return BT_INT16;
    if (c == 1071) return BT_INT32;
    return BT_UNKNOWN;
}

void drwPutCode(std::ofstream *f, int code) {
    char b[2] = { static_cast<char>(code & 0xFF), static_cast<char>((code >> 8) & 0xFF) };
    f->write(b, 2);
}
void drwPutLE(std::ofstream *f, unsigned long long v, int bytes) {
    char b[8];
    for (int i = 0; i < bytes; ++i) b[i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    f->write(b, bytes);
}
void drwPutDouble(std::ofstream *f, double d) {
    unsigned long long u;
    memcpy(&u, &d, 8);
    drwPutLE(f, u, 8);
}
bool drwEmit(std::ofstream *f, int code, long long iv, double dv, bool isDouble, DrwBinType callType) {
    DrwBinType t = drwBinTypeOf(code);
    if (t == BT_UNKNOWN) t = callType;
    long long ival = isDouble ? static_cast<long long>(dv < 0 ? dv - 0.5 : dv + 0.5) : iv;
    double dval = isDouble ? dv : static_cast<double>(iv);
    drwPutCode(f, code);
    switch (t) {
    case BT_DOUBLE: drwPutDouble(f, dval); break;
    case BT_INT16:  drwPutLE(f, static_cast<unsigned long long>(ival), 2); break;
    case BT_INT32:  drwPutLE(f, static_cast<unsigned long long>(ival), 4); break;
    case BT_INT64:  drwPutLE(f, static_cast<unsigned long long>(ival), 8); break;
    case BT_BOOL: { char b = static_cast<char>(ival != 0 ? 1 : 0); f->write(&b, 1); break; }
    case BT_STRING: default: {
        std::ostringstream os;
        if (isDouble) { os.precision(16); os << dval; } else os << ival;
        std::string txt = os.str();
        f->write(txt.c_str(), txt.size() + 1);
        break;
    }
    }
    return f->good();
}
} // namespace

bool dxfWriterBinary::writeInt16(int code, int data) {
    return drwEmit(filestr, code, data, 0.0, false, BT_INT16);
}

bool dxfWriterBinary::writeInt32(int code, int data) {
    return drwEmit(filestr, code, data, 0.0, false, BT_INT32);
}

bool dxfWriterBinary::writeInt64(int code, unsigned long long int data) {
    return drwEmit(filestr, code, static_cast<long long>(data), 0.0, false, BT_INT64);
}

bool dxfWriterBinary::writeDouble(int code, double data) {
    return drwEmit(filestr, code, 0, data, true, BT_DOUBLE);
}

bool dxfWriterBinary::writeBool(int code, bool data) {
    return drwEmit(filestr, code, data ? 1 : 0, 0.0, false, BT_BOOL);
}

dxfWriterAscii::dxfWriterAscii(std::ofstream *stream):dxfWriter(stream){
    filestr->precision(16);
}

bool dxfWriterAscii::writeString(int code, std::string text) {
//    *filestr << code << std::endl << text << std::endl ;
    filestr->width(3);
    *filestr << std::right << code << std::endl;
    filestr->width(0);
    *filestr << std::left << text << std::endl;
    /*    std::getline(*filestr, strData, '\0');
    DBG(strData); DBG("\n");*/
    return (filestr->good());
}

bool dxfWriterAscii::writeInt16(int code, int data) {
//    *filestr << std::right << code << std::endl << data << std::endl;
    filestr->width(3);
    *filestr << std::right << code << std::endl;
    filestr->width(5);
    *filestr << data << std::endl;
    return (filestr->good());
}

bool dxfWriterAscii::writeInt32(int code, int data) {
    return writeInt16(code, data);
}

bool dxfWriterAscii::writeInt64(int code, unsigned long long int data) {
//    *filestr << code << std::endl << data << std::endl;
    filestr->width(3);
    *filestr << std::right << code << std::endl;
    filestr->width(5);
    *filestr << data << std::endl;
    return (filestr->good());
}

bool dxfWriterAscii::writeDouble(int code, double data) {
//    std::streamsize prec = filestr->precision();
//    filestr->precision(12);
//    *filestr << code << std::endl << data << std::endl;
    filestr->width(3);
    *filestr << std::right << code << std::endl;
    *filestr << data << std::endl;
//    filestr->precision(prec);
    return (filestr->good());
}

//saved as int or add a bool member??
bool dxfWriterAscii::writeBool(int code, bool data) {
    *filestr << code << std::endl << data << std::endl;
    return (filestr->good());
}

