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


#include "libdxfrw.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cassert>
#include "intern/drw_textcodec.h"
#include "intern/dxfreader.h"
#include "intern/dxfwriter.h"
#include "intern/drw_utf8path.h"

/* patch dxfrw_c: grosimea (39) si vectorul de extrudare (210) erau citite, dar nu erau scrise
   pentru POINT, LINE, CIRCLE, ARC, ELLIPSE si LWPOLYLINE (entitatile oglindite isi pierdeau orientarea). */
static void drwWriteThickness(dxfWriter *writer, double thickness) {
    if (thickness != 0.0)
        writer->writeDouble(39, thickness);
}
static void drwWriteExtrusion(dxfWriter *writer, const DRW_Coord &ext) {
    if (ext.x != 0.0 || ext.y != 0.0 || ext.z != 1.0) {
        writer->writeDouble(210, ext.x);
        writer->writeDouble(220, ext.y);
        writer->writeDouble(230, ext.z);
    }
}
#include "intern/drw_dbg.h"

#define FIRSTHANDLE 48

/*enum sections {
    secUnknown,
    secHeader,
    secTables,
    secBlocks,
    secEntities,
    secObjects
};*/

dxfRW::dxfRW(const char* name){
    DRW_DBGSL(DRW_dbg::NONE);
    fileName = name;
    reader = NULL;
    writer = NULL;
    applyExt = false;
    foundEof = false; /* patch dxfrw_c */
    haveMLeaders = false; /* patch dxfrw_c */
    elParts = 128; //parts munber when convert ellipse to polyline
}
dxfRW::~dxfRW(){
    if (reader != NULL)
        delete reader;
    if (writer != NULL)
        delete writer;
    for (std::vector<DRW_ImageDef*>::iterator it=imageDef.begin(); it!=imageDef.end(); ++it)
        delete *it;

    imageDef.clear();
}

void dxfRW::setDebug(DRW::DBG_LEVEL lvl){
    switch (lvl){
    case DRW::DEBUG:
        DRW_DBGSL(DRW_dbg::DEBUG);
        break;
    default:
        DRW_DBGSL(DRW_dbg::NONE);
    }
}

bool dxfRW::read(DRW_Interface *interface_, bool ext){
    drw_assert(fileName.empty() == false);
    bool isOk = false;
    applyExt = ext;
    std::ifstream filestr;
    if ( interface_ == NULL )
                return isOk;
    DRW_DBG("dxfRW::read 1def\n");
    drw_open_file(filestr, fileName, std::ios_base::in | std::ios::binary);
    if (!filestr.is_open())
        return isOk;
    if (!filestr.good())
        return isOk;

    char line[22];
    char line2[22] = "AutoCAD Binary DXF\r\n";
    line2[20] = (char)26;
    line2[21] = '\0';
    filestr.read (line, 22);
    filestr.close();
    iface = interface_;
    DRW_DBG("dxfRW::read 2\n");
    if (strcmp(line, line2) == 0) {
        drw_open_file(filestr, fileName, std::ios_base::in | std::ios::binary);
        binFile = true;
        //skip sentinel
        filestr.seekg (22, std::ios::beg);
        reader = new dxfReaderBinary(&filestr);
        DRW_DBG("dxfRW::read binary file\n");
    } else {
        binFile = false;
        drw_open_file(filestr, fileName, std::ios_base::in);
        reader = new dxfReaderAscii(&filestr);
    }

    isOk = processDxf();
    filestr.close();
    delete reader;
    reader = NULL;
    return isOk;
}

bool dxfRW::write(DRW_Interface *interface_, DRW::Version ver, bool bin){
    bool isOk = false;
    std::ofstream filestr;
    version = ver;
    binFile = bin;
    iface = interface_;
    if (binFile) {
        drw_open_file(filestr, fileName, std::ios_base::out | std::ios::binary | std::ios::trunc);
        if (!filestr.is_open()) /* patch dxfrw_c: raporteaza esecul deschiderii */
            return false;
        //write sentinel
        filestr << "AutoCAD Binary DXF\r\n" << (char)26 << '\0';
        writer = new dxfWriterBinary(&filestr);
        DRW_DBG("dxfRW::read binary file\n");
    } else {
        drw_open_file(filestr, fileName, std::ios_base::out | std::ios::trunc);
        if (!filestr.is_open()) /* patch dxfrw_c: raporteaza esecul deschiderii */
            return false;
        writer = new dxfWriterAscii(&filestr);
        std::string comm = std::string("dxfrw ") + std::string(DRW_VERSION);
        writer->writeString(999, comm);
    }
    DRW_Header header;
    iface->writeHeader(header);
    writer->writeString(0, "SECTION");
    entCount =FIRSTHANDLE;
    /* patch dxfrw_c: handle-urile fixe ale tipurilor de linie obligatorii */
    styleHandleMap.clear();
    ltypeHandleMap.clear();
    attdefHandleMap.clear();
    currentBlock.clear();
    ltypeHandleMap["BYBLOCK"] = "14";
    ltypeHandleMap["BYLAYER"] = "15";
    ltypeHandleMap["CONTINUOUS"] = "16";
    header.write(writer, version);
    writer->writeString(0, "ENDSEC");
    if (ver > DRW::AC1009) {
        writer->writeString(0, "SECTION");
        writer->writeString(2, "CLASSES");
        /* patch dxfrw_c: clasele pentru MULTILEADER si stilul lui (valorile scrise de AutoCAD) */
        if (haveMLeaders && version > DRW::AC1018) {
            writer->writeString(0, "CLASS");
            writer->writeString(1, "MLEADERSTYLE");
            writer->writeString(2, "AcDbMLeaderStyle");
            writer->writeString(3, "ACDB_MLEADERSTYLE_CLASS");
            writer->writeInt32(90, 4095);
            writer->writeInt32(91, 0);
            writer->writeInt16(280, 0);
            writer->writeInt16(281, 0);
            writer->writeString(0, "CLASS");
            writer->writeString(1, "MULTILEADER");
            writer->writeString(2, "AcDbMLeader");
            writer->writeString(3, "ACDB_MLEADER_CLASS");
            writer->writeInt32(90, 3071);
            writer->writeInt32(91, 0);
            writer->writeInt16(280, 0);
            writer->writeInt16(281, 1);
        }
        writer->writeString(0, "ENDSEC");
    }
    writer->writeString(0, "SECTION");
    writer->writeString(2, "TABLES");
    writeTables();
    writer->writeString(0, "ENDSEC");
    writer->writeString(0, "SECTION");
    writer->writeString(2, "BLOCKS");
    writeBlocks();
    writer->writeString(0, "ENDSEC");

    writer->writeString(0, "SECTION");
    writer->writeString(2, "ENTITIES");
    currentBlock.clear(); /* patch dxfrw_c */
    iface->writeEntities();
    writer->writeString(0, "ENDSEC");

    if (version > DRW::AC1009) {
        writer->writeString(0, "SECTION");
        writer->writeString(2, "OBJECTS");
        writeObjects();
        writer->writeString(0, "ENDSEC");
    }
    writer->writeString(0, "EOF");
    filestr.flush();
    isOk = !filestr.fail(); /* patch dxfrw_c: era mereu true */
    filestr.close();
    delete writer;
    writer = NULL;
    return isOk;
}

bool dxfRW::writeEntity(DRW_Entity *ent, duint32 owner) {
    ent->handle = ++entCount;
    writer->writeString(5, toHexStr(ent->handle));
    /* patch dxfrw_c: proprietarul explicit (ATTRIB si SEQEND apartin insertiei) */
    if (owner != 0 && version > DRW::AC1009)
        writer->writeString(330, toHexStr(owner));
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbEntity");
    }
    if (ent->space == 1)
        writer->writeInt16(67, 1);
    if (version > DRW::AC1009) {
        writer->writeUtf8String(8, ent->layer);
        writer->writeUtf8String(6, ent->lineType);
    } else {
        writer->writeSymbolName(8, ent->layer);
        writer->writeSymbolName(6, ent->lineType);
    }
    writer->writeInt16(62, ent->color);
    if (version > DRW::AC1015 && ent->color24 >= 0) {
        writer->writeInt32(420, ent->color24);
    }
    if (version > DRW::AC1014) {
        writer->writeInt16(370, DRW_LW_Conv::lineWidth2dxfInt(ent->lWeight));
    }
    /* patch dxfrw_c: proprietati comune citite dar nescrise de biblioteca */
    if (version > DRW::AC1009) {
        if (ent->ltypeScale != 1.0)
            writer->writeDouble(48, ent->ltypeScale);
        if (!ent->visible)
            writer->writeInt16(60, 1);
    }
    if (version > DRW::AC1015) {
        if (!ent->colorName.empty())
            writer->writeUtf8String(430, ent->colorName);
        if (ent->transparency != DRW::Opaque)
            writer->writeInt32(440, ent->transparency);
    }
    return true;
}

bool dxfRW::writeLineType(DRW_LType *ent){
    std::string strname = ent->name;

    transform(strname.begin(), strname.end(), strname.begin(),::toupper);
//do not write linetypes handled by library
    if (strname == "BYLAYER" || strname == "BYBLOCK" || strname == "CONTINUOUS") {
        return true;
    }
    writer->writeString(0, "LTYPE");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, toHexStr(++entCount));
        ltypeHandleMap[strname] = toHexStr(entCount); /* patch dxfrw_c: pentru referintele din MULTILEADER */
        if (version > DRW::AC1012) {
            writer->writeString(330, "5");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbLinetypeTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else
        writer->writeSymbolName(2, ent->name);
    writer->writeInt16(70, ent->flags);
    writer->writeUtf8String(3, ent->desc);
    ent->update();
    writer->writeInt16(72, 65);
    writer->writeInt16(73, ent->size);
    writer->writeDouble(40, ent->length);

    for (unsigned int i = 0;  i< ent->path.size(); i++){
        writer->writeDouble(49, ent->path.at(i));
        if (version > DRW::AC1009) {
            writer->writeInt16(74, 0);
        }
    }
    return true;
}

bool dxfRW::writeLayer(DRW_Layer *ent){
    writer->writeString(0, "LAYER");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (!wlayer0 && ent->name == "0") {
        wlayer0 = true;
        if (version > DRW::AC1009) {
            writer->writeString(5, "10");
        }
    } else {
        if (version > DRW::AC1009) {
            writer->writeString(5, toHexStr(++entCount));
        }
    }
    if (version > DRW::AC1012) {
        writer->writeString(330, "2");
    }
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbLayerTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else {
        writer->writeSymbolName(2, ent->name);
    }
    writer->writeInt16(70, ent->flags);
    writer->writeInt16(62, ent->color);
    if (version > DRW::AC1015 && ent->color24 >= 0) {
        writer->writeInt32(420, ent->color24);
    }
    if (version > DRW::AC1009) {
        writer->writeUtf8String(6, ent->lineType);
        /* patch dxfrw_c: layerul "Defpoints" e intotdeauna neplotabil in AutoCAD; marcat plotabil (de ex.
           din DWG R14, care nu stocheaza flag-ul), fisierul bloca TrueView */
        std::string lname = ent->name;
        std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
        bool plot = ent->plotF && lname != "defpoints";
        if (!plot)
            writer->writeBool(290, false);
        writer->writeInt16(370, DRW_LW_Conv::lineWidth2dxfInt(ent->lWeight));
        writer->writeString(390, "F");
    } else
        writer->writeSymbolName(6, ent->lineType);
    if (!ent->extData.empty()){
        writeExtData(ent->extData);
    }
//    writer->writeString(347, "10012");
    return true;
}

bool dxfRW::writeTextstyle(DRW_Textstyle *ent){
    writer->writeString(0, "STYLE");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (!dimstyleStd) {
        //stringstream cause crash in OS/X, bug#3597944
        std::string name=ent->name;
        transform(name.begin(), name.end(), name.begin(), toupper);
        if (name == "STANDARD")
            dimstyleStd = true;
    }
    if (version > DRW::AC1009) {
        writer->writeString(5, toHexStr(++entCount));
        std::string up = ent->name; /* patch dxfrw_c: pentru referintele din MULTILEADER */
        transform(up.begin(), up.end(), up.begin(), ::toupper);
        styleHandleMap[up] = toHexStr(entCount);
    }

    if (version > DRW::AC1012) {
        writer->writeString(330, "2");
    }
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbTextStyleTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else {
        writer->writeSymbolName(2, ent->name);
    }
    writer->writeInt16(70, ent->flags);
    writer->writeDouble(40, ent->height);
    writer->writeDouble(41, ent->width);
    writer->writeDouble(50, ent->oblique);
    writer->writeInt16(71, ent->genFlag);
    writer->writeDouble(42, ent->lastHeight);
    if (version > DRW::AC1009) {
        writer->writeUtf8String(3, ent->font);
        writer->writeUtf8String(4, ent->bigFont);
        if (ent->fontFamily != 0)
            writer->writeInt32(1071, ent->fontFamily);
    } else {
        writer->writeUtf8Caps(3, ent->font);
        writer->writeUtf8Caps(4, ent->bigFont);
    }
    return true;
}

bool dxfRW::writeVport(DRW_Vport *ent){
    if (!dimstyleStd) {
        ent->name = "*ACTIVE";
        dimstyleStd = true;
    }
    writer->writeString(0, "VPORT");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, toHexStr(++entCount));
        if (version > DRW::AC1012)
            writer->writeString(330, "2");
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbViewportTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else
        writer->writeSymbolName(2, ent->name);
    writer->writeInt16(70, ent->flags);
    writer->writeDouble(10, ent->lowerLeft.x);
    writer->writeDouble(20, ent->lowerLeft.y);
    writer->writeDouble(11, ent->UpperRight.x);
    writer->writeDouble(21, ent->UpperRight.y);
    writer->writeDouble(12, ent->center.x);
    writer->writeDouble(22, ent->center.y);
    writer->writeDouble(13, ent->snapBase.x);
    writer->writeDouble(23, ent->snapBase.y);
    writer->writeDouble(14, ent->snapSpacing.x);
    writer->writeDouble(24, ent->snapSpacing.y);
    writer->writeDouble(15, ent->gridSpacing.x);
    writer->writeDouble(25, ent->gridSpacing.y);
    writer->writeDouble(16, ent->viewDir.x);
    writer->writeDouble(26, ent->viewDir.y);
    writer->writeDouble(36, ent->viewDir.z);
    writer->writeDouble(17, ent->viewTarget.x);
    writer->writeDouble(27, ent->viewTarget.y);
    writer->writeDouble(37, ent->viewTarget.z);
    writer->writeDouble(40, ent->height);
    writer->writeDouble(41, ent->ratio);
    writer->writeDouble(42, ent->lensHeight);
    writer->writeDouble(43, ent->frontClip);
    writer->writeDouble(44, ent->backClip);
    writer->writeDouble(50, ent->snapAngle);
    writer->writeDouble(51, ent->twistAngle);
    writer->writeInt16(71, ent->viewMode);
    writer->writeInt16(72, ent->circleZoom);
    writer->writeInt16(73, ent->fastZoom);
    writer->writeInt16(74, ent->ucsIcon);
    writer->writeInt16(75, ent->snap);
    writer->writeInt16(76, ent->grid);
    writer->writeInt16(77, ent->snapStyle);
    writer->writeInt16(78, ent->snapIsopair);
    if (version > DRW::AC1014) {
        writer->writeInt16(281, 0);
        writer->writeInt16(65, 1);
        writer->writeDouble(110, 0.0);
        writer->writeDouble(120, 0.0);
        writer->writeDouble(130, 0.0);
        writer->writeDouble(111, 1.0);
        writer->writeDouble(121, 0.0);
        writer->writeDouble(131, 0.0);
        writer->writeDouble(112, 0.0);
        writer->writeDouble(122, 1.0);
        writer->writeDouble(132, 0.0);
        writer->writeInt16(79, 0);
        writer->writeDouble(146, 0.0);
        if (version > DRW::AC1018) {
            writer->writeString(348, "10020");
            writer->writeInt16(60, ent->gridBehavior);//v2007 undocummented see DRW_Vport class
            writer->writeInt16(61, 5);
            writer->writeBool(292, 1);
            writer->writeInt16(282, 1);
            writer->writeDouble(141, 0.0);
            writer->writeDouble(142, 0.0);
            writer->writeInt16(63, 250);
            writer->writeInt32(421, 3358443);
        }
    }
    return true;
}

bool dxfRW::writeDimstyle(DRW_Dimstyle *ent){
    writer->writeString(0, "DIMSTYLE");
    if (version == DRW::AC1009) writer->writeString(105, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (!dimstyleStd) {
        std::string name = ent->name;
        std::transform(name.begin(), name.end(), name.begin(),::toupper);
        if (name == "STANDARD")
            dimstyleStd = true;
    }
    if (version > DRW::AC1009) {
        writer->writeString(105, toHexStr(++entCount));
    }

    if (version > DRW::AC1012) {
        writer->writeString(330, "A");
    }
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbDimStyleTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else
        writer->writeSymbolName(2, ent->name);
    writer->writeInt16(70, ent->flags);
    if ( version == DRW::AC1009 || !(ent->dimpost.empty()) )
        writer->writeUtf8String(3, ent->dimpost);
    if ( version == DRW::AC1009 || !(ent->dimapost.empty()) )
        writer->writeUtf8String(4, ent->dimapost);
    if ( version == DRW::AC1009 || !(ent->dimblk.empty()) )
        writer->writeUtf8String(5, ent->dimblk);
    if ( version == DRW::AC1009 || !(ent->dimblk1.empty()) )
        writer->writeUtf8String(6, ent->dimblk1);
    if ( version == DRW::AC1009 || !(ent->dimblk2.empty()) )
        writer->writeUtf8String(7, ent->dimblk2);
    writer->writeDouble(40, ent->dimscale);
    writer->writeDouble(41, ent->dimasz);
    writer->writeDouble(42, ent->dimexo);
    writer->writeDouble(43, ent->dimdli);
    writer->writeDouble(44, ent->dimexe);
    writer->writeDouble(45, ent->dimrnd);
    writer->writeDouble(46, ent->dimdle);
    writer->writeDouble(47, ent->dimtp);
    writer->writeDouble(48, ent->dimtm);
    if ( version > DRW::AC1018 ) /* patch dxfrw_c: DIMFXL exista doar din 2007 */
        writer->writeDouble(49, ent->dimfxl);
    writer->writeDouble(140, ent->dimtxt);
    writer->writeDouble(141, ent->dimcen);
    writer->writeDouble(142, ent->dimtsz);
    writer->writeDouble(143, ent->dimaltf);
    writer->writeDouble(144, ent->dimlfac);
    writer->writeDouble(145, ent->dimtvp);
    writer->writeDouble(146, ent->dimtfac);
    writer->writeDouble(147, ent->dimgap);
    if (version > DRW::AC1014) {
        writer->writeDouble(148, ent->dimaltrnd);
    }
    writer->writeInt16(71, ent->dimtol);
    writer->writeInt16(72, ent->dimlim);
    writer->writeInt16(73, ent->dimtih);
    writer->writeInt16(74, ent->dimtoh);
    writer->writeInt16(75, ent->dimse1);
    writer->writeInt16(76, ent->dimse2);
    writer->writeInt16(77, ent->dimtad);
    writer->writeInt16(78, ent->dimzin);
    if (version > DRW::AC1014) {
        writer->writeInt16(79, ent->dimazin);
    }
    writer->writeInt16(170, ent->dimalt);
    writer->writeInt16(171, ent->dimaltd);
    writer->writeInt16(172, ent->dimtofl);
    writer->writeInt16(173, ent->dimsah);
    writer->writeInt16(174, ent->dimtix);
    writer->writeInt16(175, ent->dimsoxd);
    writer->writeInt16(176, ent->dimclrd);
    writer->writeInt16(177, ent->dimclre);
    writer->writeInt16(178, ent->dimclrt);
    if (version > DRW::AC1014) {
        writer->writeInt16(179, ent->dimadec);
    }
    if (version > DRW::AC1009) {
        if (version < DRW::AC1015)
            writer->writeInt16(270, ent->dimunit);
        writer->writeInt16(271, ent->dimdec);
        writer->writeInt16(272, ent->dimtdec);
        writer->writeInt16(273, ent->dimaltu);
        writer->writeInt16(274, ent->dimalttd);
        writer->writeInt16(275, ent->dimaunit);
    }
    if (version > DRW::AC1014) {
        writer->writeInt16(276, ent->dimfrac);
        writer->writeInt16(277, ent->dimlunit);
        writer->writeInt16(278, ent->dimdsep);
        writer->writeInt16(279, ent->dimtmove);
    }
    if (version > DRW::AC1009) {
        writer->writeInt16(280, ent->dimjust);
        writer->writeInt16(281, ent->dimsd1);
        writer->writeInt16(282, ent->dimsd2);
        writer->writeInt16(283, ent->dimtolj);
        writer->writeInt16(284, ent->dimtzin);
        writer->writeInt16(285, ent->dimaltz);
        writer->writeInt16(286, ent->dimaltttz);
        if (version < DRW::AC1015)
            writer->writeInt16(287, ent->dimfit);
        writer->writeInt16(288, ent->dimupt);
    }
    if (version > DRW::AC1014) {
        writer->writeInt16(289, ent->dimatfit);
    }
    if ( version > DRW::AC1018 && ent->dimfxlon !=0 )
        writer->writeInt16(290, ent->dimfxlon);
    if (version > DRW::AC1009) {
        writer->writeUtf8String(340, ent->dimtxsty);
    }
    if (version > DRW::AC1014) {
        writer->writeUtf8String(341, ent->dimldrblk);
        writer->writeInt16(371, ent->dimlwd);
        writer->writeInt16(372, ent->dimlwe);
    }
    return true;
}

bool dxfRW::writeAppId(DRW_AppId *ent){
    std::string strname = ent->name;
    transform(strname.begin(), strname.end(), strname.begin(),::toupper);
//do not write mandatory ACAD appId, handled by library
    if (strname == "ACAD")
        return true;
    writer->writeString(0, "APPID");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, toHexStr(++entCount));
        if (version > DRW::AC1014) {
            writer->writeString(330, "9");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbRegAppTableRecord");
        writer->writeUtf8String(2, ent->name);
    } else {
        writer->writeSymbolName(2, ent->name);
    }
    writer->writeInt16(70, ent->flags);
    return true;
}

bool dxfRW::writePoint(DRW_Point *ent) {
    writer->writeString(0, "POINT");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbPoint");
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
    }
    drwWriteThickness(writer, ent->thickness);
    drwWriteExtrusion(writer, ent->extPoint);
    return true;
}

bool dxfRW::writeLine(DRW_Line *ent) {
    writer->writeString(0, "LINE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbLine");
    }
    drwWriteThickness(writer, ent->thickness);
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0 || ent->secPoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
        writer->writeDouble(31, ent->secPoint.z);
    } else {
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
    }
    drwWriteExtrusion(writer, ent->extPoint);
    return true;
}

bool dxfRW::writeRay(DRW_Ray *ent) {
    /* patch dxfrw_c: RAY nu exista in R12 (a aparut in R13), deci nu se scrie in R12 */
    if (version == DRW::AC1009)
        return true;
    writer->writeString(0, "RAY");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbRay");
    }
    DRW_Coord crd = ent->secPoint;
    crd.unitize();
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0 || ent->secPoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(11, crd.x);
        writer->writeDouble(21, crd.y);
        writer->writeDouble(31, crd.z);
    } else {
        writer->writeDouble(11, crd.x);
        writer->writeDouble(21, crd.y);
    }
    return true;
}

bool dxfRW::writeXline(DRW_Xline *ent) {
    /* patch dxfrw_c: XLINE nu exista in R12 (a aparut in R13), deci nu se scrie in R12 */
    if (version == DRW::AC1009)
        return true;
    writer->writeString(0, "XLINE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbXline");
    }
    DRW_Coord crd = ent->secPoint;
    crd.unitize();
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0 || ent->secPoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(11, crd.x);
        writer->writeDouble(21, crd.y);
        writer->writeDouble(31, crd.z);
    } else {
        writer->writeDouble(11, crd.x);
        writer->writeDouble(21, crd.y);
    }
    return true;
}

bool dxfRW::writeCircle(DRW_Circle *ent) {
    writer->writeString(0, "CIRCLE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbCircle");
    }
    drwWriteThickness(writer, ent->thickness);
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
    }
    writer->writeDouble(40, ent->radious);
    drwWriteExtrusion(writer, ent->extPoint);
    return true;
}

bool dxfRW::writeArc(DRW_Arc *ent) {
    writer->writeString(0, "ARC");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbCircle");
    }
    drwWriteThickness(writer, ent->thickness);
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0) {
        writer->writeDouble(30, ent->basePoint.z);
    }
    writer->writeDouble(40, ent->radious);
    drwWriteExtrusion(writer, ent->extPoint);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbArc");
    }
    writer->writeDouble(50, ent->staangle*ARAD);
    writer->writeDouble(51, ent->endangle*ARAD);
    return true;
}

bool dxfRW::writeEllipse(DRW_Ellipse *ent){
    //verify axis/ratio and params for full ellipse
    ent->correctAxis();
    if (version > DRW::AC1009) {
        writer->writeString(0, "ELLIPSE");
        writeEntity(ent);
        if (version > DRW::AC1009) {
            writer->writeString(100, "AcDbEllipse");
        }
        writer->writeDouble(10, ent->basePoint.x);
        writer->writeDouble(20, ent->basePoint.y);
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
        writer->writeDouble(31, ent->secPoint.z);
        drwWriteExtrusion(writer, ent->extPoint);
        writer->writeDouble(40, ent->ratio);
        writer->writeDouble(41, ent->staparam);
        writer->writeDouble(42, ent->endparam);
    } else {
        DRW_Polyline pol;
        //RLZ: copy properties
        ent->toPolyline(&pol, elParts);
        writePolyline(&pol);
        for (size_t i = 0; i < pol.vertlist.size(); ++i) /* patch dxfrw_c: vertecsii temporari nu erau eliberati */
            delete pol.vertlist[i];
        pol.vertlist.clear();
    }
    return true;
}

bool dxfRW::writeTrace(DRW_Trace *ent){
    writer->writeString(0, "TRACE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbTrace");
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(11, ent->secPoint.x);
    writer->writeDouble(21, ent->secPoint.y);
    writer->writeDouble(31, ent->secPoint.z);
    writer->writeDouble(12, ent->thirdPoint.x);
    writer->writeDouble(22, ent->thirdPoint.y);
    writer->writeDouble(32, ent->thirdPoint.z);
    writer->writeDouble(13, ent->fourPoint.x);
    writer->writeDouble(23, ent->fourPoint.y);
    writer->writeDouble(33, ent->fourPoint.z);
    /* patch dxfrw_c: grosimea si extrudarea erau citite, dar nu erau scrise (un TRACE oglindit isi pierdea orientarea) */
    drwWriteThickness(writer, ent->thickness);
    drwWriteExtrusion(writer, ent->extPoint);
    return true;
}

bool dxfRW::writeSolid(DRW_Solid *ent){
    writer->writeString(0, "SOLID");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbTrace");
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(11, ent->secPoint.x);
    writer->writeDouble(21, ent->secPoint.y);
    writer->writeDouble(31, ent->secPoint.z);
    writer->writeDouble(12, ent->thirdPoint.x);
    writer->writeDouble(22, ent->thirdPoint.y);
    writer->writeDouble(32, ent->thirdPoint.z);
    writer->writeDouble(13, ent->fourPoint.x);
    writer->writeDouble(23, ent->fourPoint.y);
    writer->writeDouble(33, ent->fourPoint.z);
    /* patch dxfrw_c: grosimea si extrudarea erau citite, dar nu erau scrise (un SOLID oglindit isi pierdea orientarea) */
    drwWriteThickness(writer, ent->thickness);
    drwWriteExtrusion(writer, ent->extPoint);
    return true;
}

bool dxfRW::write3dface(DRW_3Dface *ent){
    writer->writeString(0, "3DFACE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbFace");
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(11, ent->secPoint.x);
    writer->writeDouble(21, ent->secPoint.y);
    writer->writeDouble(31, ent->secPoint.z);
    writer->writeDouble(12, ent->thirdPoint.x);
    writer->writeDouble(22, ent->thirdPoint.y);
    writer->writeDouble(32, ent->thirdPoint.z);
    writer->writeDouble(13, ent->fourPoint.x);
    writer->writeDouble(23, ent->fourPoint.y);
    writer->writeDouble(33, ent->fourPoint.z);
    writer->writeInt16(70, ent->invisibleflag);
    return true;
}

bool dxfRW::writeLWPolyline(DRW_LWPolyline *ent){
    if (version > DRW::AC1009) {
        writer->writeString(0, "LWPOLYLINE");
        writeEntity(ent);
        if (version > DRW::AC1009) {
            writer->writeString(100, "AcDbPolyline");
        }
        ent->vertexnum = ent->vertlist.size();
        writer->writeInt32(90, ent->vertexnum);
        writer->writeInt16(70, ent->flags);
        writer->writeDouble(43, ent->width);
        if (ent->elevation != 0)
            writer->writeDouble(38, ent->elevation);
        if (ent->thickness != 0)
            writer->writeDouble(39, ent->thickness);
        for (int i = 0;  i< ent->vertexnum; i++){
            DRW_Vertex2D *v = ent->vertlist.at(i);
            writer->writeDouble(10, v->x);
            writer->writeDouble(20, v->y);
            if (v->stawidth != 0)
                writer->writeDouble(40, v->stawidth);
            if (v->endwidth != 0)
                writer->writeDouble(41, v->endwidth);
            if (v->bulge != 0)
                writer->writeDouble(42, v->bulge);
        }
        drwWriteExtrusion(writer, ent->extPoint); /* patch dxfrw_c */
    } else {
        /* patch dxfrw_c: in R12 LWPOLYLINE nu exista si era omisa; se scrie ca POLYLINE 2D */
        DRW_Polyline pol;
        pol.layer = ent->layer;
        pol.lineType = ent->lineType;
        pol.color = ent->color;
        pol.lWeight = ent->lWeight;
        pol.ltypeScale = ent->ltypeScale;
        pol.visible = ent->visible;
        pol.space = ent->space;
        pol.flags = ent->flags & 1;
        pol.basePoint.z = ent->elevation;
        pol.thickness = ent->thickness;
        pol.extPoint = ent->extPoint;
        pol.defstawidth = pol.defendwidth = ent->width;
        for (size_t i = 0; i < ent->vertlist.size(); ++i) {
            DRW_Vertex2D *v = ent->vertlist.at(i);
            DRW_Vertex vert(v->x, v->y, ent->elevation, v->bulge);
            vert.stawidth = v->stawidth;
            vert.endwidth = v->endwidth;
            pol.addVertex(vert);
        }
        writePolyline(&pol);
        for (size_t i = 0; i < pol.vertlist.size(); ++i)
            delete pol.vertlist[i];
        pol.vertlist.clear();
    }
    return true;
}

bool dxfRW::writePolyline(DRW_Polyline *ent) {
    writer->writeString(0, "POLYLINE");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        /* patch dxfrw_c: marcajele erau inversate (2D etichetat 3D) si lipseau mesh/polyface */
        if (ent->flags & 64)
            writer->writeString(100, "AcDbPolyFaceMesh");
        else if (ent->flags & 16)
            writer->writeString(100, "AcDbPolygonMesh");
        else if (ent->flags & 8)
            writer->writeString(100, "AcDb3dPolyline");
        else
            writer->writeString(100, "AcDb2dPolyline");
    } else
        writer->writeInt16(66, 1);
    writer->writeDouble(10, 0.0);
    writer->writeDouble(20, 0.0);
    writer->writeDouble(30, ent->basePoint.z);
    if (ent->thickness != 0) {
        writer->writeDouble(39, ent->thickness);
    }
    writer->writeInt16(70, ent->flags);
    if (ent->defstawidth != 0) {
        writer->writeDouble(40, ent->defstawidth);
    }
    if (ent->defendwidth != 0) {
        writer->writeDouble(41, ent->defendwidth);
    }
    if (ent->flags & 16 || ent->flags & 32 || ent->flags & 64) { /* patch dxfrw_c: si pentru polyface (64) */
        writer->writeInt16(71, ent->vertexcount);
        writer->writeInt16(72, ent->facecount);
    }
    if (ent->smoothM != 0) {
        writer->writeInt16(73, ent->smoothM);
    }
    if (ent->smoothN != 0) {
        writer->writeInt16(74, ent->smoothN);
    }
    if (ent->curvetype != 0) {
        writer->writeInt16(75, ent->curvetype);
    }
    DRW_Coord crd  = ent->extPoint;
    if (crd.x != 0 || crd.y != 0 || crd.z != 1) {
        writer->writeDouble(210, crd.x);
        writer->writeDouble(220, crd.y);
        writer->writeDouble(230, crd.z);
    }

    /* patch dxfrw_c: XDATA tine de entitatea POLYLINE, deci se scrie inaintea vertecsilor */
    if (!ent->extData.empty())
        writeExtData(ent->extData);

    int vertexnum = ent->vertlist.size();
    for (int i = 0;  i< vertexnum; i++){
        DRW_Vertex *v = ent->vertlist.at(i);
        writer->writeString(0, "VERTEX");
        writeEntity(ent);
        if (version > DRW::AC1009) { /* patch dxfrw_c: subclasa specifica a vertexului */
            if ((ent->flags & 64) && (v->flags & 128) && !(v->flags & 64)) {
                writer->writeString(100, "AcDbFaceRecord");
            } else {
                writer->writeString(100, "AcDbVertex");
                if (ent->flags & 64)
                    writer->writeString(100, "AcDbPolyFaceMeshVertex");
                else if (ent->flags & 16)
                    writer->writeString(100, "AcDbPolygonMeshVertex");
                else if (ent->flags & 8)
                    writer->writeString(100, "AcDb3dPolylineVertex");
                else
                    writer->writeString(100, "AcDb2dVertex");
            }
        }
        if ( (v->flags & 128) && !(v->flags & 64) ) {
            writer->writeDouble(10, 0);
            writer->writeDouble(20, 0);
            writer->writeDouble(30, 0);
        } else {
            writer->writeDouble(10, v->basePoint.x);
            writer->writeDouble(20, v->basePoint.y);
            writer->writeDouble(30, v->basePoint.z);
        }
        if (v->stawidth != 0)
            writer->writeDouble(40, v->stawidth);
        if (v->endwidth != 0)
            writer->writeDouble(41, v->endwidth);
        if (v->bulge != 0)
            writer->writeDouble(42, v->bulge);
        if (v->flags != 0) {
            writer->writeInt16(70, v->flags); /* patch dxfrw_c: era ent->flags */
        }
        if (v->flags & 2) {
            writer->writeDouble(50, v->tgdir);
        }
        if ( v->flags & 128 ) {
            if (v->vindex1 != 0) {
                writer->writeInt16(71, v->vindex1);
            }
            if (v->vindex2 != 0) {
                writer->writeInt16(72, v->vindex2);
            }
            if (v->vindex3 != 0) {
                writer->writeInt16(73, v->vindex3);
            }
            if (v->vindex4 != 0) {
                writer->writeInt16(74, v->vindex4);
            }
            /* patch dxfrw_c: codul 91 (identificator de vertex) exista doar din 2010 si era scris cu 0
               si pe fetele polyface, inclusiv in R12; TrueView se bloca la deschiderea acestor fisiere */
            if ( version > DRW::AC1021 && !(v->flags & 64) && v->identifier != 0 ) {
                writer->writeInt32(91, v->identifier);
            }
        }
    }
    writer->writeString(0, "SEQEND");
    writeEntity(ent);
    return true;
}

bool dxfRW::writeSpline(DRW_Spline *ent){
    if (version > DRW::AC1009) {
        writer->writeString(0, "SPLINE");
        writeEntity(ent);
        if (version > DRW::AC1009) {
            writer->writeString(100, "AcDbSpline");
        }
        /* patch dxfrw_c: normala (0,0,0) = spline neplanar, codul 210 se omite */
        if (ent->normalVec.x != 0.0 || ent->normalVec.y != 0.0 || ent->normalVec.z != 0.0) {
            writer->writeDouble(210, ent->normalVec.x);
            writer->writeDouble(220, ent->normalVec.y);
            writer->writeDouble(230, ent->normalVec.z);
        }
        writer->writeInt16(70, ent->flags);
        writer->writeInt16(71, ent->degree);
        writer->writeInt16(72, ent->nknots);
        writer->writeInt16(73, ent->ncontrol);
        writer->writeInt16(74, ent->nfit);
        writer->writeDouble(42, ent->tolknot);
        writer->writeDouble(43, ent->tolcontrol);
        //RLZ: warning check if nknots are correct and ncontrol
        for (int i = 0;  i< ent->nknots; i++){
            writer->writeDouble(40, ent->knotslist.at(i));
        }
        for (int i = 0;  i< ent->ncontrol; i++){
            DRW_Coord *crd = ent->controllist.at(i);
            writer->writeDouble(10, crd->x);
            writer->writeDouble(20, crd->y);
            writer->writeDouble(30, crd->z);
        }
        /* patch dxfrw_c: fit points, toleranta si tangente nu erau scrise */
        if (ent->nfit > 0) {
            writer->writeDouble(44, ent->tolfit);
            if (ent->tgStart.x != 0.0 || ent->tgStart.y != 0.0 || ent->tgStart.z != 0.0) {
                writer->writeDouble(12, ent->tgStart.x);
                writer->writeDouble(22, ent->tgStart.y);
                writer->writeDouble(32, ent->tgStart.z);
            }
            if (ent->tgEnd.x != 0.0 || ent->tgEnd.y != 0.0 || ent->tgEnd.z != 0.0) {
                writer->writeDouble(13, ent->tgEnd.x);
                writer->writeDouble(23, ent->tgEnd.y);
                writer->writeDouble(33, ent->tgEnd.z);
            }
            for (int i = 0;  i< ent->nfit; i++){
                DRW_Coord *crd = ent->fitlist.at(i);
                writer->writeDouble(11, crd->x);
                writer->writeDouble(21, crd->y);
                writer->writeDouble(31, crd->z);
            }
        }
    } else {
        //RLZ: TODO convert spline in polyline (not exist in acad 12)
    }
    return true;
}

bool dxfRW::writeHatch(DRW_Hatch *ent){
    if (version > DRW::AC1009) {
        writer->writeString(0, "HATCH");
        writeEntity(ent);
        writer->writeString(100, "AcDbHatch");
        writer->writeDouble(10, 0.0);
        writer->writeDouble(20, 0.0);
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(210, ent->extPoint.x);
        writer->writeDouble(220, ent->extPoint.y);
        writer->writeDouble(230, ent->extPoint.z);
        writer->writeUtf8String(2, ent->name); /* patch dxfrw_c */
        writer->writeInt16(70, ent->solid);
        /* patch dxfrw_c: legaturile spre obiectele de contur (97/330) nu sunt pastrate, deci hasura
           scrisa nu poate fi asociativa */
        writer->writeInt16(71, 0);
        ent->loopsnum = ent->looplist.size();
        writer->writeInt16(91, ent->loopsnum);
        //write paths data
        for (int i = 0;  i< ent->loopsnum; i++){
            DRW_HatchLoop *loop = ent->looplist.at(i);
            writer->writeInt16(92, loop->type);
            if ( (loop->type & 2) == 2){
                /* patch dxfrw_c: conturul-polilinie nu era scris deloc ("writeme"): dupa codul 92 urma
                   direct bucla urmatoare, deci hasura salvata ramanea FARA contur. Se scriu: 72 (are
                   bulge), 73 (inchisa), 93 (numar de vertecsi), 10/20 si 42 pentru fiecare vertex. */
                DRW_LWPolyline *pl = NULL;
                if (!loop->objlist.empty() && loop->objlist.at(0) != NULL && loop->objlist.at(0)->eType == DRW::LWPOLYLINE)
                    pl = static_cast<DRW_LWPolyline*>(loop->objlist.at(0));
                bool hasBulge = false;
                size_t nvert = pl ? pl->vertlist.size() : 0;
                for (size_t j = 0; j < nvert; ++j)
                    if (pl->vertlist.at(j)->bulge != 0.0) hasBulge = true;
                writer->writeInt16(72, hasBulge ? 1 : 0);
                writer->writeInt16(73, (pl && (pl->flags & 1)) ? 1 : 0);
                writer->writeInt32(93, static_cast<int>(nvert));
                for (size_t j = 0; j < nvert; ++j) {
                    DRW_Vertex2D *v = pl->vertlist.at(j);
                    writer->writeDouble(10, v->x);
                    writer->writeDouble(20, v->y);
                    if (hasBulge)
                        writer->writeDouble(42, v->bulge);
                }
                writer->writeInt32(97, 0);
            } else {
                //boundary path
                loop->update();
                /* patch dxfrw_c: se numara doar muchiile care se pot scrie; un numar (93) mai mare decat
                   muchiile efectiv scrise facea fisierul invalid */
                int writable = 0;
                for (int j = 0; j<loop->numedges; ++j) {
                    DRW_Entity *edge = loop->objlist.at(j);
                    if (edge != NULL && (edge->eType == DRW::LINE || edge->eType == DRW::ARC ||
                                         edge->eType == DRW::ELLIPSE || edge->eType == DRW::SPLINE))
                        ++writable;
                }
                writer->writeInt32(93, writable);
                for (int j = 0; j<loop->numedges; ++j) {
                    if (loop->objlist.at(j) == NULL) continue;
                    switch ( (loop->objlist.at(j))->eType) {
                    case DRW::LINE: {
                        writer->writeInt16(72, 1);
                        DRW_Line* l = (DRW_Line*)loop->objlist.at(j);
                        writer->writeDouble(10, l->basePoint.x);
                        writer->writeDouble(20, l->basePoint.y);
                        writer->writeDouble(11, l->secPoint.x);
                        writer->writeDouble(21, l->secPoint.y);
                        break; }
                    case DRW::ARC: {
                        writer->writeInt16(72, 2);
                        DRW_Arc* a = (DRW_Arc*)loop->objlist.at(j);
                        writer->writeDouble(10, a->basePoint.x);
                        writer->writeDouble(20, a->basePoint.y);
                        writer->writeDouble(40, a->radious);
                        writer->writeDouble(50, a->staangle*ARAD);
                        writer->writeDouble(51, a->endangle*ARAD);
                        writer->writeInt16(73, a->isccw);
                        break; }
                    case DRW::ELLIPSE: {
                        writer->writeInt16(72, 3);
                        DRW_Ellipse* a = (DRW_Ellipse*)loop->objlist.at(j);
                        a->correctAxis();
                        writer->writeDouble(10, a->basePoint.x);
                        writer->writeDouble(20, a->basePoint.y);
                        writer->writeDouble(11, a->secPoint.x);
                        writer->writeDouble(21, a->secPoint.y);
                        writer->writeDouble(40, a->ratio);
                        writer->writeDouble(50, a->staparam*ARAD);
                        writer->writeDouble(51, a->endparam*ARAD);
                        writer->writeInt16(73, a->isccw);
                        break; }
                    case DRW::SPLINE: {
                        /* patch dxfrw_c: muchia spline nu era scrisa ("writeme"), desi era numarata in
                           codul 93. La o spline rationala, z-ul punctului de control este ponderea. */
                        DRW_Spline* s = (DRW_Spline*)loop->objlist.at(j);
                        bool rational = (s->flags & 4) != 0;
                        writer->writeInt16(72, 4);
                        writer->writeInt32(94, s->degree);
                        writer->writeInt16(73, rational ? 1 : 0);
                        writer->writeInt16(74, (s->flags & 2) ? 1 : 0);
                        writer->writeInt32(95, static_cast<int>(s->knotslist.size()));
                        writer->writeInt32(96, static_cast<int>(s->controllist.size()));
                        for (size_t k = 0; k < s->knotslist.size(); ++k)
                            writer->writeDouble(40, s->knotslist.at(k));
                        for (size_t k = 0; k < s->controllist.size(); ++k) {
                            writer->writeDouble(10, s->controllist.at(k)->x);
                            writer->writeDouble(20, s->controllist.at(k)->y);
                            if (rational)
                                writer->writeDouble(42, s->controllist.at(k)->z);
                        }
                        if (version > DRW::AC1021) { //2010+
                            writer->writeInt32(97, static_cast<int>(s->fitlist.size()));
                            for (size_t k = 0; k < s->fitlist.size(); ++k) {
                                writer->writeDouble(11, s->fitlist.at(k)->x);
                                writer->writeDouble(21, s->fitlist.at(k)->y);
                            }
                            if (!s->fitlist.empty()) {
                                writer->writeDouble(12, s->tgStart.x);
                                writer->writeDouble(22, s->tgStart.y);
                                writer->writeDouble(13, s->tgEnd.x);
                                writer->writeDouble(23, s->tgEnd.y);
                            }
                        }
                        break; }
                    default:
                        break;
                    }
                }
                writer->writeInt32(97, 0);
            }
        }
        writer->writeInt16(75, ent->hstyle);
        writer->writeInt16(76, ent->hpattern);
        if (!ent->solid){
            writer->writeDouble(52, ent->angle);
            writer->writeDouble(41, ent->scale);
            writer->writeInt16(77, ent->doubleflag);
            /* patch dxfrw_c: biblioteca retine doar NUMARUL liniilor de definitie a modelului, nu si liniile
               (53/43/44/45/46/79/49). Scrierea 78=N fara linii facea fisierul invalid (AutoCAD/TrueView se
               bloca); se scrie 78=0 pana cand definitia completa va fi pastrata. */
            /* patch dxfrw_c: definitia modelului se scrie integral; inainte se anunta un numar de linii
               (78=N) fara continutul lor, ceea ce facea fisierul invalid */
            writer->writeInt16(78, static_cast<int>(ent->patternLines.size()));
            for (size_t i = 0; i < ent->patternLines.size(); ++i) {
                const DRW_HatchPatternLine& pl = ent->patternLines[i];
                writer->writeDouble(53, pl.angle);
                writer->writeDouble(43, pl.base.x);
                writer->writeDouble(44, pl.base.y);
                writer->writeDouble(45, pl.offset.x);
                writer->writeDouble(46, pl.offset.y);
                writer->writeInt16(79, static_cast<int>(pl.dashes.size()));
                for (size_t j = 0; j < pl.dashes.size(); ++j)
                    writer->writeDouble(49, pl.dashes[j]);
            }
        }
/*        if (ent->deflines > 0){
            writer->writeInt16(78, ent->deflines);
        }*/
        writer->writeInt32(98, 0);
    } else {
        //RLZ: TODO verify in acad12
    }
    return true;
}

bool dxfRW::writeLeader(DRW_Leader *ent){
    if (version > DRW::AC1009) {
        writer->writeString(0, "LEADER");
        writeEntity(ent);
        writer->writeString(100, "AcDbLeader");
        writer->writeUtf8String(3, ent->style);
        writer->writeInt16(71, ent->arrow);
        writer->writeInt16(72, ent->leadertype);
        writer->writeInt16(73, ent->flag);
        writer->writeInt16(74, ent->hookline);
        writer->writeInt16(75, ent->hookflag);
        writer->writeDouble(40, ent->textheight);
        writer->writeDouble(41, ent->textwidth);
        /* patch dxfrw_c: codul 76 (numarul de vertecsi) era scris de doua ori si ca numar real, iar
           vectorii 210..213 nu erau scrisi deloc */
        writer->writeInt16(76, static_cast<int>(ent->vertexlist.size()));
        for (unsigned int i=0; i<ent->vertexlist.size(); i++) {
            DRW_Coord *vert = ent->vertexlist.at(i);
            writer->writeDouble(10, vert->x);
            writer->writeDouble(20, vert->y);
            writer->writeDouble(30, vert->z);
        }
        writer->writeDouble(210, ent->extrusionPoint.x);
        writer->writeDouble(220, ent->extrusionPoint.y);
        writer->writeDouble(230, ent->extrusionPoint.z);
        if (ent->horizdir.x != 0.0 || ent->horizdir.y != 0.0 || ent->horizdir.z != 0.0) {
            writer->writeDouble(211, ent->horizdir.x);
            writer->writeDouble(221, ent->horizdir.y);
            writer->writeDouble(231, ent->horizdir.z);
        }
        writer->writeDouble(212, ent->offsetblock.x);
        writer->writeDouble(222, ent->offsetblock.y);
        writer->writeDouble(232, ent->offsetblock.z);
        writer->writeDouble(213, ent->offsettext.x);
        writer->writeDouble(223, ent->offsettext.y);
        writer->writeDouble(233, ent->offsettext.z);
    } else  {
        //RLZ: todo not supported by acad 12 saved as unnamed block
    }
    return true;
}
bool dxfRW::writeDimension(DRW_Dimension *ent) {
    if (version > DRW::AC1009) {
        writer->writeString(0, "DIMENSION");
        writeEntity(ent);
        writer->writeString(100, "AcDbDimension");
        if (!ent->getName().empty()){
            writer->writeUtf8String(2, ent->getName()); /* patch dxfrw_c */
        }
        writer->writeDouble(10, ent->getDefPoint().x);
        writer->writeDouble(20, ent->getDefPoint().y);
        writer->writeDouble(30, ent->getDefPoint().z);
        writer->writeDouble(11, ent->getTextPoint().x);
        writer->writeDouble(21, ent->getTextPoint().y);
        writer->writeDouble(31, ent->getTextPoint().z);
        if ( !(ent->type & 32))
            ent->type = ent->type +32;
        writer->writeInt16(70, ent->type);
        if ( !(ent->getText().empty()) )
            writer->writeUtf8String(1, ent->getText());
        writer->writeInt16(71, ent->getAlign());
        if ( ent->getTextLineStyle() != 1)
            writer->writeInt16(72, ent->getTextLineStyle());
        if ( ent->getTextLineFactor() != 1)
            writer->writeDouble(41, ent->getTextLineFactor());
        writer->writeUtf8String(3, ent->getStyle());
        if ( ent->getTextLineFactor() != 0)
            writer->writeDouble(53, ent->getDir());
        writer->writeDouble(210, ent->getExtrusion().x);
        writer->writeDouble(220, ent->getExtrusion().y);
        writer->writeDouble(230, ent->getExtrusion().z);

        switch (ent->eType) {
        case DRW::DIMALIGNED:
        case DRW::DIMLINEAR: {
            DRW_DimAligned * dd = (DRW_DimAligned*)ent;
            writer->writeString(100, "AcDbAlignedDimension");
            DRW_Coord crd = dd->getClonepoint();
            if (crd.x != 0 || crd.y != 0 || crd.z != 0) {
                writer->writeDouble(12, crd.x);
                writer->writeDouble(22, crd.y);
                writer->writeDouble(32, crd.z);
            }
            writer->writeDouble(13, dd->getDef1Point().x);
            writer->writeDouble(23, dd->getDef1Point().y);
            writer->writeDouble(33, dd->getDef1Point().z);
            writer->writeDouble(14, dd->getDef2Point().x);
            writer->writeDouble(24, dd->getDef2Point().y);
            writer->writeDouble(34, dd->getDef2Point().z);
            if (ent->eType == DRW::DIMLINEAR) {
                DRW_DimLinear * dl = (DRW_DimLinear*)ent;
                if (dl->getAngle() != 0)
                    writer->writeDouble(50, dl->getAngle());
                if (dl->getOblique() != 0)
                    writer->writeDouble(52, dl->getOblique());
                writer->writeString(100, "AcDbRotatedDimension");
            }
            break; }
        case DRW::DIMRADIAL: {
            DRW_DimRadial * dd = (DRW_DimRadial*)ent;
            writer->writeString(100, "AcDbRadialDimension");
            writer->writeDouble(15, dd->getDiameterPoint().x);
            writer->writeDouble(25, dd->getDiameterPoint().y);
            writer->writeDouble(35, dd->getDiameterPoint().z);
            writer->writeDouble(40, dd->getLeaderLength());
            break; }
        case DRW::DIMDIAMETRIC: {
            DRW_DimDiametric * dd = (DRW_DimDiametric*)ent;
            writer->writeString(100, "AcDbDiametricDimension");
            writer->writeDouble(15, dd->getDiameter1Point().x);
            writer->writeDouble(25, dd->getDiameter1Point().y);
            writer->writeDouble(35, dd->getDiameter1Point().z);
            writer->writeDouble(40, dd->getLeaderLength());
            break; }
        case DRW::DIMANGULAR: {
            DRW_DimAngular * dd = (DRW_DimAngular*)ent;
            writer->writeString(100, "AcDb2LineAngularDimension");
            writer->writeDouble(13, dd->getFirstLine1().x);
            writer->writeDouble(23, dd->getFirstLine1().y);
            writer->writeDouble(33, dd->getFirstLine1().z);
            writer->writeDouble(14, dd->getFirstLine2().x);
            writer->writeDouble(24, dd->getFirstLine2().y);
            writer->writeDouble(34, dd->getFirstLine2().z);
            writer->writeDouble(15, dd->getSecondLine1().x);
            writer->writeDouble(25, dd->getSecondLine1().y);
            writer->writeDouble(35, dd->getSecondLine1().z);
            writer->writeDouble(16, dd->getDimPoint().x);
            writer->writeDouble(26, dd->getDimPoint().y);
            writer->writeDouble(36, dd->getDimPoint().z);
            break; }
        case DRW::DIMANGULAR3P: {
            DRW_DimAngular3p * dd = (DRW_DimAngular3p*)ent;
            writer->writeDouble(13, dd->getFirstLine().x);
            writer->writeDouble(23, dd->getFirstLine().y);
            writer->writeDouble(33, dd->getFirstLine().z);
            writer->writeDouble(14, dd->getSecondLine().x);
            writer->writeDouble(24, dd->getSecondLine().y);
            writer->writeDouble(34, dd->getSecondLine().z);
            writer->writeDouble(15, dd->getVertexPoint().x);
            writer->writeDouble(25, dd->getVertexPoint().y);
            writer->writeDouble(35, dd->getVertexPoint().z);
            break; }
        case DRW::DIMORDINATE: {
            DRW_DimOrdinate * dd = (DRW_DimOrdinate*)ent;
            writer->writeString(100, "AcDbOrdinateDimension");
            writer->writeDouble(13, dd->getFirstLine().x);
            writer->writeDouble(23, dd->getFirstLine().y);
            writer->writeDouble(33, dd->getFirstLine().z);
            writer->writeDouble(14, dd->getSecondLine().x);
            writer->writeDouble(24, dd->getSecondLine().y);
            writer->writeDouble(34, dd->getSecondLine().z);
            break; }
        default:
            break;
        }
    } else  {
        //RLZ: todo not supported by acad 12 saved as unnamed block
    }
    return true;
}

bool dxfRW::writeInsert(DRW_Insert *ent){
    writer->writeString(0, "INSERT");
    writeEntity(ent);
    duint32 insertHandle = ent->handle;
    bool haveAttribs = !ent->attributes.empty();
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbBlockReference");
        if (haveAttribs)
            writer->writeInt16(66, 1); /* patch dxfrw_c: urmeaza atributele */
        writer->writeUtf8String(2, ent->name);
    } else {
        if (haveAttribs)
            writer->writeInt16(66, 1);
        writer->writeSymbolName(2, ent->name);
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(41, ent->xscale);
    writer->writeDouble(42, ent->yscale);
    writer->writeDouble(43, ent->zscale);
    writer->writeDouble(50, (ent->angle)*ARAD); //in dxf angle is writed in degrees
    writer->writeInt16(70, ent->colcount);
    writer->writeInt16(71, ent->rowcount);
    writer->writeDouble(44, ent->colspace);
    writer->writeDouble(45, ent->rowspace);
    /* patch dxfrw_c: extrudarea era citita, dar nu era scrisa: un bloc inserat oglindit (normala
       (0,0,-1)) ajungea in alt loc si cu alta orientare */
    drwWriteExtrusion(writer, ent->extPoint);
    /* patch dxfrw_c: atributele insertiei: XDATA insertiei se scrie inaintea lor, apoi fiecare ATTRIB
       (cu XDATA proprie) si SEQEND, toate cu proprietarul = insertia */
    if (haveAttribs) {
        writeEntityExtData(ent);
        for (size_t i = 0; i < ent->attributes.size(); ++i) {
            DRW_Attrib *a = &ent->attributes[i];
            a->eType = DRW::ATTRIB;
            writeAttribute(a, insertHandle);
            if (version > DRW::AC1009)
                writeEntityExtData(a);
        }
        writer->writeString(0, "SEQEND");
        writeEntity(ent, insertHandle);
    }
    return true;
}

/* patch dxfrw_c: scrierea ATTRIB/ATTDEF. Partea de text e ca la TEXT (subclasa AcDbText); urmeaza
   AcDbAttribute / AcDbAttributeDefinition: din R2010 versiunea clasei (280 = 0), textul de cerere (3,
   doar ATTDEF), eticheta (2), flag-urile (70), lungimea campului (73), alinierea verticala (74) si,
   din R2010, blocarea pozitiei (280). In R12 aceleasi coduri, fara marcaje de subclasa. */
bool dxfRW::writeAttribute(DRW_Attrib *ent, duint32 owner){
    bool def = ent->eType == DRW::ATTDEF;
    writer->writeString(0, def ? "ATTDEF" : "ATTRIB");
    writeEntity(ent, owner);
    if (def && !currentBlock.empty()) {  /* referit de atributele blocurilor din MULTILEADER */
        std::string tag = ent->tag;
        transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
        attdefHandleMap[currentBlock + "\n" + tag] = toHexStr(ent->handle);
    }
    if (version > DRW::AC1009)
        writer->writeString(100, "AcDbText");
    if (ent->thickness != 0)
        writer->writeDouble(39, ent->thickness);
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(40, ent->height);
    writer->writeUtf8String(1, ent->text);
    writer->writeDouble(50, ent->angle);
    writer->writeDouble(41, ent->widthscale);
    writer->writeDouble(51, ent->oblique);
    if (version > DRW::AC1009)
        writer->writeUtf8String(7, ent->style);
    else
        writer->writeSymbolName(7, ent->style);
    writer->writeInt16(71, ent->textgen);
    if (ent->alignH != DRW_Text::HLeft)
        writer->writeInt16(72, ent->alignH);
    if (ent->alignH != DRW_Text::HLeft || ent->alignV != DRW_Text::VBaseLine) {
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
        writer->writeDouble(31, ent->secPoint.z);
    }
    drwWriteExtrusion(writer, ent->extPoint);
    if (version > DRW::AC1009) {
        writer->writeString(100, def ? "AcDbAttributeDefinition" : "AcDbAttribute");
        if (version > DRW::AC1021)
            writer->writeInt16(280, 0);
    }
    if (def)
        writer->writeUtf8String(3, ent->prompt);
    writer->writeUtf8String(2, ent->tag);
    writer->writeInt16(70, ent->flags);
    writer->writeInt16(73, ent->fieldLength);
    if (ent->alignV != DRW_Text::VBaseLine)
        writer->writeInt16(74, ent->alignV);
    if (version > DRW::AC1021 && ent->lockPosition)
        writer->writeInt16(280, 1);
    return true;
}

bool dxfRW::writeAttdef(DRW_Attdef *ent){
    return writeAttribute(ent, 0);
}

/* patch dxfrw_c: handle-ul scris al unui bloc / al unei inregistrari de tabel, dupa nume (fara diferente
   de majuscule); "" daca nu exista */
std::string dxfRW::blockHandleOf(const std::string& name){
    if (name.empty())
        return std::string();
    std::map<std::string,int>::iterator it = blockMap.find(name);
    if (it != blockMap.end())
        return toHexStr(it->second);
    std::string up = name;
    transform(up.begin(), up.end(), up.begin(), ::toupper);
    for (it = blockMap.begin(); it != blockMap.end(); ++it) {
        std::string k = it->first;
        transform(k.begin(), k.end(), k.begin(), ::toupper);
        if (k == up)
            return toHexStr(it->second);
    }
    return std::string();
}

std::string dxfRW::tableHandleOf(const std::map<std::string, std::string>& m, const std::string& name){
    std::string up = name;
    transform(up.begin(), up.end(), up.begin(), ::toupper);
    std::map<std::string, std::string>::const_iterator it = m.find(up);
    if (it != m.end())
        return it->second;
    it = m.find("STANDARD");   /* stilul de text implicit, pentru referintele care trebuie sa existe */
    return it != m.end() ? it->second : std::string();
}

static void drwWritePoint(dxfWriter *w, int code, const DRW_Coord& p){
    w->writeDouble(code, p.x);
    w->writeDouble(code + 10, p.y);
    w->writeDouble(code + 20, p.z);
}

/* patch dxfrw_c: scrierea MULTILEADER, cu aceeasi ordine a codurilor ca AutoCAD/ezdxf. Entitatea trimite
   spre stilul "Standard" scris in OBJECTS (2B), iar toate proprietatile sunt marcate ca suprascrise
   (90), deci aspectul nu depinde de stil. Referintele la tabele si blocuri se scriu prin handle-urile
   din fisierul nou; un atribut al blocului de continut trimite spre ATTDEF-ul cu aceeasi eticheta. */
bool dxfRW::writeMLeader(DRW_MLeader *ent){
    if (version < DRW::AC1021)
        return false;   /* nu exista inainte de 2007 */
    writer->writeString(0, "MULTILEADER");
    writeEntity(ent);
    writer->writeString(100, "AcDbMLeader");
    writer->writeInt16(270, ent->classVersion > 0 ? ent->classVersion : 2);
    writer->writeString(300, "CONTEXT_DATA{");
    writer->writeDouble(40, ent->ctxScale);
    drwWritePoint(writer, 10, ent->contentBase);
    writer->writeDouble(41, ent->ctxTextHeight);
    writer->writeDouble(140, ent->ctxArrowSize);
    writer->writeDouble(145, ent->landingGap);
    writer->writeInt16(174, ent->ctxTextLeft);
    writer->writeInt16(175, ent->ctxTextRight);
    writer->writeInt16(176, ent->ctxTextAngleType);
    writer->writeInt16(177, ent->ctxTextAlignType);
    writer->writeBool(290, ent->hasText);
    if (ent->hasText) {
        writer->writeUtf8String(304, ent->text);
        drwWritePoint(writer, 11, ent->textNormal);
        std::string h = tableHandleOf(styleHandleMap, ent->textStyle);
        if (!h.empty())
            writer->writeString(340, h);
        drwWritePoint(writer, 12, ent->textLocation);
        drwWritePoint(writer, 13, ent->textDirection);
        writer->writeDouble(42, ent->textRotation);
        writer->writeDouble(43, ent->textWidth);
        writer->writeDouble(44, ent->textDefinedHeight);
        writer->writeDouble(45, ent->lineSpacingFactor);
        writer->writeInt16(170, ent->lineSpacingStyle);
        writer->writeInt32(90, ent->textColor);
        writer->writeInt16(171, ent->textAttachment);
        writer->writeInt16(172, ent->flowDirection);
        writer->writeInt32(91, ent->bgColor);
        writer->writeDouble(141, ent->bgScale);
        writer->writeInt32(92, ent->bgTransparency);
        writer->writeBool(291, ent->bgFill);
        writer->writeBool(292, ent->bgMaskFill);
        writer->writeInt16(173, ent->columnType);
        writer->writeBool(293, ent->textHeightAuto);
        writer->writeDouble(142, ent->columnWidth);
        writer->writeDouble(143, ent->columnGutter);
        writer->writeBool(294, ent->columnFlowReversed);
        for (size_t i = 0; i < ent->columnSizes.size(); ++i)
            writer->writeDouble(144, ent->columnSizes[i]);
        writer->writeBool(295, ent->wordBreak);
    }
    bool block = ent->hasBlock && !ent->hasText;
    writer->writeBool(296, block);
    if (block) {
        std::string h = blockHandleOf(ent->blockName);
        if (!h.empty())
            writer->writeString(341, h);
        drwWritePoint(writer, 14, ent->blockNormal);
        drwWritePoint(writer, 15, ent->blockLocation);
        drwWritePoint(writer, 16, ent->blockScale);
        writer->writeDouble(46, ent->blockRotation);
        writer->writeInt32(93, ent->blockColor);
        for (int i = 0; i < 16; ++i)
            writer->writeDouble(47, ent->blockTransform[i]);
    }
    drwWritePoint(writer, 110, ent->planeOrigin);
    drwWritePoint(writer, 111, ent->planeXDir);
    drwWritePoint(writer, 112, ent->planeYDir);
    writer->writeBool(297, ent->normalReversed);
    for (size_t i = 0; i < ent->leaders.size(); ++i) {
        const DRW_MLeaderRoot &r = ent->leaders[i];
        writer->writeString(302, "LEADER{");
        writer->writeBool(290, r.hasLastPoint);
        writer->writeBool(291, r.hasDogleg);
        drwWritePoint(writer, 10, r.lastPoint);
        drwWritePoint(writer, 11, r.doglegVector);
        for (size_t k = 0; k < r.breakStart.size() && k < r.breakEnd.size(); ++k) {
            drwWritePoint(writer, 12, r.breakStart[k]);
            drwWritePoint(writer, 13, r.breakEnd[k]);
        }
        writer->writeInt32(90, r.branchIndex);
        writer->writeDouble(40, r.doglegLength);
        for (size_t j = 0; j < r.lines.size(); ++j) {
            const DRW_MLeaderLine &l = r.lines[j];
            writer->writeString(304, "LEADER_LINE{");
            for (size_t k = 0; k < l.vertices.size(); ++k)
                drwWritePoint(writer, 10, l.vertices[k]);
            if (!l.breakStart.empty()) {
                writer->writeInt32(90, l.breakIndex);
                for (size_t k = 0; k < l.breakStart.size() && k < l.breakEnd.size(); ++k) {
                    drwWritePoint(writer, 11, l.breakStart[k]);
                    drwWritePoint(writer, 12, l.breakEnd[k]);
                }
            }
            writer->writeInt32(91, l.lineIndex);
            if (version > DRW::AC1021 && l.haveOverrides) {
                writer->writeInt16(170, l.lineType);
                writer->writeInt32(92, l.color);
                std::string lt = l.lineTypeName.empty() ? std::string() : tableHandleOf(ltypeHandleMap, l.lineTypeName);
                if (!lt.empty())
                    writer->writeString(340, lt);
                writer->writeInt16(171, l.lineWeight);
                writer->writeDouble(40, l.arrowSize);
                std::string ab = blockHandleOf(l.arrowBlock);
                if (!ab.empty())
                    writer->writeString(341, ab);
                writer->writeInt32(93, l.flags);
            } else
                writer->writeInt32(92, l.color);
            writer->writeString(305, "}");
        }
        if (version > DRW::AC1021)
            writer->writeInt16(271, r.attachDir);
        writer->writeString(303, "}");
    }
    if (version > DRW::AC1021) {
        writer->writeInt16(272, ent->ctxTextBottom);
        writer->writeInt16(273, ent->ctxTextTop);
    }
    writer->writeString(301, "}");
    /* proprietatile entitatii */
    writer->writeString(340, "2B");
    writer->writeInt32(90, 0x7FFFFFFF);
    writer->writeInt16(170, ent->leaderType);
    writer->writeInt32(91, ent->lineColor);
    std::string lt = tableHandleOf(ltypeHandleMap, ent->leaderLineType.empty() ? std::string("ByBlock") : ent->leaderLineType);
    if (!lt.empty())
        writer->writeString(341, lt);
    writer->writeInt16(171, ent->leaderLineWeight);
    writer->writeBool(290, ent->landingEnabled);
    writer->writeBool(291, ent->doglegEnabled);
    writer->writeDouble(41, ent->landingDistance);
    std::string ab = blockHandleOf(ent->arrowBlock);
    if (!ab.empty())
        writer->writeString(342, ab);
    writer->writeDouble(42, ent->arrowSize);
    writer->writeInt16(172, ent->contentType);
    std::string ts = tableHandleOf(styleHandleMap, ent->entTextStyle);
    if (!ts.empty())
        writer->writeString(343, ts);
    writer->writeInt16(173, ent->textLeftAttach);
    writer->writeInt16(95, ent->textRightAttach);
    writer->writeInt16(174, ent->textAngleType);
    writer->writeInt16(175, ent->textAlignType);
    writer->writeInt32(92, ent->entTextColor);
    writer->writeBool(292, ent->textFrame);
    std::string eb = blockHandleOf(ent->entBlock);
    if (!eb.empty())
        writer->writeString(344, eb);
    writer->writeInt32(93, ent->entBlockColor);
    drwWritePoint(writer, 10, ent->entBlockScale);
    writer->writeDouble(43, ent->entBlockRotation);
    writer->writeInt16(176, ent->blockConnection);
    writer->writeBool(293, ent->annotative);
    std::string contentBlock = ent->blockName.empty() ? ent->entBlock : ent->blockName;
    std::transform(contentBlock.begin(), contentBlock.end(), contentBlock.begin(), ::toupper);
    for (size_t i = 0; i < ent->blockAttribs.size(); ++i) {
        const DRW_MLeaderBlockAttr &a = ent->blockAttribs[i];
        std::string tag = a.tag;
        std::transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
        std::map<std::string, std::string>::iterator it = attdefHandleMap.find(contentBlock + "\n" + tag);
        if (it == attdefHandleMap.end())
            continue;   /* fara ATTDEF-ul corespunzator valoarea nu poate fi legata de bloc */
        writer->writeString(330, it->second);
        writer->writeInt16(177, a.index);
        writer->writeDouble(44, a.width);
        writer->writeUtf8String(302, a.text);
    }
    writer->writeBool(294, ent->textDirNegative);
    writer->writeInt16(178, ent->ipeAlign);
    writer->writeInt16(179, ent->justification);
    writer->writeDouble(45, ent->scale);
    if (version > DRW::AC1021) {
        writer->writeInt16(271, ent->textAttachDir);
        writer->writeInt16(272, ent->textBottomAttach);
        writer->writeInt16(273, ent->textTopAttach);
    }
    if (version > DRW::AC1024)
        writer->writeBool(295, ent->extendToText);
    return true;
}

bool dxfRW::writeText(DRW_Text *ent){
    writer->writeString(0, "TEXT");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbText");
    }
//    writer->writeDouble(39, ent->thickness);
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(40, ent->height);
    writer->writeUtf8String(1, ent->text);
    writer->writeDouble(50, ent->angle);
    writer->writeDouble(41, ent->widthscale);
    writer->writeDouble(51, ent->oblique);
    if (version > DRW::AC1009)
        writer->writeUtf8String(7, ent->style);
    else
        writer->writeSymbolName(7, ent->style);
    writer->writeInt16(71, ent->textgen);
    if (ent->alignH != DRW_Text::HLeft) {
        writer->writeInt16(72, ent->alignH);
    }
    if (ent->alignH != DRW_Text::HLeft || ent->alignV != DRW_Text::VBaseLine) {
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
        writer->writeDouble(31, ent->secPoint.z);
    }
    writer->writeDouble(210, ent->extPoint.x);
    writer->writeDouble(220, ent->extPoint.y);
    writer->writeDouble(230, ent->extPoint.z);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbText");
    }
    if (ent->alignV != DRW_Text::VBaseLine) {
        writer->writeInt16(73, ent->alignV);
    }
    return true;
}

bool dxfRW::writeMText(DRW_MText *ent){
    if (version > DRW::AC1009) {
        writer->writeString(0, "MTEXT");
        writeEntity(ent);
        writer->writeString(100, "AcDbMText");
        writer->writeDouble(10, ent->basePoint.x);
        writer->writeDouble(20, ent->basePoint.y);
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(40, ent->height);
        writer->writeDouble(41, ent->widthscale);
        writer->writeInt16(71, ent->textgen);
        writer->writeInt16(72, ent->alignH);
        std::string text = writer->fromUtf8String(ent->text);

        /* patch dxfrw_c: bucatile de 250 octeti nu mai taie caractere UTF-8 multi-octet
           sau secvente \U+XXXX; stilul este convertit in code page-ul fisierului */
        size_t i = 0;
        while (text.size() - i > 250) {
            size_t cut = i + 250;
            while (cut > i && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut;
            for (size_t k = (cut >= i + 6 ? cut - 6 : i); k < cut; ++k) {
                if (text[k] == '\\' && k + 2 < text.size() && text[k+1] == 'U' && text[k+2] == '+' && k + 7 > cut) {
                    cut = k;
                    break;
                }
            }
            if (cut == i) cut = i + 250;
            writer->writeString(3, text.substr(i, cut - i));
            i = cut;
        }
        writer->writeString(1, text.substr(i));
        writer->writeUtf8String(7, ent->style);
        writer->writeDouble(210, ent->extPoint.x);
        writer->writeDouble(220, ent->extPoint.y);
        writer->writeDouble(230, ent->extPoint.z);
        writer->writeDouble(50, ent->angle);
        writer->writeInt16(73, ent->alignV);
        writer->writeDouble(44, ent->interlin);
//RLZ ... 11, 21, 31 needed?
    } else {
        //RLZ: TODO convert mtext in text lines (not exist in acad 12)
    }
    return true;
}

bool dxfRW::writeViewport(DRW_Viewport *ent) {
    /* patch dxfrw_c: in R12 VIEWPORT isi tine proprietatile in XDATA obligatoriu ("ACAD"), pe care
       biblioteca nu il genereaza; fara el fisierul e invalid. Ferestrele din spatiul hartie se omit in R12. */
    if (version == DRW::AC1009)
        return true;
    writer->writeString(0, "VIEWPORT");
    writeEntity(ent);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbViewport");
    }
    writer->writeDouble(10, ent->basePoint.x);
    writer->writeDouble(20, ent->basePoint.y);
    if (ent->basePoint.z != 0.0)
        writer->writeDouble(30, ent->basePoint.z);
    writer->writeDouble(40, ent->pswidth);
    writer->writeDouble(41, ent->psheight);
    writer->writeInt16(68, ent->vpstatus);
    writer->writeInt16(69, ent->vpID);
    writer->writeDouble(12, ent->centerPX);//RLZ: verify if exist in V12
    writer->writeDouble(22, ent->centerPY);//RLZ: verify if exist in V12
    /* patch dxfrw_c: restul vederii (directie, tinta, inaltime, unghiuri) era citit din DWG, dar nu era scris */
    writer->writeDouble(13, ent->snapPX);
    writer->writeDouble(23, ent->snapPY);
    writer->writeDouble(14, ent->snapSpPX);
    writer->writeDouble(24, ent->snapSpPY);
    writer->writeDouble(16, ent->viewDir.x);
    writer->writeDouble(26, ent->viewDir.y);
    writer->writeDouble(36, ent->viewDir.z);
    writer->writeDouble(17, ent->viewTarget.x);
    writer->writeDouble(27, ent->viewTarget.y);
    writer->writeDouble(37, ent->viewTarget.z);
    writer->writeDouble(42, ent->viewLength);
    writer->writeDouble(43, ent->frontClip);
    writer->writeDouble(44, ent->backClip);
    writer->writeDouble(45, ent->viewHeight);
    writer->writeDouble(50, ent->snapAngle);
    writer->writeDouble(51, ent->twistAngle);
    return true;
}

DRW_ImageDef* dxfRW::writeImage(DRW_Image *ent, std::string name){
    if (version > DRW::AC1009) {
        //search if exist imagedef with this mane (image inserted more than 1 time)
        //RLZ: imagedef_reactor seem needed to read in acad
        DRW_ImageDef *id = NULL;
        for (unsigned int i=0; i<imageDef.size(); i++) {
            if (imageDef.at(i)->name == name ) {
                id = imageDef.at(i);
                continue;
            }
        }
        if (id == NULL) {
            id = new DRW_ImageDef();
            imageDef.push_back(id);
            id->handle = ++entCount;
        }
        id->name = name;
        /* patch dxfrw_c: marimile din IMAGEDEF nu erau completate niciodata (se scriau valori
           neinitializate): marimea in pixeli si marimea unui pixel vin din entitatea IMAGE */
        id->u = ent->sizeu;
        id->v = ent->sizev;
        id->up = sqrt(ent->secPoint.x*ent->secPoint.x + ent->secPoint.y*ent->secPoint.y + ent->secPoint.z*ent->secPoint.z);
        id->vp = sqrt(ent->vVector.x*ent->vVector.x + ent->vVector.y*ent->vVector.y + ent->vVector.z*ent->vVector.z);
        id->loaded = 1;
        id->resolution = 0;
        std::string idReactor = toHexStr(++entCount);

        writer->writeString(0, "IMAGE");
        writeEntity(ent);
        writer->writeString(100, "AcDbRasterImage");
        writer->writeDouble(10, ent->basePoint.x);
        writer->writeDouble(20, ent->basePoint.y);
        writer->writeDouble(30, ent->basePoint.z);
        writer->writeDouble(11, ent->secPoint.x);
        writer->writeDouble(21, ent->secPoint.y);
        writer->writeDouble(31, ent->secPoint.z);
        writer->writeDouble(12, ent->vVector.x);
        writer->writeDouble(22, ent->vVector.y);
        writer->writeDouble(32, ent->vVector.z);
        writer->writeDouble(13, ent->sizeu);
        writer->writeDouble(23, ent->sizev);
        writer->writeString(340, toHexStr(id->handle));
        writer->writeInt16(70, 1);
        writer->writeInt16(280, ent->clip);
        writer->writeInt16(281, ent->brightness);
        writer->writeInt16(282, ent->contrast);
        writer->writeInt16(283, ent->fade);
        writer->writeString(360, idReactor);
        /* patch dxfrw_c: conturul de decupare lipsea; cel implicit este dreptunghiul intregii imagini */
        writer->writeInt16(71, 1);
        writer->writeInt32(91, 2);
        writer->writeDouble(14, -0.5);
        writer->writeDouble(24, -0.5);
        writer->writeDouble(14, ent->sizeu - 0.5);
        writer->writeDouble(24, ent->sizev - 0.5);
        id->reactors[idReactor] = toHexStr(ent->handle);
        return id;
    }
    return NULL; //not exist in acad 12
}

bool dxfRW::writeBlockRecord(std::string name){
    if (version > DRW::AC1009) {
        writer->writeString(0, "BLOCK_RECORD");
        writer->writeString(5, toHexStr(++entCount));

        blockMap[name] = entCount;
        entCount = 2+entCount;//reserve 2 for BLOCK & ENDBLOCK
        if (version > DRW::AC1014) {
            writer->writeString(330, "1");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbBlockTableRecord");
        writer->writeUtf8String(2, name);
        if (version > DRW::AC1018) {
            //    writer->writeInt16(340, 22);
            writer->writeInt16(70, 0);
            writer->writeInt16(280, 1);
            writer->writeInt16(281, 0);
        }
    }
    return true;
}

bool dxfRW::writeBlock(DRW_Block *bk){
    if (writingBlock) {
        writer->writeString(0, "ENDBLK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
        if (version > DRW::AC1009) {
            writer->writeString(5, toHexStr(currHandle+2));
            if (version > DRW::AC1014) {
                writer->writeString(330, toHexStr(currHandle));
            }
            writer->writeString(100, "AcDbEntity");
        }
        writer->writeString(8, "0");
        if (version > DRW::AC1009) {
            writer->writeString(100, "AcDbBlockEnd");
        }
    }
    writingBlock = true;
    currentBlock = bk->name; /* patch dxfrw_c: ATTDEF-urile se inregistreaza pe bloc */
    transform(currentBlock.begin(), currentBlock.end(), currentBlock.begin(), ::toupper);
    writer->writeString(0, "BLOCK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        currHandle = (*(blockMap.find(bk->name))).second;
        writer->writeString(5, toHexStr(currHandle+1));
        if (version > DRW::AC1014) {
            writer->writeString(330, toHexStr(currHandle));
        }
        writer->writeString(100, "AcDbEntity");
    }
    /* patch dxfrw_c: layerul blocului era citit, dar se scria intotdeauna "0" */
    if (bk->layer.empty())
        writer->writeString(8, "0");
    else if (version > DRW::AC1009)
        writer->writeUtf8String(8, bk->layer);
    else
        writer->writeSymbolName(8, bk->layer);
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbBlockBegin");
        writer->writeUtf8String(2, bk->name);
    } else
        writer->writeSymbolName(2, bk->name);
    {
        int bflags = bk->flags;
        if (version == DRW::AC1009) {
            /* patch dxfrw_c: in R12, un bloc anonim cu alt prefix decat *U, *D, *X (de ex. *T, tabele din 2005+)
               e redenumit de writeSymbolName ("_T13"), deci nu mai e anonim; flag-ul 1 ar fi contradictoriu.
               Flag-ul 2 ("are atribute") trebuie sa corespunda continutului (ATTDEF-urile se scriu acum);
               aplicatia il potriveste inainte de scriere (dxfrw_c o face in Writer::writeBlocks). */
            const std::string& nm = bk->name;
            bool validAnon = nm.size() > 1 && nm[0] == '*' &&
                             (nm[1] == 'U' || nm[1] == 'u' || nm[1] == 'D' || nm[1] == 'd' || nm[1] == 'X' || nm[1] == 'x');
            if (!validAnon)
                bflags &= ~1;
        }
        writer->writeInt16(70, bflags);
    }
    writer->writeDouble(10, bk->basePoint.x);
    writer->writeDouble(20, bk->basePoint.y);
    if (bk->basePoint.z != 0.0) {
        writer->writeDouble(30, bk->basePoint.z);
    }
    if (version > DRW::AC1009)
        writer->writeUtf8String(3, bk->name);
    else
        writer->writeSymbolName(3, bk->name);
    writer->writeString(1, "");

    return true;
}

bool dxfRW::writeTables() {
    writer->writeString(0, "TABLE");
    writer->writeString(2, "VPORT");
    if (version > DRW::AC1009) {
        writer->writeString(5, "8");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 1); //end table def
/*** VPORT ***/
    dimstyleStd =false;
    iface->writeVports();
    if (!dimstyleStd) {
        DRW_Vport portact;
        portact.name = "*ACTIVE";
        writeVport(&portact);
    }
    writer->writeString(0, "ENDTAB");
/*** LTYPE ***/
    writer->writeString(0, "TABLE");
    writer->writeString(2, "LTYPE");
    if (version > DRW::AC1009) {
        writer->writeString(5, "5");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 4); //end table def
//Mandatory linetypes
    writer->writeString(0, "LTYPE");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "14");
        if (version > DRW::AC1014) {
            writer->writeString(330, "5");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbLinetypeTableRecord");
        writer->writeString(2, "ByBlock");
    } else
        writer->writeString(2, "BYBLOCK");
    writer->writeInt16(70, 0);
    writer->writeString(3, "");
    writer->writeInt16(72, 65);
    writer->writeInt16(73, 0);
    writer->writeDouble(40, 0.0);

    writer->writeString(0, "LTYPE");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "15");
        if (version > DRW::AC1014) {
            writer->writeString(330, "5");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbLinetypeTableRecord");
        writer->writeString(2, "ByLayer");
    } else
        writer->writeString(2, "BYLAYER");
    writer->writeInt16(70, 0);
    writer->writeString(3, "");
    writer->writeInt16(72, 65);
    writer->writeInt16(73, 0);
    writer->writeDouble(40, 0.0);

    writer->writeString(0, "LTYPE");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "16");
        if (version > DRW::AC1014) {
            writer->writeString(330, "5");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbLinetypeTableRecord");
        writer->writeString(2, "Continuous");
    } else {
        writer->writeString(2, "CONTINUOUS");
    }
    writer->writeInt16(70, 0);
    writer->writeString(3, "Solid line");
    writer->writeInt16(72, 65);
    writer->writeInt16(73, 0);
    writer->writeDouble(40, 0.0);
//Aplication linetypes
    iface->writeLTypes();
    writer->writeString(0, "ENDTAB");
/*** LAYER ***/
    writer->writeString(0, "TABLE");
    writer->writeString(2, "LAYER");
    if (version > DRW::AC1009) {
        writer->writeString(5, "2");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 1); //end table def
    wlayer0 =false;
    iface->writeLayers();
    if (!wlayer0) {
        DRW_Layer lay0;
        lay0.name = "0";
        writeLayer(&lay0);
    }
    writer->writeString(0, "ENDTAB");
/*** STYLE ***/
    writer->writeString(0, "TABLE");
    writer->writeString(2, "STYLE");
    if (version > DRW::AC1009) {
        writer->writeString(5, "3");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 3); //end table def
    dimstyleStd =false;
    iface->writeTextstyles();
    if (!dimstyleStd) {
        DRW_Textstyle tsty;
        tsty.name = "Standard";
        writeTextstyle(&tsty);
    }
    writer->writeString(0, "ENDTAB");

    writer->writeString(0, "TABLE");
    writer->writeString(2, "VIEW");
    if (version > DRW::AC1009) {
        writer->writeString(5, "6");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 0); //end table def
    writer->writeString(0, "ENDTAB");

    writer->writeString(0, "TABLE");
    writer->writeString(2, "UCS");
    if (version > DRW::AC1009) {
        writer->writeString(5, "7");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 0); //end table def
    writer->writeString(0, "ENDTAB");

    writer->writeString(0, "TABLE");
    writer->writeString(2, "APPID");
    if (version > DRW::AC1009) {
        writer->writeString(5, "9");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 1); //end table def
    writer->writeString(0, "APPID");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "12");
        if (version > DRW::AC1014) {
            writer->writeString(330, "9");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbRegAppTableRecord");
    }
    writer->writeString(2, "ACAD");
    writer->writeInt16(70, 0);
    iface->writeAppId();
    writer->writeString(0, "ENDTAB");

    writer->writeString(0, "TABLE");
    writer->writeString(2, "DIMSTYLE");
    if (version > DRW::AC1009) {
        writer->writeString(5, "A");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
    }
    writer->writeInt16(70, 1); //end table def
    if (version > DRW::AC1014) {
        writer->writeString(100, "AcDbDimStyleTable");
        writer->writeInt16(71, 1); //end table def
    }
    dimstyleStd =false;
    iface->writeDimstyles();
    if (!dimstyleStd) {
        DRW_Dimstyle dsty;
        dsty.name = "Standard";
        writeDimstyle(&dsty);
    }
    writer->writeString(0, "ENDTAB");

    if (version > DRW::AC1009) {
        writer->writeString(0, "TABLE");
        writer->writeString(2, "BLOCK_RECORD");
        writer->writeString(5, "1");
        if (version > DRW::AC1014) {
            writer->writeString(330, "0");
        }
        writer->writeString(100, "AcDbSymbolTable");
        writer->writeInt16(70, 2); //end table def
        writer->writeString(0, "BLOCK_RECORD");
        writer->writeString(5, "1F");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbBlockTableRecord");
        writer->writeString(2, "*Model_Space");
        if (version > DRW::AC1018) {
            //    writer->writeInt16(340, 22);
            writer->writeInt16(70, 0);
            writer->writeInt16(280, 1);
            writer->writeInt16(281, 0);
        }
        writer->writeString(0, "BLOCK_RECORD");
        writer->writeString(5, "1E");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1");
        }
        writer->writeString(100, "AcDbSymbolTableRecord");
        writer->writeString(100, "AcDbBlockTableRecord");
        writer->writeString(2, "*Paper_Space");
        if (version > DRW::AC1018) {
            //    writer->writeInt16(340, 22);
            writer->writeInt16(70, 0);
            writer->writeInt16(280, 1);
            writer->writeInt16(281, 0);
        }
    }
    /* allways call writeBlockRecords to iface for prepare unnamed blocks */
    iface->writeBlockRecords();
    if (version > DRW::AC1009) {
        writer->writeString(0, "ENDTAB");
    }
return true;
}

bool dxfRW::writeBlocks() {
    writer->writeString(0, "BLOCK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "20");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1F");
        }
        writer->writeString(100, "AcDbEntity");
    }
    writer->writeString(8, "0");
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbBlockBegin");
        writer->writeString(2, "*Model_Space");
    } else
        writer->writeString(2, "$MODEL_SPACE");
    writer->writeInt16(70, 0);
    writer->writeDouble(10, 0.0);
    writer->writeDouble(20, 0.0);
    writer->writeDouble(30, 0.0);
    if (version > DRW::AC1009)
        writer->writeString(3, "*Model_Space");
    else
        writer->writeString(3, "$MODEL_SPACE");
    writer->writeString(1, "");
    writer->writeString(0, "ENDBLK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "21");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1F");
        }
        writer->writeString(100, "AcDbEntity");
    }
    writer->writeString(8, "0");
    if (version > DRW::AC1009)
        writer->writeString(100, "AcDbBlockEnd");

    writer->writeString(0, "BLOCK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "1C");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1B");
        }
        writer->writeString(100, "AcDbEntity");
    }
    writer->writeString(8, "0");
    if (version > DRW::AC1009) {
        writer->writeString(100, "AcDbBlockBegin");
        writer->writeString(2, "*Paper_Space");
    } else
        writer->writeString(2, "$PAPER_SPACE");
    writer->writeInt16(70, 0);
    writer->writeDouble(10, 0.0);
    writer->writeDouble(20, 0.0);
    writer->writeDouble(30, 0.0);
    if (version > DRW::AC1009)
        writer->writeString(3, "*Paper_Space");
    else
        writer->writeString(3, "$PAPER_SPACE");
    writer->writeString(1, "");
    writer->writeString(0, "ENDBLK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
    if (version > DRW::AC1009) {
        writer->writeString(5, "1D");
        if (version > DRW::AC1014) {
            writer->writeString(330, "1F");
        }
        writer->writeString(100, "AcDbEntity");
    }
    writer->writeString(8, "0");
    if (version > DRW::AC1009)
        writer->writeString(100, "AcDbBlockEnd");
    writingBlock = false;
    iface->writeBlocks();
    if (writingBlock) {
        writingBlock = false;
        writer->writeString(0, "ENDBLK");
    if (version == DRW::AC1009) writer->writeString(5, toHexStr(++entCount)); /* patch dxfrw_c: R12 cere handle-uri */
        if (version > DRW::AC1009) {
            writer->writeString(5, toHexStr(currHandle+2));
//            writer->writeString(5, "1D");
            if (version > DRW::AC1014) {
                writer->writeString(330, toHexStr(currHandle));
            }
            writer->writeString(100, "AcDbEntity");
        }
        writer->writeString(8, "0");
        if (version > DRW::AC1009)
            writer->writeString(100, "AcDbBlockEnd");
    }
    return true;
}

bool dxfRW::writeObjects() {
    writer->writeString(0, "DICTIONARY");
    std::string imgDictH;
    writer->writeString(5, "C");
    if (version > DRW::AC1014) {
        writer->writeString(330, "0");
    }
    writer->writeString(100, "AcDbDictionary");
    writer->writeInt16(281, 1);
    writer->writeString(3, "ACAD_GROUP");
    writer->writeString(350, "D");
    if (imageDef.size() != 0) {
        writer->writeString(3, "ACAD_IMAGE_DICT");
        imgDictH = toHexStr(++entCount);
        writer->writeString(350, imgDictH);
    }
    /* patch dxfrw_c: dictionarul stilurilor MULTILEADER (handle fix 2A) cu stilul "Standard" (2B) */
    bool mleaderStyle = haveMLeaders && version > DRW::AC1018;
    if (mleaderStyle) {
        writer->writeString(3, "ACAD_MLEADERSTYLE");
        writer->writeString(350, "2A");
    }
    writer->writeString(0, "DICTIONARY");
    writer->writeString(5, "D");
    writer->writeString(330, "C");
    writer->writeString(100, "AcDbDictionary");
    writer->writeInt16(281, 1);
    if (mleaderStyle) {
        writer->writeString(0, "DICTIONARY");
        writer->writeString(5, "2A");
        writer->writeString(330, "C");
        writer->writeString(100, "AcDbDictionary");
        writer->writeInt16(281, 1);
        writer->writeString(3, "Standard");
        writer->writeString(350, "2B");
        /* valorile stilului "Standard" creat de AutoCAD */
        writer->writeString(0, "MLEADERSTYLE");
        writer->writeString(5, "2B");
        writer->writeString(330, "2A");
        writer->writeString(100, "AcDbMLeaderStyle");
        writer->writeInt16(179, 2);
        writer->writeInt16(170, 2);
        writer->writeInt16(171, 1);
        writer->writeInt16(172, 0);
        writer->writeInt32(90, 2);
        writer->writeDouble(40, 0.0);
        writer->writeDouble(41, 0.0);
        writer->writeInt16(173, 1);
        writer->writeInt32(91, static_cast<int>(0xC1000000));
        writer->writeInt32(92, -2);
        writer->writeBool(290, true);
        writer->writeDouble(42, 2.0);
        writer->writeBool(291, true);
        writer->writeDouble(43, 8.0);
        writer->writeString(3, "Standard");
        writer->writeDouble(44, 4.0);
        writer->writeString(300, "");
        std::string th = tableHandleOf(styleHandleMap, "Standard");
        if (!th.empty())
            writer->writeString(342, th);
        writer->writeInt16(174, 1);
        writer->writeInt16(175, 1);
        writer->writeInt16(176, 0);
        writer->writeInt16(178, 1);
        writer->writeInt32(93, static_cast<int>(0xC1000000));
        writer->writeDouble(45, 4.0);
        writer->writeBool(292, false);
        writer->writeBool(297, false);
        writer->writeDouble(46, 4.0);
        writer->writeInt32(94, static_cast<int>(0xC1000000));
        writer->writeDouble(47, 1.0);
        writer->writeDouble(49, 1.0);
        writer->writeDouble(140, 1.0);
        writer->writeBool(294, true);
        writer->writeDouble(141, 0.0);
        writer->writeInt16(177, 0);
        writer->writeDouble(142, 1.0);
        writer->writeBool(295, false);
        writer->writeBool(296, false);
        writer->writeDouble(143, 3.75);
        if (version > DRW::AC1021) {
            writer->writeInt16(271, 0);
            writer->writeInt16(272, 9);
            writer->writeInt16(273, 9);
        }
    }
//write IMAGEDEF_REACTOR
    for (unsigned int i=0; i<imageDef.size(); i++) {
        DRW_ImageDef *id = imageDef.at(i);
        std::map<std::string, std::string>::iterator it;
        for ( it=id->reactors.begin() ; it != id->reactors.end(); ++it ) {
            writer->writeString(0, "IMAGEDEF_REACTOR");
            writer->writeString(5, (*it).first);
            writer->writeString(330, (*it).second);
            writer->writeString(100, "AcDbRasterImageDefReactor");
            writer->writeInt16(90, 2); //version 2=R14 to v2010
            writer->writeString(330, (*it).second);
        }
    }
    if (imageDef.size() != 0) {
        writer->writeString(0, "DICTIONARY");
        writer->writeString(5, imgDictH);
        writer->writeString(330, "C");
        writer->writeString(100, "AcDbDictionary");
        writer->writeInt16(281, 1);
        for (unsigned int i=0; i<imageDef.size(); i++) {
            size_t f1, f2;
            f1 = imageDef.at(i)->name.find_last_of("/\\");
            f2 =imageDef.at(i)->name.find_last_of('.');
            ++f1;
            writer->writeString(3, imageDef.at(i)->name.substr(f1,f2-f1));
            writer->writeString(350, toHexStr(imageDef.at(i)->handle) );
        }
    }
    for (unsigned int i=0; i<imageDef.size(); i++) {
        DRW_ImageDef *id = imageDef.at(i);
        writer->writeString(0, "IMAGEDEF");
        writer->writeString(5, toHexStr(id->handle) );
        if (version > DRW::AC1014) {
            /* patch dxfrw_c: proprietarul IMAGEDEF este dictionarul ACAD_IMAGE_DICT; codul 330 lipsea */
            writer->writeString(330, imgDictH);
        }
        writer->writeString(102, "{ACAD_REACTORS");
        std::map<std::string, std::string>::iterator it;
        for ( it=id->reactors.begin() ; it != id->reactors.end(); ++it ) {
            writer->writeString(330, (*it).first);
        }
        writer->writeString(102, "}");
        writer->writeString(100, "AcDbRasterImageDef");
        writer->writeInt16(90, 0); //version 0=R14 to v2010
        writer->writeUtf8String(1, id->name);
        writer->writeDouble(10, id->u);
        writer->writeDouble(20, id->v);
        writer->writeDouble(11, id->up);
        writer->writeDouble(21, id->vp);
        writer->writeInt16(280, id->loaded);
        writer->writeInt16(281, id->resolution);
    }
    //no more needed imageDef, delete it
    while (!imageDef.empty()) {
       delete imageDef.back(); /* patch dxfrw_c: obiectele erau scoase din lista fara a fi eliberate */
       imageDef.pop_back();
    }

    return true;
}

bool dxfRW::writeExtData(const std::vector<DRW_Variant*> &ed){
    for (std::vector<DRW_Variant*>::const_iterator it=ed.begin(); it!=ed.end(); ++it){
        switch ((*it)->code()) {
        case 1000:
        case 1001:
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        {int cc = (*it)->code();
            if ((*it)->type() == DRW_Variant::STRING)
                writer->writeUtf8String(cc, *(*it)->content.s);
//            writer->writeUtf8String((*it)->code, (*it)->content.s);
            break;}
        case 1010:
        case 1011:
        case 1012:
        case 1013:
            if ((*it)->type() == DRW_Variant::COORD) {
                writer->writeDouble((*it)->code(), (*it)->content.v->x);
                writer->writeDouble((*it)->code()+10 , (*it)->content.v->y);
                writer->writeDouble((*it)->code()+20 , (*it)->content.v->z);
            }
            break;
        case 1040:
        case 1041:
        case 1042:
            if ((*it)->type() == DRW_Variant::DOUBLE)
                writer->writeDouble((*it)->code(), (*it)->content.d);
            break;
        case 1070:
            if ((*it)->type() == DRW_Variant::INTEGER)
                writer->writeInt16((*it)->code(), (*it)->content.i);
            break;
        case 1071:
            if ((*it)->type() == DRW_Variant::INTEGER)
                writer->writeInt32((*it)->code(), (*it)->content.i);
            break;
        default:
            break;
        }
    }
    return true;
}

/********* Reader Process *********/

bool dxfRW::processDxf() {
    DRW_DBG("dxfRW::processDxf() start processing dxf\n");
    int code;
    bool more = true;
    std::string sectionstr;
    int sectionsFound = 0; /* patch dxfrw_c */
    foundEof = false;
//    section = secUnknown;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG(" processDxf\n");
        if (code == 999) {
            header.addComment(reader->getString());
        } else if (code == 0) {
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG(" processDxf\n");
            if (sectionstr == "EOF") {
                foundEof = true; /* patch dxfrw_c */
                return true;  //found EOF terminate
            }
            if (sectionstr == "SECTION") {
                ++sectionsFound; /* patch dxfrw_c */
                more = reader->readRec(&code);
                DRW_DBG(code); DRW_DBG(" processDxf\n");
                if (!more)
                    return false; //wrong dxf file
                if (code == 2) {
                    sectionstr = reader->getString();
                    DRW_DBG(sectionstr); DRW_DBG("  processDxf\n");
                //found section, process it
                    if (sectionstr == "HEADER") {
                        processHeader();
                    } else if (sectionstr == "CLASSES") {
//                        processClasses();
                    } else if (sectionstr == "TABLES") {
                        processTables();
                    } else if (sectionstr == "BLOCKS") {
                        processBlocks();
                    } else if (sectionstr == "ENTITIES") {
                        processEntities(false);
                    } else if (sectionstr == "OBJECTS") {
                        processObjects();
                    }
                }
            }
        }
/*    if (!more)
        return true;*/
    }
    /* patch dxfrw_c: fluxul s-a terminat fara EOF; fara nicio sectiune nu este un fisier DXF */
    return sectionsFound > 0;
}

/********* Header Section *********/

bool dxfRW::processHeader() {
    DRW_DBG("dxfRW::processHeader\n");
    int code;
    std::string sectionstr;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG(" processHeader\n");
        if (code == 0) {
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG(" processHeader\n\n");
            if (sectionstr == "ENDSEC") {
                iface->addHeader(&header);
                return true;  //found ENDSEC terminate
            }
        } else header.parseCode(code, reader);
    }
    return true;
}

/********* Tables Section *********/

bool dxfRW::processTables() {
    DRW_DBG("dxfRW::processTables\n");
    int code;
    std::string sectionstr;
    bool more = true;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG(" processHeader\n\n");
            if (sectionstr == "TABLE") {
                more = reader->readRec(&code);
                DRW_DBG(code); DRW_DBG("\n");
                if (!more)
                    return false; //wrong dxf file
                if (code == 2) {
                    sectionstr = reader->getString();
                    DRW_DBG(sectionstr); DRW_DBG(" processHeader\n\n");
                //found section, process it
                    if (sectionstr == "LTYPE") {
                        processLType();
                    } else if (sectionstr == "LAYER") {
                        processLayer();
                    } else if (sectionstr == "STYLE") {
                        processTextStyle();
                    } else if (sectionstr == "VPORT") {
                        processVports();
                    } else if (sectionstr == "VIEW") {
//                        processView();
                    } else if (sectionstr == "UCS") {
//                        processUCS();
                    } else if (sectionstr == "APPID") {
                        processAppId();
                    } else if (sectionstr == "DIMSTYLE") {
                        processDimStyle();
                    } else if (sectionstr == "BLOCK_RECORD") {
//                        processBlockRecord();
                    }
                }
            } else if (sectionstr == "ENDSEC") {
                return true;  //found ENDSEC terminate
            }
        }
    }
    return true;
}

bool dxfRW::processLType() {
    DRW_DBG("dxfRW::processLType\n");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_LType ltype;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading) {
                ltype.update();
                iface->addLType(ltype);
            }
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "LTYPE") {
                reading = true;
                ltype.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            ltype.parseCode(code, reader);
    }
    return true;
}

bool dxfRW::processLayer() {
    DRW_DBG("dxfRW::processLayer\n");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_Layer layer;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading)
                iface->addLayer(layer);
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "LAYER") {
                reading = true;
                layer.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            layer.parseCode(code, reader);
    }
    return true;
}

bool dxfRW::processDimStyle() {
    DRW_DBG("dxfRW::processDimStyle");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_Dimstyle dimSty;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading)
                iface->addDimStyle(dimSty);
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "DIMSTYLE") {
                reading = true;
                dimSty.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            dimSty.parseCode(code, reader);
    }
    return true;
}

bool dxfRW::processTextStyle(){
    DRW_DBG("dxfRW::processTextStyle");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_Textstyle TxtSty;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading)
                iface->addTextStyle(TxtSty);
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "STYLE") {
                reading = true;
                TxtSty.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            TxtSty.parseCode(code, reader);
    }
    return true;
}

bool dxfRW::processVports(){
    DRW_DBG("dxfRW::processVports");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_Vport vp;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading)
                iface->addVport(vp);
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "VPORT") {
                reading = true;
                vp.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            vp.parseCode(code, reader);
    }
    return true;
}

bool dxfRW::processAppId(){
    DRW_DBG("dxfRW::processAppId");
    int code;
    std::string sectionstr;
    bool reading = false;
    DRW_AppId vp;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            if (reading)
                iface->addAppId(vp);
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "APPID") {
                reading = true;
                vp.reset();
            } else if (sectionstr == "ENDTAB") {
                return true;  //found ENDTAB terminate
            }
        } else if (reading)
            vp.parseCode(code, reader);
    }
    return true;
}

/********* Block Section *********/

bool dxfRW::processBlocks() {
    DRW_DBG("dxfRW::processBlocks\n");
    int code;
    std::string sectionstr;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code == 0) {
            sectionstr = reader->getString();
            DRW_DBG(sectionstr); DRW_DBG("\n");
            if (sectionstr == "BLOCK") {
                processBlock();
            } else if (sectionstr == "ENDSEC") {
                return true;  //found ENDSEC terminate
            }
        }
    }
    return true;
}

bool dxfRW::processBlock() {
    DRW_DBG("dxfRW::processBlock");
    int code;
    DRW_Block block;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addBlock(block);
            if (nextentity == "ENDBLK") {
                iface->endBlock();
                return true;  //found ENDBLK, terminate
            } else {
                processEntities(true);
                iface->endBlock();
                return true;  //found ENDBLK, terminate
            }
        }
        default:
            block.parseCode(code, reader);
            break;
        }
    }
    return true;
}


/********* Entities Section *********/

bool dxfRW::processEntities(bool isblock) {
    DRW_DBG("dxfRW::processEntities\n");
    int code;
    bool next = true;
    /* patch dxfrw_c: intr-un bloc, processBlock a citit deja "0 <entitate>"; citirea inca unei inregistrari
       arunca primul cod al primei entitati din bloc (handle-ul, sau in R12 fara handle-uri layerul) */
    if (!isblock) {
        if (!reader->readRec(&code)){
            return false;
        }
        if (code == 0) {
            nextentity = reader->getString();
        } else {
            return false;  //first record in entities is 0
        }
    }
    do {
        if (nextentity == "ENDSEC" || nextentity == "ENDBLK") {
            return true;  //found ENDSEC or ENDBLK terminate
        } else if (nextentity == "POINT") {
            processPoint();
        } else if (nextentity == "LINE") {
            processLine();
        } else if (nextentity == "CIRCLE") {
            processCircle();
        } else if (nextentity == "ARC") {
            processArc();
        } else if (nextentity == "ELLIPSE") {
            processEllipse();
        } else if (nextentity == "TRACE") {
            processTrace();
        } else if (nextentity == "SOLID") {
            processSolid();
        } else if (nextentity == "INSERT") {
            processInsert();
        } else if (nextentity == "LWPOLYLINE") {
            processLWPolyline();
        } else if (nextentity == "POLYLINE") {
            processPolyline();
        } else if (nextentity == "TEXT") {
            processText();
        } else if (nextentity == "MTEXT") {
            processMText();
        } else if (nextentity == "HATCH") {
            processHatch();
        } else if (nextentity == "SPLINE") {
            processSpline();
        } else if (nextentity == "3DFACE") {
            process3dface();
        } else if (nextentity == "VIEWPORT") {
            processViewport();
        } else if (nextentity == "IMAGE") {
            processImage();
        } else if (nextentity == "DIMENSION") {
            processDimension();
        } else if (nextentity == "LEADER") {
            processLeader();
        } else if (nextentity == "RAY") {
            processRay();
        } else if (nextentity == "XLINE") {
            processXline();
        } else if (nextentity == "ATTDEF") { /* patch dxfrw_c */
            processAttdef();
        } else if (nextentity == "MULTILEADER" || nextentity == "MLEADER") { /* patch dxfrw_c */
            processMLeader();
        } else {
            if (reader->readRec(&code)){
                if (code == 0)
                    nextentity = reader->getString();
            } else
                return false; //end of file without ENDSEC
        }
        /* patch dxfrw_c: la sfarsitul fluxului functiile process*() intorc true fara sa avanseze
           nextentity, iar bucla relua acelasi element la infinit (fisiere trunchiate/corupte) */
        if (!reader->isGood())
            return false;

    } while (next);
    return true;
}

bool dxfRW::processEllipse() {
    DRW_DBG("dxfRW::processEllipse");
    int code;
    DRW_Ellipse ellipse;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                ellipse.applyExtrusion();
            iface->addEllipse(ellipse);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            ellipse.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processTrace() {
    DRW_DBG("dxfRW::processTrace");
    int code;
    DRW_Trace trace;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                trace.applyExtrusion();
            iface->addTrace(trace);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            trace.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processSolid() {
    DRW_DBG("dxfRW::processSolid");
    int code;
    DRW_Solid solid;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                solid.applyExtrusion();
            iface->addSolid(solid);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            solid.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::process3dface() {
    DRW_DBG("dxfRW::process3dface");
    int code;
    DRW_3Dface face;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->add3dFace(face);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            face.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processViewport() {
    DRW_DBG("dxfRW::processViewport");
    int code;
    DRW_Viewport vp;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addViewport(vp);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            vp.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processPoint() {
    DRW_DBG("dxfRW::processPoint\n");
    int code;
    DRW_Point point;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addPoint(point);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            point.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processLine() {
    DRW_DBG("dxfRW::processLine\n");
    int code;
    DRW_Line line;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addLine(line);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            line.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processRay() {
    DRW_DBG("dxfRW::processRay\n");
    int code;
    DRW_Ray line;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addRay(line);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            line.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processXline() {
    DRW_DBG("dxfRW::processXline\n");
    int code;
    DRW_Xline line;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addXline(line);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            line.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processCircle() {
    DRW_DBG("dxfRW::processPoint\n");
    int code;
    DRW_Circle circle;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                circle.applyExtrusion();
            iface->addCircle(circle);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            circle.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processArc() {
    DRW_DBG("dxfRW::processPoint\n");
    int code;
    DRW_Arc arc;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                arc.applyExtrusion();
            iface->addArc(arc);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            arc.parseCode(code, reader);
            break;
        }
    }
    return true;
}

/* patch dxfrw_c: entitatile ATTRIB care urmeaza unui INSERT (pana la SEQEND) erau ignorate; acum se
   ataseaza insertiei (DRW_Insert::attributes), iar SEQEND-ul lor se sare. */
bool dxfRW::processInsert() {
    DRW_DBG("dxfRW::processInsert");
    int code;
    DRW_Insert insert;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            bool sawAttrib = false;
            while (nextentity == "ATTRIB") {
                sawAttrib = true;
                DRW_Attrib attrib;
                bool done = false;
                while (!done && reader->readRec(&code)) {
                    if (code == 0) {
                        nextentity = reader->getString();
                        done = true;
                    } else
                        attrib.parseCode(code, reader);
                }
                if (!done) { //sfarsitul fluxului
                    nextentity.clear();
                    break;
                }
                insert.attributes.push_back(attrib);
            }
            if (sawAttrib && nextentity == "SEQEND") {
                while (reader->readRec(&code)) {
                    if (code == 0) {
                        nextentity = reader->getString();
                        break;
                    }
                }
            }
            iface->addInsert(insert);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            insert.parseCode(code, reader);
            break;
        }
    }
    return true;
}

/* patch dxfrw_c: MULTILEADER era ignorat */
bool dxfRW::processMLeader() {
    DRW_DBG("dxfRW::processMLeader");
    int code;
    DRW_MLeader ml;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addMLeader(&ml);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            ml.parseCode(code, reader);
            break;
        }
    }
    return true;
}

/* patch dxfrw_c: definitiile de atribut (ATTDEF) erau ignorate */
bool dxfRW::processAttdef() {
    DRW_DBG("dxfRW::processAttdef");
    int code;
    DRW_Attdef attdef;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addAttdef(attdef);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            attdef.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processLWPolyline() {
    DRW_DBG("dxfRW::processLWPolyline");
    int code;
    DRW_LWPolyline pl;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            if (applyExt)
                pl.applyExtrusion();
            iface->addLWPolyline(pl);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            pl.parseCode(code, reader);
            break;
        }
    }
    /* patch dxfrw_c: la sfarsitul fluxului polilinia nepredata isi pierdea vertecsii */
    for (size_t i = 0; i < pl.vertlist.size(); ++i)
        delete pl.vertlist[i];
    pl.vertlist.clear();
    return true;
}

/* patch dxfrw_c: rescrise processPolyline/processVertex.
   - un VERTEX urmat de alta entitate decat VERTEX/SEQEND era adaugat de doua ori (double free);
   - la EOF vertexul curent si vertecsii polilinei nepredate se pierdeau (leak);
   - inregistrarea SEQEND suprascria handle-ul si layerul polilinei. */
bool dxfRW::processPolyline() {
    DRW_DBG("dxfRW::processPolyline");
    int code;
    DRW_Polyline pl;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code != 0) {
            pl.parseCode(code, reader);
            continue;
        }
        nextentity = reader->getString();
        DRW_DBG(nextentity); DRW_DBG("\n");
        if (nextentity == "VERTEX") {
            processVertex(&pl);
            if (nextentity == "SEQEND") {
                /* se sar inregistrarile SEQEND pana la urmatoarea entitate */
                while (reader->readRec(&code)) {
                    if (code == 0) {
                        nextentity = reader->getString();
                        break;
                    }
                }
            }
        }
        iface->addPolyline(pl);
        return true;  //found new entity or ENDSEC, terminate
    }
    for (size_t i = 0; i < pl.vertlist.size(); ++i)
        delete pl.vertlist[i];
    pl.vertlist.clear();
    return true;
}

bool dxfRW::processVertex(DRW_Polyline *pl) {
    DRW_DBG("dxfRW::processVertex");
    int code;
    DRW_Vertex *v = new DRW_Vertex();
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        if (code != 0) {
            v->parseCode(code, reader);
            continue;
        }
        pl->appendVertex(v);
        v = NULL;
        nextentity = reader->getString();
        DRW_DBG(nextentity); DRW_DBG("\n");
        if (nextentity != "VERTEX")
            return true;  //found SEQEND (or unexpected entity), terminate
        v = new DRW_Vertex(); //another vertex
    }
    delete v;
    return true;
}

bool dxfRW::processText() {
    DRW_DBG("dxfRW::processText");
    int code;
    DRW_Text txt;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addText(txt);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            txt.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processMText() {
    DRW_DBG("dxfRW::processMText");
    int code;
    DRW_MText txt;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            txt.updateAngle();
            iface->addMText(txt);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            txt.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processHatch() {
    DRW_DBG("dxfRW::processHatch");
    int code;
    DRW_Hatch hatch;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addHatch(&hatch);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            hatch.parseCode(code, reader);
            break;
        }
    }
    return true;
}


bool dxfRW::processSpline() {
    DRW_DBG("dxfRW::processSpline");
    int code;
    DRW_Spline sp;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addSpline(&sp);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            sp.parseCode(code, reader);
            break;
        }
    }
    return true;
}


bool dxfRW::processImage() {
    DRW_DBG("dxfRW::processImage");
    int code;
    DRW_Image img;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addImage(&img);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            img.parseCode(code, reader);
            break;
        }
    }
    return true;
}


bool dxfRW::processDimension() {
    DRW_DBG("dxfRW::processDimension");
    int code;
    DRW_Dimension dim;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            int type = dim.type & 0x07;   /* patch dxfrw_c: tipul cotei sta in primii 3 biti (0-6) */
            switch (type) {
            case 0: {
                DRW_DimLinear d(dim);
                iface->addDimLinear(&d);
                break; }
            case 1: {
                DRW_DimAligned d(dim);
                iface->addDimAlign(&d);
                break; }
            case 2:  {
                DRW_DimAngular d(dim);
                iface->addDimAngular(&d);
                break;}
            case 3: {
                DRW_DimDiametric d(dim);
                iface->addDimDiametric(&d);
                break; }
            case 4: {
                DRW_DimRadial d(dim);
                iface->addDimRadial(&d);
                break; }
            case 5: {
                DRW_DimAngular3p d(dim);
                iface->addDimAngular3P(&d);
                break; }
            case 6: {
                DRW_DimOrdinate d(dim);
                iface->addDimOrdinate(&d);
                break; }
            }
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            dim.parseCode(code, reader);
            break;
        }
    }
    return true;
}

bool dxfRW::processLeader() {
    DRW_DBG("dxfRW::processLeader");
    int code;
    DRW_Leader leader;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->addLeader(&leader);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            leader.parseCode(code, reader);
            break;
        }
    }
    return true;
}


/********* Objects Section *********/

bool dxfRW::processObjects() {
    DRW_DBG("dxfRW::processObjects\n");
    int code;
    if (!reader->readRec(&code)){
        return false;
    }
    bool next = true;
    if (code == 0) {
            nextentity = reader->getString();
    } else {
            return false;  //first record in objects is 0
   }
    do {
        if (nextentity == "ENDSEC") {
            return true;  //found ENDSEC terminate
        } else if (nextentity == "IMAGEDEF") {
            processImageDef();
        } else {
            if (reader->readRec(&code)){
                if (code == 0)
                    nextentity = reader->getString();
            } else
                return false; //end of file without ENDSEC
        }
        /* patch dxfrw_c: la sfarsitul fluxului functiile process*() intorc true fara sa avanseze
           nextentity, iar bucla relua acelasi element la infinit (fisiere trunchiate/corupte) */
        if (!reader->isGood())
            return false;

    } while (next);
    return true;
}

bool dxfRW::processImageDef() {
    DRW_DBG("dxfRW::processImageDef");
    int code;
    DRW_ImageDef img;
    while (reader->readRec(&code)) {
        DRW_DBG(code); DRW_DBG("\n");
        switch (code) {
        case 0: {
            nextentity = reader->getString();
            DRW_DBG(nextentity); DRW_DBG("\n");
            iface->linkImage(&img);
            return true;  //found new entity or ENDSEC, terminate
        }
        default:
            img.parseCode(code, reader);
            break;
        }
    }
    return true;
}

/** utility function
 * convert a int to string in hex
 **/
std::string dxfRW::toHexStr(int n){
#if defined(__APPLE__)
    char buffer[9]= {'\0'};
    snprintf(buffer,9, "%X", n);
    return std::string(buffer);
#else
    std::ostringstream Convert;
    Convert << std::uppercase << std::hex << n;
    return Convert.str();
#endif
}
