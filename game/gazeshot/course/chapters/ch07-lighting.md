# Chapter 07: 라이팅 기초

## 데모 미리보기

```
┌─────────────────────────────────────────────┐
│              ☀ Directional Light            │
│             ╲                               │
│              ╲                              │
│   ■          ●          ▌         ▬         │
│  밝은 면   스펙큘러    그림자 면   바닥 반사   │
│  + 어두운 면  하이라이트  + 앰비언트           │
│                                             │
│  [A]mbient  [D]iffuse  [S]pecular 토글      │
└─────────────────────────────────────────────┘
```

- **데모**: Ch.06의 4가지 도형에 Phong 라이팅 적용
- **인터랙션**: A/D/S 키로 ambient/diffuse/specular 개별 토글
- 블로그에 "Phong 모델의 3가지 성분 비교" 스크린샷 3장 포함 가능

---

## 학습 목표

1. Phong 라이팅 모델의 세 가지 성분을 이해하고 셰이더로 구현한다
2. Directional Light를 구현한다
3. 법선 행렬(Normal Matrix)의 필요성을 이해한다
4. Material 구조체를 설계한다
5. `std::array`, `alignas`, GPU 메모리 레이아웃을 실습한다

---

## 1. 배경 지식

### Phong 라이팅 모델

```
최종 색상 = Ambient + Diffuse + Specular

Ambient  = material.ambient × light.ambient
           → 전역 간접광 근사 (어두운 면도 약간 보이게)

Diffuse  = material.diffuse × light.diffuse × max(N·L, 0)
           → Lambert의 코사인 법칙
           → N: 표면 법선, L: 빛 방향 (표면→광원)

Specular = material.specular × light.specular × pow(max(R·V, 0), shininess)
           → R: 반사 벡터 = reflect(-L, N)
           → V: 시선 방향 (표면→카메라)
           → shininess: 높을수록 반사가 작고 날카로움
```

시각화:
```
         N (법선)
         │
    L    │    R (반사)
     ╲   │   ╱
      ╲  │  ╱
       ╲ │ ╱
────────────────── 표면
              V (시선)

N·L > 0 → 빛을 받는 면 (밝음)
N·L ≤ 0 → 빛을 받지 않는 면 (어두움, ambient만)
R·V ≈ 1 → 반사광이 눈에 직접 들어옴 (specular highlight)
```

### 법선 행렬 (Normal Matrix)

모델 행렬에 비균등 스케일이 있으면 법선이 왜곡된다:

```
scale(2, 1, 1) 적용 시:
표면은 X방향으로 늘어남 → 법선은 X방향으로 줄어들어야 함

Normal Matrix = transpose(inverse(model))의 좌상단 3x3
```

균등 스케일만 쓰면 Normal Matrix = Model Matrix로 충분하지만,
올바른 습관을 위해 항상 Normal Matrix를 전달한다.

---

## 2. 구현 가이드

### Step 1: Light와 Material 구조체

```hpp
// engine/include/gazeshot/engine/Light.hpp

#pragma once

#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::engine {

struct DirectionalLight {
    core::math::Vec3f direction{-0.3f, -1.0f, -0.5f};  // 빛이 향하는 방향
    core::math::Vec3f ambient{0.15f, 0.15f, 0.15f};
    core::math::Vec3f diffuse{0.8f, 0.8f, 0.75f};
    core::math::Vec3f specular{1.0f, 1.0f, 1.0f};
};

} // namespace gazeshot::engine
```

```hpp
// engine/include/gazeshot/engine/Material.hpp

#pragma once

#include <gazeshot/core/math/Vec3.hpp>

namespace gazeshot::engine {

struct Material {
    core::math::Vec3f ambient{0.2f, 0.2f, 0.2f};
    core::math::Vec3f diffuse{0.7f, 0.3f, 0.2f};
    core::math::Vec3f specular{0.5f, 0.5f, 0.5f};
    core::f32 shininess = 32.0f;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: `alignas`와 GPU 메모리 레이아웃**

나중에 Uniform Buffer Object (UBO)를 사용할 때 `std140` 레이아웃 규칙을 따라야 한다:

```cpp
// std140 규칙: vec3는 16바이트 경계에 정렬
struct alignas(16) LightUBO {
    core::math::Vec3f direction; float _pad0;  // 16 bytes
    core::math::Vec3f ambient;   float _pad1;  // 16 bytes
    core::math::Vec3f diffuse;   float _pad2;  // 16 bytes
    core::math::Vec3f specular;  float _pad3;  // 16 bytes
};
static_assert(sizeof(LightUBO) == 64);
```

지금은 uniform으로 개별 전달하지만, 이 구조를 기억해두면 Ch.16에서 유용하다.

### Step 2: Phong 셰이더

```glsl
// shaders/phong.vert
// (버전 태그는 렌더러가 자동 추가)

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * worldPos;
}
```

```glsl
// shaders/phong.frag

out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

// Light
uniform vec3 uLightDir;
uniform vec3 uLightAmbient;
uniform vec3 uLightDiffuse;
uniform vec3 uLightSpecular;

// Material
uniform vec3 uMatAmbient;
uniform vec3 uMatDiffuse;
uniform vec3 uMatSpecular;
uniform float uMatShininess;

// Camera
uniform vec3 uViewPos;

// 디버그 토글
uniform bool uShowAmbient;
uniform bool uShowDiffuse;
uniform bool uShowSpecular;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);         // 빛 방향 반전 (표면→광원)
    vec3 V = normalize(uViewPos - vWorldPos); // 표면→카메라
    vec3 R = reflect(-L, N);                 // 반사 벡터

    // ── Ambient ──
    vec3 ambient = uLightAmbient * uMatAmbient;

    // ── Diffuse ──
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = uLightDiffuse * uMatDiffuse * diff;

    // ── Specular ──
    float spec = pow(max(dot(R, V), 0.0), uMatShininess);
    vec3 specular = uLightSpecular * uMatSpecular * spec;

    // ── 토글 ──
    vec3 result = vec3(0.0);
    if (uShowAmbient)  result += ambient;
    if (uShowDiffuse)  result += diffuse;
    if (uShowSpecular) result += specular;

    FragColor = vec4(result, 1.0);
}
```

### Step 3: Normal Matrix 계산

```cpp
// 렌더링 시:
Mat4f model = translate(positions[i]) * rotateY(angle);
Mat4f normalMat4 = transpose(inverse(model));

// Mat4 → Mat3 추출 (좌상단 3x3)
// Mat3 타입이 아직 없으므로 float[9]로 전달
float normalMat[9] = {
    normalMat4[0][0], normalMat4[0][1], normalMat4[0][2],
    normalMat4[1][0], normalMat4[1][1], normalMat4[1][2],
    normalMat4[2][0], normalMat4[2][1], normalMat4[2][2],
};
glUniformMatrix3fv(normalLoc, 1, GL_FALSE, normalMat);
// → 추후 ShaderProgram에 setMat3 추가하여 정리
```

### Step 4: 데모 코드

```cpp
void render(App& app, f32 alpha) {
    // ...
    app.shader->bind();

    // 광원 설정
    auto& light = app.light;
    app.shader->setVec3("uLightDir", normalize(light.direction));
    app.shader->setVec3("uLightAmbient", light.ambient);
    app.shader->setVec3("uLightDiffuse", light.diffuse);
    app.shader->setVec3("uLightSpecular", light.specular);
    app.shader->setVec3("uViewPos", cameraPos);

    // 토글 상태
    app.shader->setInt("uShowAmbient",  app.showAmbient ? 1 : 0);
    app.shader->setInt("uShowDiffuse",  app.showDiffuse ? 1 : 0);
    app.shader->setInt("uShowSpecular", app.showSpecular ? 1 : 0);

    // 머터리얼별로 도형 렌더링
    Material materials[] = {
        {.diffuse={0.8f, 0.2f, 0.2f}, .shininess=16},   // 빨간 박스 (매트)
        {.diffuse={0.2f, 0.6f, 0.8f}, .shininess=64},   // 파란 구 (광택)
        {.diffuse={0.6f, 0.6f, 0.2f}, .shininess=32},   // 노란 실린더
        {.diffuse={0.4f, 0.4f, 0.4f}, .shininess=8},    // 회색 바닥 (러프)
    };

    for (int i = 0; i < 4; ++i) {
        app.shader->setVec3("uMatAmbient", materials[i].ambient);
        app.shader->setVec3("uMatDiffuse", materials[i].diffuse);
        app.shader->setVec3("uMatSpecular", materials[i].specular);
        app.shader->setFloat("uMatShininess", materials[i].shininess);

        Mat4f model = translate(Vec3f{positions[i], 0, 0}) * rotateY(angle);
        app.shader->setMat4("uModel", model);
        app.shader->setMat4("uView", view);
        app.shader->setMat4("uProjection", proj);
        // normalMatrix 전달 ...

        app.meshes[i].draw(*app.renderer);
    }
}

void update(App& app, f32 dt) {
    // A/D/S 토글
    if (app.input.isKeyPressed(SDLK_A)) app.showAmbient  = !app.showAmbient;
    if (app.input.isKeyPressed(SDLK_D)) app.showDiffuse  = !app.showDiffuse;
    if (app.input.isKeyPressed(SDLK_S)) app.showSpecular = !app.showSpecular;
}
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| Phong 적용 | 밝은 면과 어두운 면이 구분된다 |
| Specular 하이라이트 | 구에서 광택 점이 보인다 |
| A 키 토글 | Ambient만 → 전체가 어둡게 균일 |
| D 키 토글 | Diffuse만 → 빛 방향에 따라 밝기 변화 |
| S 키 토글 | Specular만 → 하이라이트 점만 보임 |
| shininess 차이 | 박스(16)는 넓은 반사, 구(64)는 날카로운 반사 |

---

## 4. 블로그 데모 아이디어

1. **3분할 스크린샷**: Ambient only / Diffuse only / Specular only / 합성
2. **shininess 비교**: 8, 32, 128, 512 스펙큘러 지수 비교
3. **법선 행렬 필요성**: 비균등 스케일 시 법선 왜곡 전/후 비교
4. **Phong vs Flat shading**: smooth normal vs face normal 비교

---

## 다음 챕터 예고

**Chapter 08: 엔티티와 씬 관리**

오브젝트들을 Entity로 관리하고 Scene에서 일괄 렌더링한다.
데모: 10+개의 오브젝트가 있는 씬, 클릭으로 오브젝트 선택/하이라이트.
