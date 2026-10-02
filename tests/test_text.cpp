/******************************************************************************
**  libDXFrw - Text Entity Tests                                            **
**                                                                           **
**  Copyright (C) 2025 libdxfrw contributors                                **
**                                                                           **
**  This library is free software, licensed under the terms of the GNU       **
**  General Public License as published by the Free Software Foundation,     **
**  either version 2 of the License, or (at your option) any later version.  **
**  You should have received a copy of the GNU General Public License        **
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.    **
******************************************************************************/

#include "libdxfrw.h"
#include "test_interface.h"
#include "intern/dwgbuffer.h"
#include "intern/drw_textcodec.h"
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

bool testSingleLineText() {
    std::cout << "\n=== Test: Single Line Text ===" << std::endl;

    const char* filename = "test_text.dxf";

    // Write text
    {
        dxfRW dxf(filename);
        class TextWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                DRW_Text text;
                text.basePoint.x = 10.0;
                text.basePoint.y = 20.0;
                text.basePoint.z = 0.0;
                text.text = "Hello, DXF World!";
                text.height = 5.0;
                text.angle = 0.0;
                dxfWriter->writeText(&text);
            }
            dxfRW* dxfWriter;
        };

        TextWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write text" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read text" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.textCount != 1) {
            std::cout << "✗ Expected 1 text entity, got " << reader.textCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Single line text test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testMultiLineText() {
    std::cout << "\n=== Test: Multi-Line Text (MText) ===" << std::endl;

    const char* filename = "test_mtext.dxf";

    // Write MText
    {
        dxfRW dxf(filename);
        class MTextWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                DRW_MText mtext;
                mtext.basePoint.x = 50.0;
                mtext.basePoint.y = 50.0;
                mtext.basePoint.z = 0.0;
                mtext.text = "Line 1\\PLine 2\\PLine 3";  // \\P is paragraph break
                mtext.height = 3.0;
                mtext.widthscale = 1.0;
                dxfWriter->writeMText(&mtext);
            }
            dxfRW* dxfWriter;
        };

        MTextWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write MText" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read MText" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.mtextCount != 1) {
            std::cout << "✗ Expected 1 MText entity, got " << reader.mtextCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Multi-line text test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testRotatedText() {
    std::cout << "\n=== Test: Rotated Text ===" << std::endl;

    const char* filename = "test_rotated_text.dxf";

    // Write rotated text
    {
        dxfRW dxf(filename);
        class RotatedTextWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                // Text at 0 degrees
                DRW_Text text1;
                text1.basePoint.x = 0.0;
                text1.basePoint.y = 0.0;
                text1.basePoint.z = 0.0;
                text1.text = "0 degrees";
                text1.height = 5.0;
                text1.angle = 0.0;
                dxfWriter->writeText(&text1);

                // Text at 45 degrees
                DRW_Text text2;
                text2.basePoint.x = 0.0;
                text2.basePoint.y = 20.0;
                text2.basePoint.z = 0.0;
                text2.text = "45 degrees";
                text2.height = 5.0;
                text2.angle = M_PI / 4.0;  // 45 degrees in radians
                dxfWriter->writeText(&text2);

                // Text at 90 degrees
                DRW_Text text3;
                text3.basePoint.x = 0.0;
                text3.basePoint.y = 40.0;
                text3.basePoint.z = 0.0;
                text3.text = "90 degrees";
                text3.height = 5.0;
                text3.angle = M_PI / 2.0;  // 90 degrees in radians
                dxfWriter->writeText(&text3);
            }
            dxfRW* dxfWriter;
        };

        RotatedTextWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write rotated text" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read rotated text" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.textCount != 3) {
            std::cout << "✗ Expected 3 text entities, got " << reader.textCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Rotated text test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testTextWithDifferentHeights() {
    std::cout << "\n=== Test: Text with Different Heights ===" << std::endl;

    const char* filename = "test_text_heights.dxf";

    // Write text with different heights
    {
        dxfRW dxf(filename);
        class TextHeightWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                for (int i = 1; i <= 5; i++) {
                    DRW_Text text;
                    text.basePoint.x = 0.0;
                    text.basePoint.y = i * 15.0;
                    text.basePoint.z = 0.0;
                    text.text = "Height " + std::to_string(i * 2);
                    text.height = i * 2.0;
                    text.angle = 0.0;
                    dxfWriter->writeText(&text);
                }
            }
            dxfRW* dxfWriter;
        };

        TextHeightWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write text with different heights" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read text with different heights" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.textCount != 5) {
            std::cout << "✗ Expected 5 text entities, got " << reader.textCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Text with different heights test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

// patch dxfrw_c: the rotation of an MTEXT read from a DWG must come from its X-axis direction.
// DRW_MText::parseDwg read the direction but never set haveXAxis, so every MTEXT from a DWG had
// rotation 0. The entity is built here bit by bit in the R2000 format (no DWG file needed).
namespace {

// Writes a DWG bit stream: bits MSB first, raw multi-byte values little-endian.
class DwgBitWriter {
public:
    std::vector<duint8> bytes;

    void bit(int v) {
        if (pos == 0) bytes.push_back(0);
        if (v) bytes.back() |= static_cast<duint8>(0x80 >> pos);
        pos = (pos + 1) & 7;
    }
    void bits(unsigned v, int n) { for (int i = n - 1; i >= 0; --i) bit((v >> i) & 1); }
    void rc(duint8 v) { bits(v, 8); }
    void rs(duint16 v) { rc(v & 0xFF); rc(v >> 8); }
    void rl(duint32 v) { rs(v & 0xFFFF); rs(v >> 16); }
    void rd(double d) {
        duint8 raw[8];
        std::memcpy(raw, &d, 8);
        for (int i = 0; i < 8; ++i) rc(raw[i]);
    }
    void bs(duint16 v) {                       // BS: 10 = 0, 01 = one byte, 00 = two bytes
        if (v == 0) bits(2, 2);
        else if (v < 256) { bits(1, 2); rc(static_cast<duint8>(v)); }
        else { bits(0, 2); rs(v); }
    }
    void bd(double d) {                        // BD: 10 = 0.0, 01 = 1.0, 00 = raw double
        if (d == 0.0) bits(2, 2);
        else if (d == 1.0) bits(1, 2);
        else { bits(0, 2); rd(d); }
    }
    void bd3(double x, double y, double z) { bd(x); bd(y); bd(z); }
    void handle(duint8 code, duint8 ref) {     // H: code|size, then the reference bytes
        if (ref == 0) { rc(static_cast<duint8>(code << 4)); return; }
        rc(static_cast<duint8>((code << 4) | 1));
        rc(ref);
    }
    void text(const std::string& s) {          // TV up to R2004: BS length + bytes
        bs(static_cast<duint16>(s.size()));
        for (char c : s) rc(static_cast<duint8>(c));
    }

private:
    int pos = 0;
};

class MTextProbe : public DRW_MText {
public:
    bool parse(DRW::Version v, dwgBuffer* buf) { return parseDwg(v, buf, 0); }
};

}  // namespace

bool testDwgMTextRotation() {
    std::cout << "\n=== Test: MTEXT rotation read from DWG ===" << std::endl;

    // a vertical MTEXT (X axis = 0,1,0) and one at 30 degrees
    const double dirs[2][2] = {{0.0, 1.0}, {std::cos(M_PI / 6), std::sin(M_PI / 6)}};
    const double expected[2] = {90.0, 30.0};

    for (int k = 0; k < 2; ++k) {
        DwgBitWriter w;
        // common entity data (R2000)
        w.bs(44);                   // object type MTEXT
        w.rl(0);                    // object size in bits (not used for R2000)
        w.handle(0, 0x2A);          // entity handle
        w.bs(0);                    // no extended data
        w.bit(0);                   // no proxy graphics
        w.bits(2, 2);               // entity mode: model space
        w.bs(0);                    // reactors
        w.bit(1);                   // no prev/next links
        w.bs(7);                    // color
        w.bd(1.0);                  // linetype scale
        w.bits(0, 2);               // linetype ByLayer
        w.bits(0, 2);               // plot style ByLayer
        w.bs(0);                    // visible
        w.rc(29);                   // lineweight
        // MTEXT data
        w.bd3(10.0, 20.0, 0.0);     // insertion point
        w.bd3(0.0, 0.0, 1.0);       // extrusion
        w.bd3(dirs[k][0], dirs[k][1], 0.0);  // X-axis direction
        w.bd(50.0);                 // reference rectangle width
        w.bd(2.5);                  // text height
        w.bs(7);                    // attachment: bottom left
        w.bs(1);                    // drawing direction
        w.bd(2.5);                  // extents height
        w.bd(10.0);                 // extents width
        w.text("733");
        w.bs(1);                    // line spacing style
        w.bd(1.0);                  // line spacing factor
        w.bit(0);                   // unknown bit
        // handles
        w.handle(3, 0);             // extension dictionary (none)
        w.handle(5, 0x10);          // layer
        w.handle(5, 0x11);          // text style
        for (int pad = 0; pad < 8; ++pad) w.rc(0);

        DRW_TextCodec codec;
        dwgBuffer buf(w.bytes.data(), static_cast<int>(w.bytes.size()), &codec);
        MTextProbe mtext;
        if (!mtext.parse(DRW::AC1015, &buf)) {
            std::cout << "✗ MTEXT bit stream not parsed" << std::endl;
            return false;
        }
        if (mtext.text != "733" || std::fabs(mtext.height - 2.5) > 1e-9) {
            std::cout << "✗ MTEXT fields read back wrong: '" << mtext.text << "', height " << mtext.height << std::endl;
            return false;
        }
        if (std::fabs(mtext.angle - expected[k]) > 1e-9) {
            std::cout << "✗ MTEXT rotation " << mtext.angle << ", expected " << expected[k] << std::endl;
            return false;
        }
    }

    std::cout << "✓ MTEXT rotation from DWG test passed" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "libdxfrw Text Entity Tests" << std::endl;
    std::cout << "==========================" << std::endl;

    int failedTests = 0;
    int totalTests = 0;

    totalTests++;
    if (!testSingleLineText()) failedTests++;

    totalTests++;
    if (!testMultiLineText()) failedTests++;

    totalTests++;
    if (!testRotatedText()) failedTests++;

    totalTests++;
    if (!testTextWithDifferentHeights()) failedTests++;

    totalTests++;
    if (!testDwgMTextRotation()) failedTests++;

    std::cout << "\n==========================" << std::endl;
    std::cout << "Tests: " << (totalTests - failedTests) << "/" << totalTests << " passed" << std::endl;

    if (failedTests > 0) {
        std::cout << "✗ " << failedTests << " test(s) failed" << std::endl;
        return 1;
    } else {
        std::cout << "✓ All text entity tests passed!" << std::endl;
        return 0;
    }
}
