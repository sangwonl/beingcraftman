# Chapter 18: 텍스처와 머터리얼 고급

## 데모 미리보기

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│    ┌──────────┐   ┌──────────┐   ┌──────────┐       │
│    │▓▓▒▒░░▓▓▒▒│   │▓▓▒▒░░▓▓▒▒│   │▓▓▒▒░░▓▓▒▒│       │
│    │▒▒▓▓▒▒░░▓▓│   │▒▒▓▓▒▒░░▓▓│   │▒▒▓▓▒▒░░▓▓│       │
│    │░░▒▒▓▓▒▒░░│   │░░▒▒▓▓▒▒░░│   │░░▒▒▓▓▒▒░░│       │
│    │▓▓░░▒▒▓▓▒▒│   │▓▓░░▒▒▓▓▒▒│   │▓▓░░▒▒▓▓▒▒│       │
│    └──────────┘   └──────────┘   └──────────┘       │
│     Diffuse Only   + Specular    + Normal Map       │
│                      Map                             │
│                                                      │
│   [1] Diffuse  [2] Specular  [3] Normal  토글       │
│   평면 큐브에 Normal Map → 벽돌 요철이 보인다!        │
└──────────────────────────────────────────────────────┘
```

- **데모**: 동일한 low-poly 큐브에 텍스처 맵을 하나씩 추가하며 비교
- **인터랙션**: 1/2/3 키로 Diffuse/Specular/Normal Map 토글
- 블로그에 "텍스처 맵 단계별 적용 비교" 스크린샷 3장 포함 가능

---

## 학습 목표

1. Diffuse Map, Specular Map, Normal Map의 역할과 차이를 이해한다
2. Normal Mapping의 원리(Tangent Space, TBN 행렬)를 이해하고 구현한다
3. Ch.07의 Material 구조체를 텍스처 슬롯으로 확장한다
4. 텍스처 유닛 바인딩과 멀티 텍스처링을 구현한다
5. `enum class` 고급 활용, `std::bitset`을 실습한다

---

## 1. 배경 지식

### 텍스처 맵의 종류

```
┌─────────────────────────────────────────────────────────────┐
│  Diffuse Map        Specular Map       Normal Map          │
│  ┌────────────┐     ┌────────────┐     ┌────────────┐      │
│  │ RGB 색상    │     │ Grayscale  │     │ RGB = XYZ  │      │
│  │ 표면의 기본 │     │ 밝을수록    │     │ 법선 방향   │      │
│  │ 색상/패턴   │     │ 반사율 높음 │     │ 인코딩     │      │
│  └────────────┘     └────────────┘     └────────────┘      │
│                                                             │
│  용도: 벽돌 색상     용도: 금속 vs 나무   용도: 요철/디테일   │
│        나무결 무늬          광택 영역            균열/패턴    │
└─────────────────────────────────────────────────────────────┘
```

| 맵 종류 | 데이터 | 셰이더에서의 역할 |
|---------|--------|-----------------|
| Diffuse Map | RGB 색상 | `material.diffuse` 대신 텍스처에서 색상 샘플링 |
| Specular Map | Grayscale (또는 RGB) | 픽셀마다 반사 강도를 다르게 적용 |
| Normal Map | RGB (탄젠트 공간 법선) | 표면 법선을 픽셀마다 교란하여 요철 표현 |

### Normal Map과 Tangent Space

Normal Map은 왜 보라색(보라~파랑)으로 보일까?

```
Normal Map 색상 인코딩:
  R → X (좌우 기울기)     0.5 = 중립 → 127 (0~255)
  G → Y (상하 기울기)     0.5 = 중립 → 127
  B → Z (표면에서 나오는 방향)  1.0 = 수직 → 255

  평평한 표면의 법선 = (0, 0, 1)
  → 인코딩: (0.5, 0.5, 1.0) → RGB(127, 127, 255) = 보라/파랑색!
```

이 법선은 **Tangent Space**(접선 공간)에서 정의된다:

```
     N (표면 법선)
     │
     │    B (Bitangent)
     │   ╱
     │  ╱
     │ ╱
     └──────── T (Tangent)

  Tangent Space:
    T = 텍스처의 U 방향 (접선)
    B = 텍스처의 V 방향 (종접선)
    N = 표면 법선

  TBN 행렬 = [T | B | N]  (3x3)
  → Tangent Space 법선을 World Space로 변환
```

Tangent Space를 사용하는 이유:
- Normal Map 하나로 어떤 방향의 표면에도 적용 가능
- 표면이 회전해도 법선이 자동으로 올바르게 변환됨
- 텍스처 좌표에 자연스럽게 대응

---

## 2. 구현 가이드

### Step 1: TextureSlot과 확장된 Material

```hpp
// engine/include/gazeshot/engine/TextureSlot.hpp

#pragma once

#include <cstdint>

namespace gazeshot::engine {

// ── enum class 고급 활용 ──
// uint8_t 기반으로 메모리 절약, 배열 인덱스로 안전하게 사용
enum class TextureSlot : uint8_t {
    Diffuse  = 0,
    Specular = 1,
    Normal   = 2,
    Count             // 슬롯 개수로 사용 (= 3)
};

// enum class → 정수 변환 헬퍼
// static_cast를 매번 쓰지 않도록 편의 함수 제공
constexpr auto toIndex(TextureSlot slot) -> std::size_t {
    return static_cast<std::size_t>(slot);
}

// 슬롯 개수 상수
inline constexpr std::size_t kTextureSlotCount =
    static_cast<std::size_t>(TextureSlot::Count);

} // namespace gazeshot::engine
```

```hpp
// engine/include/gazeshot/engine/Material.hpp (확장)

#pragma once

#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/engine/TextureSlot.hpp>
#include <array>
#include <bitset>
#include <string>
#include <cstdint>

namespace gazeshot::engine {

// Ch.04의 Texture2D 인터페이스 전방 선언
class Texture2D;

struct Material {
    // ── 기존 Ch.07 속성 (텍스처 없을 때 폴백) ──
    core::math::Vec3f ambient{0.2f, 0.2f, 0.2f};
    core::math::Vec3f diffuse{0.7f, 0.3f, 0.2f};
    core::math::Vec3f specular{0.5f, 0.5f, 0.5f};
    core::f32 shininess = 32.0f;

    // ── 텍스처 슬롯 (이번 챕터에서 추가) ──
    std::array<Texture2D*, kTextureSlotCount> textures{};
    // nullptr이면 해당 슬롯 비활성 → 기존 색상값 사용

    // ── 활성 슬롯 추적 ──
    std::bitset<kTextureSlotCount> activeSlots;

    // ── 텍스처 설정 ──
    void setTexture(TextureSlot slot, Texture2D* tex) {
        textures[toIndex(slot)] = tex;
        activeSlots.set(toIndex(slot), tex != nullptr);
    }

    // ── 슬롯 활성 여부 확인 ──
    [[nodiscard]] bool hasTexture(TextureSlot slot) const {
        return activeSlots.test(toIndex(slot));
    }

    // ── 셰이더에 바인딩 ──
    void bindToShader(class ShaderProgram& shader) const;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `enum class` 고급 활용**

```cpp
// ── C-style enum vs enum class ──

// C-style enum: 위험!
enum OldSlot { DIFFUSE, SPECULAR, NORMAL, SLOT_COUNT };
int x = DIFFUSE;        // 암시적 int 변환 → 의도치 않은 연산 가능
if (DIFFUSE == 0) {}    // int와 비교 가능 → 타입 안전성 없음

// enum class: 안전!
enum class TextureSlot : uint8_t { Diffuse, Specular, Normal, Count };
// int x = TextureSlot::Diffuse;            // 컴파일 에러!
auto x = static_cast<int>(TextureSlot::Diffuse);  // 명시적 캐스트 필요

// 기저 타입 지정: uint8_t → 1바이트만 사용
// 기본값은 int (4바이트)이므로, 배열 인덱스용이면 uint8_t로 충분
static_assert(sizeof(TextureSlot) == 1);
```

**C++ 학습 포인트: `std::bitset`**

```cpp
#include <bitset>

// ── std::bitset: 고정 크기 비트 플래그 컨테이너 ──
// bool 배열보다 메모리 효율적, 비트 연산 편의 제공

std::bitset<kTextureSlotCount> activeSlots;  // 3비트 (실제로는 1바이트)

// 개별 비트 설정
activeSlots.set(toIndex(TextureSlot::Diffuse));   // Diffuse 활성
activeSlots.set(toIndex(TextureSlot::Normal));     // Normal 활성

// 확인
bool hasDiffuse = activeSlots.test(toIndex(TextureSlot::Diffuse));  // true
bool hasSpec    = activeSlots.test(toIndex(TextureSlot::Specular)); // false

// 전체 확인
bool anyActive = activeSlots.any();   // 하나라도 활성?
bool allActive = activeSlots.all();   // 전부 활성?
size_t count   = activeSlots.count(); // 활성 슬롯 수 (= 2)

// 비교: bool 배열 vs bitset
// bool flags[3];          → 3바이트, 비트 연산 불편
// std::bitset<3> flags;   → 1바이트, .set/.test/.any() 등 편의 메서드
```

### Step 2: Tangent 계산

메쉬 로딩 시 tangent/bitangent를 계산한다. Ch.17의 OBJ 로더에서 위치, 법선, UV가 이미 있으므로 삼각형 단위로 tangent를 구할 수 있다.

```cpp
// engine/src/TangentCalculator.cpp

#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec2.hpp>
#include <vector>

namespace gazeshot::engine {

struct Vertex {
    core::math::Vec3f position;
    core::math::Vec3f normal;
    core::math::Vec2f texCoord;
    core::math::Vec3f tangent;
    core::math::Vec3f bitangent;
};

void calculateTangents(std::vector<Vertex>& vertices,
                       const std::vector<uint32_t>& indices) {
    // ── 모든 tangent을 0으로 초기화 ──
    for (auto& v : vertices) {
        v.tangent = {0.0f, 0.0f, 0.0f};
        v.bitangent = {0.0f, 0.0f, 0.0f};
    }

    // ── 삼각형 단위로 tangent/bitangent 계산 ──
    for (std::size_t i = 0; i < indices.size(); i += 3) {
        auto& v0 = vertices[indices[i + 0]];
        auto& v1 = vertices[indices[i + 1]];
        auto& v2 = vertices[indices[i + 2]];

        // 엣지 벡터
        auto edge1 = v1.position - v0.position;
        auto edge2 = v2.position - v0.position;

        // UV 차이
        auto deltaUV1 = v1.texCoord - v0.texCoord;
        auto deltaUV2 = v2.texCoord - v0.texCoord;

        // ── TBN 행렬 유도 ──
        // edge1 = deltaUV1.x * T + deltaUV1.y * B
        // edge2 = deltaUV2.x * T + deltaUV2.y * B
        //
        // 행렬 형태:
        // [edge1]   [deltaUV1.x  deltaUV1.y] [T]
        // [edge2] = [deltaUV2.x  deltaUV2.y] [B]
        //
        // 역행렬을 곱하면 T, B를 구할 수 있다

        f32 det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(det) < 1e-8f) continue;  // 퇴화 삼각형 스킵

        f32 invDet = 1.0f / det;

        core::math::Vec3f tangent{
            invDet * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
            invDet * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
            invDet * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
        };

        core::math::Vec3f bitangent{
            invDet * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
            invDet * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
            invDet * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
        };

        // 삼각형을 공유하는 정점에 누적 (나중에 정규화)
        v0.tangent = v0.tangent + tangent;
        v1.tangent = v1.tangent + tangent;
        v2.tangent = v2.tangent + tangent;

        v0.bitangent = v0.bitangent + bitangent;
        v1.bitangent = v1.bitangent + bitangent;
        v2.bitangent = v2.bitangent + bitangent;
    }

    // ── Gram-Schmidt 직교화 + 정규화 ──
    for (auto& v : vertices) {
        // T를 N에 직교하게 만듦
        // T' = normalize(T - N * dot(N, T))
        auto& n = v.normal;
        auto& t = v.tangent;
        t = core::math::normalize(t - n * core::math::dot(n, t));

        // handedness 확인: cross(N, T)와 B의 방향이 같은지
        auto crossNT = core::math::cross(n, t);
        if (core::math::dot(crossNT, v.bitangent) < 0.0f) {
            t = t * -1.0f;  // 뒤집기
        }

        // bitangent는 셰이더에서 cross(N, T)로 재계산 가능
        // → 정점당 vec3 하나 절약 (tangent만 전달)
        v.bitangent = crossNT;
    }
}

} // namespace gazeshot::engine
```

### Step 3: 텍스처 로딩 (ResourceManager 연동)

Ch.16의 ResourceManager를 활용하여 텍스처를 캐싱한다.

```cpp
// engine/src/Material.cpp

#include <gazeshot/engine/Material.hpp>
#include <gazeshot/engine/ShaderProgram.hpp>
#include <gazeshot/engine/Texture2D.hpp>

namespace gazeshot::engine {

// 슬롯별 셰이더 uniform 이름과 텍스처 유닛 매핑
struct SlotBinding {
    const char* samplerUniform;  // 셰이더 sampler 이름
    const char* flagUniform;     // 활성 플래그 이름
    int textureUnit;             // GL_TEXTURE0 + unit
};

// 슬롯 바인딩 테이블 (컴파일 타임 상수)
static constexpr std::array<SlotBinding, kTextureSlotCount> kSlotBindings{{
    {"uDiffuseMap",  "uHasDiffuseMap",  0},  // TextureSlot::Diffuse
    {"uSpecularMap", "uHasSpecularMap", 1},  // TextureSlot::Specular
    {"uNormalMap",   "uHasNormalMap",   2},  // TextureSlot::Normal
}};

void Material::bindToShader(ShaderProgram& shader) const {
    // ── 기본 색상값 전달 (텍스처 없을 때 폴백) ──
    shader.setVec3("uMatAmbient", ambient);
    shader.setVec3("uMatDiffuse", diffuse);
    shader.setVec3("uMatSpecular", specular);
    shader.setFloat("uMatShininess", shininess);

    // ── 텍스처 슬롯별 바인딩 ──
    for (std::size_t i = 0; i < kTextureSlotCount; ++i) {
        const auto& binding = kSlotBindings[i];
        bool active = activeSlots.test(i) && textures[i] != nullptr;

        shader.setInt(binding.flagUniform, active ? 1 : 0);

        if (active) {
            textures[i]->bind(binding.textureUnit);
            shader.setInt(binding.samplerUniform, binding.textureUnit);
        }
    }
}

} // namespace gazeshot::engine
```

```cpp
// ResourceManager에서 텍스처 로딩 (Ch.16 확장)

Texture2D* ResourceManager::loadTexture(const std::string& path,
                                        bool flipY) {
    // 캐시 확인
    if (auto it = textureCache_.find(path); it != textureCache_.end()) {
        return it->second.get();
    }

    // stb_image로 로딩
    int width, height, channels;
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
    unsigned char* data = stbi_load(path.c_str(),
                                     &width, &height, &channels, 0);
    if (!data) {
        std::fprintf(stderr, "[ResourceManager] Failed to load: %s\n",
                     path.c_str());
        return nullptr;
    }

    // Texture2D 생성 (Ch.04 인터페이스)
    auto format = (channels == 4) ? TextureFormat::RGBA
                                  : TextureFormat::RGB;
    auto tex = std::make_unique<Texture2D>(width, height, format, data);
    stbi_image_free(data);

    auto* ptr = tex.get();
    textureCache_.emplace(path, std::move(tex));
    return ptr;
}
```

### Step 4: Normal Mapping 셰이더

```glsl
// shaders/normal_map.vert

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec2 vTexCoord;
out mat3 vTBN;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vTexCoord = aTexCoord;

    // ── TBN 행렬 구성 ──
    // Normal Matrix로 변환하여 월드 공간 TBN을 구한다
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(uNormalMatrix * aNormal);

    // Gram-Schmidt 재직교화 (보간으로 인한 오차 보정)
    T = normalize(T - dot(T, N) * N);

    // Bitangent는 cross product로 계산 (정점 데이터 절약)
    vec3 B = cross(N, T);

    vTBN = mat3(T, B, N);

    gl_Position = uProjection * uView * worldPos;
}
```

```glsl
// shaders/normal_map.frag

out vec4 FragColor;

in vec3 vWorldPos;
in vec2 vTexCoord;
in mat3 vTBN;

// ── 텍스처 샘플러 ──
uniform sampler2D uDiffuseMap;
uniform sampler2D uSpecularMap;
uniform sampler2D uNormalMap;

// ── 텍스처 활성 플래그 ──
uniform bool uHasDiffuseMap;
uniform bool uHasSpecularMap;
uniform bool uHasNormalMap;

// ── 폴백 색상 (텍스처 없을 때) ──
uniform vec3 uMatAmbient;
uniform vec3 uMatDiffuse;
uniform vec3 uMatSpecular;
uniform float uMatShininess;

// ── 광원 ──
uniform vec3 uLightDir;
uniform vec3 uLightAmbient;
uniform vec3 uLightDiffuse;
uniform vec3 uLightSpecular;

// ── 카메라 ──
uniform vec3 uViewPos;

void main() {
    // ── 1. Normal 결정 ──
    vec3 N;
    if (uHasNormalMap) {
        // Normal Map에서 [0,1] → [-1,1] 변환
        vec3 tangentNormal = texture(uNormalMap, vTexCoord).rgb;
        tangentNormal = tangentNormal * 2.0 - 1.0;

        // Tangent Space → World Space
        N = normalize(vTBN * tangentNormal);
    } else {
        N = normalize(vTBN[2]);  // TBN의 세 번째 열 = 표면 법선
    }

    // ── 2. Diffuse 색상 결정 ──
    vec3 albedo = uHasDiffuseMap
        ? texture(uDiffuseMap, vTexCoord).rgb
        : uMatDiffuse;

    // ── 3. Specular 강도 결정 ──
    vec3 specStrength = uHasSpecularMap
        ? texture(uSpecularMap, vTexCoord).rgb
        : uMatSpecular;

    // ── 4. Phong 라이팅 (Ch.07 복습) ──
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 R = reflect(-L, N);

    vec3 ambient  = uLightAmbient * uMatAmbient * albedo;
    float diff    = max(dot(N, L), 0.0);
    vec3 diffuse  = uLightDiffuse * albedo * diff;
    float spec    = pow(max(dot(R, V), 0.0), uMatShininess);
    vec3 specular = uLightSpecular * specStrength * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

### Step 5: 데모 코드

```cpp
// game/src/Ch18Demo.cpp

#include <gazeshot/engine/Material.hpp>
#include <gazeshot/engine/ResourceManager.hpp>
#include <gazeshot/engine/ShaderProgram.hpp>
#include <gazeshot/engine/TangentCalculator.hpp>

using namespace gazeshot;
using namespace gazeshot::engine;

struct Ch18App {
    std::unique_ptr<ShaderProgram> shader;
    Material brickMaterial;
    Material metalMaterial;
    Material woodMaterial;

    // 텍스처 토글 (데모용)
    bool showDiffuse  = true;
    bool showSpecular = true;
    bool showNormal   = true;

    DirectionalLight light;
};

void init(Ch18App& app, ResourceManager& resources) {
    app.shader = resources.loadShader("shaders/normal_map.vert",
                                       "shaders/normal_map.frag");

    // ── 벽돌 머터리얼 ──
    app.brickMaterial.ambient  = {0.1f, 0.1f, 0.1f};
    app.brickMaterial.diffuse  = {0.8f, 0.4f, 0.3f};
    app.brickMaterial.specular = {0.3f, 0.3f, 0.3f};
    app.brickMaterial.shininess = 16.0f;
    app.brickMaterial.setTexture(TextureSlot::Diffuse,
        resources.loadTexture("textures/brick_diffuse.png"));
    app.brickMaterial.setTexture(TextureSlot::Normal,
        resources.loadTexture("textures/brick_normal.png", false));
    // Normal Map은 flipY하지 않는 경우가 많음 (DirectX vs OpenGL 차이 주의)

    // ── 금속 머터리얼 ──
    app.metalMaterial.ambient  = {0.1f, 0.1f, 0.12f};
    app.metalMaterial.diffuse  = {0.5f, 0.5f, 0.55f};
    app.metalMaterial.specular = {1.0f, 1.0f, 1.0f};
    app.metalMaterial.shininess = 128.0f;
    app.metalMaterial.setTexture(TextureSlot::Diffuse,
        resources.loadTexture("textures/metal_diffuse.png"));
    app.metalMaterial.setTexture(TextureSlot::Specular,
        resources.loadTexture("textures/metal_specular.png"));
    app.metalMaterial.setTexture(TextureSlot::Normal,
        resources.loadTexture("textures/metal_normal.png", false));

    // ── 나무 머터리얼 (Specular Map 없음) ──
    app.woodMaterial.ambient  = {0.15f, 0.1f, 0.05f};
    app.woodMaterial.diffuse  = {0.6f, 0.4f, 0.2f};
    app.woodMaterial.specular = {0.1f, 0.1f, 0.1f};
    app.woodMaterial.shininess = 8.0f;
    app.woodMaterial.setTexture(TextureSlot::Diffuse,
        resources.loadTexture("textures/wood_diffuse.png"));
    // Specular Map 없음 → activeSlots에 Specular 비트 꺼짐
    // → 셰이더에서 uMatSpecular 폴백 사용
}

void update(Ch18App& app, f32 dt) {
    // 텍스처 토글
    if (app.input.isKeyPressed(SDLK_1)) app.showDiffuse  = !app.showDiffuse;
    if (app.input.isKeyPressed(SDLK_2)) app.showSpecular = !app.showSpecular;
    if (app.input.isKeyPressed(SDLK_3)) app.showNormal   = !app.showNormal;
}

void render(Ch18App& app, f32 alpha) {
    app.shader->bind();

    // 광원 설정
    app.shader->setVec3("uLightDir", normalize(app.light.direction));
    app.shader->setVec3("uLightAmbient", app.light.ambient);
    app.shader->setVec3("uLightDiffuse", app.light.diffuse);
    app.shader->setVec3("uLightSpecular", app.light.specular);
    app.shader->setVec3("uViewPos", app.camera.position());

    // ── 오브젝트별 렌더링 ──
    struct ObjectDef {
        Material* material;
        core::math::Vec3f position;
        Mesh* mesh;
    };

    ObjectDef objects[] = {
        {&app.brickMaterial, {-3.0f, 0, 0}, &app.cubeMesh},
        {&app.metalMaterial, { 0.0f, 0, 0}, &app.sphereMesh},
        {&app.woodMaterial,  { 3.0f, 0, 0}, &app.cubeMesh},
    };

    for (auto& obj : objects) {
        // 토글에 따라 임시로 슬롯 활성/비활성
        Material tempMat = *obj.material;
        if (!app.showDiffuse)  tempMat.activeSlots.reset(toIndex(TextureSlot::Diffuse));
        if (!app.showSpecular) tempMat.activeSlots.reset(toIndex(TextureSlot::Specular));
        if (!app.showNormal)   tempMat.activeSlots.reset(toIndex(TextureSlot::Normal));

        tempMat.bindToShader(*app.shader);

        auto model = translate(obj.position);
        app.shader->setMat4("uModel", model);
        app.shader->setMat4("uView", app.camera.viewMatrix());
        app.shader->setMat4("uProjection", app.camera.projectionMatrix());

        // Normal Matrix
        auto normalMat = transpose(inverse(model));
        float nm[9] = {
            normalMat[0][0], normalMat[0][1], normalMat[0][2],
            normalMat[1][0], normalMat[1][1], normalMat[1][2],
            normalMat[2][0], normalMat[2][1], normalMat[2][2],
        };
        glUniformMatrix3fv(
            app.shader->uniformLocation("uNormalMatrix"),
            1, GL_FALSE, nm);

        obj.mesh->draw(*app.renderer);
    }
}
```

### Step 6: Vertex Layout 확장

Ch.04의 Vertex Layout에 tangent 속성을 추가한다.

```cpp
// Vertex Layout 정의 (Ch.04 확장)
VertexLayout layout;
layout.push<f32>(3);  // location 0: position
layout.push<f32>(3);  // location 1: normal
layout.push<f32>(2);  // location 2: texCoord
layout.push<f32>(3);  // location 3: tangent  ← 새로 추가

// stride = (3 + 3 + 2 + 3) * sizeof(f32) = 44 bytes per vertex
```

---

## 3. C++ 학습 포인트 정리

### (1) `enum class`와 Scoped Enumerations

```cpp
// ── C-style enum의 문제 ──
enum Color { Red, Green, Blue };
enum TrafficLight { Red, Yellow, Green };
// 컴파일 에러! Red, Green 이름 충돌

// ── enum class의 해결 ──
enum class Color { Red, Green, Blue };
enum class TrafficLight { Red, Yellow, Green };
// OK! Color::Red와 TrafficLight::Red는 별개의 타입

// 기저 타입 지정
enum class TextureSlot : uint8_t { Diffuse, Specular, Normal, Count };
// → sizeof(TextureSlot) == 1
// → 배열 인덱스, 비트 플래그 등에서 메모리 효율적

// switch 문에서의 활용
void applySlot(TextureSlot slot) {
    switch (slot) {
        case TextureSlot::Diffuse:  /* ... */ break;
        case TextureSlot::Specular: /* ... */ break;
        case TextureSlot::Normal:   /* ... */ break;
        case TextureSlot::Count:    break;  // sentinel, 사용 안 함
        // -Wswitch 경고: 새 슬롯 추가 시 여기 빠뜨리면 경고!
    }
}
```

### (2) `std::bitset` 활용 패턴

```cpp
#include <bitset>

// ── 게임 엔진에서의 활용 예시 ──

// 1. 컴포넌트 플래그
enum class Component : uint8_t {
    Transform, Mesh, Material, Physics, Audio, Count
};
std::bitset<static_cast<size_t>(Component::Count)> components;
components.set(static_cast<size_t>(Component::Mesh));

// 2. 비트 연산
auto combined = flagsA & flagsB;  // AND
auto merged   = flagsA | flagsB;  // OR
auto toggled  = flagsA ^ flagsB;  // XOR
auto inverted = ~flagsA;          // NOT

// 3. 문자열 변환 (디버깅에 유용)
std::bitset<8> bits(0b10110010);
std::string str = bits.to_string();  // "10110010"
unsigned long val = bits.to_ulong(); // 178
```

### (3) `constexpr` 바인딩 테이블

```cpp
// 컴파일 타임에 결정되는 상수 테이블
// → 런타임 오버헤드 없음, 타입 안전

static constexpr std::array<SlotBinding, kTextureSlotCount> kSlotBindings{{
    {"uDiffuseMap",  "uHasDiffuseMap",  0},
    {"uSpecularMap", "uHasSpecularMap", 1},
    {"uNormalMap",   "uHasNormalMap",   2},
}};

// for 루프에서 인덱스와 바인딩 정보를 함께 사용
// → 새 슬롯 추가 시 이 테이블만 수정하면 됨
// → TextureSlot enum, kSlotBindings, 셰이더 세 곳을 동기화해야 함
//   (이를 자동화하는 것은 Ch.20 이후 리플렉션에서 다룸)
```

---

## 4. Normal Map이 Low-Poly 메쉬에 주는 효과

```
Low-Poly 큐브 (6면, 12삼각형):

Normal Map 없이:               Normal Map 적용:
┌──────────────┐               ┌──────────────┐
│              │               │▓▒░▓▒░▓▒░▓▒░▓│
│  밋밋한 면    │               │░▓▒░▓▒░▓▒░▓▒░│
│              │       →       │▒░▓벽돌 요철!▓▒│
│              │               │▓▒░▓▒░▓▒░▓▒░▓│
└──────────────┘               └──────────────┘

→ 폴리곤 수를 늘리지 않고 디테일 추가!
→ 실루엣은 여전히 평면 (한계점)
→ 실루엣이 중요한 경우 Displacement Map 필요 (고급 주제)
```

핵심 원리: Normal Map은 **픽셀 단위로 법선 방향을 교란**하여, 라이팅 계산 시 표면이 울퉁불퉁한 것처럼 보이게 한다. 실제 지오메트리는 변하지 않으므로 렌더링 비용이 거의 증가하지 않는다.

---

## 5. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Diffuse Map 적용 | 1 키 토글 → 텍스처 색상 ↔ 단색 전환 |
| Specular Map 적용 | 2 키 토글 → 금속 구의 반사 영역이 달라짐 |
| Normal Map 적용 | 3 키 토글 → 벽돌 큐브의 요철이 나타남/사라짐 |
| 빛 방향 반응 | 카메라 이동 시 Normal Map 요철의 음영 변화 |
| 폴백 동작 | 나무 큐브에 Specular Map 없음 → 단색 specular 사용 |
| TBN 정확성 | 법선이 올바른 방향 → 빛이 비출 때 요철이 자연스러움 |
| 텍스처 유닛 충돌 없음 | 3개 맵 동시 활성 시 깨짐 없음 |

---

## 6. 블로그 데모 아이디어

1. **텍스처 맵 단계별 적용**: Diffuse only → +Specular → +Normal 3분할 비교 스크린샷
2. **Normal Map ON/OFF GIF**: 동일 카메라 위치에서 토글하는 애니메이션
3. **Tangent Space 시각화**: TBN 벡터를 컬러 라인으로 표시 (디버그 렌더링)
4. **머터리얼 비교**: 벽돌(거친) vs 금속(광택) vs 나무(매트) 동시 렌더링
5. **Normal Map 없이 같은 디테일**: 고폴리 메쉬 vs 저폴리+Normal Map FPS 비교

---

## 다음 챕터 예고

**Chapter 19: 파티클 시스템**

총구 화염, 타겟 파편, 먼지 이펙트를 파티클로 구현한다.
데모: 사격 시 총구에서 화염 파티클, 피격 시 파편 파티클이 튀는 이펙트.
`std::span`, 오브젝트 풀링, instanced rendering을 실습한다.
