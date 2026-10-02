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

#include <algorithm> /* patch dxfrw_c */
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "dwgreader.h"
#include "drw_textcodec.h"
#include "drw_dbg.h"

dwgReader::~dwgReader(){
    for (std::map<duint32, DRW_LType*>::iterator it=ltypemap.begin(); it!=ltypemap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Layer*>::iterator it=layermap.begin(); it!=layermap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Block*>::iterator it=blockmap.begin(); it!=blockmap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Textstyle*>::iterator it=stylemap.begin(); it!=stylemap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Dimstyle*>::iterator it=dimstylemap.begin(); it!=dimstylemap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Vport*>::iterator it=vportmap.begin(); it!=vportmap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Class*>::iterator it=classesmap.begin(); it!=classesmap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_Block_Record*>::iterator it=blockRecordmap.begin(); it!=blockRecordmap.end(); ++it)
        delete(it->second);
    for (std::map<duint32, DRW_AppId*>::iterator it=appIdmap.begin(); it!=appIdmap.end(); ++it)
        delete(it->second);

    delete fileBuf;
}

void dwgReader::parseAttribs(DRW_Entity* e){
    if (e != NULL){
        duint32 ltref =e->lTypeH.ref;
        duint32 lyref =e->layerH.ref;
        std::map<duint32, DRW_LType*>::iterator lt_it = ltypemap.find(ltref);
        if (lt_it != ltypemap.end()){
            e->lineType = (lt_it->second)->name;
        }
        std::map<duint32, DRW_Layer*>::iterator ly_it = layermap.find(lyref);
        if (ly_it != layermap.end()){
            e->layer = (ly_it->second)->name;
        }
        /* patch dxfrw_c: in XDATA citit din DWG, numele aplicatiei (1001) si numele layerului (1003)
           sunt handle-uri; se inlocuiesc cu numele din tabele. Un grup al carui APPID nu exista se
           elimina (nu poate fi scris corect in DXF), la fel o referinta la un layer inexistent. */
        if (!e->extData.empty()) {
            std::vector<DRW_Variant*> kept;
            bool dropGroup = false;
            for (std::vector<DRW_Variant*>::iterator it = e->extData.begin(); it != e->extData.end(); ++it) {
                DRW_Variant *v = *it;
                if (v->code() == 1001) {
                    dropGroup = false;
                    if (v->type() == DRW_Variant::INTEGER) {
                        std::map<duint32, DRW_AppId*>::iterator ap = appIdmap.find(static_cast<duint32>(v->content.i));
                        if (ap == appIdmap.end() || ap->second->name.empty())
                            dropGroup = true;
                        else
                            v->addString(1001, ap->second->name);
                    }
                } else if (v->code() == 1003 && v->type() == DRW_Variant::INTEGER) {
                    std::string name = findTableName(DRW::LAYER, v->content.i);
                    if (name.empty()) {
                        delete v;
                        continue;
                    }
                    v->addString(1003, name);
                }
                if (dropGroup) {
                    delete v;
                    continue;
                }
                kept.push_back(v);
            }
            e->extData.swap(kept);
        }
    }
}

std::string dwgReader::findTableName(DRW::TTYPE table, dint32 handle){
    std::string name;
    switch (table){
    case DRW::STYLE:{
        std::map<duint32, DRW_Textstyle*>::iterator st_it = stylemap.find(handle);
        if (st_it != stylemap.end())
            name = (st_it->second)->name;
        break;}
    case DRW::DIMSTYLE:{
        std::map<duint32, DRW_Dimstyle*>::iterator ds_it = dimstylemap.find(handle);
        if (ds_it != dimstylemap.end())
            name = (ds_it->second)->name;
        break;}
    case DRW::BLOCK_RECORD:{ //use DRW_Block because name are more correct
//        std::map<duint32, DRW_Block*>::iterator bk_it = blockmap.find(handle);
//        if (bk_it != blockmap.end())
        std::map<duint32, DRW_Block_Record*>::iterator bk_it = blockRecordmap.find(handle);
        if (bk_it != blockRecordmap.end())
            name = (bk_it->second)->name;
        break;}
/*    case DRW::VPORT:{
        std::map<duint32, DRW_Vport*>::iterator vp_it = vportmap.find(handle);
        if (vp_it != vportmap.end())
            name = (vp_it->second)->name;
        break;}*/
    case DRW::LAYER:{
        std::map<duint32, DRW_Layer*>::iterator ly_it = layermap.find(handle);
        if (ly_it != layermap.end())
            name = (ly_it->second)->name;
        break;}
    case DRW::LTYPE:{
        std::map<duint32, DRW_LType*>::iterator lt_it = ltypemap.find(handle);
        if (lt_it != ltypemap.end())
            name = (lt_it->second)->name;
        break;}
    default:
        break;
    }
    return name;
}

bool dwgReader::readDwgHeader(DRW_Header& hdr, dwgBuffer *buf, dwgBuffer *hBuf){
    bool ret = hdr.parseDwg(version, buf, hBuf, maintenanceVersion);
    //RLZ: copy objectControl handles
    return ret;
}

//RLZ: TODO add check instead print
bool dwgReader::checkSentinel(dwgBuffer *buf, enum secEnum::DWGSection, bool start){
    DRW_UNUSED(start);
    for (int i=0; i<16;i++) {
        DRW_DBGH(buf->getRawChar8()); DRW_DBG(" ");
    }
    return true;
}

/*********** objects map ************************/
/** Note: object map are split in sections with max size 2035?
 *  heach section are 2 bytes size + data bytes + 2 bytes crc
 *  size value are data bytes + 2 and to calculate crc are used
 *  2 bytes size + data bytes
 *  last section are 2 bytes size + 2 bytes crc (size value always 2)
**/
bool dwgReader::readDwgHandles(dwgBuffer *dbuf, duint32 offset, duint32 size) {
    DRW_DBG("\ndwgReader::readDwgHandles\n");
    if (!dbuf->setPosition(offset))
        return false;

    duint32 maxPos = offset + size;
    DRW_DBG("\nSection HANDLES offset= "); DRW_DBG(offset);
    DRW_DBG("\nSection HANDLES size= "); DRW_DBG(size);
    DRW_DBG("\nSection HANDLES maxPos= "); DRW_DBG(maxPos);

    int startPos = offset;

    while (maxPos > dbuf->getPosition()) {
        DRW_DBG("\nstart handles section buf->curPosition()= "); DRW_DBG(dbuf->getPosition()); DRW_DBG("\n");
        duint16 size = dbuf->getBERawShort16();
        /* patch dxfrw_c: o inregistrare care nu incape in sectiune inseamna sfarsitul datelor utile
           (altfel se citea dupa limita si toata harta era declarata invalida) */
        if (size < 2 || static_cast<duint64>(startPos) + size + 2 > maxPos)
            break;
        DRW_DBG("object map section size= "); DRW_DBG(size); DRW_DBG("\n");
        dbuf->setPosition(startPos);
        duint8 *tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        /* patch dxfrw_c: o inregistrare goala (doar suma de control) marcheaza sfarsitul hartii de obiecte.
           Biblioteca o sarea si continua, interpretand ca date rezidurile unei salvari anterioare; perechile
           rezultate suprascriau pozitiile reale ale obiectelor, iar tabelele nu mai erau gasite (fisiere goale). */
        if (size == 2) {
            delete[] tmpByteStr;
            break;
        }
        if (size != 2){
            buff.setPosition(2);
            int lastHandle = 0;
            int lastLoc = 0;
            //read data
            while(buff.getPosition()< size){
                lastHandle += buff.getUModularChar();
                DRW_DBG("object map lastHandle= "); DRW_DBGH(lastHandle);
                lastLoc += buff.getModularChar();
                DRW_DBG(" lastLoc= "); DRW_DBG(lastLoc); DRW_DBG("\n");
                ObjectMap[lastHandle]= objHandle(0, lastHandle, lastLoc);
            }
        }
        //verify crc
        duint16 crcCalc = buff.crc8(0xc0c1,0,size);
        delete[]tmpByteStr;
        duint16 crcRead = dbuf->getBERawShort16();
        DRW_DBG("object map section crc8 read= "); DRW_DBG(crcRead);
        DRW_DBG("\nobject map section crc8 calculated= "); DRW_DBG(crcCalc);
        DRW_DBG("\nobject section buf->curPosition()= "); DRW_DBG(dbuf->getPosition()); DRW_DBG("\n");
        /* patch dxfrw_c: suma de control era calculata, dar niciodata comparata. Fara aceasta verificare,
           zona de umplutura de la sfarsitul sectiunii era interpretata ca inregistrari valide, iar perechile
           rezultate suprascriau pozitiile reale ale obiectelor (tabelele nu mai erau gasite). */
        if (crcCalc != crcRead)
            break;
        startPos = dbuf->getPosition();
    }

    /* patch dxfrw_c: rezultatul se judeca dupa continutul citit, nu dupa steagul bufferului, care
       ramane pe "eroare" dupa orice citire la limita, chiar daca harta a fost citita integral */
    return !ObjectMap.empty();
}

/*********** objects ************************/
/**
 * Reads all the object referenced in the object map section of the DWG file
 * (using their object file offsets)
 */
bool dwgReader::readDwgTables(DRW_Header& hdr, dwgBuffer *dbuf) {
    DRW_DBG("\ndwgReader::readDwgTables start\n");
    bool ret = true;
    bool ret2 = true;
    objHandle oc;
    std::map<duint32, objHandle>::iterator mit;
    dint16 oType;
    duint32 bs = 0; //bit size of handle stream 2010+
    duint8 *tmpByteStr;

    //parse linetypes, start with linetype Control
    mit = ObjectMap.find(hdr.linetypeCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: LineType control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing LineType control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl ltControl;
        dbuf->setPosition(oc.loc);
        int csize = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[csize]();
        dbuf->getBytes(tmpByteStr, csize);
        dwgBuffer cbuff(tmpByteStr, csize, &decoder);
        //verify if object are correct
        oType = cbuff.getObjType(version);
        if (oType != 0x38) {
                DRW_DBG("\nWARNING: Not LineType control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x38\n");
                ret = false;
            } else { //reset position
            cbuff.resetPosition();
            ret2 = ltControl.parseDwg(version, &cbuff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=ltControl.hadlesList.begin(); it != ltControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: LineType not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("\nLineType Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" loc.: "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_LType *lt = new DRW_LType();
                dbuf->setPosition(oc.loc);
                int lsize = dbuf->getModularShort();
                DRW_DBG("LineType size in bytes= "); DRW_DBG(lsize);
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[lsize]();
                dbuf->getBytes(tmpByteStr, lsize);
                dwgBuffer lbuff(tmpByteStr, lsize, &decoder);
                ret2 = lt->parseDwg(version, &lbuff, bs);
                if (ltypemap.count(lt->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete ltypemap[lt->handle];
                ltypemap[lt->handle] = lt;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //parse layers, start with layer Control
    mit = ObjectMap.find(hdr.layerCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: Layer control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing Layer control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl layControl;
        dbuf->setPosition(oc.loc);
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x32) {
                DRW_DBG("\nWARNING: Not Layer control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x32\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = layControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=layControl.hadlesList.begin(); it != layControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: Layer not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("Layer Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Layer *la = new DRW_Layer();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = la->parseDwg(version, &buff, bs);
                if (layermap.count(la->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete layermap[la->handle];
                layermap[la->handle] = la;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //set linetype in layer
    for (std::map<duint32, DRW_Layer*>::iterator it=layermap.begin(); it!=layermap.end(); ++it) {
        DRW_Layer *ly = it->second;
        duint32 ref =ly->lTypeH.ref;
        std::map<duint32, DRW_LType*>::iterator lt_it = ltypemap.find(ref);
        if (lt_it != ltypemap.end()){
            ly->lineType = (lt_it->second)->name;
        }
    }

    //parse text styles, start with style Control
    mit = ObjectMap.find(hdr.styleCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: Style control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing Style control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl styControl;
        dbuf->setPosition(oc.loc);
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x34) {
                DRW_DBG("\nWARNING: Not Text Style control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x34\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = styControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=styControl.hadlesList.begin(); it != styControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: Style not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("Style Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Textstyle *sty = new DRW_Textstyle();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = sty->parseDwg(version, &buff, bs);
                if (stylemap.count(sty->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete stylemap[sty->handle];
                stylemap[sty->handle] = sty;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //parse dim styles, start with dimstyle Control
    mit = ObjectMap.find(hdr.dimstyleCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: Dimension Style control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing Dimension Style control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl dimstyControl;
        dbuf->setPosition(oc.loc);
        duint32 size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x44) {
                DRW_DBG("\nWARNING: Not Dim Style control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x44\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = dimstyControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=dimstyControl.hadlesList.begin(); it != dimstyControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: Dimension Style not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("Dimstyle Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Dimstyle *sty = new DRW_Dimstyle();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = sty->parseDwg(version, &buff, bs);
                if (dimstylemap.count(sty->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete dimstylemap[sty->handle];
                dimstylemap[sty->handle] = sty;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //parse vports, start with vports Control
    mit = ObjectMap.find(hdr.vportCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: vports control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing vports control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl vportControl;
        dbuf->setPosition(oc.loc);
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x40) {
                DRW_DBG("\nWARNING: Not VPorts control object, found oType: ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x40\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = vportControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=vportControl.hadlesList.begin(); it != vportControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: vport not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("Vport Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Vport *vp = new DRW_Vport();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = vp->parseDwg(version, &buff, bs);
                if (vportmap.count(vp->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete vportmap[vp->handle];
                vportmap[vp->handle] = vp;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //parse Block_records , start with Block_record Control
    mit = ObjectMap.find(hdr.blockCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: Block_record control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing Block_record control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_ObjControl blockControl;
        dbuf->setPosition(oc.loc);
        int csize = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[csize]();
        dbuf->getBytes(tmpByteStr, csize);
        dwgBuffer buff(tmpByteStr, csize, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x30) {
                DRW_DBG("\nWARNING: Not Block Record control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x30\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = blockControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=blockControl.hadlesList.begin(); it != blockControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: block record not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("block record Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Block_Record *br = new DRW_Block_Record();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = br->parseDwg(version, &buff, bs);
                if (blockRecordmap.count(br->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete blockRecordmap[br->handle];
                blockRecordmap[br->handle] = br;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //parse appId , start with appId Control
    mit = ObjectMap.find(hdr.appidCtrl);
    if (mit==ObjectMap.end()) {
        DRW_DBG("\nWARNING: AppId control not found\n");
        ret = false;
    } else {
        DRW_DBG("\n**********Parsing AppId control*******\n");
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_DBG("AppId Control Obj Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
        DRW_ObjControl appIdControl;
        dbuf->setPosition(oc.loc);
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //verify if object are correct
        oType = buff.getObjType(version);
        if (oType != 0x42) {
                DRW_DBG("\nWARNING: Not AppId control object, found oType ");
                DRW_DBG(oType);  DRW_DBG(" instead 0x42\n");
                ret = false;
            } else { //reset position
            buff.resetPosition();
            ret2 = appIdControl.parseDwg(version, &buff, bs);
            if(ret)
                ret = ret2;
        }
        delete[]tmpByteStr;
        for (std::list<duint32>::iterator it=appIdControl.hadlesList.begin(); it != appIdControl.hadlesList.end(); ++it){
            mit = ObjectMap.find(*it);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: AppId not found\n");
                /* patch dxfrw_c: o intrare la care tabelul trimite, dar care nu mai exista in desen
                   (element sters), nu este o eroare de citire: se sare peste ea */
            } else {
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("AppId Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_AppId *ai = new DRW_AppId();
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                ret2 = ai->parseDwg(version, &buff, bs);
                if (appIdmap.count(ai->handle)) /* patch dxfrw_c: handle duplicat in fisier corupt */
                    delete appIdmap[ai->handle];
                appIdmap[ai->handle] = ai;
                if(ret)
                    ret = ret2;
                delete[]tmpByteStr;
            }
        }
    }

    //RLZ: parse remaining object controls, TODO: implement all
    if (DRW_DBGGL == DRW_dbg::DEBUG){
        mit = ObjectMap.find(hdr.viewCtrl);
        if (mit==ObjectMap.end()) {
            DRW_DBG("\nWARNING: View control not found\n");
            ret = false;
        } else {
            DRW_DBG("\n**********Parsing View control*******\n");
            oc = mit->second;
            ObjectMap.erase(mit);
            DRW_DBG("View Control Obj Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
            DRW_ObjControl viewControl;
            dbuf->setPosition(oc.loc);
            int size = dbuf->getModularShort();
            if (version > DRW::AC1021) //2010+
                bs = dbuf->getUModularChar();
            else
                bs = 0;
            tmpByteStr = new duint8[size]();
            dbuf->getBytes(tmpByteStr, size);
            dwgBuffer buff(tmpByteStr, size, &decoder);
            //verify if object are correct
            oType = buff.getObjType(version);
            if (oType != 0x3C) {
                    DRW_DBG("\nWARNING: Not View control object, found oType ");
                    DRW_DBG(oType);  DRW_DBG(" instead 0x3C\n");
                    ret = false;
                } else { //reset position
                buff.resetPosition();
                ret2 = viewControl.parseDwg(version, &buff, bs);
                if(ret)
                    ret = ret2;
            }
            delete[]tmpByteStr;
        }

        mit = ObjectMap.find(hdr.ucsCtrl);
        if (mit==ObjectMap.end()) {
            DRW_DBG("\nWARNING: Ucs control not found\n");
            ret = false;
        } else {
            oc = mit->second;
            ObjectMap.erase(mit);
            DRW_DBG("\n**********Parsing Ucs control*******\n");
            DRW_DBG("Ucs Control Obj Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
            DRW_ObjControl ucsControl;
            dbuf->setPosition(oc.loc);
            int size = dbuf->getModularShort();
            if (version > DRW::AC1021) //2010+
                bs = dbuf->getUModularChar();
            else
                bs = 0;
            tmpByteStr = new duint8[size]();
            dbuf->getBytes(tmpByteStr, size);
            dwgBuffer buff(tmpByteStr, size, &decoder);
            //verify if object are correct
            oType = buff.getObjType(version);
            if (oType != 0x3E) {
                    DRW_DBG("\nWARNING: Not Ucs control object, found oType ");
                    DRW_DBG(oType);  DRW_DBG(" instead 0x3E\n");
                    ret = false;
                } else { //reset position
                buff.resetPosition();
                ret2 = ucsControl.parseDwg(version, &buff, bs);
                if(ret)
                    ret = ret2;
            }
            delete[]tmpByteStr;
        }

        if (version < DRW::AC1018) {//r2000-
            mit = ObjectMap.find(hdr.vpEntHeaderCtrl);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: vpEntHeader control not found\n");
                ret = false;
            } else {
                DRW_DBG("\n**********Parsing vpEntHeader control*******\n");
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("vpEntHeader Control Obj Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_ObjControl vpEntHeaderCtrl;
                dbuf->setPosition(oc.loc);
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) //2010+
                    bs = dbuf->getUModularChar();
                else
                    bs = 0;
                tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                //verify if object are correct
                oType = buff.getObjType(version);
                if (oType != 0x46) {
                        DRW_DBG("\nWARNING: Not vpEntHeader control object, found oType ");
                        DRW_DBG(oType);  DRW_DBG(" instead 0x46\n");
                        ret = false;
                    } else { //reset position
                    buff.resetPosition();
/* RLZ: writeme                   ret2 = vpEntHeader.parseDwg(version, &buff, bs);
                    if(ret)
                        ret = ret2;*/
                }
                delete[]tmpByteStr;
            }
        }
    }

    return ret;
}

bool dwgReader::readDwgBlocks(DRW_Interface& intfa, dwgBuffer *dbuf){
    bool ret = true;
    bool ret2 = true;
    duint32 bs =0;
    duint8 *tmpByteStr;
    std::map<duint32, objHandle>::iterator mit;
    DRW_DBG("\nobject map total size= "); DRW_DBG(ObjectMap.size());

    /* patch dxfrw_c: inregistrarea unui bloc anonim are in DWG doar prefixul ("*D", "*U"), iar numele
       complet ("*D834") este in entitatea BLOCK. Numele era actualizat abia cand se ajungea la blocul
       respectiv, deci cotele si INSERT-urile din blocurile citite inainte primeau numele incomplet si
       trimiteau spre un bloc inexistent (cote fara geometrie, insertii pierdute). Se citesc intai numele
       tuturor blocurilor, apoi entitatile. */
    for (std::map<duint32, DRW_Block_Record*>::iterator it=blockRecordmap.begin(); it != blockRecordmap.end(); ++it){
        DRW_Block_Record* bkr= it->second;
        mit = ObjectMap.find(bkr->block);
        if (mit == ObjectMap.end() || !dbuf->setPosition(mit->second.loc))
            continue;
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        if (size <= 0 || size > dbuf->numRemainingBytes())
            continue;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        DRW_Block bk;
        if (bk.parseDwg(version, &buff, bs) && !bk.name.empty())
            bkr->name = bk.name;
        delete[]tmpByteStr;
    }
    /* patch dxfrw_c: un DWG poate contine doua blocuri anonime distincte cu acelasi nume (de ex. "*D841"
       de doua ori); in DWG referintele sunt prin handle, dar in DXF prin nume, deci al doilea ar fi
       scris peste primul (handle-uri duplicate, cote cu geometria gresita). Blocurile anonime cu nume
       deja folosit primesc urmatorul numar liber cu acelasi prefix, inainte de rezolvarea referintelor. */
    {
        std::map<std::string, bool> used;
        std::map<std::string, int> maxNum;   /* cel mai mare numar folosit pentru fiecare prefix ("*D") */
        for (std::map<duint32, DRW_Block_Record*>::iterator it=blockRecordmap.begin(); it != blockRecordmap.end(); ++it){
            std::string up = it->second->name;
            std::transform(up.begin(), up.end(), up.begin(), ::toupper);
            used[up] = true;
            if (up.size() > 2 && up[0] == '*' && up.find_first_not_of("0123456789", 2) == std::string::npos) {
                int n = std::atoi(up.c_str() + 2);
                std::string prefix = up.substr(0, 2);
                if (n > maxNum[prefix]) maxNum[prefix] = n;
            }
        }
        std::map<std::string, bool> seen;
        for (std::map<duint32, DRW_Block_Record*>::iterator it=blockRecordmap.begin(); it != blockRecordmap.end(); ++it){
            DRW_Block_Record* bkr = it->second;
            std::string up = bkr->name;
            std::transform(up.begin(), up.end(), up.begin(), ::toupper);
            if (seen[up] && up.size() > 1 && up[0] == '*' && up.compare(0, 6, "*PAPER") != 0 && up.compare(0, 6, "*MODEL") != 0) {
                std::string prefix = bkr->name.substr(0, 2);
                std::string fresh;
                do {
                    std::ostringstream os;
                    os << prefix << ++maxNum[up.substr(0, 2)];
                    fresh = os.str();
                    up = fresh;
                    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
                } while (used[up]);
                used[up] = true;
                bkr->name = fresh;
            }
            seen[up] = true;
        }
    }

    for (std::map<duint32, DRW_Block_Record*>::iterator it=blockRecordmap.begin(); it != blockRecordmap.end(); ++it){
        DRW_Block_Record* bkr= it->second;
        DRW_DBG("\nParsing Block, record handle= "); DRW_DBGH(it->first); DRW_DBG(" Name= "); DRW_DBG(bkr->name); DRW_DBG("\n");
        DRW_DBG("\nFinding Block, handle= "); DRW_DBGH(bkr->block); DRW_DBG("\n");
        mit = ObjectMap.find(bkr->block);
        if (mit==ObjectMap.end()) {
            DRW_DBG("\nWARNING: block entity not found\n");
            ret = false;
            continue;
        }
        objHandle oc = mit->second;
        ObjectMap.erase(mit);
        DRW_DBG("Block Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" Location: "); DRW_DBG(oc.loc); DRW_DBG("\n");
        if ( !(dbuf->setPosition(oc.loc)) ){
            DRW_DBG("Bad Location reading blocks\n");
            ret = false;
            continue;
        }
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        DRW_Block bk;
        ret2 = bk.parseDwg(version, &buff, bs);
        delete[]tmpByteStr;
        ret = ret && ret2;
        parseAttribs(&bk);
        //complete block entity with block record data
        bk.basePoint = bkr->basePoint;
        bk.flags = bkr->flags;
        /* patch dxfrw_c: numele stabilit la inceput (complet si unic) are prioritate */
        if (!bkr->name.empty())
            bk.name = bkr->name;
        intfa.addBlock(bk);
        //and update block record name
        bkr->name = bk.name;

        /**read & send block entities**/
        // in dwg code 330 are not set like dxf in ModelSpace & PaperSpace, set it (RLZ: only tested in 2000)
        if (bk.parentHandle == DRW::NoHandle) {
            // in dwg code 330 are not set like dxf in ModelSpace & PaperSpace, set it
            bk.parentHandle= bkr->handle;
            //and do not send block entities like dxf
        } else {
            if (version < DRW::AC1018) { //pre 2004
                duint32 nextH = bkr->firstEH;
                while (nextH != 0){
                    mit = ObjectMap.find(nextH);
                    if (mit==ObjectMap.end()) {
                        nextH = bkr->lastEH;//end while if entity not foud
                        DRW_DBG("\nWARNING: Entity of block not found\n");
                        ret = false;
                        continue;
                    } else {//foud entity reads it
                        oc = mit->second;
                        ObjectMap.erase(mit);
                        ret2 = readDwgEntity(dbuf, oc, intfa);
                        if (!ret2) ++failedObjects;   /* patch dxfrw_c */
                        ret = ret && ret2;
                    }
                    if (nextH == bkr->lastEH)
                        nextH = 0; //redundant, but prevent read errors
                    else
                        nextH = nextEntLink;
                }
            } else {//2004+
                for (std::vector<duint32>::iterator it = bkr->entMap.begin() ; it != bkr->entMap.end(); ++it){
                    duint32 nextH = *it;
                    mit = ObjectMap.find(nextH);
                    if (mit==ObjectMap.end()) {
                        DRW_DBG("\nWARNING: Entity of block not found\n");
                        ret = false;
                        continue;
                    } else {//foud entity reads it
                        oc = mit->second;
                        ObjectMap.erase(mit);
                        DRW_DBG("\nBlocks, parsing entity: "); DRW_DBGH(oc.handle); DRW_DBG(", pos: "); DRW_DBG(oc.loc); DRW_DBG("\n");
                        ret2 = readDwgEntity(dbuf, oc, intfa);
                        if (!ret2) ++failedObjects;   /* patch dxfrw_c */
                        ret = ret && ret2;
                    }
                }
            }//end 2004+
        }

        //end block entity, really needed to parse a dummy entity??
        mit = ObjectMap.find(bkr->endBlock);
        if (mit==ObjectMap.end()) {
            DRW_DBG("\nWARNING: end block entity not found\n");
            ret = false;
            continue;
        }
        oc = mit->second;
        ObjectMap.erase(mit);
        DRW_DBG("End block Handle= "); DRW_DBGH(oc.handle); DRW_DBG(" Location: "); DRW_DBG(oc.loc); DRW_DBG("\n");
        dbuf->setPosition(oc.loc);
        size = dbuf->getModularShort();
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        else
            bs = 0;
        tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff1(tmpByteStr, size, &decoder);
        DRW_Block end;
        end.isEnd = true;
        ret2 = end.parseDwg(version, &buff1, bs);
        delete[]tmpByteStr;
        ret = ret && ret2;
        if (bk.parentHandle == DRW::NoHandle) bk.parentHandle= bkr->handle;
        parseAttribs(&end);
        intfa.endBlock();
    }

    return ret;
}

bool dwgReader::readPlineVertex(DRW_Polyline& pline, dwgBuffer *dbuf){
    bool ret = true;
    bool ret2 = true;
    objHandle oc;
    duint32 bs = 0;
    std::map<duint32, objHandle>::iterator mit;

    if (version < DRW::AC1018) { //pre 2004
        duint32 nextH = pline.firstEH;
        while (nextH != 0){
            mit = ObjectMap.find(nextH);
            if (mit==ObjectMap.end()) {
                nextH = pline.lastEH;//end while if entity not foud
                DRW_DBG("\nWARNING: pline vertex not found\n");
                ret = false;
                continue;
            } else {//foud entity reads it
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_Vertex vt;
                dbuf->setPosition(oc.loc);
                //RLZ: verify if pos is ok
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) {//2010+
                    bs = dbuf->getUModularChar();
                }
                duint8 *tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                dint16 oType = buff.getObjType(version);
                buff.resetPosition();
                DRW_DBG(" object type= "); DRW_DBG(oType); DRW_DBG("\n");
                ret2 = vt.parseDwg(version, &buff, bs, pline.basePoint.z);
                delete[]tmpByteStr;
                pline.addVertex(vt);
                nextEntLink = vt.nextEntLink; \
                prevEntLink = vt.prevEntLink;
                ret = ret && ret2;
            }
            if (nextH == pline.lastEH)
                nextH = 0; //redundant, but prevent read errors
            else
                nextH = nextEntLink;
        }
    } else {//2004+
        for (std::list<duint32>::iterator it = pline.hadlesList.begin() ; it != pline.hadlesList.end(); ++it){
            duint32 nextH = *it;
            mit = ObjectMap.find(nextH);
            if (mit==ObjectMap.end()) {
                DRW_DBG("\nWARNING: Entity of block not found\n");
                ret = false;
                continue;
            } else {//foud entity reads it
                oc = mit->second;
                ObjectMap.erase(mit);
                DRW_DBG("\nPline vertex, parsing entity: "); DRW_DBGH(oc.handle); DRW_DBG(", pos: "); DRW_DBG(oc.loc); DRW_DBG("\n");
                DRW_Vertex vt;
                dbuf->setPosition(oc.loc);
                //RLZ: verify if pos is ok
                int size = dbuf->getModularShort();
                if (version > DRW::AC1021) {//2010+
                    bs = dbuf->getUModularChar();
                }
                duint8 *tmpByteStr = new duint8[size]();
                dbuf->getBytes(tmpByteStr, size);
                dwgBuffer buff(tmpByteStr, size, &decoder);
                dint16 oType = buff.getObjType(version);
                buff.resetPosition();
                DRW_DBG(" object type= "); DRW_DBG(oType); DRW_DBG("\n");
                ret2 = vt.parseDwg(version, &buff, bs, pline.basePoint.z);
                delete[]tmpByteStr;
                pline.addVertex(vt);
                nextEntLink = vt.nextEntLink; \
                prevEntLink = vt.prevEntLink;
                ret = ret && ret2;
            }
        }
    }//end 2004+
    DRW_DBG("\nRemoved SEQEND entity: "); DRW_DBGH(pline.seqEndH.ref);DRW_DBG("\n");
    ObjectMap.erase(pline.seqEndH.ref);

    return ret;
}

/* patch dxfrw_c: citeste atributele (ATTRIB) unei insertii, dupa handle-urile retinute de
   DRW_Insert::parseDwg (pana la R2000 o lista inlantuita intre primul si ultimul atribut, din R2004 lista
   completa), le ataseaza insertiei si scoate din harta obiectele citite si SEQEND-ul. Un atribut deja
   mutat in lista obiectelor necitite (handle mai mic decat al insertiei) este cautat si acolo. */
bool dwgReader::readInsertAttribs(DRW_Insert& ins, dwgBuffer *dbuf){
    bool ret = true;
    duint32 savedNext = nextEntLink, savedPrev = prevEntLink;
    std::vector<duint32> handles = ins.attribHandles;
    bool linked = (version < DRW::AC1018);
    duint32 nextH = linked ? ins.firstAttribH : 0;
    size_t idx = 0;
    size_t guard = 0;
    while (guard++ < 100000) {
        duint32 h;
        if (linked) {
            if (nextH == 0) break;
            h = nextH;
        } else {
            if (idx >= handles.size()) break;
            h = handles[idx++];
        }
        objHandle oc;
        std::map<duint32, objHandle>::iterator mit = ObjectMap.find(h);
        if (mit != ObjectMap.end()) {
            oc = mit->second;
            ObjectMap.erase(mit);
        } else {
            mit = objObjectMap.find(h);
            if (mit == objObjectMap.end()) {
                ret = false;
                if (linked) break;
                continue;
            }
            oc = mit->second;
            objObjectMap.erase(mit);
        }
        if (!dbuf->setPosition(oc.loc)) { ret = false; break; }
        int size = dbuf->getModularShort();
        duint32 bs = 0;
        if (version > DRW::AC1021) //2010+
            bs = dbuf->getUModularChar();
        if (size <= 0 || size > dbuf->numRemainingBytes()) { ret = false; break; }
        duint8 *tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        dwgBuffer buff(tmpByteStr, size, &decoder);
        DRW_Attrib a;
        bool ok = a.parseDwg(version, &buff, bs);
        delete[]tmpByteStr;
        duint32 linkNext = a.nextEntLink;
        if (ok) {
            parseAttribs(&a);
            a.style = findTableName(DRW::STYLE, a.styleH.ref);
            ins.attributes.push_back(a);
        } else {
            ++failedObjects;
            ret = false;
        }
        if (linked) {
            if (h == ins.lastAttribH) break;
            nextH = linkNext;
        }
    }
    ObjectMap.erase(ins.seqendH.ref);
    objObjectMap.erase(ins.seqendH.ref);
    nextEntLink = savedNext;
    prevEntLink = savedPrev;
    return ret;
}

bool dwgReader::readDwgEntities(DRW_Interface& intfa, dwgBuffer *dbuf){
    bool ret = true;
    bool ret2 = true;

    DRW_DBG("\nobject map total size= "); DRW_DBG(ObjectMap.size());
    std::map<duint32, objHandle>::iterator itB=ObjectMap.begin();
    std::map<duint32, objHandle>::iterator itE=ObjectMap.end();
    while (itB != itE){
        ret2 = readDwgEntity(dbuf, itB->second, intfa);
        if (!ret2) ++failedObjects;   /* patch dxfrw_c */
        ObjectMap.erase(itB);
        itB=ObjectMap.begin();
        if (ret)
            ret = ret2;
    }
    return ret;
}

/**
 * Reads a dwg drawing entity (dwg object entity) given its offset in the file
 */
bool dwgReader::readDwgEntity(dwgBuffer *dbuf, objHandle& obj, DRW_Interface& intfa){
    bool ret = true;
    duint32 bs = 0;

#define ENTRY_PARSE(e) \
    ret = e.parseDwg(version, &buff, bs); \
    parseAttribs(&e); \
    nextEntLink = e.nextEntLink; \
    prevEntLink = e.prevEntLink;

    nextEntLink = prevEntLink = 0;// set to 0 to skip unimplemented entities
        dbuf->setPosition(obj.loc);
        //verify if position is ok:
        if (!dbuf->isGood()){
            DRW_DBG(" Warning: readDwgEntity, bad location\n");
            return false;
        }
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) {//2010+
            bs = dbuf->getUModularChar();
        }
        duint8 *tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        //verify if getBytes is ok:
        if (!dbuf->isGood()){
            DRW_DBG(" Warning: readDwgEntity, bad size\n");
            delete[]tmpByteStr;
            return false;
        }
        dwgBuffer buff(tmpByteStr, size, &decoder);
        dint16 oType = buff.getObjType(version);
        buff.resetPosition();

        if (oType > 499){
            std::map<duint32, DRW_Class*>::iterator it = classesmap.find(oType);
            if (it == classesmap.end()){//fail, not found in classes set error
                DRW_DBG("Class "); DRW_DBG(oType);DRW_DBG("not found, handle: "); DRW_DBG(obj.handle); DRW_DBG("\n");
                delete[]tmpByteStr;
                return false;
            } else {
                DRW_Class *cl = it->second;
                if (cl->dwgType != 0)
                    oType = cl->dwgType;
            }
        }

        obj.type = oType;
        switch (oType){
        case 17: {
            DRW_Arc e;
            ENTRY_PARSE(e)
            intfa.addArc(e);
            break; }
        case 18: {
            DRW_Circle e;
            ENTRY_PARSE(e)
            intfa.addCircle(e);
            break; }
        case 19:{
            DRW_Line e;
            ENTRY_PARSE(e)
            intfa.addLine(e);
            break;}
        case 27: {
            DRW_Point e;
            ENTRY_PARSE(e)
            intfa.addPoint(e);
            break; }
        case 35: {
            DRW_Ellipse e;
            ENTRY_PARSE(e)
            intfa.addEllipse(e);
            break; }
        case 7:
        case 8: {//minsert = 8
            DRW_Insert e;
            ENTRY_PARSE(e)
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockRecH.ref);//RLZ: find as block or blockrecord (ps & ps0)
            if (ret && e.hasAttribs)
                readInsertAttribs(e, dbuf); /* patch dxfrw_c */
            intfa.addInsert(e);
            break; }
        case 103: { /* patch dxfrw_c: MULTILEADER (clasa, vezi DRW_Class::toDwgType) */
            DRW_MLeader e;
            ENTRY_PARSE(e)
            if (ret) {
                e.textStyle = findTableName(DRW::STYLE, e.ctxTextStyleH);
                e.entTextStyle = findTableName(DRW::STYLE, e.entTextStyleH);
                e.leaderLineType = findTableName(DRW::LTYPE, e.lineTypeH);
                e.arrowBlock = findTableName(DRW::BLOCK_RECORD, e.arrowH);
                e.blockName = findTableName(DRW::BLOCK_RECORD, e.ctxBlockH);
                e.entBlock = findTableName(DRW::BLOCK_RECORD, e.entBlockH);
                for (size_t i = 0; i < e.leaders.size(); ++i)
                    for (size_t j = 0; j < e.leaders[i].lines.size(); ++j) {
                        DRW_MLeaderLine &l = e.leaders[i].lines[j];
                        l.lineTypeName = findTableName(DRW::LTYPE, l.lineTypeH);
                        l.arrowBlock = findTableName(DRW::BLOCK_RECORD, l.arrowH);
                    }
                for (size_t i = 0; i < e.blockAttribs.size(); ++i) {
                    std::map<duint32, std::string>::iterator at = attdefTags.find(e.blockAttribs[i].attdefH);
                    if (at != attdefTags.end())
                        e.blockAttribs[i].tag = at->second;
                }
                intfa.addMLeader(&e);
            }
            break; }
        case 3: { /* patch dxfrw_c: ATTDEF */
            DRW_Attdef e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::STYLE, e.styleH.ref);
            attdefTags[e.handle] = e.tag;   /* pentru atributele blocurilor din MULTILEADER */
            intfa.addAttdef(e);
            break; }
        case 77: {
            DRW_LWPolyline e;
            ENTRY_PARSE(e)
            intfa.addLWPolyline(e);
            break; }
        case 1: {
            DRW_Text e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::STYLE, e.styleH.ref);
            intfa.addText(e);
            break; }
        case 44: {
            DRW_MText e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::STYLE, e.styleH.ref);
            intfa.addMText(e);
            break; }
        case 28: {
            DRW_3Dface e;
            ENTRY_PARSE(e)
            intfa.add3dFace(e);
            break; }
        case 20: {
            DRW_DimOrdinate e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimOrdinate(&e);
            break; }
        case 21: {
            DRW_DimLinear e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimLinear(&e);
            break; }
        case 22: {
            DRW_DimAligned e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimAlign(&e);
            break; }
        case 23: {
            DRW_DimAngular3p e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimAngular3P(&e);
            break; }
        case 24: {
            DRW_DimAngular e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimAngular(&e);
            break; }
        case 25: {
            DRW_DimRadial e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimRadial(&e);
            break; }
        case 26: {
            DRW_DimDiametric e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            /* patch dxfrw_c: blocul de geometrie al cotei era citit ca handle, dar niciodata rezolvat in nume */
            e.name = findTableName(DRW::BLOCK_RECORD, e.blockH.ref);
            intfa.addDimDiametric(&e);
            break; }
        case 45: {
            DRW_Leader e;
            ENTRY_PARSE(e)
            e.style = findTableName(DRW::DIMSTYLE, e.dimStyleH.ref);
            intfa.addLeader(&e);
            break; }
        case 31: {
            DRW_Solid e;
            ENTRY_PARSE(e)
            intfa.addSolid(e);
            break; }
        case 78: {
            DRW_Hatch e;
            ENTRY_PARSE(e)
            intfa.addHatch(&e);
            break; }
        case 32: {
            DRW_Trace e;
            ENTRY_PARSE(e)
            intfa.addTrace(e);
            break; }
        case 34: {
            DRW_Viewport e;
            ENTRY_PARSE(e)
            intfa.addViewport(e);
            break; }
        case 36: {
            DRW_Spline e;
            ENTRY_PARSE(e)
            intfa.addSpline(&e);
            break; }
        case 40: {
            DRW_Ray e;
            ENTRY_PARSE(e)
            intfa.addRay(e);
            break; }
        case 15:    // pline 2D
        case 16:    // pline 3D
        case 29: {  // pline PFACE
            DRW_Polyline e;
            ENTRY_PARSE(e)
            readPlineVertex(e, dbuf);
            intfa.addPolyline(e);
            break; }
//        case 30: {
//            DRW_Polyline e;// MESH (not pline)
//            ENTRY_PARSE(e)
//            intfa.addRay(e);
//            break; }
        case 41: {
            DRW_Xline e;
            ENTRY_PARSE(e)
            intfa.addXline(e);
            break; }
        case 101: {
            DRW_Image e;
            ENTRY_PARSE(e)
            intfa.addImage(&e);
            break; }

        default:
            //not supported or are object add to remaining map
            objObjectMap[obj.handle]= obj;
            break;
        }
        if (!ret){
            DRW_DBG("Warning: Entity type "); DRW_DBG(oType);DRW_DBG("has failed, handle: "); DRW_DBG(obj.handle); DRW_DBG("\n");
        }
        delete[]tmpByteStr;
    return ret;
}

bool dwgReader::readDwgObjects(DRW_Interface& intfa, dwgBuffer *dbuf){
    bool ret = true;
    bool ret2 = true;

    duint32 i=0;
    DRW_DBG("\nentities map total size= "); DRW_DBG(ObjectMap.size());
    DRW_DBG("\nobjects map total size= "); DRW_DBG(objObjectMap.size());
    std::map<duint32, objHandle>::iterator itB=objObjectMap.begin();
    std::map<duint32, objHandle>::iterator itE=objObjectMap.end();
    while (itB != itE){
        ret2 = readDwgObject(dbuf, itB->second, intfa);
        objObjectMap.erase(itB);
        itB=objObjectMap.begin();
        if (ret)
            ret = ret2;
    }
    if (DRW_DBGGL == DRW_dbg::DEBUG) {
        for (std::map<duint32, objHandle>::iterator it=remainingMap.begin(); it != remainingMap.end(); ++it){
            DRW_DBG("\nnum.# "); DRW_DBG(i++); DRW_DBG(" Remaining object Handle, loc, type= "); DRW_DBG(it->first);
            DRW_DBG(" "); DRW_DBG(it->second.loc); DRW_DBG(" "); DRW_DBG(it->second.type);
        }
        DRW_DBG("\n");
    }
    return ret;
}

/**
 * Reads a dwg drawing object (dwg object object) given its offset in the file
 */
bool dwgReader::readDwgObject(dwgBuffer *dbuf, objHandle& obj, DRW_Interface& intfa){
    bool ret = true;
    duint32 bs = 0;

        dbuf->setPosition(obj.loc);
        //verify if position is ok:
        if (!dbuf->isGood()){
            DRW_DBG(" Warning: readDwgObject, bad location\n");
            return false;
        }
        int size = dbuf->getModularShort();
        if (version > DRW::AC1021) {//2010+
            bs = dbuf->getUModularChar();
        }
        duint8 *tmpByteStr = new duint8[size]();
        dbuf->getBytes(tmpByteStr, size);
        //verify if getBytes is ok:
        if (!dbuf->isGood()){
            DRW_DBG(" Warning: readDwgObject, bad size\n");
            delete[]tmpByteStr;
            return false;
        }
        dwgBuffer buff(tmpByteStr, size, &decoder);
        //oType are set parsing entities
        dint16 oType = obj.type;

        switch (oType){
        case 102: {
            DRW_ImageDef e;
            ret = e.parseDwg(version, &buff, bs);
            intfa.linkImage(&e);
            break; }
        default:
            //not supported object or entity add to remaining map for debug
            remainingMap[obj.handle]= obj;
            break;
        }
        if (!ret){
            DRW_DBG("Warning: Object type "); DRW_DBG(oType);DRW_DBG("has failed, handle: "); DRW_DBG(obj.handle); DRW_DBG("\n");
        }
        delete[]tmpByteStr;
    return ret;
}



bool DRW_ObjControl::parseDwg(DRW::Version version, dwgBuffer *buf, duint32 bs){
int unkData=0;
    bool ret = DRW_TableEntry::parseDwg(version, buf, NULL, bs);
    DRW_DBG("\n***************************** parsing object control entry *********************************************\n");
    if (!ret)
        return ret;
    //last parsed is: XDic Missing Flag 2004+
    int numEntries = buf->getBitLong();
    DRW_DBG(" num entries: "); DRW_DBG(numEntries); DRW_DBG("\n");
    DRW_DBG("Remaining bytes: "); DRW_DBG(buf->numRemainingBytes()); DRW_DBG("\n");

//    if (oType == 68 && version== DRW::AC1015){//V2000 dimstyle seems have one unknown byte hard handle counter??
    if (oType == 68 && version > DRW::AC1014){//dimstyle seems have one unknown byte hard handle counter??
        unkData = buf->getRawChar8();
        DRW_DBG(" unknown v2000 byte: "); DRW_DBG( unkData); DRW_DBG("\n");
    }
    if (version > DRW::AC1018){//from v2007+ have a bit for strings follows (ObjControl do not have)
        int stringBit = buf->getBit();
        DRW_DBG(" string bit for  v2007+: "); DRW_DBG( stringBit); DRW_DBG("\n");
    }

    dwgHandle objectH = buf->getHandle();
    DRW_DBG(" NULL Handle: "); DRW_DBGHL(objectH.code, objectH.size, objectH.ref); DRW_DBG("\n");
    DRW_DBG("Remaining bytes: "); DRW_DBG(buf->numRemainingBytes()); DRW_DBG("\n");

//    if (oType == 56 && version== DRW::AC1015){//linetype in 2004 seems not have XDicObjH or NULL handle
    if (xDictFlag !=1){//linetype in 2004 seems not have XDicObjH or NULL handle
        dwgHandle XDicObjH = buf->getHandle();
        DRW_DBG(" XDicObj control Handle: "); DRW_DBGHL(XDicObjH.code, XDicObjH.size, XDicObjH.ref); DRW_DBG("\n");
        DRW_DBG("Remaining bytes: "); DRW_DBG(buf->numRemainingBytes()); DRW_DBG("\n");
    }
//add 2 for modelspace, paperspace blocks & bylayer, byblock linetypes
    numEntries = ((oType == 48) || (oType == 56)) ? (numEntries +2) : numEntries;

    for (int i =0; (i< numEntries) && buf->isGood(); i++){
        objectH = buf->getOffsetHandle(handle);
        if (objectH.ref != 0) //in vports R14  I found some NULL handles
            hadlesList.push_back (objectH.ref);
        DRW_DBG(" objectH Handle: "); DRW_DBGHL(objectH.code, objectH.size, objectH.ref); DRW_DBG("\n");
        DRW_DBG("Remaining bytes: "); DRW_DBG(buf->numRemainingBytes()); DRW_DBG("\n");
    }

    for (int i =0; (i< unkData) && buf->isGood(); i++){
        objectH = buf->getOffsetHandle(handle);
        DRW_DBG(" unknown Handle: "); DRW_DBGHL(objectH.code, objectH.size, objectH.ref); DRW_DBG("\n");
        DRW_DBG("Remaining bytes: "); DRW_DBG(buf->numRemainingBytes()); DRW_DBG("\n");
    }
    return buf->isGood();
}

