# Chapter 16: 리소스 매니저

## 데모 미리보기

```
┌─────────────────────────────────────────────────┐
│  ResourceManager                                │
│  ┌──────────────────────────────────────────┐   │
│  │  ShaderCache                             │   │
│  │  "phong"   → Handle(0,gen=1)            │   │
│  │  "outline" → Handle(1,gen=1)            │   │
│  ├──────────────────────────────────────────┤   │
│  │  TextureCache                            │   │
│  │  "crate.png" → Handle(0,gen=1) [mipmap] │   │
│  │  "floor.png" → Handle(1,gen=1) [mipmap] │   │
│  ├──────────────────────────────────────────┤   │
│  │  MeshCache                               │   │
│  │  "sphere_32x16" → Handle(0) [procedural] │   │
│  │  "box_1x1x1"    → Handle(1) [procedural] │   │
│  │  "target.obj"   → Handle(2) [file]       │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│  loadShader("phong") → 캐시 히트, 즉시 반환      │
│  Console: "Shaders: 2, Textures: 2, Meshes: 3" │
└─────────────────────────────────────────────────┘
```

- **데모**: 리소스 매니저를 통해 셰이더/텍스처/메시를 로딩, 중복 요청 시 캐시에서 즉시 반환
- **콘솔**: 로딩된 리소스 종류별 개수, 캐시 히트/미스 통계 출력
- 블로그에 "핸들 기반 리소스 관리 아키텍처" 다이어그램 포함 가능

---

## 학습 목표

1. 타입별 리소스 캐시와 경로 기반 중복 로딩 방지를 구현한다
2. 핸들(Handle) 기반 참조 시스템으로 댕글링 포인터를 방지한다
3. 셰이더/텍스처/메시 매니저를 ResourceManager에 통합한다
4. type erasure, `std::any`, CRTP, handle/generation 패턴을 실습한다

---

## 1. 배경 지식

### 지금까지의 리소스 관리 방식

```
Ch.04 ~ Ch.15까지의 문제점:

1. App 구조체에 리소스를 직접 보관
   App { Mesh boxMesh; ShaderProgram* shader; };

2. 같은 메시를 여러 엔티티가 참조 → 누가 해제?
   entity1.setMesh(&app.boxMesh);  // non-owning
   entity2.setMesh(&app.boxMesh);  // 같은 메시를 또 참조
   // app이 먼저 소멸되면? → 댕글링 포인터

3. 셰이더를 매번 새로 로딩
   auto s1 = loadShader("phong.vs", "phong.fs");
   auto s2 = loadShader("phong.vs", "phong.fs");  // 같은 걸 또?
```

### 핸들(Handle) vs 포인터

```
포인터 방식:
  Mesh* mesh = loadMesh("box");
  deleteMesh(mesh);       // mesh 해제
  entity.mesh()->draw();  // 댕글링 포인터!

핸들 방식:
  ResourceHandle h = loadMesh("box");
  deleteMesh(h);           // h의 generation 무효화
  auto* mesh = resolve(h); // nullptr — 안전
```

핸들은 인덱스 + 세대 번호의 조합이다.
리소스가 해제되고 같은 슬롯이 재사용되면 세대 번호가 증가하므로,
이전 핸들로 접근하면 세대 불일치로 `nullptr`을 반환한다.

---

## 2. 구현 가이드

### Step 1: ResourceHandle

```hpp
// engine/include/gazeshot/engine/ResourceHandle.hpp
#pragma once
#include <gazeshot/core/Types.hpp>
#include <functional>

namespace gazeshot::engine {

struct ResourceHandle {
    core::u32 index      = 0;   // 배열 인덱스
    core::u32 generation = 0;   // 세대 번호 (재사용 감지)

    bool operator==(const ResourceHandle&) const = default;
    [[nodiscard]] bool isValid() const { return generation != 0; }
    static constexpr ResourceHandle invalid() { return {0, 0}; }
};

} // namespace gazeshot::engine

template<>
struct std::hash<gazeshot::engine::ResourceHandle> {
    std::size_t operator()(const gazeshot::engine::ResourceHandle& h) const noexcept {
        auto h1 = std::hash<gazeshot::core::u32>{}(h.index);
        auto h2 = std::hash<gazeshot::core::u32>{}(h.generation);
        return h1 ^ (h2 << 16);
    }
};
```

**C++ 학습 포인트: handle/generation 패턴**

```cpp
struct Slot {
    u32 generation = 1;    // 현재 세대 (0은 invalid 예약)
    bool occupied  = false;
};

ResourceHandle create() {
    u32 idx = findFreeSlot();
    slots_[idx].occupied = true;
    return { idx, slots_[idx].generation };
}

void destroy(ResourceHandle handle) {
    auto& slot = slots_[handle.index];
    if (slot.generation != handle.generation) return;
    slot.occupied = false;
    slot.generation++;  // 세대 증가 → 이전 핸들 무효화
}

T* resolve(ResourceHandle handle) {
    auto& slot = slots_[handle.index];
    if (!slot.occupied || slot.generation != handle.generation)
        return nullptr;
    return &resources_[handle.index];
}
```

이 패턴은 게임 엔진에서 매우 흔하다 (Unity InstanceID, Rust generational-arena 등).

### Step 2: ResourcePool (CRTP 기반)

```hpp
// engine/include/gazeshot/engine/ResourcePool.hpp
#pragma once
#include <gazeshot/engine/ResourceHandle.hpp>
#include <gazeshot/core/Types.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace gazeshot::engine {

template<typename Derived, typename ResourceType>
class ResourcePool {
public:
    using Handle = ResourceHandle;

    [[nodiscard]] Handle acquire(const std::string& key) {
        if (auto it = cache_.find(key); it != cache_.end()) {
            auto& slot = slots_[it->second.index];
            if (slot.occupied && slot.generation == it->second.generation) {
                ++cacheHits_; return it->second;
            }
        }
        ++cacheMisses_;
        auto handle = allocateSlot();
        resources_[handle.index] = static_cast<Derived*>(this)->load(key);
        cache_[key] = handle;
        return handle;
    }

    [[nodiscard]] ResourceType* resolve(Handle h) {
        if (!h.isValid() || h.index >= slots_.size()) return nullptr;
        auto& s = slots_[h.index];
        return (s.occupied && s.generation == h.generation) ? &resources_[h.index] : nullptr;
    }

    void release(Handle h) {
        if (!h.isValid() || h.index >= slots_.size()) return;
        auto& s = slots_[h.index];
        if (s.generation != h.generation) return;
        s.occupied = false; s.generation++;
        freeList_.push_back(h.index);
    }

    void clear() { slots_.clear(); resources_.clear(); freeList_.clear(); cache_.clear(); }
    [[nodiscard]] core::u32 count() const {
        core::u32 n = 0; for (auto& s : slots_) if (s.occupied) ++n; return n;
    }
    [[nodiscard]] core::u32 cacheHits() const { return cacheHits_; }
    [[nodiscard]] core::u32 cacheMisses() const { return cacheMisses_; }

protected:
    struct Slot { core::u32 generation = 1; bool occupied = false; };
    std::vector<Slot> slots_;
    std::vector<ResourceType> resources_;
    std::vector<core::u32> freeList_;
    std::unordered_map<std::string, Handle> cache_;
    core::u32 cacheHits_ = 0, cacheMisses_ = 0;

private:
    Handle allocateSlot() {
        core::u32 idx;
        if (!freeList_.empty()) { idx = freeList_.back(); freeList_.pop_back(); }
        else { idx = static_cast<core::u32>(slots_.size()); slots_.push_back({}); resources_.emplace_back(); }
        slots_[idx].occupied = true;
        return { idx, slots_[idx].generation };
    }
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: CRTP (Curiously Recurring Template Pattern)**

```cpp
template<typename Derived, typename ResourceType>
class ResourcePool {
    Handle acquire(const std::string& key) {
        // 파생 클래스의 load()를 호출 — 가상 함수가 아니다!
        resource = static_cast<Derived*>(this)->load(key);
    }
};

class ShaderPool : public ResourcePool<ShaderPool, ShaderProgram> {
    ShaderProgram load(const std::string& key) { /* ... */ }
};
```

CRTP의 핵심:
- **정적 다형성**: `virtual` 없이 파생 클래스의 메서드를 호출, vtable 오버헤드 없음
- **인터페이스 강제**: `load()`를 구현하지 않으면 컴파일 에러
- Ch.04의 Renderer(가상 함수)와 비교: 리소스 풀은 타입이 컴파일 타임에 결정 → CRTP 적합

### Step 3: ShaderPool

```hpp
// engine/include/gazeshot/engine/ShaderPool.hpp
#pragma once
#include <gazeshot/engine/ResourcePool.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gazeshot::engine {

class ShaderPool
    : public ResourcePool<ShaderPool, std::unique_ptr<renderer::ShaderProgram>> {
public:
    using ResourceType = std::unique_ptr<renderer::ShaderProgram>;
    explicit ShaderPool(renderer::Renderer& renderer) : renderer_(renderer) {}

    // key 형식: "shaders/phong" → phong.vert + phong.frag 로딩
    ResourceType load(const std::string& key) {
        auto vertSrc = readFile(key + ".vert");
        auto fragSrc = readFile(key + ".frag");
        return renderer_.createShaderProgram(vertSrc, fragSrc);
    }

private:
    renderer::Renderer& renderer_;

    static std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open())
            throw std::runtime_error("Failed to open shader: " + path);
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
};

} // namespace gazeshot::engine
```

### Step 4: TexturePool

```hpp
// engine/include/gazeshot/engine/TexturePool.hpp
#pragma once
#include <gazeshot/engine/ResourcePool.hpp>
#include <gazeshot/renderer/Renderer.hpp>
#include <stb_image.h>

namespace gazeshot::engine {

struct TextureData {
    core::u32 width = 0, height = 0, channels = 0;
    std::vector<core::u8> pixels;
    core::u32 glHandle = 0;
};

class TexturePool : public ResourcePool<TexturePool, TextureData> {
public:
    explicit TexturePool(renderer::Renderer& renderer) : renderer_(renderer) {}

    TextureData load(const std::string& path) {
        TextureData tex;
        int w, h, ch;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
        if (!data) throw std::runtime_error("Failed to load texture: " + path);
        tex.width = static_cast<core::u32>(w);
        tex.height = static_cast<core::u32>(h);
        tex.channels = static_cast<core::u32>(ch);
        tex.pixels.assign(data, data + (w * h * ch));
        stbi_image_free(data);
        tex.glHandle = uploadToGPU(tex);  // GPU 업로드 + 밉맵
        return tex;
    }

private:
    renderer::Renderer& renderer_;
    core::u32 uploadToGPU(const TextureData& tex) { return 0; /* 실제 구현 */ }
};

} // namespace gazeshot::engine
```

밉맵(Mipmap): 원본 텍스처를 절반씩 축소해 레벨별로 저장.
먼 오브젝트는 작은 레벨을 사용 → aliasing 감소 + GPU 캐시 효율 향상.
`glGenerateMipmap()` + `GL_LINEAR_MIPMAP_LINEAR`(trilinear) 필터 사용.

### Step 5: MeshPool

```hpp
// engine/include/gazeshot/engine/MeshPool.hpp
#pragma once
#include <gazeshot/engine/ResourcePool.hpp>
#include <gazeshot/engine/Mesh.hpp>
#include <gazeshot/engine/MeshGen.hpp>
#include <gazeshot/renderer/Renderer.hpp>

namespace gazeshot::engine {

class MeshPool : public ResourcePool<MeshPool, Mesh> {
public:
    explicit MeshPool(renderer::Renderer& renderer) : renderer_(renderer) {}

    ResourceHandle registerProcedural(const std::string& name, Mesh mesh) {
        mesh.upload(renderer_);
        if (auto it = cache_.find(name); it != cache_.end()) {
            auto& s = slots_[it->second.index];
            if (s.occupied && s.generation == it->second.generation)
                return it->second;
        }
        auto h = acquire(name);  // allocate + cache
        if (auto* r = resolve(h)) *r = std::move(mesh);
        return h;
    }

    Mesh load(const std::string& path) {
        if (path.ends_with(".obj"))
            throw std::runtime_error("OBJ: Ch.17에서 구현 예정");
        if (path == "sphere") return MeshGen::sphere(0.5f);
        if (path == "box")    return MeshGen::box();
        if (path == "plane")  return MeshGen::plane(10.0f, 10.0f);
        throw std::runtime_error("Unknown mesh: " + path);
    }

private:
    renderer::Renderer& renderer_;
};

} // namespace gazeshot::engine
```

### Step 6: ResourceManager (통합)

```hpp
// engine/include/gazeshot/engine/ResourceManager.hpp
#pragma once
#include <gazeshot/engine/ShaderPool.hpp>
#include <gazeshot/engine/TexturePool.hpp>
#include <gazeshot/engine/MeshPool.hpp>
#include <cstdio>

namespace gazeshot::engine {

class ResourceManager {
public:
    explicit ResourceManager(renderer::Renderer& renderer)
        : shaders_(renderer), textures_(renderer), meshes_(renderer) {}

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // ── 셰이더 ──
    [[nodiscard]] ResourceHandle loadShader(const std::string& name) {
        return shaders_.acquire(name);
    }
    [[nodiscard]] renderer::ShaderProgram* getShader(ResourceHandle h) {
        auto* ptr = shaders_.resolve(h);
        return ptr ? ptr->get() : nullptr;
    }

    // ── 텍스처 ──
    [[nodiscard]] ResourceHandle loadTexture(const std::string& path) {
        return textures_.acquire(path);
    }
    [[nodiscard]] TextureData* getTexture(ResourceHandle h) {
        return textures_.resolve(h);
    }

    // ── 메시 ──
    [[nodiscard]] ResourceHandle loadMesh(const std::string& key) {
        return meshes_.acquire(key);
    }
    [[nodiscard]] ResourceHandle registerMesh(const std::string& name, Mesh mesh) {
        return meshes_.registerProcedural(name, std::move(mesh));
    }
    [[nodiscard]] Mesh* getMesh(ResourceHandle h) {
        return meshes_.resolve(h);
    }

    void clear() { shaders_.clear(); textures_.clear(); meshes_.clear(); }

    void printStats() const {
        std::printf("ResourceManager stats:\n");
        std::printf("  Shaders:  %u (hits: %u, misses: %u)\n",
            shaders_.count(), shaders_.cacheHits(), shaders_.cacheMisses());
        std::printf("  Textures: %u (hits: %u, misses: %u)\n",
            textures_.count(), textures_.cacheHits(), textures_.cacheMisses());
        std::printf("  Meshes:   %u (hits: %u, misses: %u)\n",
            meshes_.count(), meshes_.cacheHits(), meshes_.cacheMisses());
    }

private:
    ShaderPool  shaders_;
    TexturePool textures_;
    MeshPool    meshes_;
};

} // namespace gazeshot::engine
```

**C++ 학습 포인트: type erasure와 `std::any`**

```cpp
// 대안적 접근: std::any로 타입을 지운 범용 캐시
class GenericResourceCache {
    std::unordered_map<std::type_index,
        std::unordered_map<std::string, std::any>> caches_;
public:
    template<typename T>
    void store(const std::string& key, T resource) {
        caches_[typeid(T)][key] = std::move(resource);
    }
    template<typename T>
    T* get(const std::string& key) {
        auto it = caches_.find(typeid(T));
        if (it == caches_.end()) return nullptr;
        auto r = it->second.find(key);
        return (r != it->second.end()) ? std::any_cast<T>(&r->second) : nullptr;
    }
};
// 장점: 코드 간결. 단점: 런타임 타입 검사, any_cast 실패 시 예외
// 우리의 선택: CRTP 기반 타입별 풀이 더 안전하고 효율적
```

### Step 7: 기존 코드 마이그레이션

```cpp
// game/src/main.cpp (Ch.16 — ResourceManager 적용)

struct App {
    std::unique_ptr<renderer::Renderer> renderer;
    std::unique_ptr<engine::ResourceManager> resources;
    // Before: Mesh boxMesh; ShaderProgram* shader;
    // After: 핸들로 참조
    engine::ResourceHandle phongShader, boxMesh, sphereMesh;
};

void init(App& app) {
    app.resources = std::make_unique<engine::ResourceManager>(*app.renderer);

    app.phongShader = app.resources->loadShader("assets/shaders/phong");
    app.boxMesh = app.resources->registerMesh("box", engine::MeshGen::box());
    app.sphereMesh = app.resources->registerMesh(
        "sphere", engine::MeshGen::sphere(0.5f, 32, 16));

    // 중복 로딩 → 캐시 히트 (같은 핸들 반환)
    auto dup = app.resources->loadShader("assets/shaders/phong");
    app.resources->printStats();
}

void initScene(App& app) {
    auto* boxPtr = app.resources->getMesh(app.boxMesh);

    auto& floor = app.scene.createEntity("floor");
    floor.setMesh(boxPtr);  // non-owning, ResourceManager가 소유
    floor.transform().scale = {10, 0.1f, 10};
}

void render(App& app, core::f32 alpha) {
    auto* shader = app.resources->getShader(app.phongShader);
    if (!shader) return;  // 핸들 무효 → 안전하게 리턴
    shader->bind();
    // ... 렌더링
}
```

### Step 8: 리소스 라이프사이클

```
생성:  loadShader("phong") → 캐시 미스 → 파일 읽기 → GPU 업로드 → 핸들 반환
참조:  getShader(handle)   → generation 검사 → 유효하면 포인터, 무효하면 nullptr
재요청: loadShader("phong") → 캐시 히트 → 기존 핸들 반환 (로딩 없음)
해제:  release(handle)     → generation++ → 이전 핸들 무효화
전체:  clear()             → 모든 풀 초기화, GPU 리소스 해제

※ 이번 챕터에서는 동기(sync) 방식. Ch.24에서 std::jthread로 비동기 확장 가능.
```

---

## 3. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 셰이더 캐싱 | 같은 셰이더를 두 번 로딩 → 캐시 히트 1회 |
| 메시 등록 | 프로시저럴 메시 3종 등록, 핸들로 접근 가능 |
| 핸들 유효성 | 해제된 핸들로 resolve → nullptr 반환 |
| 세대 번호 | 해제 후 재할당 → 이전 핸들로 접근 불가 |
| 통계 출력 | printStats()로 정확한 수치 확인 |
| 기존 기능 유지 | Ch.11까지의 렌더링/사격이 동일하게 동작 |
| 메모리 정리 | 프로그램 종료 시 clear() → leak 없음 |

---

## 블로그 데모 아이디어

1. **아키텍처 다이어그램**: ResourceManager → Pool별 캐시 → 핸들 참조 흐름도
2. **Before/After 코드**: App 구조체에서 raw 리소스 → 핸들 기반으로 전환
3. **캐시 히트/미스 통계**: 콘솔 출력 스크린샷
4. **핸들 무효화 시연**: 리소스 해제 후 접근 시도 → 안전하게 nullptr
5. **CRTP vs virtual 비교표**: 정적 다형성과 동적 다형성의 트레이드오프

---

## 다음 챕터 예고

**Chapter 17: 모델 로딩**

OBJ 파일 파서를 직접 구현하고 ResourceManager에 통합한다.
데모: 외부 3D 모델(총, 타겟)을 로딩하여 프로시저럴 도형 대신 렌더링한다. `std::from_chars`로 빠른 파싱, `std::ranges` 파이프라인으로 선언적 데이터 처리를 학습한다.
