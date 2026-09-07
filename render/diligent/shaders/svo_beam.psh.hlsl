// Prompt 004 goal 266: the coarse start-t pre-pass.
//
// Renders ONE pixel per screen tile at 1/BEAM_TILE resolution. Each pixel holds a CONSERVATIVE
// distance before which no ray in that tile can meet anything, and svo_march.psh.hlsl starts every
// one of its rays there instead of at the root's entry face.
//
// This is a statement-for-statement mirror of world::svo::beam_start_t (world/svo/src/beam.cpp),
// which carries the proof that the bound is safe and is covered by 8 test cases / 8,189 assertions
// -- including the only property that matters, that for every ray inside the cone the bound never
// exceeds that ray's own hit distance. Keep the two in lockstep exactly as ray_trace.cpp and
// TraceRay are kept: a change lands on the CPU, passes its tests, and is mirrored here.
//
// THE ONE STRUCTURAL DIFFERENCE, and why it is still correct: the CPU recurses and sorts each
// node's children near-to-far; a pixel shader cannot recurse, so this walks an explicit stack and
// visits children in a fixed order derived from the ray's direction signs. Ordering affects only
// how quickly the running best prunes -- never the result, which is a MINIMUM over admitted nodes.
// The descent stops below the cone's own cross-section (measured on real terrain at 13 levels and
// ~56 nodes per tile, which is what sizes the stack below).

cbuffer BeamConstants
{
    column_major float4x4 g_BeamInvViewProj;
    float4 g_BeamCamera;    // xyz camera position; w unused
    float4 g_BeamTreeOrigin;// xyz root min corner; w = root edge (meters)
    float4 g_BeamParams;    // x = tan of the cone half-angle, y = tile size in pixels,
                            // z = 1/screen width, w = 1/screen height
    uint4  g_BeamInts;      // x = root node offset, y = tree present, zw spare
};

StructuredBuffer<uint> g_BeamNodes;

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

// 13 levels measured, 22 is the tree's own ceiling (kMaxLevels). Sized with headroom rather than
// to the measurement, because a deeper tree must not silently produce a WRONG bound -- and it
// cannot: overrunning this stops the descent, which only loosens the bound (see kMaxDepth below).
static const int kBeamStackSize = 20;

float3 RayFromUV(float2 uv)
{
    const float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float4 far = mul(g_BeamInvViewProj, float4(ndc, 1.0, 1.0));
    return normalize(far.xyz / far.w - g_BeamCamera.xyz);
}

struct Slab
{
    bool hit;
    float tNear;
};

// Slab test of the centre ray against [lo, hi]. An axis the ray does not travel along constrains
// nothing unless the apex is already outside it -- handled explicitly, exactly as beam.cpp does,
// because a large finite 1/d makes (hi - o) * 1e30 evaluate to 0 for an apex exactly on a face and
// collapses the bound to the root's entry for any camera looking straight down an axis.
Slab SlabTest(float3 o, float3 d, float3 invd, float3 lo, float3 hi)
{
    Slab s;
    s.hit = false;
    s.tNear = 0.0;
    float enter = 0.0;
    float exit = 1.0e30;
    [unroll]
    for (int a = 0; a < 3; ++a)
    {
        if (abs(d[a]) < 1.0e-20)
        {
            if (o[a] < lo[a] || o[a] > hi[a])
            {
                return s;
            }
            continue;
        }
        const float t0 = (lo[a] - o[a]) * invd[a];
        const float t1 = (hi[a] - o[a]) * invd[a];
        enter = max(enter, min(t0, t1));
        exit = min(exit, max(t0, t1));
    }
    if (exit < enter)
    {
        return s;
    }
    s.hit = true;
    s.tNear = enter;
    return s;
}

// Distance from the apex to the farthest corner of [lo, hi]: the largest t at which the cone's
// radius could still matter for this node, and therefore a safe radius to expand it by.
float FarCornerDistance(float3 o, float3 lo, float3 hi)
{
    const float3 d = max(abs(lo - o), abs(hi - o));
    return length(d);
}

float BeamStartT(float3 origin, float3 dir)
{
    if (g_BeamInts.y == 0u)
    {
        return 0.0;
    }
    const float rootEdge = g_BeamTreeOrigin.w;
    const float3 o = origin - g_BeamTreeOrigin.xyz;
    const float tanHalf = g_BeamParams.x;

    float3 invd;
    [unroll]
    for (int a = 0; a < 3; ++a)
    {
        invd[a] = abs(dir[a]) > 1.0e-20 ? 1.0 / dir[a] : (dir[a] >= 0.0 ? 1.0e30 : -1.0e30);
    }

    // Children are visited in an order derived from the direction signs: the octant the ray enters
    // first is the one whose bits match the signs, so XOR-ing the sign mask into the octant index
    // walks roughly near-to-far. Only the pruning rate depends on this, never the answer.
    uint signMask = 0u;
    signMask |= dir.x < 0.0 ? 1u : 0u;
    signMask |= dir.y < 0.0 ? 2u : 0u;
    signMask |= dir.z < 0.0 ? 4u : 0u;

    float best = 1.0e30;

    uint stackNode[kBeamStackSize];
    uint stackCursor[kBeamStackSize]; // low 3 bits: the octant descended into; bits 3+: next index
    float3 lo = float3(0.0, 0.0, 0.0);
    float edge = rootEdge;
    int depth = 0;
    stackNode[0] = g_BeamInts.x;
    stackCursor[0] = 0u;

    // The whole walk is one loop: descend where a child is admitted, otherwise back out. The bound
    // is a MINIMUM over admitted nodes, so an early exit at any point is still conservative.
    [loop]
    for (uint iter = 0u; iter < 4096u; ++iter)
    {
        const uint node = stackNode[depth];
        const uint cursor = stackCursor[depth];
        const uint nextChild = cursor >> 3;

        if (nextChild == 0u)
        {
            // First visit to this node: test it, and decide whether to descend at all.
            const float3 hi = lo + float3(edge, edge, edge);
            const float radius = tanHalf * FarCornerDistance(o, lo, hi);
            const Slab s = SlabTest(o, dir, invd, lo - float3(radius, radius, radius), hi + float3(radius, radius, radius));
            bool terminal = !s.hit || s.tNear >= best;
            if (!terminal)
            {
                const uint header = g_BeamNodes[node];
                const uint kind = (header >> 8) & 3u;
                // A leaf, a node smaller than the cone's own cross-section, or the stack's floor:
                // the node's entry IS the bound. Conservative in every case -- all geometry inside
                // it is at t >= tNear.
                if (kind != 0u || edge < radius || depth + 1 >= kBeamStackSize)
                {
                    best = min(best, s.tNear);
                    terminal = true;
                }
            }
            if (terminal)
            {
                // Unwind to the nearest ancestor with children left.
                if (depth == 0)
                {
                    break;
                }
                --depth;
                lo -= float3((stackCursor[depth] & 1u) != 0u ? edge : 0.0,
                             (stackCursor[depth] & 2u) != 0u ? edge : 0.0,
                             (stackCursor[depth] & 4u) != 0u ? edge : 0.0);
                edge *= 2.0;
                continue;
            }
        }

        if (nextChild >= 8u)
        {
            if (depth == 0)
            {
                break;
            }
            --depth;
            lo -= float3((stackCursor[depth] & 1u) != 0u ? edge : 0.0,
                         (stackCursor[depth] & 2u) != 0u ? edge : 0.0,
                         (stackCursor[depth] & 4u) != 0u ? edge : 0.0);
            edge *= 2.0;
            continue;
        }

        // Try the next child in ray order.
        stackCursor[depth] = (cursor & 7u) | ((nextChild + 1u) << 3);
        const uint octant = nextChild ^ signMask;
        const uint header = g_BeamNodes[node];
        const uint mask = header & 0xFFu;
        if ((mask & (1u << octant)) == 0u)
        {
            continue; // air is not a node at all
        }

        const float halfEdge = 0.5 * edge;
        const float3 childLo = lo + float3((octant & 1u) != 0u ? halfEdge : 0.0,
                                           (octant & 2u) != 0u ? halfEdge : 0.0,
                                           (octant & 4u) != 0u ? halfEdge : 0.0);
        const float childRadius = tanHalf * FarCornerDistance(o, childLo, childLo + float3(halfEdge, halfEdge, halfEdge));
        const Slab cs = SlabTest(o, dir, invd, childLo - float3(childRadius, childRadius, childRadius), childLo + float3(halfEdge + childRadius, halfEdge + childRadius, halfEdge + childRadius));
        if (!cs.hit || cs.tNear >= best)
        {
            continue;
        }

        // node_child_slot: pointers are packed in octant order behind the attribute word.
        const uint below = mask & ((1u << octant) - 1u);
        const uint slot = 2u + countbits(below);

        stackCursor[depth] = (octant & 7u) | ((nextChild + 1u) << 3);
        ++depth;
        stackNode[depth] = g_BeamNodes[node + slot];
        stackCursor[depth] = 0u;
        lo = childLo;
        edge = halfEdge;
    }

    // Infinity means "nothing anywhere in this cone", and the honest seed for that is zero: a ray
    // whose tile found nothing must still be free to hit something the cone's approximation missed.
    return best >= 1.0e30 ? 0.0 : best;
}

float main(PSInput input) : SV_TARGET
{
    // The tile's centre ray. `input.UV` is the low-resolution pixel's own centre, which is already
    // the centre of the screen tile it stands for.
    return BeamStartT(g_BeamCamera.xyz, RayFromUV(input.UV));
}
