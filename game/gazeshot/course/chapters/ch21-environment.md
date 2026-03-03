# Chapter 21: 환경 렌더링

## 데모 미리보기

```
┌─────────────────────────────────────────────────────┐
│  ░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░   ← 스카이박스 (하늘)     │
│  ░▒▓███████████████████▓░   ← 구름 + 산 텍스처       │
│  ─────────────────────────                          │
│  ▓▓▒▒░░░░░░░░░▒▒▓▓████     ← 터레인 (풀/흙/바위)    │
│  ▒░░  ◎  ◎  ◎  ░▒▓██                               │
│     ┃     ┃     ┃          ← 장애물                 │
│   ▄▄▄   ▄▄▄   ▄▄▄         ← 그림자 (shadow map)    │
│                                                     │
│  ● ← 플레이어                                       │
├─────────────────────────────────────────────────────┤
│  [F2] Skybox ON/OFF  |  [F3] Shadows ON/OFF        │
└─────────────────────────────────────────────────────┘
```

- **데모**: 사격장에 스카이박스 + 터레인 + 그림자가 추가된 완전한 환경
- **비주얼 업그레이드**: Ch.10의 단색 배경 → 하늘, 바닥 → 지형 텍스처
- 블로그에 "Before/After 비교"와 "Shadow Map 시각화" 포함 가능

---

## 학습 목표

1. 큐브맵 텍스처를 로드하고 스카이박스를 렌더링한다
2. 하이트맵 기반 터레인을 생성하고 텍스처 스플래팅을 적용한다
3. 섀도우 매핑의 원리를 이해하고 기본 구현한다
4. `std::filesystem`으로 에셋 경로를 플랫폼 독립적으로 관리한다

---

## 1. 배경 지식

### 큐브맵 (Cubemap)

```
하나의 큐브맵 = 6장의 2D 텍스처를 큐브 면에 매핑

        ┌──────┐
        │ +Y   │
   ┌────┼──────┼────┬──────┐
   │ -X │ +Z   │ +X │ -Z   │
   └────┼──────┼────┴──────┘
        │ -Y   │
        └──────┘

샘플링: 3D 방향 벡터 → 가장 큰 성분의 축 → 해당 면의 2D 좌표
OpenGL: glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, ...)
셰이더: samplerCube + 3D 방향 벡터로 샘플링
```

### 하이트맵 (Heightmap)

```
그레이스케일 이미지 → 높이 데이터
  픽셀 0 (검정) = 최저점, 픽셀 255 (흰색) = 최고점
  256x256 이미지 → 256x256 격자 메시, Y = pixel/255 * maxHeight

법선: 유한 차분법 (인접 정점 기울기)
  N = normalize(vec3(h(x-1,z)-h(x+1,z), 2.0, h(x,z-1)-h(x,z+1)))
```

### 섀도우 매핑 (Shadow Mapping)

```
2-패스 알고리즘:

Pass 1: 광원 시점 → FBO에 깊이만 렌더링 → "shadow map"
Pass 2: 카메라 시점 → 각 프래그먼트를 광원 공간으로 변환
        → shadow map 깊이와 비교 → 현재 깊이 > 저장 깊이 = 그림자!

Shadow Acne: 자기 표면이 자기 그림자를 만드는 줄무늬 현상
  → 해결: bias 추가 (0.001~0.005)
Peter Panning: bias 과다 → 그림자가 물체에서 떨어짐
  → 해결: slope-scale bias로 적절히 조절
```

---

## 2. 구현 가이드

### Step 1: 에셋 경로 관리

```hpp
// engine/include/gazeshot/engine/AssetPath.hpp
#pragma once
#include <string>
#if !defined(__EMSCRIPTEN__)
#include <filesystem>
#endif

namespace gazeshot::engine {

#if defined(__EMSCRIPTEN__)
// WASM: 가상 파일시스템, 단순 문자열 결합
inline std::string assetPath(const std::string& category,
                             const std::string& name) {
    return "assets/" + category + "/" + name;
}
#else
namespace fs = std::filesystem;
// 네이티브: std::filesystem으로 안전한 경로 조합
inline std::string assetPath(const std::string& category,
                             const std::string& name) {
    return (fs::path("assets") / category / name).string();
}
inline std::vector<std::string> listAssets(const std::string& dir) {
    std::vector<std::string> files;
    auto dirPath = fs::path("assets") / dir;
    if (fs::exists(dirPath) && fs::is_directory(dirPath)) {
        for (const auto& entry : fs::directory_iterator(dirPath))
            if (entry.is_regular_file())
                files.push_back(entry.path().string());
    }
    return files;
}
#endif

} // namespace gazeshot::engine
```

### Step 2: 큐브맵 로딩 + 스카이박스

```hpp
// engine/include/gazeshot/engine/Cubemap.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <array>
#include <string>

namespace gazeshot::engine {

class Cubemap {
public:
    static constexpr std::array<const char*, 6> FACE_NAMES = {
        "right", "left", "top", "bottom", "front", "back"
    };

    bool load(const std::string& directory) {
        glGenTextures(1, &textureId_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureId_);

        for (core::u32 i = 0; i < 6; ++i) {
            auto path = assetPath(directory, std::string(FACE_NAMES[i]) + ".jpg");
            int w, h, ch;
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 3);
            if (!data) return false;

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        return true;
    }

    void bind(core::u32 unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureId_);
    }

    ~Cubemap() { if (textureId_) glDeleteTextures(1, &textureId_); }
    Cubemap(Cubemap&& o) noexcept : textureId_(std::exchange(o.textureId_, 0)) {}
    Cubemap(const Cubemap&) = delete;

private:
    unsigned int textureId_ = 0;
};

} // namespace gazeshot::engine
```

### Step 3: 스카이박스 렌더링

```hpp
// engine/include/gazeshot/engine/Skybox.hpp
#pragma once
#include <gazeshot/engine/Cubemap.hpp>
#include <gazeshot/renderer/Renderer.hpp>

namespace gazeshot::engine {

class Skybox {
public:
    void init(renderer::Renderer& renderer, const std::string& dir) {
        cubemap_.load(dir);
        // 큐브 정점 36개 (내부에서 바라보므로 와인딩 반전)
        float verts[] = {
            -1,-1,-1, 1,-1,-1, 1, 1,-1, -1,-1,-1, 1, 1,-1, -1, 1,-1, // -Z
            -1,-1, 1,-1, 1, 1, 1, 1, 1, -1,-1, 1, 1, 1, 1,  1,-1, 1, // +Z
            -1,-1,-1,-1, 1,-1,-1, 1, 1, -1,-1,-1,-1, 1, 1, -1,-1, 1, // -X
             1,-1,-1, 1,-1, 1, 1, 1, 1,  1,-1,-1, 1, 1, 1,  1, 1,-1, // +X
            -1, 1,-1, 1, 1,-1, 1, 1, 1, -1, 1,-1, 1, 1, 1, -1, 1, 1, // +Y
            -1,-1,-1,-1,-1, 1, 1,-1, 1, -1,-1,-1, 1,-1, 1,  1,-1,-1, // -Y
        };
        vao_ = renderer.createVertexArray();
        renderer.bindVertexArray(vao_);
        vbo_ = renderer.createVertexBuffer(verts, sizeof(verts),
            renderer::BufferUsage::Static);
        renderer.setVertexLayout({{"aPosition", renderer::AttribType::Float3}});
        shader_ = renderer.createShaderProgram(VERT, FRAG);
    }

    void render(renderer::Renderer& renderer,
                const core::math::Mat4f& view,
                const core::math::Mat4f& proj) {
        glDepthFunc(GL_LEQUAL);  // 항상 가장 뒤에 렌더링
        shader_->bind();
        // view에서 이동 성분 제거 → 카메라 이동해도 하늘은 고정
        auto skyView = view;
        skyView[3][0] = skyView[3][1] = skyView[3][2] = 0.0f;
        shader_->setMat4("uView", skyView);
        shader_->setMat4("uProjection", proj);
        shader_->setInt("uSkybox", 0);
        cubemap_.bind(0);
        renderer.bindVertexArray(vao_);
        renderer.drawArrays(36);
        glDepthFunc(GL_LESS);  // 복원
    }

private:
    Cubemap cubemap_;
    std::unique_ptr<renderer::ShaderProgram> shader_;
    std::unique_ptr<renderer::VertexBuffer> vbo_;
    core::u32 vao_ = 0;

    static constexpr const char* VERT = R"(
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uView;
        uniform mat4 uProjection;
        out vec3 vTexCoord;
        void main() {
            vTexCoord = aPosition;
            vec4 pos = uProjection * uView * vec4(aPosition, 1.0);
            gl_Position = pos.xyww;  // z=w → 깊이 항상 1.0
        }
    )";
    static constexpr const char* FRAG = R"(
        in vec3 vTexCoord;
        out vec4 FragColor;
        uniform samplerCube uSkybox;
        void main() { FragColor = texture(uSkybox, vTexCoord); }
    )";
};

} // namespace gazeshot::engine
```

**핵심 트릭: `gl_Position = pos.xyww`**

`z = w` → 투영 후 `z/w = 1.0` (최대 깊이). `glDepthFunc(GL_LEQUAL)`과 조합하면 다른 모든 오브젝트 뒤에 렌더링된다.

### Step 4: 터레인 생성

```hpp
// engine/include/gazeshot/engine/Terrain.hpp
#pragma once
#include <gazeshot/engine/Mesh.hpp>
#include <gazeshot/core/math/Math.hpp>
#include <vector>

namespace gazeshot::engine {

class Terrain {
public:
    struct Config {
        core::f32 width = 100.0f, depth = 100.0f;
        core::f32 maxHeight = 5.0f;
        core::u32 resolution = 128;
    };

    Mesh generateFromHeightmap(const std::string& path, const Config& cfg) {
        int imgW, imgH, ch;
        unsigned char* data = stbi_load(path.c_str(), &imgW, &imgH, &ch, 1);
        if (!data) return generateFlat(cfg);  // 폴백

        auto res = cfg.resolution;
        std::vector<core::Vertex> verts;
        std::vector<core::u32> idxs;
        verts.reserve((res + 1) * (res + 1));

        for (core::u32 z = 0; z <= res; ++z) {
            for (core::u32 x = 0; x <= res; ++x) {
                core::f32 u = float(x) / float(res);
                core::f32 v = float(z) / float(res);
                int ix = int(u * (imgW - 1)), iz = int(v * (imgH - 1));
                core::f32 h = float(data[iz * imgW + ix]) / 255.0f;

                verts.push_back({
                    .position = {(u-0.5f)*cfg.width, h*cfg.maxHeight, (v-0.5f)*cfg.depth},
                    .normal   = {0, 1, 0},
                    .texCoord = {u * 4.0f, v * 4.0f},
                });
            }
        }
        stbi_image_free(data);
        computeNormals(verts, res);
        buildIndices(idxs, res);
        return Mesh(std::move(verts), std::move(idxs));
    }

    Mesh generateFlat(const Config& cfg) {
        auto res = cfg.resolution;
        std::vector<core::Vertex> verts;
        std::vector<core::u32> idxs;
        for (core::u32 z = 0; z <= res; ++z)
            for (core::u32 x = 0; x <= res; ++x) {
                core::f32 u = float(x)/float(res), v = float(z)/float(res);
                verts.push_back({
                    {(u-0.5f)*cfg.width, 0, (v-0.5f)*cfg.depth},
                    {0, 1, 0}, {u*8, v*8}
                });
            }
        buildIndices(idxs, res);
        return Mesh(std::move(verts), std::move(idxs));
    }

private:
    void buildIndices(std::vector<core::u32>& idxs, core::u32 res) {
        core::u32 cols = res + 1;
        for (core::u32 z = 0; z < res; ++z)
            for (core::u32 x = 0; x < res; ++x) {
                core::u32 tl = z*cols+x, tr = tl+1, bl = tl+cols, br = bl+1;
                idxs.insert(idxs.end(), {tl, bl, tr, tr, bl, br});
            }
    }

    void computeNormals(std::vector<core::Vertex>& v, core::u32 res) {
        using namespace core::math;
        core::u32 cols = res + 1;
        for (core::u32 z = 0; z <= res; ++z)
            for (core::u32 x = 0; x <= res; ++x) {
                auto hL = v[z*cols + std::max(x,1u)-1].position.y;
                auto hR = v[z*cols + std::min(x,res-1)+1].position.y;
                auto hU = v[std::max(z,1u)*cols-cols + x].position.y;
                auto hD = v[std::min(z,res-1)*cols+cols + x].position.y;
                v[z*cols+x].normal = normalize(Vec3f{hL-hR, 2.0f, hU-hD});
            }
    }
};

} // namespace gazeshot::engine
```

### Step 5: 터레인 셰이더 (텍스처 스플래팅 + 그림자)

```glsl
// shaders/terrain.frag
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vLightSpacePos;
out vec4 FragColor;

uniform sampler2D uGrassTex, uDirtTex, uRockTex, uShadowMap;
uniform vec3 uLightDir, uLightAmbient, uLightDiffuse;
uniform float uMaxHeight;

float calcShadow(vec4 lsPos) {
    vec3 proj = lsPos.xyz / lsPos.w * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;

    // slope-scale bias
    float bias = max(0.005 * (1.0 - dot(normalize(vNormal),
                     normalize(-uLightDir))), 0.001);

    // 3x3 PCF (부드러운 그림자 경계)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y) {
            float d = texture(uShadowMap, proj.xy + vec2(x,y)*texelSize).r;
            shadow += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    return shadow / 9.0;
}

void main() {
    // 스플래팅: 경사와 높이로 풀/흙/바위 블렌딩
    float height = vWorldPos.y / uMaxHeight;
    float slope = 1.0 - dot(normalize(vNormal), vec3(0,1,0));

    float wGrass = clamp(1.0 - slope*3.0 - height*0.5, 0.0, 1.0);
    float wDirt  = clamp(1.0 - abs(slope-0.3)*4.0, 0.0, 1.0);
    float wRock  = clamp(slope*3.0 + height*0.3 - 0.5, 0.0, 1.0);
    float total  = wGrass + wDirt + wRock;
    if (total > 0.0) { wGrass /= total; wDirt /= total; wRock /= total; }

    vec3 tex = texture(uGrassTex, vTexCoord).rgb * wGrass
             + texture(uDirtTex,  vTexCoord).rgb * wDirt
             + texture(uRockTex,  vTexCoord).rgb * wRock;

    // Phong + 그림자
    float diff = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
    float shadow = calcShadow(vLightSpacePos);
    FragColor = vec4(uLightAmbient*tex + uLightDiffuse*tex*diff*shadow, 1.0);
}
```

### Step 6: 섀도우 맵

```hpp
// engine/include/gazeshot/engine/ShadowMap.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <memory>

namespace gazeshot::engine {

class ShadowMap {
public:
    void init(renderer::Renderer& r, core::u32 size = 2048) {
        size_ = size;
        glGenFramebuffers(1, &fbo_);
        glGenTextures(1, &depthTex_);
        glBindTexture(GL_TEXTURE_2D, depthTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16,
                     size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, depthTex_, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        shader_ = r.createShaderProgram(DEPTH_V, DEPTH_F);
    }

    core::math::Mat4f lightSpaceMatrix(const core::math::Vec3f& lightDir,
                                       const core::math::Vec3f& center,
                                       core::f32 radius) const {
        using namespace core::math;
        auto pos = center - normalize(lightDir) * radius;
        return ortho(-radius, radius, -radius, radius, 0.1f, radius*2) *
               lookAt(pos, center, Vec3f{0,1,0});
    }

    void beginDepthPass() {
        glViewport(0, 0, size_, size_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glClear(GL_DEPTH_BUFFER_BIT);
        shader_->bind();
    }

    void endDepthPass(core::i32 w, core::i32 h) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
    }

    void bindDepthTexture(core::u32 unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, depthTex_);
    }

    renderer::ShaderProgram& depthShader() { return *shader_; }

    ~ShadowMap() {
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        if (depthTex_) glDeleteTextures(1, &depthTex_);
    }

private:
    unsigned int fbo_ = 0, depthTex_ = 0;
    core::u32 size_ = 0;
    std::unique_ptr<renderer::ShaderProgram> shader_;

    static constexpr const char* DEPTH_V = R"(
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uLightSpaceMatrix;
        uniform mat4 uModel;
        void main() {
            gl_Position = uLightSpaceMatrix * uModel * vec4(aPosition, 1.0);
        }
    )";
    static constexpr const char* DEPTH_F = R"(
        void main() { /* 깊이 자동 기록 */ }
    )";
};

} // namespace gazeshot::engine
```

### Step 7: 전체 통합

```cpp
// game/src/EnvironmentDemo.cpp
#include <gazeshot/engine/Skybox.hpp>
#include <gazeshot/engine/Terrain.hpp>
#include <gazeshot/engine/ShadowMap.hpp>
#include <gazeshot/engine/AssetPath.hpp>

using namespace gazeshot;
using namespace gazeshot::engine;

struct EnvironmentState {
    Skybox skybox;
    Terrain terrain;
    ShadowMap shadowMap;
    Mesh terrainMesh;
    bool showSkybox = true, showShadows = true;
};

void initEnvironment(EnvironmentState& env, renderer::Renderer& r) {
    env.skybox.init(r, assetPath("textures", "skybox"));
    env.terrainMesh = env.terrain.generateFromHeightmap(
        assetPath("textures", "heightmap.png"),
        {.width=120, .depth=120, .maxHeight=3, .resolution=64});
    env.terrainMesh.upload(r);
    env.shadowMap.init(r, 2048);
}

void renderEnvironment(EnvironmentState& env, App& app) {
    auto& r = *app.renderer;
    auto view = app.camera.viewMatrix();
    auto proj = app.camera.projectionMatrix();

    // Pass 1: 깊이 패스 (섀도우 맵)
    core::math::Mat4f lightMat;
    if (env.showShadows) {
        lightMat = env.shadowMap.lightSpaceMatrix(
            app.light.direction, {0,0,-35}, 80.0f);
        env.shadowMap.beginDepthPass();
        env.shadowMap.depthShader().setMat4("uLightSpaceMatrix", lightMat);
        for (auto& e : app.scene.activeEntities()) {
            env.shadowMap.depthShader().setMat4("uModel", e.modelMatrix());
            e.mesh()->draw(r);
        }
        env.shadowMap.endDepthPass(app.window.width(), app.window.height());
    }

    // Pass 2: 메인 렌더링
    r.clear({0.1f, 0.1f, 0.12f, 1.0f});
    if (env.showSkybox) env.skybox.render(r, view, proj);
    // 터레인 + 씬 오브젝트 렌더링 (셰이더에 lightMat, shadowMap 전달)
    // ...

    if (app.input.isKeyPressed(SDLK_F2)) env.showSkybox  = !env.showSkybox;
    if (app.input.isKeyPressed(SDLK_F3)) env.showShadows = !env.showShadows;
}
```

---

## 3. C++ 학습 포인트: `std::filesystem`

### 경로 조합과 `operator/`

```cpp
namespace fs = std::filesystem;

// operator/로 플랫폼 독립적 경로 조합
auto texPath = fs::path("assets") / "textures" / "skybox";
// Windows: "assets\textures\skybox"
// Linux/Mac: "assets/textures/skybox"

// 유용한 메서드들
fs::path file("heightmap.png");
file.extension();    // ".png"
file.stem();         // "heightmap"
fs::exists(texPath); // 존재 확인
```

### 디렉토리 순회

```cpp
// 큐브맵 면 자동 발견
for (const auto& entry : fs::directory_iterator(texPath)) {
    if (entry.is_regular_file()) {
        auto name = entry.path().stem().string();  // "right", "left", ...
        // → 큐브맵 면 이름으로 매핑
    }
}
```

### WASM 환경의 제한

```cpp
// Emscripten 가상 FS에서 std::filesystem 기능이 제한됨:
// - directory_iterator: 동작하지만 느릴 수 있음
// - 실제 파일은 --preload-file로 미리 번들링 필요
// → 에셋 목록 하드코딩이 WASM에서 더 안전

#if defined(__EMSCRIPTEN__)
constexpr std::array<const char*, 6> SKYBOX_FACES = {
    "assets/textures/skybox/right.jpg", /* ... */
};
#else
auto faces = listAssets("textures/skybox");
#endif
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 스카이박스 표시 | 사방을 둘러봐도 하늘 텍스처가 연속적 |
| 카메라 이동 불변 | 이동해도 스카이박스가 따라오지 않음 |
| 깊이 순서 정확 | 모든 오브젝트가 스카이박스 앞에 렌더링 |
| 터레인 표시 | 하이트맵 기복 또는 텍스처 바닥 표시 |
| 텍스처 스플래팅 | 높이/경사별 풀/흙/바위 자연스러운 블렌딩 |
| 그림자 표시 | 장애물이 바닥에 그림자를 드리움 |
| Shadow acne 없음 | 표면에 줄무늬 없음 (bias 적용) |
| F2/F3 토글 | 스카이박스/그림자 ON/OFF 전환 |
| 사격장 유지 | Ch.10 타겟/장애물이 환경 위에 정상 표시 |

---

## 5. 블로그 데모 아이디어

1. **Before/After**: Ch.10 단색 배경 vs Ch.21 스카이박스+터레인 비교
2. **큐브맵 전개도**: 6장 텍스처와 결합 결과
3. **Shadow Map 시각화**: 깊이 텍스처를 디버그 뷰로 출력
4. **Shadow Acne 비교**: bias 0 (줄무늬) vs bias 0.003 (깨끗)
5. **스플래팅 분해**: 개별 텍스처(풀/흙/바위)와 블렌딩 결과 비교
6. **PCF 비교**: PCF 없음 (계단) vs 3x3 PCF (부드러운 경계)

---

## 다음 챕터 예고

**Chapter 22: 사운드 시스템**

게임에 소리를 추가한다! 사격 효과음, 타겟 피격음, 환경 사운드를 재생한다.
데모: 사격 시 총성 + 타겟 피격 시 금속 타격음 + 배경에 은은한 환경음이 깔린다.
