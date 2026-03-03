# Chapter 17: 모델 로딩

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│        ╱╲                    ╔══════╗        │
│       ╱  ╲  ← target.obj    ║ ╱──╲ ║       │
│      ╱ ◎  ╲                 ║│ ◎  │║       │
│     ╱──────╲                ║╲──╱ ║       │
│                              ╚══════╝       │
│    ┌──╮                                     │
│    │▓▓├──╮  ← gun.obj                       │
│    └──╯──╯                                  │
│                                             │
│  콘솔: "Loaded gun.obj: 1284 verts, 3840 idx"│
│  콘솔: "AABB: min(-0.3,-0.5,-0.2)"          │
└─────────────────────────────────────────────┘
```

- **데모**: OBJ에서 로딩한 총기/타겟 모델이 씬에 배치, 콘솔에 로딩 정보 출력
- 블로그에 "OBJ 파서를 직접 만드는 과정" 다이어그램 포함 가능

---

## 학습 목표

1. OBJ 파일 포맷의 구조를 이해하고 직접 파서를 구현한다
2. MTL(머터리얼) 파일을 파싱하여 모델에 기본 머터리얼을 적용한다
3. 중복 정점 제거(인덱싱)와 바운딩 볼륨 자동 계산을 구현한다
4. 좌표계 변환(Z-up → Y-up)을 처리한다
5. `std::from_chars`, `std::string_view` zero-copy 파싱, `std::ranges` 파이프라인을 실습한다

---

## 1. 배경 지식

### OBJ 파일 포맷

OBJ는 Wavefront Technologies가 만든 텍스트 기반 3D 모델 포맷이다.
바이너리 포맷(FBX, glTF)보다 느리지만, 파서를 직접 만들기 쉽다.

```
mtllib gun.mtl            ← 머터리얼 파일 참조
v -0.5 0.0  0.5           ← 정점 위치
vn 0.0 0.0 1.0            ← 법선
vt 0.0 0.0                ← 텍스처 좌표
usemtl gun_body           ← 머터리얼 전환
f 1/1/1 2/2/1 3/3/1       ← 면: 정점/텍스처/법선 인덱스
```

핵심: 인덱스는 **1-based** (파싱 시 -1), 면은 **N각형** 가능 (삼각화 필요), **음수 인덱스**는 뒤에서부터 참조.

### 정점 인덱싱과 GPU 효율

OBJ에서는 position/normal/texcoord가 별도 인덱스를 가진다.
GPU는 하나의 인덱스로 모든 속성을 참조하므로, 고유한 (pos, normal, uv) 조합마다 하나의 정점을 만들고 해시맵으로 중복을 제거한다.

### 좌표계 변환

Blender(Z-up) → OpenGL(Y-up): `(x, y, z) → (x, z, -y)`

---

## 2. 구현 가이드

### Step 1: 파싱 결과 데이터 구조

```hpp
// engine/include/gazeshot/engine/ModelData.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/Vertex.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace gazeshot::engine {

struct ObjMaterial {
    std::string name;
    core::math::Vec3f ambient  {0.1f, 0.1f, 0.1f};
    core::math::Vec3f diffuse  {0.8f, 0.8f, 0.8f};
    core::math::Vec3f specular {1.0f, 1.0f, 1.0f};
    core::f32 shininess = 32.0f;
    std::string diffuseMap;   // 텍스처 경로 (Ch.18에서 사용)
};

struct BoundingVolume {
    core::math::Vec3f aabbMin;
    core::math::Vec3f aabbMax;
    core::math::Vec3f center;
    core::f32 sphereRadius = 0.0f;
};

struct SubMesh {
    std::string materialName;
    core::u32 indexOffset = 0;
    core::u32 indexCount  = 0;
};

struct ModelData {
    std::vector<core::Vertex> vertices;
    std::vector<core::u32> indices;
    std::vector<SubMesh> subMeshes;
    std::unordered_map<std::string, ObjMaterial> materials;
    BoundingVolume bounds;
};

} // namespace gazeshot::engine
```

### Step 2: OBJ 파서 — 유틸리티

```hpp
// engine/include/gazeshot/engine/ObjLoader.hpp
#pragma once
#include <gazeshot/engine/ModelData.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <charconv>
#include <ranges>
#include <unordered_map>
#include <cstdio>
#include <filesystem>

namespace gazeshot::engine {

namespace detail {

// ── string_view → float (locale-independent) ──
[[nodiscard]] inline core::f32 svToFloat(std::string_view sv) {
    core::f32 val = 0.0f;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{}) {
        std::fprintf(stderr, "ObjLoader: failed to parse float '%.*s'\n",
            static_cast<int>(sv.size()), sv.data());
    }
    return val;
}

// ── string_view → int ──
[[nodiscard]] inline core::i32 svToInt(std::string_view sv) {
    core::i32 val = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    return val;
}

// ── 공백 토큰화 (zero-copy) ──
[[nodiscard]] inline std::vector<std::string_view> tokenize(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    while (start < line.size()) {
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            ++start;
        if (start >= line.size()) break;
        std::size_t end = start;
        while (end < line.size() && line[end] != ' ' && line[end] != '\t')
            ++end;
        tokens.push_back(line.substr(start, end - start));
        start = end;
    }
    return tokens;
}

// ── face 인덱스 파싱: "v/vt/vn", "v//vn", "v/vt", "v" ──
struct FaceIndex {
    core::i32 v = 0, vt = 0, vn = 0;  // 1-based, 0 = 없음
};

[[nodiscard]] inline FaceIndex parseFaceIndex(std::string_view token) {
    FaceIndex fi{};
    auto slash1 = token.find('/');
    if (slash1 == std::string_view::npos) { fi.v = svToInt(token); return fi; }

    fi.v = svToInt(token.substr(0, slash1));
    auto slash2 = token.find('/', slash1 + 1);
    if (slash2 == std::string_view::npos) {
        fi.vt = svToInt(token.substr(slash1 + 1));
        return fi;
    }
    auto vtStr = token.substr(slash1 + 1, slash2 - slash1 - 1);
    if (!vtStr.empty()) fi.vt = svToInt(vtStr);
    fi.vn = svToInt(token.substr(slash2 + 1));
    return fi;
}

} // namespace detail
```

**C++ 학습 포인트: `std::from_chars` — locale-free 고속 파싱**

`std::stof`와 달리 예외를 던지지 않고 에러 코드를 반환하며, 시스템 locale에 영향받지 않는다 (항상 `.`을 소수점으로 사용). `std::string` 할당 없이 `const char*` 범위만으로 동작하므로 2~5배 빠르다.
OBJ 파일은 수만 줄의 숫자를 파싱하므로 이 차이가 체감된다.

**C++ 학습 포인트: `std::string_view` — zero-copy 파싱**

`tokenize` 함수는 `string_view` 배열을 반환한다. 각 토큰은 원본 `line`의 메모리를 직접 참조하므로, 토큰 수만큼의 `std::string` 할당이 0이 된다.
단, **원본이 살아있는 동안만 유효** — dangling 주의.

### Step 3: MTL 파서

```hpp
// ObjLoader.hpp (계속)

[[nodiscard]] inline std::unordered_map<std::string, ObjMaterial>
loadMtl(const std::filesystem::path& mtlPath) {
    std::unordered_map<std::string, ObjMaterial> materials;
    std::ifstream file(mtlPath);
    if (!file.is_open()) return materials;

    ObjMaterial* current = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        auto tokens = detail::tokenize(line);
        if (tokens.empty()) continue;

        if (tokens[0] == "newmtl" && tokens.size() >= 2) {
            auto name = std::string(tokens[1]);
            materials[name] = ObjMaterial{.name = name};
            current = &materials[name];
        }
        else if (!current) { continue; }
        else if (tokens[0] == "Kd" && tokens.size() >= 4) {
            current->diffuse = {
                detail::svToFloat(tokens[1]),
                detail::svToFloat(tokens[2]),
                detail::svToFloat(tokens[3])
            };
        }
        else if (tokens[0] == "Ks" && tokens.size() >= 4) {
            current->specular = {
                detail::svToFloat(tokens[1]),
                detail::svToFloat(tokens[2]),
                detail::svToFloat(tokens[3])
            };
        }
        else if (tokens[0] == "Ns" && tokens.size() >= 2) {
            current->shininess = detail::svToFloat(tokens[1]);
        }
        else if (tokens[0] == "map_Kd" && tokens.size() >= 2) {
            current->diffuseMap = std::string(tokens[1]);
        }
    }
    return materials;
}
```

### Step 4: OBJ 파서 본체

```hpp
// ObjLoader.hpp (계속)

class ObjLoader {
public:
    enum class CoordSystem { YUp, ZUp };

    struct Options {
        CoordSystem sourceCoord = CoordSystem::YUp;
        bool generateNormals    = true;
        bool flipWindingOrder   = false;
    };

    [[nodiscard]] static ModelData load(const std::filesystem::path& objPath,
                                        const Options& opts = {}) {
        std::ifstream file(objPath);
        if (!file.is_open()) {
            std::fprintf(stderr, "ObjLoader: cannot open '%s'\n",
                objPath.string().c_str());
            return {};
        }

        std::vector<core::math::Vec3f> positions;
        std::vector<core::math::Vec3f> normals;
        std::vector<core::math::Vec2f> texcoords;

        struct RawFace { std::vector<detail::FaceIndex> indices; };
        std::vector<RawFace> faces;

        std::string currentMaterial = "__default";
        struct MaterialRange { std::string name; core::u32 faceStart = 0; };
        std::vector<MaterialRange> materialRanges;
        materialRanges.push_back({currentMaterial, 0});

        std::unordered_map<std::string, ObjMaterial> materials;
        auto baseDir = objPath.parent_path();

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            auto tokens = detail::tokenize(line);
            if (tokens.empty()) continue;
            auto cmd = tokens[0];

            if (cmd == "v" && tokens.size() >= 4) {
                core::math::Vec3f pos{
                    detail::svToFloat(tokens[1]),
                    detail::svToFloat(tokens[2]),
                    detail::svToFloat(tokens[3])
                };
                if (opts.sourceCoord == CoordSystem::ZUp)
                    pos = {pos.x, pos.z, -pos.y};
                positions.push_back(pos);
            }
            else if (cmd == "vn" && tokens.size() >= 4) {
                core::math::Vec3f n{
                    detail::svToFloat(tokens[1]),
                    detail::svToFloat(tokens[2]),
                    detail::svToFloat(tokens[3])
                };
                if (opts.sourceCoord == CoordSystem::ZUp)
                    n = {n.x, n.z, -n.y};
                normals.push_back(n);
            }
            else if (cmd == "vt" && tokens.size() >= 3) {
                texcoords.push_back({
                    detail::svToFloat(tokens[1]),
                    detail::svToFloat(tokens[2])
                });
            }
            else if (cmd == "f" && tokens.size() >= 4) {
                RawFace face;
                for (std::size_t i = 1; i < tokens.size(); ++i)
                    face.indices.push_back(detail::parseFaceIndex(tokens[i]));
                faces.push_back(std::move(face));
            }
            else if (cmd == "mtllib" && tokens.size() >= 2) {
                materials = loadMtl(baseDir / std::string(tokens[1]));
            }
            else if (cmd == "usemtl" && tokens.size() >= 2) {
                currentMaterial = std::string(tokens[1]);
                materialRanges.push_back({
                    currentMaterial, static_cast<core::u32>(faces.size())
                });
            }
        }

        return assemble(positions, normals, texcoords,
                        faces, materialRanges, materials, opts);
    }

private:
    struct VertexKey {
        core::i32 v, vt, vn;
        bool operator==(const VertexKey&) const = default;
    };
    struct VertexKeyHash {
        std::size_t operator()(const VertexKey& k) const {
            std::size_t h = 2166136261u;
            h ^= static_cast<std::size_t>(k.v);  h *= 16777619u;
            h ^= static_cast<std::size_t>(k.vt); h *= 16777619u;
            h ^= static_cast<std::size_t>(k.vn); h *= 16777619u;
            return h;
        }
    };

    [[nodiscard]] static ModelData assemble(
        const std::vector<core::math::Vec3f>& positions,
        const std::vector<core::math::Vec3f>& normals,
        const std::vector<core::math::Vec2f>& texcoords,
        const std::vector<RawFace>& faces,
        const std::vector<MaterialRange>& materialRanges,
        const std::unordered_map<std::string, ObjMaterial>& materials,
        const Options& opts)
    {
        ModelData result;
        result.materials = materials;

        std::unordered_map<VertexKey, core::u32, VertexKeyHash> vertexMap;
        vertexMap.reserve(positions.size());
        result.vertices.reserve(positions.size());

        std::size_t rangeIdx = 0;
        core::u32 subMeshStart = 0;
        std::string activeMat = materialRanges.empty()
            ? "__default" : materialRanges[0].name;

        auto flushSubMesh = [&]() {
            core::u32 count = static_cast<core::u32>(result.indices.size()) - subMeshStart;
            if (count > 0)
                result.subMeshes.push_back({activeMat, subMeshStart, count});
            subMeshStart = static_cast<core::u32>(result.indices.size());
        };

        for (std::size_t fi = 0; fi < faces.size(); ++fi) {
            // 머터리얼 전환
            if (rangeIdx + 1 < materialRanges.size()
                && fi >= materialRanges[rangeIdx + 1].faceStart) {
                flushSubMesh();
                ++rangeIdx;
                activeMat = materialRanges[rangeIdx].name;
            }

            const auto& face = faces[fi];

            // ── 삼각화 (fan): [0,1,2], [0,2,3], [0,3,4], ... ──
            for (std::size_t i = 2; i < face.indices.size(); ++i) {
                for (auto idx : {std::size_t(0), i - 1, i}) {
                    auto& fe = face.indices[idx];
                    core::i32 vi = fe.v > 0 ? fe.v - 1
                        : static_cast<core::i32>(positions.size()) + fe.v;
                    core::i32 ti = fe.vt > 0 ? fe.vt - 1
                        : (fe.vt < 0 ? static_cast<core::i32>(texcoords.size()) + fe.vt : -1);
                    core::i32 ni = fe.vn > 0 ? fe.vn - 1
                        : (fe.vn < 0 ? static_cast<core::i32>(normals.size()) + fe.vn : -1);

                    VertexKey key{vi, ti, ni};
                    auto [it, inserted] = vertexMap.try_emplace(
                        key, static_cast<core::u32>(result.vertices.size()));

                    if (inserted) {
                        core::Vertex vert{};
                        vert.position = positions[static_cast<std::size_t>(vi)];
                        if (ni >= 0) vert.normal = normals[static_cast<std::size_t>(ni)];
                        if (ti >= 0) vert.texCoord = texcoords[static_cast<std::size_t>(ti)];
                        result.vertices.push_back(vert);
                    }
                    result.indices.push_back(it->second);
                }
            }
        }
        flushSubMesh();

        if (normals.empty() && opts.generateNormals)
            generateFlatNormals(result);
        result.bounds = computeBounds(result.vertices);

        std::printf("ObjLoader: %zu verts, %zu indices, %zu submeshes\n",
            result.vertices.size(), result.indices.size(), result.subMeshes.size());
        return result;
    }

    static void generateFlatNormals(ModelData& data) {
        for (auto& v : data.vertices) v.normal = {0, 0, 0};
        for (std::size_t i = 0; i + 2 < data.indices.size(); i += 3) {
            auto& v0 = data.vertices[data.indices[i]];
            auto& v1 = data.vertices[data.indices[i + 1]];
            auto& v2 = data.vertices[data.indices[i + 2]];
            auto fn = core::math::cross(v1.position - v0.position,
                                        v2.position - v0.position);
            v0.normal = v0.normal + fn;
            v1.normal = v1.normal + fn;
            v2.normal = v2.normal + fn;
        }
        for (auto& v : data.vertices) v.normal = core::math::normalize(v.normal);
    }

    [[nodiscard]] static BoundingVolume computeBounds(
        const std::vector<core::Vertex>& vertices)
    {
        if (vertices.empty()) return {};
        BoundingVolume bv;
        bv.aabbMin = bv.aabbMax = vertices[0].position;
        for (const auto& v : vertices) {
            bv.aabbMin.x = std::min(bv.aabbMin.x, v.position.x);
            bv.aabbMin.y = std::min(bv.aabbMin.y, v.position.y);
            bv.aabbMin.z = std::min(bv.aabbMin.z, v.position.z);
            bv.aabbMax.x = std::max(bv.aabbMax.x, v.position.x);
            bv.aabbMax.y = std::max(bv.aabbMax.y, v.position.y);
            bv.aabbMax.z = std::max(bv.aabbMax.z, v.position.z);
        }
        bv.center = (bv.aabbMin + bv.aabbMax) * 0.5f;
        bv.sphereRadius = 0.0f;
        for (const auto& v : vertices)
            bv.sphereRadius = std::max(bv.sphereRadius,
                core::math::length(v.position - bv.center));
        return bv;
    }
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `try_emplace`로 중복 정점 제거**

`try_emplace`는 키가 이미 있으면 값을 생성하지 않는다 (불필요한 복사 없음).
`[it, inserted]` structured binding으로 삽입 여부를 한 번에 받아, 새 정점이면 추가하고 기존이면 인덱스만 재사용한다.

**C++ 학습 포인트: `std::ranges` 파이프라인 (C++20)**

수동 `tokenize` 함수를 ranges로 대체할 수 있다:

```cpp
auto toSv = [](auto&& rng) -> std::string_view {
    return {&*std::ranges::begin(rng),
            static_cast<std::size_t>(std::ranges::distance(rng))};
};
auto tokens = line
    | std::views::split(' ')
    | std::views::transform(toSv)
    | std::views::filter([](auto sv) { return !sv.empty(); });
```

선언적이고 지연 평가되지만, 컴파일 시간이 늘어난다. 익숙해지면 교체해 볼 수 있다.

### Step 5: ResourceManager 연동

```cpp
// Ch.16의 ResourceManager에 모델 로딩 추가
Mesh* ResourceManager::loadModel(const std::string& path,
                                  ObjLoader::Options opts) {
    if (auto it = meshCache_.find(path); it != meshCache_.end())
        return it->second.get();

    auto data = ObjLoader::load(path, opts);
    if (data.vertices.empty()) return nullptr;

    auto mesh = std::make_unique<Mesh>(
        std::move(data.vertices), std::move(data.indices));
    mesh->upload(renderer_);

    auto* ptr = mesh.get();
    meshCache_.emplace(path, std::move(mesh));
    boundsCache_.emplace(path, data.bounds);
    for (auto& [name, mat] : data.materials)
        materialCache_.emplace(name, mat);
    return ptr;
}
```

### Step 6: 데모 — 총기와 타겟 로딩

```cpp
// game/src/main.cpp (Ch.17)

void init(App& app) {
    auto& rm = app.resourceManager;

    // Blender에서 Z-up으로 내보낸 모델
    auto* gunMesh = rm.loadModel("assets/models/gun.obj", {
        .sourceCoord = ObjLoader::CoordSystem::ZUp
    });
    auto* targetMesh = rm.loadModel("assets/models/target.obj", {
        .sourceCoord = ObjLoader::CoordSystem::ZUp
    });

    // 총기 배치
    auto& gun = app.scene.createEntity("gun");
    gun.setMesh(gunMesh);
    gun.transform().position = {0.3f, -0.4f, -0.8f};
    gun.transform().scale    = {0.1f, 0.1f, 0.1f};

    // 타겟 3개 배치
    for (int i = 0; i < 3; ++i) {
        auto name = "target_" + std::to_string(i);
        auto& target = app.scene.createEntity(name);
        target.setMesh(targetMesh);
        target.transform().position = {
            -2.0f + static_cast<float>(i) * 2.0f, 1.0f, -15.0f
        };
    }

    // 바운딩 볼륨 디버그 출력
    if (auto* b = rm.getBounds("assets/models/gun.obj")) {
        std::printf("Gun AABB: (%.2f,%.2f,%.2f) ~ (%.2f,%.2f,%.2f)\n",
            b->aabbMin.x, b->aabbMin.y, b->aabbMin.z,
            b->aabbMax.x, b->aabbMax.y, b->aabbMax.z);
    }
}

// render()는 Ch.08과 동일 — scene.render() 한 줄로 모든 엔티티 렌더링
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| OBJ 로딩 | 콘솔에 정점/인덱스 수 출력됨 |
| 모델 표시 | 총기와 타겟이 올바른 형태로 렌더링 |
| 좌표계 변환 | Z-up 모델이 뒤집히지 않음 |
| 삼각화 | 4각형 이상의 면이 올바르게 삼각형으로 분할 |
| 인덱싱 | 중복 정점 제거되어 정점 수가 면 수×3보다 적음 |
| 바운딩 볼륨 | AABB min/max가 모델 크기에 맞게 출력 |
| MTL 파싱 | 머터리얼 색상이 모델에 적용 (Kd 기준) |
| 캐싱 | 같은 OBJ를 두 번 로딩해도 파싱은 1회 |
| 법선 자동 생성 | vn이 없는 OBJ도 조명이 정상 작동 |

---

## 블로그 데모 아이디어

1. **OBJ 파싱 과정 다이어그램**: 텍스트 → positions/normals/texcoords → 고유 정점 + 인덱스
2. **인덱싱 Before/After**: 중복 정점 수 비교 (예: 3840 → 1284)
3. **좌표계 변환 GIF**: Z-up vs Y-up 변환 결과 나란히
4. **바운딩 볼륨 시각화**: 모델 위에 AABB 와이어프레임 오버레이

---

## 다음 챕터 예고

**Chapter 18: 텍스처와 머터리얼 고급**

OBJ의 `map_Kd`로 참조된 텍스처를 로딩하고, PBR 기반 머터리얼 시스템을 구축한다.
데모: 디퓨즈 텍스처가 적용된 총기 모델 — 금속 질감과 나무 손잡이가 구분된다.
