/******************************************************************************
**  libDXFrw - Entity Tests                                                 **
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
#include <iostream>
#include <cstdio>
#include <cmath>
#include <new>

// Helper to compare doubles
bool doubleEquals(double a, double b, double epsilon = 0.0001) {
    return std::abs(a - b) < epsilon;
}

bool testArcEntity() {
    std::cout << "\n=== Test: Arc Entity ===" << std::endl;

    const char* filename = "test_arc.dxf";

    // Write arc
    {
        dxfRW dxf(filename);
        class ArcWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                DRW_Arc arc;
                arc.basePoint.x = 100.0;
                arc.basePoint.y = 100.0;
                arc.basePoint.z = 0.0;
                arc.radious = 50.0;
                arc.staangle = 0.0;          // 0 degrees
                arc.endangle = M_PI / 2.0;   // 90 degrees
                dxfWriter->writeArc(&arc);
            }
            dxfRW* dxfWriter;
        };

        ArcWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write arc" << std::endl;
            return false;
        }
    }

    // Read and verify arc
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read arc" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.arcCount != 1) {
            std::cout << "✗ Expected 1 arc, got " << reader.arcCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Arc entity test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testEllipseEntity() {
    std::cout << "\n=== Test: Ellipse Entity ===" << std::endl;

    const char* filename = "test_ellipse.dxf";

    // Write ellipse
    {
        dxfRW dxf(filename);
        class EllipseWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                DRW_Ellipse ellipse;
                ellipse.basePoint.x = 50.0;
                ellipse.basePoint.y = 50.0;
                ellipse.basePoint.z = 0.0;
                ellipse.secPoint.x = 30.0;  // Major axis endpoint
                ellipse.secPoint.y = 0.0;
                ellipse.secPoint.z = 0.0;
                ellipse.ratio = 0.5;        // Minor/major axis ratio
                ellipse.staparam = 0.0;
                ellipse.endparam = 2.0 * M_PI;
                dxfWriter->writeEllipse(&ellipse);
            }
            dxfRW* dxfWriter;
        };

        EllipseWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write ellipse" << std::endl;
            return false;
        }
    }

    // Read and verify ellipse
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read ellipse" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.ellipseCount != 1) {
            std::cout << "✗ Expected 1 ellipse, got " << reader.ellipseCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Ellipse entity test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testRayAndXlineEntities() {
    std::cout << "\n=== Test: Ray and XLine Entities ===" << std::endl;

    const char* filename = "test_ray_xline.dxf";

    // Write ray and xline
    {
        dxfRW dxf(filename);
        class RayXlineWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                // Write ray
                DRW_Ray ray;
                ray.basePoint.x = 10.0;
                ray.basePoint.y = 10.0;
                ray.basePoint.z = 0.0;
                ray.secPoint.x = 1.0;  // Direction vector
                ray.secPoint.y = 1.0;
                ray.secPoint.z = 0.0;
                dxfWriter->writeRay(&ray);

                // Write xline (infinite line)
                DRW_Xline xline;
                xline.basePoint.x = 20.0;
                xline.basePoint.y = 20.0;
                xline.basePoint.z = 0.0;
                xline.secPoint.x = 0.0;  // Direction vector
                xline.secPoint.y = 1.0;
                xline.secPoint.z = 0.0;
                dxfWriter->writeXline(&xline);
            }
            dxfRW* dxfWriter;
        };

        RayXlineWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write ray/xline" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read ray/xline" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.rayCount != 1 || reader.xlineCount != 1) {
            std::cout << "✗ Expected 1 ray and 1 xline, got " << reader.rayCount
                      << " rays and " << reader.xlineCount << " xlines" << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Ray and XLine entity test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testTraceAndSolidEntities() {
    std::cout << "\n=== Test: Trace and Solid Entities ===" << std::endl;

    const char* filename = "test_trace_solid.dxf";

    // Write trace and solid
    {
        dxfRW dxf(filename);
        class TraceSolidWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                // Write trace (filled triangle/quad)
                DRW_Trace trace;
                trace.basePoint.x = 0.0;
                trace.basePoint.y = 0.0;
                trace.basePoint.z = 0.0;
                trace.secPoint.x = 10.0;
                trace.secPoint.y = 0.0;
                trace.secPoint.z = 0.0;
                trace.thirdPoint.x = 10.0;
                trace.thirdPoint.y = 10.0;
                trace.thirdPoint.z = 0.0;
                trace.fourPoint.x = 0.0;
                trace.fourPoint.y = 10.0;
                trace.fourPoint.z = 0.0;
                dxfWriter->writeTrace(&trace);

                // Write solid
                DRW_Solid solid;
                solid.basePoint.x = 20.0;
                solid.basePoint.y = 0.0;
                solid.basePoint.z = 0.0;
                solid.secPoint.x = 30.0;
                solid.secPoint.y = 0.0;
                solid.secPoint.z = 0.0;
                solid.thirdPoint.x = 30.0;
                solid.thirdPoint.y = 10.0;
                solid.thirdPoint.z = 0.0;
                solid.fourPoint.x = 20.0;
                solid.fourPoint.y = 10.0;
                solid.fourPoint.z = 0.0;
                dxfWriter->writeSolid(&solid);
            }
            dxfRW* dxfWriter;
        };

        TraceSolidWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write trace/solid" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read trace/solid" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.traceCount != 1 || reader.solidCount != 1) {
            std::cout << "✗ Expected 1 trace and 1 solid, got " << reader.traceCount
                      << " traces and " << reader.solidCount << " solids" << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Trace and Solid entity test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool test3DFaceEntity() {
    std::cout << "\n=== Test: 3D Face Entity ===" << std::endl;

    const char* filename = "test_3dface.dxf";

    // Write 3D face
    {
        dxfRW dxf(filename);
        class Face3DWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                DRW_3Dface face;
                face.basePoint.x = 0.0;
                face.basePoint.y = 0.0;
                face.basePoint.z = 0.0;
                face.secPoint.x = 10.0;
                face.secPoint.y = 0.0;
                face.secPoint.z = 5.0;
                face.thirdPoint.x = 10.0;
                face.thirdPoint.y = 10.0;
                face.thirdPoint.z = 10.0;
                face.fourPoint.x = 0.0;
                face.fourPoint.y = 10.0;
                face.fourPoint.z = 5.0;
                dxfWriter->write3dface(&face);
            }
            dxfRW* dxfWriter;
        };

        Face3DWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write 3D face" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read 3D face" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.face3dCount != 1) {
            std::cout << "✗ Expected 1 3D face, got " << reader.face3dCount << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ 3D Face entity test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

bool testMultipleEntities() {
    std::cout << "\n=== Test: Multiple Entities ===" << std::endl;

    const char* filename = "test_multiple.dxf";

    // Write multiple entities
    {
        dxfRW dxf(filename);
        class MultiWriter : public TestInterface {
        public:
            virtual void writeEntities() {
                // Multiple points
                for (int i = 0; i < 5; i++) {
                    DRW_Point point;
                    point.basePoint.x = i * 10.0;
                    point.basePoint.y = i * 10.0;
                    point.basePoint.z = 0.0;
                    dxfWriter->writePoint(&point);
                }

                // Multiple lines
                for (int i = 0; i < 3; i++) {
                    DRW_Line line;
                    line.basePoint.x = i * 20.0;
                    line.basePoint.y = 0.0;
                    line.basePoint.z = 0.0;
                    line.secPoint.x = i * 20.0;
                    line.secPoint.y = 50.0;
                    line.secPoint.z = 0.0;
                    dxfWriter->writeLine(&line);
                }

                // Multiple circles
                for (int i = 0; i < 4; i++) {
                    DRW_Circle circle;
                    circle.basePoint.x = i * 30.0;
                    circle.basePoint.y = 100.0;
                    circle.basePoint.z = 0.0;
                    circle.radious = 10.0 + i * 5.0;
                    dxfWriter->writeCircle(&circle);
                }
            }
            dxfRW* dxfWriter;
        };

        MultiWriter writer;
        writer.dxfWriter = &dxf;
        if (!dxf.write(&writer, DRW::AC1015, false)) {
            std::cout << "✗ Failed to write multiple entities" << std::endl;
            return false;
        }
    }

    // Read and verify
    {
        dxfRW dxf(filename);
        TestInterface reader;
        if (!dxf.read(&reader, false)) {
            std::cout << "✗ Failed to read multiple entities" << std::endl;
            std::remove(filename);
            return false;
        }

        if (reader.pointCount != 5 || reader.lineCount != 3 || reader.circleCount != 4) {
            std::cout << "✗ Expected 5 points, 3 lines, 4 circles, got "
                      << reader.pointCount << " points, "
                      << reader.lineCount << " lines, "
                      << reader.circleCount << " circles" << std::endl;
            std::remove(filename);
            return false;
        }

        std::cout << "✓ Multiple entities test passed" << std::endl;
    }

    std::remove(filename);
    return true;
}

// Builds a T over memory filled with a non-zero byte pattern, so that a member the
// constructor leaves uninitialized keeps the pattern instead of a lucky zero.
template <class T>
class OverPattern {
public:
    OverPattern() : mem(fill()), obj(new (mem) T()) {}
    OverPattern(double x, double y, double z, double b) : mem(fill()), obj(new (mem) T(x, y, z, b)) {}
    ~OverPattern() { obj->~T(); ::operator delete(mem); }
    T* operator->() { return obj; }
private:
    // volatile: the compiler may treat plain stores made before a constructor as dead
    // (gcc -flifetime-dse) and remove them
    static void* fill() {
        void* m = ::operator new(sizeof(T));
        volatile unsigned char* p = static_cast<volatile unsigned char*>(m);
        for (size_t i = 0; i < sizeof(T); ++i) p[i] = 0xA5;
        return m;
    }
    OverPattern(const OverPattern&);
    OverPattern& operator=(const OverPattern&);
    void* mem;
    T* obj;
};

bool testDefaultValues() {
    std::cout << "\n=== Test: Defaults of Members Read from Optional Group Codes ===" << std::endl;

    // These members are only set by the reader when their group code is present. The
    // tangent direction (50) of a VERTEX is only written for curve-fit vertices, and only
    // read from DWG for 2D vertices, so most vertices used to get whatever was in memory.
    bool ok = true;
    struct Check {
        bool& ok;
        explicit Check(bool& o) : ok(o) {}
        void operator()(bool cond, const char* what) {
            if (!cond) { std::cout << "✗ " << what << std::endl; ok = false; }
        }
    } expect(ok);

    { OverPattern<DRW_Vertex> v; expect(v->tgdir == 0.0, "VERTEX tangent direction"); }
    {
        OverPattern<DRW_Vertex> v(1.0, 2.0, 0.0, 0.5);
        expect(v->tgdir == 0.0, "VERTEX(x, y, z, bulge) tangent direction");
        expect(v->eType == DRW::VERTEX, "VERTEX(x, y, z, bulge) type");
    }
    { OverPattern<DRW_Circle> c; expect(c->radious == 0.0, "CIRCLE radius"); }
    {
        OverPattern<DRW_Arc> a;
        expect(a->staangle == 0.0 && a->endangle == 0.0, "ARC angles");
    }
    {
        OverPattern<DRW_Ellipse> e;
        expect(e->ratio == 1.0 && e->staparam == 0.0 && e->endparam == M_PIx2, "ELLIPSE ratio and parameters (full ellipse)");
    }
    { OverPattern<DRW_Text> t; expect(t->height == 0.0, "TEXT height"); }
    { OverPattern<DRW_MText> t; expect(t->height == 0.0, "MTEXT height"); }
    { OverPattern<DRW_Attrib> t; expect(t->height == 0.0, "ATTRIB height"); }
    { OverPattern<DRW_LWPolyline> l; expect(l->vertexnum == 0, "LWPOLYLINE vertex count"); }
    { OverPattern<DRW_DimRadial> d; expect(d->getLeaderLength() == 0.0, "radial DIMENSION leader length"); }
    { OverPattern<DRW_DimDiametric> d; expect(d->getLeaderLength() == 0.0, "diametric DIMENSION leader length"); }
    { OverPattern<DRW_Layer> l; expect(l->handle == 0, "table entry handle"); }

    if (ok) std::cout << "✓ Default values test passed" << std::endl;
    return ok;
}

int main(int argc, char* argv[]) {
    std::cout << "libdxfrw Entity Tests" << std::endl;
    std::cout << "=====================" << std::endl;

    int failedTests = 0;
    int totalTests = 0;

    totalTests++;
    if (!testArcEntity()) failedTests++;

    totalTests++;
    if (!testEllipseEntity()) failedTests++;

    totalTests++;
    if (!testRayAndXlineEntities()) failedTests++;

    totalTests++;
    if (!testTraceAndSolidEntities()) failedTests++;

    totalTests++;
    if (!test3DFaceEntity()) failedTests++;

    totalTests++;
    if (!testMultipleEntities()) failedTests++;

    totalTests++;
    if (!testDefaultValues()) failedTests++;

    std::cout << "\n=====================" << std::endl;
    std::cout << "Tests: " << (totalTests - failedTests) << "/" << totalTests << " passed" << std::endl;

    if (failedTests > 0) {
        std::cout << "✗ " << failedTests << " test(s) failed" << std::endl;
        return 1;
    } else {
        std::cout << "✓ All entity tests passed!" << std::endl;
        return 0;
    }
}
