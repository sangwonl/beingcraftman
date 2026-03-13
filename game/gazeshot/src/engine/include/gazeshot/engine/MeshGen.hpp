#pragma once

#include <cmath>
#include <gazeshot/core/math/Math.hpp>
#include <gazeshot/engine/Mesh.hpp>

namespace gazeshot::engine {

namespace MeshGen {

// ────────────────────────────────────────
// 평면 (Plane)
// ────────────────────────────────────────
// XZ 평면, Y=0, 법선은 +Y
//
//  (-w/2, 0, -d/2) ──── (w/2, 0, -d/2)
//       │                     │
//       │      subdivs...     │
//       │                     │
//  (-w/2, 0, d/2)  ──── (w/2, 0, d/2)

inline Mesh plane(core::f32 width, core::f32 depth, core::u32 subdivs = 1) {
  using namespace core;
  using namespace core::math;

  std::vector<Vertex> verts;
  std::vector<u32> idxs;

  u32 cols = subdivs + 1;
  u32 rows = subdivs + 1;
  verts.reserve(cols * rows);

  for (u32 r = 0; r < rows; ++r) {
    for (u32 c = 0; c < cols; ++c) {
      f32 u = static_cast<f32>(c) / subdivs;
      f32 v = static_cast<f32>(r) / subdivs;

      verts.push_back({
          .position = {(u - 0.5f) * width, 0.0f, (v - 0.5f) * depth},
          .normal = {0.0f, 1.0f, 0.0f},
          .texCoord = {u, v},
      });
    }
  }

  for (u32 r = 0; r < subdivs; ++r) {
    for (u32 c = 0; c < subdivs; ++c) {
      u32 tl = r * cols + c;
      u32 tr = tl + 1;
      u32 bl = tl + cols;
      u32 br = bl + 1;

      // CCW winding
      idxs.insert(idxs.end(), {tl, bl, tr, tr, bl, br});
    }
  }

  return Mesh{std::move(verts), std::move(idxs)};
}

// ────────────────────────────────────────
// 박스 (Box)
// ────────────────────────────────────────
// 6면 × 4정점 = 24정점 (법선이 면마다 다르므로 정점 공유 불가)
// 6면 × 2삼각형 × 3 = 36 인덱스
inline Mesh box(core::f32 w = 1, core::f32 h = 1, core::f32 d = 1) {
  using namespace core;
  using namespace core::math;

  f32 hw = w / 2, hh = h / 2, hd = d / 2;
  std::vector<Vertex> verts;
  std::vector<u32> idxs;
  verts.reserve(24);
  idxs.reserve(36);

  auto addFace = [&](Vec3f p0, Vec3f p1, Vec3f p2, Vec3f p3, Vec3f n) {
    u32 base = static_cast<u32>(verts.size());
    verts.push_back({p0, n, {0, 0}});
    verts.push_back({p1, n, {1, 0}});
    verts.push_back({p2, n, {1, 1}});
    verts.push_back({p3, n, {0, 1}});
    idxs.insert(
        idxs.end(), {base, base + 1, base + 2, base, base + 2, base + 3}
    );
  };

  // 앞 (+Z)
  addFace(
      {-hw, -hh, hd}, {hw, -hh, hd}, {hw, hh, hd}, {-hw, hh, hd}, {0, 0, 1}
  );
  // 뒤 (-Z)
  addFace(
      {hw, -hh, -hd}, {-hw, -hh, -hd}, {-hw, hh, -hd}, {hw, hh, -hd},
      {0, 0, -1}
  );
  // 우 (+X)
  addFace(
      {hw, -hh, hd}, {hw, -hh, -hd}, {hw, hh, -hd}, {hw, hh, hd}, {1, 0, 0}
  );
  // 좌 (-X)
  addFace(
      {-hw, -hh, -hd}, {-hw, -hh, hd}, {-hw, hh, hd}, {-hw, hh, -hd},
      {-1, 0, 0}
  );
  // 상 (+Y)
  addFace(
      {-hw, hh, hd}, {hw, hh, hd}, {hw, hh, -hd}, {-hw, hh, -hd}, {0, 1, 0}
  );
  // 하 (-Y)
  addFace(
      {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, -hh, hd}, {-hw, -hh, hd},
      {0, -1, 0}
  );

  return Mesh{std::move(verts), std::move(idxs)};
}

// ────────────────────────────────────────
// 구 (Sphere)
// ────────────────────────────────────────
// 구면 좌표계 (theta, phi) → 직교 좌표계 (x, y, z)
//
// theta: 0 ~ π (위에서 아래, 위도)
// phi:   0 ~ 2π (한 바퀴, 경도)
//
// x = r * sin(theta) * cos(phi)
// y = r * cos(theta)
// z = r * sin(theta) * sin(phi)
inline Mesh sphere(
    core::f32 radius, core::u32 segments = 32, core::u32 rings = 16
) {
  using namespace core;
  using namespace core::math;

  std::vector<Vertex> verts;
  std::vector<u32> idxs;
  verts.reserve((rings + 1) * (segments + 1));

  for (u32 ring = 0; ring <= rings; ++ring) {
    f32 theta = static_cast<f32>(ring) * PI / static_cast<f32>(rings);
    f32 sinT = std::sin(theta);
    f32 cosT = std::cos(theta);

    for (u32 seg = 0; seg <= segments; ++seg) {
      f32 phi =
          static_cast<f32>(seg) * 2.0f * PI / static_cast<f32>(segments);
      f32 sinP = std::sin(phi);
      f32 cosP = std::cos(phi);

      Vec3f normal{sinT * cosP, cosT, sinT * sinP};
      Vec3f position = normal * radius;
      Vec2f uv{
          static_cast<f32>(seg) / static_cast<f32>(segments),
          static_cast<f32>(ring) / static_cast<f32>(rings)
      };

      verts.push_back({position, normal, uv});
    }
  }

  // 인덱스: 쿼드를 두 개의 삼각형으로 (CCW)
  for (u32 ring = 0; ring < rings; ++ring) {
    for (u32 seg = 0; seg < segments; ++seg) {
      u32 cur = ring * (segments + 1) + seg;
      u32 next = cur + segments + 1;

      idxs.insert(idxs.end(), {cur, cur + 1, next});
      idxs.insert(idxs.end(), {cur + 1, next + 1, next});
    }
  }

  return Mesh{std::move(verts), std::move(idxs)};
}

// ────────────────────────────────────────
// 실린더 (Cylinder)
// ────────────────────────────────────────
// 옆면: 원의 파라메트릭 방정식으로 스트립 생성
// 상면/하면: 부채꼴
inline Mesh cylinder(
    core::f32 radius, core::f32 height, core::u32 segments = 32
) {
  using namespace core;
  using namespace core::math;

  std::vector<Vertex> verts;
  std::vector<u32> idxs;
  f32 hh = height / 2.0f;

  // ── 옆면 ──
  for (u32 i = 0; i <= segments; ++i) {
    f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
    f32 c = std::cos(angle), s = std::sin(angle);
    f32 u = static_cast<f32>(i) / static_cast<f32>(segments);

    Vec3f normal{c, 0, s};
    verts.push_back({{c * radius, hh, s * radius}, normal, {u, 1}});
    verts.push_back({{c * radius, -hh, s * radius}, normal, {u, 0}});
  }

  for (u32 i = 0; i < segments; ++i) {
    u32 top = i * 2;
    u32 bot = top + 1;
    idxs.insert(idxs.end(), {top, top + 2, bot});
    idxs.insert(idxs.end(), {bot, top + 2, bot + 2});
  }

  // ── 상면 (cap) ──
  u32 topCenter = static_cast<u32>(verts.size());
  verts.push_back({{0, hh, 0}, {0, 1, 0}, {0.5f, 0.5f}});

  for (u32 i = 0; i <= segments; ++i) {
    f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
    f32 c = std::cos(angle), s = std::sin(angle);
    verts.push_back(
        {{c * radius, hh, s * radius},
         {0, 1, 0},
         {c * 0.5f + 0.5f, s * 0.5f + 0.5f}}
    );
  }

  for (u32 i = 0; i < segments; ++i) {
    idxs.insert(
        idxs.end(), {topCenter, topCenter + i + 2, topCenter + i + 1}
    );
  }

  // ── 하면 (cap) ──
  u32 botCenter = static_cast<u32>(verts.size());
  verts.push_back({{0, -hh, 0}, {0, -1, 0}, {0.5f, 0.5f}});

  for (u32 i = 0; i <= segments; ++i) {
    f32 angle = static_cast<f32>(i) * 2.0f * PI / static_cast<f32>(segments);
    f32 c = std::cos(angle), s = std::sin(angle);
    verts.push_back(
        {{c * radius, -hh, s * radius},
         {0, -1, 0},
         {c * 0.5f + 0.5f, s * 0.5f + 0.5f}}
    );
  }

  for (u32 i = 0; i < segments; ++i) {
    idxs.insert(
        idxs.end(), {botCenter, botCenter + i + 1, botCenter + i + 2}
    );
  }

  return Mesh{std::move(verts), std::move(idxs)};
}

}  // namespace MeshGen

}  // namespace gazeshot::engine
