/*
 * Benchmark for paint session sorting code. Code extracted from OpenRCT2 as of cf44ea7e2
 *
 * Generate data file with OpenRCT2 PR: https://github.com/OpenRCT2/OpenRCT2/issues/8442
 * E.g.
 *
 *     ./openrct2 screenshot ~/.config/OpenRCT2/save/dome-roof-on_zoom1.sv6 /dev/null giant 0 0 > out
 *
 * Save results as a file and remove any errors there may have been reported there (e.g. stuff about invalid/duplicate objects)
 * Make sure you have Google benchmark installed and available.
 * Compile this with... Warning, the output of the screenshot command may be large and require lots of RAM to compile
 *
 *     g++ paint_struct_benchmark.cpp -Wall -Wextra -Wno-missing-field-initializers -lbenchmark \
 *         -g -O2 -o paint_struct_bench -DSESSION_FILE=\"out\" -std=c++17 -O2
 *
 * You can limit amount of data provided to compilation when not doing real benchmark, but just playing around by providing
 * only a few paint sessions. The provided data (out.gz) contains values extracted from the "dome" park
 * (https://github.com/OpenRCT2/OpenRCT2/issues/4388) and out-min file contains the first three sessions of said file.
 *
 * Run the benchmark... Make sure you observe benchmark's warning regarding CPU scaling, if emitted
 *
 *     ./paint_struct_bench
 *
 * Observe the results:
 *
 *     2018-12-14 22:35:48
 *     Running ./bench-gcc
 *     Run on (8 X 4400 MHz CPU s)
 *     CPU Caches:
 *     L1 Data 32K (x4)
 *     L1 Instruction 32K (x4)
 *     L2 Unified 256K (x4)
 *     L3 Unified 8192K (x1)
 *     ----------------------------------------------------------------
 *     Benchmark                         Time           CPU Iterations
 *     ----------------------------------------------------------------
 *     BM_paint_session_arrange        945 ns        825 ns     872513
 *
 * Play with code, compiler and benchmark options.
 */

#include <benchmark/benchmark.h>
#include <cstdint>
#include <iterator>
#define MAX_PAINT_QUADRANTS 512
#define assert_struct_size(x, y) static_assert(sizeof(x) == (y), "Improper struct size")

#ifndef SESSION_FILE
#    define SESSION_FILE "out-min"
#endif

struct SurfaceElement;
struct PathElement;
struct TrackElement;
struct SmallSceneryElement;
struct LargeSceneryElement;
struct WallElement;
struct EntranceElement;
struct BannerElement;
struct CorruptElement;

struct TileElementBase
{
    uint8_t type;             // 0
    uint8_t flags;            // 1
    uint8_t base_height;      // 2
    uint8_t clearance_height; // 3

    uint8_t GetType() const;
    void SetType(uint8_t newType);
    uint8_t GetDirection() const;
    void SetDirection(uint8_t direction);
    uint8_t GetDirectionWithOffset(uint8_t offset) const;
    bool IsLastForTile() const;
    bool IsGhost() const;
    void Remove();
};

enum class TileElementType : uint8_t
{
    Surface = (0 << 2),
    Path = (1 << 2),
    Track = (2 << 2),
    SmallScenery = (3 << 2),
    Entrance = (4 << 2),
    Wall = (5 << 2),
    LargeScenery = (6 << 2),
    Banner = (7 << 2),
    Corrupt = (8 << 2),
};

/**
 * Map element structure
 * size: 0x08
 */
struct TileElement : public TileElementBase
{
    uint8_t pad_04[4];

    template<typename TType, TileElementType TClass> TType* as() const
    {
        return (TileElementType)GetType() == TClass ? (TType*)this : nullptr;
    }

public:
    SurfaceElement* AsSurface() const
    {
        return as<SurfaceElement, TileElementType::Surface>();
    }
    PathElement* AsPath() const
    {
        return as<PathElement, TileElementType::Path>();
    }
    TrackElement* AsTrack() const
    {
        return as<TrackElement, TileElementType::Track>();
    }
    SmallSceneryElement* AsSmallScenery() const
    {
        return as<SmallSceneryElement, TileElementType::SmallScenery>();
    }
    LargeSceneryElement* AsLargeScenery() const
    {
        return as<LargeSceneryElement, TileElementType::LargeScenery>();
    }
    WallElement* AsWall() const
    {
        return as<WallElement, TileElementType::Wall>();
    }
    EntranceElement* AsEntrance() const
    {
        return as<EntranceElement, TileElementType::Entrance>();
    }
    BannerElement* AsBanner() const
    {
        return as<BannerElement, TileElementType::Banner>();
    }
    CorruptElement* AsCorrupt() const
    {
        return as<CorruptElement, TileElementType::Corrupt>();
    }

    void ClearAs(uint8_t newType);
};
assert_struct_size(TileElement, 8);

#pragma pack(push, 1)
struct paint_struct_bound_box
{
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint16_t x_end;
    uint16_t y_end;
    uint16_t z_end;
};

struct attached_paint_struct
{
    uint32_t image_id; // 0x00
    union
    {
        uint32_t tertiary_colour;
        // If masked image_id is masked_id
        uint32_t colour_image_id;
    };
    uint16_t x;    // 0x08
    uint16_t y;    // 0x0A
    uint8_t flags; // 0x0C
    uint8_t pad_0D;
    attached_paint_struct* next; // 0x0E
};

/* size 0x34 */
struct paint_struct
{
    uint32_t image_id; // 0x00
    union
    {
        uint32_t tertiary_colour; // 0x04
        // If masked image_id is masked_id
        uint32_t colour_image_id; // 0x04
    };
    paint_struct_bound_box bounds; // 0x08
    uint16_t x;                    // 0x14
    uint16_t y;                    // 0x16
    uint16_t quadrant_index;
    uint8_t flags;
    uint8_t quadrant_flags;
    attached_paint_struct* attached_ps; // 0x1C
    paint_struct* children;
    paint_struct* next_quadrant_ps; // 0x24
    uint8_t sprite_type;            // 0x28
    uint8_t var_29;
    uint16_t pad_2A;
    uint16_t map_x;           // 0x2C
    uint16_t map_y;           // 0x2E
    TileElement* tileElement; // 0x30 (or sprite pointer)
};

using rct_string_id = uint16_t;

struct paint_string_struct
{
    rct_string_id string_id;   // 0x00
    paint_string_struct* next; // 0x02
    uint16_t x;                // 0x06
    uint16_t y;                // 0x08
    uint32_t args[4];          // 0x0A
    uint8_t* y_offsets;        // 0x1A
};
#pragma pack(pop)

union paint_entry
{
    paint_struct basic;
    attached_paint_struct attached;
    paint_string_struct string;
};
static_assert(sizeof(paint_entry) == sizeof(paint_struct), "Invalid size");

struct paint_session
{
    paint_entry PaintStructs[4000];
    paint_struct* Quadrants[MAX_PAINT_QUADRANTS]; // needed
    paint_struct PaintHead;                       // needed
    uint32_t QuadrantBackIndex;                   // needed
    uint32_t QuadrantFrontIndex;                  // needed
};

enum PAINT_QUADRANT_FLAGS
{
    PAINT_QUADRANT_FLAG_IDENTICAL = (1 << 0),
    PAINT_QUADRANT_FLAG_BIGGER = (1 << 7),
    PAINT_QUADRANT_FLAG_NEXT = (1 << 1),
};

uint8_t gCurrentRotation;

uint8_t get_current_rotation()
{
    return gCurrentRotation & 3;
}

template<uint8_t> static bool check_bounding_box(const paint_struct_bound_box&, const paint_struct_bound_box&)
{
    return false;
}

template<> bool check_bounding_box<0>(const paint_struct_bound_box& initialBBox, const paint_struct_bound_box& currentBBox)
{
    if (initialBBox.z_end >= currentBBox.z && initialBBox.y_end >= currentBBox.y && initialBBox.x_end >= currentBBox.x
        && !(initialBBox.z < currentBBox.z_end && initialBBox.y < currentBBox.y_end && initialBBox.x < currentBBox.x_end))
    {
        return true;
    }
    return false;
}

template<> bool check_bounding_box<1>(const paint_struct_bound_box& initialBBox, const paint_struct_bound_box& currentBBox)
{
    if (initialBBox.z_end >= currentBBox.z && initialBBox.y_end >= currentBBox.y && initialBBox.x_end < currentBBox.x
        && !(initialBBox.z < currentBBox.z_end && initialBBox.y < currentBBox.y_end && initialBBox.x >= currentBBox.x_end))
    {
        return true;
    }
    return false;
}

template<> bool check_bounding_box<2>(const paint_struct_bound_box& initialBBox, const paint_struct_bound_box& currentBBox)
{
    if (initialBBox.z_end >= currentBBox.z && initialBBox.y_end < currentBBox.y && initialBBox.x_end < currentBBox.x
        && !(initialBBox.z < currentBBox.z_end && initialBBox.y >= currentBBox.y_end && initialBBox.x >= currentBBox.x_end))
    {
        return true;
    }
    return false;
}

template<> bool check_bounding_box<3>(const paint_struct_bound_box& initialBBox, const paint_struct_bound_box& currentBBox)
{
    if (initialBBox.z_end >= currentBBox.z && initialBBox.y_end < currentBBox.y && initialBBox.x_end >= currentBBox.x
        && !(initialBBox.z < currentBBox.z_end && initialBBox.y >= currentBBox.y_end && initialBBox.x < currentBBox.x_end))
    {
        return true;
    }
    return false;
}

static_assert(sizeof(paint_struct) == 0x44);
template<uint8_t _TRotation>
static paint_struct* paint_arrange_structs_helper_rotation(paint_struct* ps_next, uint16_t quadrantIndex, uint8_t flag)
{
    paint_struct* ps;
    paint_struct* ps_temp;
    do
    {
        ps = ps_next;
        ps_next = ps_next->next_quadrant_ps;
        if (ps_next == nullptr)
            return ps;
    } while (quadrantIndex > ps_next->quadrant_index);

    // Cache the last visited node so we don't have to walk the whole list again
    paint_struct* ps_cache = ps;

    ps_temp = ps;
    do
    {
        ps = ps->next_quadrant_ps;
        if (ps == nullptr)
            break;

        if (ps->quadrant_index > quadrantIndex + 1)
        {
            ps->quadrant_flags = PAINT_QUADRANT_FLAG_BIGGER;
        }
        else if (ps->quadrant_index == quadrantIndex + 1)
        {
            ps->quadrant_flags = PAINT_QUADRANT_FLAG_NEXT | PAINT_QUADRANT_FLAG_IDENTICAL;
        }
        else if (ps->quadrant_index == quadrantIndex)
        {
            ps->quadrant_flags = flag | PAINT_QUADRANT_FLAG_IDENTICAL;
        }
    } while (ps->quadrant_index <= quadrantIndex + 1);
    ps = ps_temp;

    while (true)
    {
        while (true)
        {
            ps_next = ps->next_quadrant_ps;
            if (ps_next == nullptr)
                return ps_cache;
            if (ps_next->quadrant_flags & PAINT_QUADRANT_FLAG_BIGGER)
                return ps_cache;
            if (ps_next->quadrant_flags & PAINT_QUADRANT_FLAG_IDENTICAL)
                break;
            ps = ps_next;
        }

        ps_next->quadrant_flags &= ~PAINT_QUADRANT_FLAG_IDENTICAL;
        ps_temp = ps;

        const paint_struct_bound_box& initialBBox = ps_next->bounds;

        while (true)
        {
            ps = ps_next;
            ps_next = ps_next->next_quadrant_ps;
            if (ps_next == nullptr)
                break;
            if (ps_next->quadrant_flags & PAINT_QUADRANT_FLAG_BIGGER)
                break;
            if (!(ps_next->quadrant_flags & PAINT_QUADRANT_FLAG_NEXT))
                continue;

            const paint_struct_bound_box& currentBBox = ps_next->bounds;

            const bool compareResult = check_bounding_box<_TRotation>(initialBBox, currentBBox);

            if (compareResult)
            {
                ps->next_quadrant_ps = ps_next->next_quadrant_ps;
                paint_struct* ps_temp2 = ps_temp->next_quadrant_ps;
                ps_temp->next_quadrant_ps = ps_next;
                ps_next->next_quadrant_ps = ps_temp2;
                ps_next = ps;
            }
        }

        ps = ps_temp;
    }
}

paint_struct* paint_arrange_structs_helper(paint_struct* ps_next, uint16_t quadrantIndex, uint8_t flag, uint8_t rotation)
{
    switch (rotation)
    {
        case 0:
            return paint_arrange_structs_helper_rotation<0>(ps_next, quadrantIndex, flag);
        case 1:
            return paint_arrange_structs_helper_rotation<1>(ps_next, quadrantIndex, flag);
        case 2:
            return paint_arrange_structs_helper_rotation<2>(ps_next, quadrantIndex, flag);
        case 3:
            return paint_arrange_structs_helper_rotation<3>(ps_next, quadrantIndex, flag);
    }
    return nullptr;
}

void paint_session_arrange(paint_session* session)
{
    paint_struct* psHead = &session->PaintHead;

    paint_struct* ps = psHead;
    ps->next_quadrant_ps = nullptr;

    uint32_t quadrantIndex = session->QuadrantBackIndex;
    const uint8_t rotation = get_current_rotation();
    if (quadrantIndex != UINT32_MAX)
    {
        do
        {
            paint_struct* ps_next = session->Quadrants[quadrantIndex];
            if (ps_next != nullptr)
            {
                ps->next_quadrant_ps = ps_next;
                do
                {
                    ps = ps_next;
                    ps_next = ps_next->next_quadrant_ps;
                } while (ps_next != nullptr);
            }
        } while (++quadrantIndex <= session->QuadrantFrontIndex);

        paint_struct* ps_cache = paint_arrange_structs_helper(
            psHead, session->QuadrantBackIndex & 0xFFFF, PAINT_QUADRANT_FLAG_NEXT, rotation);

        quadrantIndex = session->QuadrantBackIndex;
        while (++quadrantIndex < session->QuadrantFrontIndex)
        {
            ps_cache = paint_arrange_structs_helper(ps_cache, quadrantIndex & 0xFFFF, 0, rotation);
        }
    }
}
paint_session s[] = {
#include SESSION_FILE
};

static void fixup_pointers(paint_session* s, size_t paint_session_entries, size_t paint_struct_entries, size_t quadrant_entries)
{
    for (size_t i = 0; i < paint_session_entries; i++)
    {
        for (size_t j = 0; j < paint_struct_entries; j++)
        {
            if (s[i].PaintStructs[j].basic.next_quadrant_ps == (paint_struct*)paint_struct_entries)
            {
                s[i].PaintStructs[j].basic.next_quadrant_ps = nullptr;
            }
            else
            {
                s[i].PaintStructs[j].basic.next_quadrant_ps = &s[i].PaintStructs
                                                                   [(uintptr_t)s[i].PaintStructs[j].basic.next_quadrant_ps]
                                                                       .basic;
            }
        }
        for (size_t j = 0; j < quadrant_entries; j++)
        {
            if (s[i].Quadrants[j] == (paint_struct*)quadrant_entries)
            {
                s[i].Quadrants[j] = nullptr;
            }
            else
            {
                s[i].Quadrants[j] = &s[i].PaintStructs[(size_t)s[i].Quadrants[j]].basic;
            }
        }
    }
}

#if 0
int main()
{
    fixup_pointers(s, std::size(s), std::size(s->PaintStructs), std::size(s->Quadrants));
    paint_session_arrange(s);
    return 0;
}
#endif

#if 1
static void BM_paint_session_arrange(benchmark::State& state)
{
    std::set<int> data;
    for (auto _ : state)
    {
        state.PauseTiming();
        paint_session* local_s = new paint_session[std::size(s)];
        std::copy_n(s, std::size(s), local_s);
        fixup_pointers(local_s, std::size(s), std::size(s->PaintStructs), std::size(s->Quadrants));
        state.ResumeTiming();
        paint_session_arrange(local_s);
        state.PauseTiming();
        delete[] local_s;
        state.ResumeTiming();
    }
}
BENCHMARK(BM_paint_session_arrange);

BENCHMARK_MAIN();
#endif
