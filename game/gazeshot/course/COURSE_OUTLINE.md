# Project GazeShot: 시선 추적 3D 스나이퍼 게임

## 코스 개요

OpenGL 기초 학습(13단계)을 마친 상태에서, 직접 미니 엔진을 설계하고 스나이퍼 게임을 완성하는 과정입니다.
외부 라이브러리 의존을 최소화하고, 수학/물리/렌더링/게임 로직을 직접 구현하며 학습합니다.

### 핵심 게임 메카닉 이해

```
[눈의 위치(얼굴)] ──→ 가늠자(Rear Sight) ──→ 카메라 위치 오프셋
[눈의 시선방향]   ──→ 가늠쇠(Front Sight) ──→ 레티클(조준점) 위치

카메라 위치 → 레티클 → 연장선 = 탄착점(사격 방향)

얼굴 이동 → 시점 변화 → 장애물 뒤 패럴랙스 효과로 숨겨진 타겟 발견
```

### 모던 C++ 학습 로드맵

이 코스는 게임 엔진을 만들며 **모던 C++(C++20 이상)** 을 자연스럽게 익히도록 설계한다.
각 챕터에서 해당 기능을 만들 때 가장 적합한 C++ 기법을 함께 학습한다.

```
Phase A (엔진 기반)
  Ch.01 ── namespace, 헤더/소스 분리, forward declaration, #pragma once
  Ch.02 ── class template, operator overloading, constexpr, friend, explicit
  Ch.03 ── template specialization, static_assert, user-defined literals, [[nodiscard]]
  Ch.04 ── virtual/override/final, std::unique_ptr, RAII, move semantics
  Ch.05 ── std::variant, std::visit, lambda, std::function, std::chrono

Phase B (3D 월드)
  Ch.06 ── std::vector 메모리 모델, std::span (C++20), emplace_back, structured bindings
  Ch.07 ── std::array, alignas, uniform 메모리 레이아웃
  Ch.08 ── unique_ptr/shared_ptr 소유권, std::optional, Rule of Zero
  Ch.09 ── 상속 vs 합성, std::clamp, std::lerp (C++20)
  Ch.10 ── designated initializers (C++20), constexpr 배열, aggregate types

Phase C (게임 메카닉)
  Ch.11 ── std::optional<HitResult>, [[nodiscard]], structured bindings 심화
  Ch.12 ── C++20 concepts, requires 절, 제네릭 프로그래밍
  Ch.13 ── 함수형 패턴: 고차 함수, 이징 함수를 lambda로
  Ch.14 ── std::string_view, std::format (C++20), std::span
  Ch.15 ── variant 기반 상태 머신, overloaded 패턴, std::chrono 타이머

Milestone 2 (비주얼)
  Ch.16 ── type erasure, std::any, CRTP, handle/generation 패턴
  Ch.17 ── std::from_chars, string_view 파싱, std::ranges 파이프라인
  Ch.18 ── enum class, std::bitset, scoped enumerations
  Ch.19 ── object pool, placement new, std::pmr (다형적 메모리 리소스)
  Ch.20 ── 함수 합성, std::function 체인, 렌더 패스 파이프라인
  Ch.21 ── std::filesystem, 에셋 경로 관리
  Ch.22 ── std::atomic, lock-free 기초, 콜백 패턴

Milestone 3 (시선 추적)
  Ch.24 ── std::jthread (C++20), std::stop_token, std::mutex, condition_variable
  Ch.25 ── ring buffer (std::array 기반), 신호 처리와 필터링
  Ch.27 ── [[likely]]/[[unlikely]], 프로파일링, 최적화 기법
  Ch.28 ── C 라이브러리 RAII 래핑, std::span으로 텐서 데이터 전달
```

### 크로스 플랫폼 전략

이 프로젝트는 **Desktop (macOS/Linux/Windows) + Web (WASM)** 을 모두 타겟으로 한다.
이를 위해 처음부터 다음 제약을 따른다:

- **렌더링**: OpenGL ES 3.0 / WebGL 2.0 기준 (GLSL 300 es)
  - Desktop에서는 OpenGL 3.3+ Core Profile로 구동 (ES 3.0 상위호환)
  - Web에서는 Emscripten이 WebGL 2.0으로 변환
- **플랫폼**: SDL3 사용 (윈도우, 입력, 오디오, 카메라 통합)
  - Emscripten이 SDL을 1급으로 지원 → WASM 빌드 시 자동 매핑
- **빌드**: CMake + Emscripten 툴체인 듀얼 빌드
- **게임 루프**: `emscripten_set_main_loop` 호환 구조로 설계
- **파일 I/O**: WASM에서는 가상 파일시스템 → 에셋 임베딩 또는 fetch

### 의존성 정책

| 구분 | 정책 | 비고 |
|------|------|------|
| 플랫폼/윈도우/입력 | SDL3 허용 | 윈도우 + 입력 + 오디오 + 카메라 통합, WASM 1급 지원 |
| OpenGL 로더 | GLAD 허용 (Desktop) | WASM에서는 Emscripten GL 직접 사용 |
| 수학 | **직접 구현** | GLM 대체 → 커스텀 math 라이브러리 |
| 물리/충돌 | **직접 구현** | Ray casting, AABB, 탄도 등 |
| 렌더링 | **직접 구현** | OpenGL ES 3.0 위에 추상화 레이어 |
| 이미지 로딩 | stb_image 허용 | 단일 헤더, 학습 가치 낮음 |
| 모델/에셋 | **직접 구현** | 간단한 포맷 파서 직접 작성 |
| 테스트 | doctest 허용 | 싱글 헤더, 빠른 컴파일, 간결한 문법 |
| 시선 추적 | 라이브러리 허용 | 후반부에 직접 구현 방법도 탐구 |

> **왜 GLFW가 아니라 SDL인가?**
>
> GLFW는 "OpenGL 윈도우를 띄우는 것"에 특화되어 있다. 반면 SDL은:
> - 오디오 내장 (별도 라이브러리 불필요)
> - SDL3에서 카메라(웹캠) API 추가 → 시선 추적 시 활용
> - Emscripten/WASM 빌드를 공식적으로 1급 지원
> - 게임 개발 생태계의 사실상 표준
>
> 이 프로젝트에서 필요한 윈도우/입력/오디오/카메라를 SDL 하나로 커버할 수 있다.

---

## Milestone 1: 프로토타이핑 (Chapter 01 ~ 15)

> 기본 오브젝트(구, 박스 등)로 게임의 핵심 메카닉을 모두 구현한다.
> 입력은 키보드(가늠자) + 마우스(가늠쇠)로 시작한다.

### Phase A: 엔진 기반 (Ch.01 ~ 05)

기존 튜토리얼의 반복 보일러플레이트를 정리하고, 재사용 가능한 엔진 구조를 세운다.

---

#### Chapter 01: 프로젝트 아키텍처와 빌드 시스템

**학습 주제**: 게임 엔진의 레이어 구조, 모듈화, CMake 듀얼 빌드 (Desktop + WASM)

**내용**:
- 엔진 아키텍처 레이어 설계
  ```
  [Game Layer]     ─ 스나이퍼 게임 고유 로직
  [Engine Layer]   ─ Scene, Entity, Input, Resource, Audio
  [Renderer Layer] ─ 추상 렌더러 + OpenGL ES 3.0 백엔드
  [Platform Layer] ─ Window, Context, Audio (SDL3)
  [Core Layer]     ─ Math, Memory, Logger, Types
  ```
- 디렉토리 구조 설계 및 CMake 타겟 분리
- `core/`, `renderer/`, `engine/`, `game/` 모듈별 static library
- 헤더 의존성 방향: 하위 → 상위 단방향만 허용
- **듀얼 빌드 설정**
  - Desktop: `cmake -B build` (SDL3 + GLAD + OpenGL)
  - WASM: `emcmake cmake -B build-web` (SDL3 + Emscripten GL)
  - 플랫폼별 조건 분기: `if(EMSCRIPTEN)` 가드
- doctest 테스트 프레임워크 통합
  - `tests/` 디렉토리, CTest 연동
  - `cmake --build build --target test`로 전체 테스트 실행
- 빌드 검증: 빈 main()에서 각 모듈 include 확인 + 기본 테스트 통과

**C++ 학습 포인트**:
- `namespace` 설계: `gazeshot::core`, `gazeshot::renderer` 등 중첩 네임스페이스
- 헤더/소스 분리 원칙과 `#pragma once`
- `forward declaration`으로 컴파일 의존성 끊기
- `#include` 순서 컨벤션 (프로젝트 → 서드파티 → 표준)
- C++20 `modules` 맛보기 (지원 상황 확인, 현 시점에서는 헤더 방식 유지)

**결과물**: Desktop + WASM 듀얼 빌드 가능한 프로젝트 스켈레톤

---

#### Chapter 02: 커스텀 수학 라이브러리 (1) - 벡터와 행렬

**학습 주제**: 게임 수학의 핵심, GLM 없이 직접 구현하는 선형대수

**내용**:
- `Vec2`, `Vec3`, `Vec4` 구현
  - 산술 연산자, dot, cross, normalize, length, lerp
  - `constexpr` 활용, 컴파일 타임 연산
- `Mat3`, `Mat4` 구현
  - 행렬 곱셈, 전치, 역행렬 (가우스-조르단 또는 코팩터)
  - 열 우선(column-major) 저장 → OpenGL 호환
- doctest로 단위 테스트 작성
  ```cpp
  TEST_CASE("Vec3 cross product") {
      Vec3 x{1, 0, 0}, y{0, 1, 0};
      auto z = cross(x, y);
      CHECK(z.x == doctest::Approx(0.0f));
      CHECK(z.y == doctest::Approx(0.0f));
      CHECK(z.z == doctest::Approx(1.0f));
  }
  ```
  - GLM과 결과 비교하여 검증 후 GLM 제거

**핵심 개념**:
- 왜 column-major인가? (OpenGL 메모리 레이아웃)
- `constexpr`과 `inline`의 차이, 헤더 온리 수학 라이브러리의 장단점
- 부동소수점 비교: `doctest::Approx`와 epsilon

**C++ 학습 포인트**:
- **class template**: `Vec<N, T>`로 Vec2/Vec3/Vec4를 하나의 템플릿으로
  ```cpp
  template<std::size_t N, typename T = float>
  struct Vec { T data[N]; };
  using Vec3 = Vec<3, float>;
  ```
- **operator overloading**: `+`, `-`, `*`, `==` 등 산술/비교 연산자
- **`constexpr`**: 컴파일 타임에 벡터 연산 가능하게
- **`friend` 함수**: 비멤버 연산자를 클래스 내부에서 정의
- **`explicit`**: 단일 인자 생성자의 암시적 변환 방지
- **aggregate initialization**: `Vec3{1.0f, 2.0f, 3.0f}`

**결과물**: `core/math/Vec.hpp`, `core/math/Mat.hpp` + doctest 테스트

---

#### Chapter 03: 커스텀 수학 라이브러리 (2) - 변환과 투영

**학습 주제**: 모델/뷰/프로젝션 변환, 쿼터니언

**내용**:
- 변환 함수 구현
  - `math::translate()`, `math::rotate()`, `math::scale()`
  - `math::lookAt()` - 카메라 뷰 행렬
  - `math::perspective()`, `math::ortho()` - 투영 행렬
- 쿼터니언 `Quat` 구현
  - 짐벌 락 문제 이해
  - 쿼터니언 회전, slerp 보간
  - 오일러 → 쿼터니언 → 행렬 변환
- 기존 cameraclass.cpp를 커스텀 수학으로 전환하여 검증

**핵심 개념**:
- perspective 행렬의 각 요소가 의미하는 것
- near/far plane과 depth buffer 정밀도의 관계
- 짐벌 락이 스나이퍼 조준에서 왜 문제가 되는지

**C++ 학습 포인트**:
- **template specialization**: 4x4 행렬에 특화된 역행렬 구현
- **`static_assert`**: 컴파일 타임 제약 검증
  ```cpp
  static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes for OpenGL");
  ```
- **user-defined literals**: 각도 표현을 직관적으로
  ```cpp
  constexpr float operator""_deg(long double d) { return d * PI / 180.0; }
  auto angle = 45.0_deg;
  ```
- **`[[nodiscard]]`**: 변환 함수의 반환값 무시 방지

**결과물**: `core/math/Transform.hpp`, `core/math/Quat.hpp` + doctest 테스트

---

#### Chapter 04: 렌더링 추상화 레이어

**학습 주제**: 그래픽스 API 추상화 패턴, OpenGL을 감싸는 렌더러 설계

**내용**:
- 추상 인터페이스 설계 (API 독립적)
  ```cpp
  // 추상 타입들
  struct VertexBuffer;
  struct IndexBuffer;
  struct VertexArray;
  struct ShaderProgram;
  struct Texture2D;

  // 추상 렌더러
  class Renderer {
  public:
      virtual void init() = 0;
      virtual void clear(const Vec4& color) = 0;
      virtual void drawIndexed(const VertexArray& vao, uint32_t count) = 0;
      virtual void setViewport(int x, int y, int w, int h) = 0;
      // ...
  };
  ```
- OpenGL 백엔드 구현
  - `GLVertexBuffer`, `GLIndexBuffer`, `GLVertexArray`
  - `GLShaderProgram` (기존 Shader 클래스 리팩터링)
  - `GLTexture2D`
  - `GLRenderer`
- Vertex Layout 시스템
  - `VertexLayout` 클래스로 attribute 구성 선언적으로 정의
  - `{Position, Float3}, {Normal, Float3}, {TexCoord, Float2}` 형태

**핵심 개념**:
- 왜 추상화하는가? (Desktop OpenGL ↔ WebGL 2.0 동일 코드)
- OpenGL ES 3.0 서브셋으로 제한하는 이유 (WebGL 2.0 = ES 3.0)
- 인터페이스 vs 타입 이레이저 패턴
- RAII로 GPU 리소스 관리 (소멸자에서 해제)
- `std::unique_ptr`로 렌더러 백엔드 교체
- GLSL 300 es를 기본으로, Desktop에서는 `#version 330 core` 자동 변환

**C++ 학습 포인트**:
- **가상 함수와 추상 클래스**: `virtual`, `= 0`, `override`, `final`
  ```cpp
  class Renderer {
  public:
      virtual ~Renderer() = default;
      virtual void drawIndexed(const VertexArray& vao, uint32_t count) = 0;
  };
  class GLRenderer final : public Renderer { /* override */ };
  ```
- **`std::unique_ptr`**: 렌더러 백엔드의 소유권 관리
  ```cpp
  std::unique_ptr<Renderer> renderer = std::make_unique<GLRenderer>();
  ```
- **RAII (Resource Acquisition Is Initialization)**
  - GPU 리소스(VBO, VAO, 텍스처)를 생성자에서 할당, 소멸자에서 해제
  - 복사 금지, 이동만 허용 → move semantics
- **move semantics**: `std::move`, 이동 생성자/대입 연산자
  - GPU 핸들은 복사 불가 → `delete` copy, `default` move

**결과물**: `renderer/` 모듈 완성, Desktop + WASM 모두에서 삼각형/큐브 렌더링

---

#### Chapter 05: 윈도우, 입력, 게임 루프

**학습 주제**: SDL3 기반 플랫폼 레이어, 이벤트 시스템, Emscripten 호환 게임 루프

**내용**:
- `Window` 클래스
  - SDL3 래핑: `SDL_CreateWindow` + `SDL_GL_CreateContext`
  - SDL_Event 폴링 → 내부 이벤트 큐 변환
- `Input` 시스템
  - 키보드 상태 (pressed/released/held)
  - 마우스 위치, 델타, 버튼 상태
  - `SDL_GetKeyboardState` + 이벤트 기반 이중 접근
- 이벤트 시스템
  - `Event` 타입 (KeyEvent, MouseEvent, WindowEvent)
  - `EventDispatcher` - 타입별 핸들러 등록
  - `std::variant` + `std::visit` 활용
- **Emscripten 호환 게임 루프**
  ```cpp
  // 핵심: 한 프레임을 함수로 분리
  void oneFrame() {
      processInput();
      update(fixedDeltaTime);
      render(interpolation);
  }

  // Desktop
  while (running) { oneFrame(); }

  // WASM
  emscripten_set_main_loop(oneFrame, 0, true);
  ```
  - `#ifdef __EMSCRIPTEN__` 분기
  - 고정 시간 스텝의 필요성 (물리 안정성)
  - 보간(interpolation)으로 부드러운 렌더링

**핵심 개념**:
- 왜 WASM에서 `while(true)` 루프를 쓸 수 없는가 (브라우저 메인 스레드 블로킹)
- 고정 vs 가변 시간 스텝, 나선형 지옥(spiral of death)
- `std::variant`를 이벤트 시스템에 활용하는 모던 C++ 패턴
- 입력 지연(input lag)과 반응성의 트레이드오프

**C++ 학습 포인트**:
- **`std::variant`로 타입 안전한 이벤트**
  ```cpp
  struct KeyEvent { int key; bool pressed; };
  struct MouseMoveEvent { float x, y, dx, dy; };
  struct WindowResizeEvent { int width, height; };
  using Event = std::variant<KeyEvent, MouseMoveEvent, WindowResizeEvent>;
  ```
- **`std::visit` + overloaded 패턴**
  ```cpp
  template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
  std::visit(overloaded{
      [](const KeyEvent& e) { /* ... */ },
      [](const MouseMoveEvent& e) { /* ... */ },
      [](auto&) { /* fallback */ }
  }, event);
  ```
- **lambda 표현식**: 캡처, `mutable`, generic lambda (`auto` 매개변수)
- **`std::function`**: 이벤트 핸들러 콜백 저장
- **`std::chrono`**: 고해상도 타이머, `steady_clock`, `duration_cast`

**결과물**: Desktop + WASM 둘 다 동작하는 윈도우 데모 (키 입력으로 배경색 변경)

---

### Phase B: 3D 월드 구성 (Ch.06 ~ 10)

프로시저럴 메시와 씬 관리로 사격장 월드를 구축한다.

---

#### Chapter 06: 프로시저럴 메시 생성

**학습 주제**: 기본 도형의 정점/인덱스 데이터 생성, 법선 벡터 계산

**내용**:
- 기본 도형 생성기 구현
  - `MeshGenerator::createBox(width, height, depth)`
  - `MeshGenerator::createSphere(radius, segments, rings)`
  - `MeshGenerator::createCylinder(radius, height, segments)`
  - `MeshGenerator::createPlane(width, depth, subdivisions)`
- 각 도형의 수학적 유도
  - 구: 구면 좌표계 → 직교 좌표계
  - 실린더: 원의 파라메트릭 방정식
- 법선 벡터 자동 계산
  - face normal vs vertex normal
  - smooth shading을 위한 정점 법선 평균
- `Mesh` 클래스: 정점 데이터 + 인덱스 + 렌더링

**핵심 개념**:
- 삼각형 와인딩 순서(winding order)와 face culling
- 법선 벡터가 라이팅에서 하는 역할 (다음 챕터 예고)
- 인덱스 공유와 메모리 효율

**C++ 학습 포인트**:
- **`std::vector` 메모리 모델**: capacity vs size, `reserve()`, 재할당 비용
- **`emplace_back` vs `push_back`**: 불필요한 복사 제거
- **`std::span` (C++20)**: 소유하지 않는 연속 메모리 뷰
  ```cpp
  void uploadToGPU(std::span<const Vertex> vertices) { /* ... */ }
  ```
- **structured bindings (구조적 바인딩)**
  ```cpp
  for (auto& [position, normal, texcoord] : vertices) { /* ... */ }
  ```

**결과물**: 구, 박스, 실린더, 평면을 렌더링하는 장면

---

#### Chapter 07: 라이팅 기초

**학습 주제**: Phong 라이팅 모델, 셰이더에서의 조명 계산

**내용**:
- Phong 라이팅 모델
  - Ambient: 전역 간접광 근사
  - Diffuse: Lambert의 코사인 법칙 (N·L)
  - Specular: 반사광 (R·V)^shininess
- 광원 타입
  - Directional Light (태양) - 사격장 주 조명
  - Point Light - 보조 조명 (선택)
- 셰이더 구현
  - vertex shader에서 월드 좌표 변환
  - fragment shader에서 per-pixel 라이팅
- Material 구조체
  ```cpp
  struct Material {
      Vec3 ambient;
      Vec3 diffuse;
      Vec3 specular;
      float shininess;
  };
  ```

**핵심 개념**:
- 조명 계산을 월드 스페이스 vs 뷰 스페이스에서 하는 차이
- 왜 법선 행렬(normal matrix)이 필요한가
- 감마 보정의 필요성

**C++ 학습 포인트**:
- **`std::array`**: 고정 크기 광원 배열 (최대 광원 수 제한)
  ```cpp
  static constexpr int MAX_LIGHTS = 4;
  std::array<Light, MAX_LIGHTS> lights;
  ```
- **`alignas`**: GPU 유니폼 버퍼의 메모리 정렬 요구사항
  ```cpp
  struct alignas(16) LightData {  // std140 레이아웃 호환
      Vec3 position; float _pad1;
      Vec3 color;    float intensity;
  };
  ```
- **aggregate types**: Material, Light를 패딩 없이 깔끔하게 설계

**결과물**: 조명이 적용된 기본 도형 장면

---

#### Chapter 08: 엔티티와 씬 관리

**학습 주제**: 게임 오브젝트 관리, 컴포넌트 패턴, 씬 그래프

**내용**:
- 간단한 Entity 시스템
  ```cpp
  struct Transform {
      Vec3 position;
      Quat rotation;
      Vec3 scale;
      Mat4 getModelMatrix() const;
  };

  struct Entity {
      std::string name;
      Transform transform;
      Mesh* mesh;
      Material material;
      bool active;
  };
  ```
- Scene 클래스
  - 엔티티 생성/삭제/조회
  - 렌더링 순회: 모든 활성 엔티티를 그리기
  - 광원 관리
- 본격적인 ECS가 아닌, 게임에 필요한 만큼만의 구조
  - 오버엔지니어링 방지: "지금 필요한 것"에 집중
  - 추후 필요 시 컴포넌트 분리 가능하도록 열어둠

**핵심 개념**:
- 상속 기반 vs 컴포넌트 기반 게임 오브젝트 모델
- 왜 full ECS가 이 프로젝트에서는 과하다고 할 수 있는지
- 업데이트 순서가 게임 로직에 미치는 영향

**C++ 학습 포인트**:
- **소유권과 스마트 포인터**
  ```cpp
  class Scene {
      std::vector<std::unique_ptr<Entity>> entities;  // Scene이 소유
  public:
      Entity& createEntity(std::string name);          // 참조 반환 (소유권 이전 X)
  };
  ```
- **`std::optional`**: "있을 수도 없을 수도" 있는 값
  ```cpp
  std::optional<Entity&> findEntity(std::string_view name);
  ```
- **Rule of Zero**: 스마트 포인터를 쓰면 소멸자/복사/이동을 직접 안 써도 됨
- **`std::ranges::views::filter` (C++20)**: 활성 엔티티만 순회
  ```cpp
  for (auto& e : entities | std::views::filter(&Entity::isActive)) { /* render */ }
  ```

**결과물**: 다수의 오브젝트가 배치된 씬 렌더링

---

#### Chapter 09: 스나이퍼 카메라 시스템

**학습 주제**: 1인칭 고정 카메라, 미세 오프셋, 스코프 뷰

**내용**:
- 스나이퍼 카메라 설계
  - 기존 자유 이동 카메라와 다른 점
  - 고정 위치 + 미세 오프셋(가늠자 = 머리 움직임)
  - 제한된 시야각(FOV) = 스코프 배율
  ```cpp
  class SniperCamera {
      Vec3 basePosition;     // 고정 위치
      Vec3 headOffset;       // 가늠자 오프셋 (키보드 → 추후 얼굴)
      Vec2 gazeDirection;    // 가늠쇠 방향 (마우스 → 추후 시선)
      float scopeZoom;       // 스코프 배율 (FOV 조절)
  };
  ```
- 뷰 행렬 계산
  - `basePosition + headOffset` → 카메라 위치
  - `gazeDirection` → 레티클 위치 → 조준 방향
- 입력 매핑 (프로토타입)
  - 키보드 WASD → headOffset (느린 속도, 제한된 범위)
  - 마우스 이동 → gazeDirection (스코프 내 레티클)

**핵심 개념**:
- 가늠자-가늠쇠 원리의 수학적 모델링
- 시차(parallax): 카메라 위치 변화에 따른 깊이별 상대 이동
- FOV와 스코프 배율의 관계

**C++ 학습 포인트**:
- **상속 vs 합성**: 기존 Camera를 상속할까, 내부에 포함할까?
  - 합성 선호: SniperCamera "has-a" 뷰 행렬 계산 로직
- **`std::clamp` (C++17)**: 오프셋 범위 제한
  ```cpp
  headOffset.x = std::clamp(headOffset.x, -0.15f, 0.15f);
  ```
- **`std::lerp` (C++20)**: 부드러운 보간
  ```cpp
  current = std::lerp(current, target, smoothFactor * dt);
  ```

**결과물**: 키보드로 시점 이동, 마우스로 레티클 이동하는 스코프 뷰

---

#### Chapter 10: 사격장 씬 구성

**학습 주제**: 게임 레벨 설계, 공간 배치, 거리감 표현

**내용**:
- 사격장 레이아웃 설계
  ```
  [플레이어 위치]
      |
      |── 근거리 (10m): 타겟 3개, 장애물 없음
      |── 중거리 (30m): 타겟 3개, 기둥 장애물 1~2개
      |── 원거리 (60m): 타겟 3개, 기둥 장애물 2~3개
  ```
- 타겟 오브젝트
  - 원형 과녁 (동심원 구 또는 디스크)
  - 피격 상태 표시 (색 변경, 쓰러짐)
- 장애물 오브젝트
  - 기둥(실린더), 벽(박스) 등
  - 반투명이 아닌 불투명 → 가려짐 효과
- 배경 요소
  - 바닥 평면 (격자 텍스처 또는 단색)
  - 하늘 (단색 또는 그래디언트)
- 거리에 따른 시각 효과
  - 원거리 타겟이 작게 보이는 것은 perspective가 자동 처리
  - 안개(fog) 효과로 거리감 강화 (선택)

**핵심 개념**:
- 월드 스케일: 미터 단위 사용의 중요성
- 장애물이 패럴랙스 효과를 만드는 원리
- 깊이 버퍼 정밀도와 원거리 렌더링

**C++ 학습 포인트**:
- **designated initializers (C++20)**: 씬 설정을 선언적으로
  ```cpp
  auto target = Target{
      .position = {0.0f, 1.5f, -30.0f},
      .radius = 0.5f,
      .points = 100,
      .distance = Distance::Mid
  };
  ```
- **`constexpr` 배열**: 컴파일 타임에 레벨 데이터 정의
  ```cpp
  constexpr std::array targetPositions = {
      Vec3{-2, 1.5, -10}, Vec3{0, 1.5, -10}, Vec3{2, 1.5, -10}, // 근거리
      // ...
  };
  ```
- **`enum class`**: 거리 단계를 타입 안전하게
  ```cpp
  enum class Distance : uint8_t { Near, Mid, Far };
  ```

**결과물**: 9개 타겟 + 장애물이 배치된 사격장 씬

---

### Phase C: 게임 메카닉 (Ch.11 ~ 15)

사격, 충돌, 점수 등 게임의 핵심 플레이를 구현한다.

---

#### Chapter 11: 레이캐스팅과 탄도학

**학습 주제**: 광선-오브젝트 교차 검사, 간이 탄도 시뮬레이션

**내용**:
- Ray 구조체와 레이캐스팅
  ```cpp
  struct Ray {
      Vec3 origin;     // 카메라 위치
      Vec3 direction;  // 조준 방향 (정규화)
  };
  ```
- 조준선 계산
  - 카메라 위치(가늠자) → 레티클 월드 좌표(가늠쇠) → 방향 벡터
  - 화면 좌표 → 월드 좌표 역변환 (unproject)
- 탄도 시뮬레이션 (선택적 복잡도)
  - Level 1: 즉발(hitscan) - 레이가 즉시 목표에 도달
  - Level 2: 탄속 - 총알 오브젝트가 일정 속도로 이동
  - Level 3: 중력 낙차 - 포물선 궤적 (원거리에서 의미)
- 총알 궤적 시각화
  - 발사 시 트레이서(광선) 이펙트
  - 라인 렌더링으로 궤적 표시

**핵심 개념**:
- 화면 좌표 ↔ 월드 좌표 변환 (project/unproject)
- 레이-구 교차, 레이-박스 교차의 수학적 유도
- hitscan vs projectile 방식의 장단점

**C++ 학습 포인트**:
- **`std::optional`로 교차 결과 표현**
  ```cpp
  [[nodiscard]] std::optional<HitResult> castRay(const Ray& ray, const Scene& scene);
  // 사용 측
  if (auto hit = castRay(ray, scene)) {
      handleHit(hit->entity, hit->point, hit->distance);
  }
  ```
- **`[[nodiscard]]`**: 레이캐스트 결과를 무시하면 컴파일 경고
- **structured bindings 심화**: HitResult 분해
  ```cpp
  auto [entity, point, normal, distance] = *hit;
  ```

**결과물**: 마우스 클릭으로 사격, 궤적 시각화

---

#### Chapter 12: 충돌 감지

**학습 주제**: AABB, 구-구 충돌, 레이-AABB 교차

**내용**:
- 바운딩 볼륨
  - AABB (Axis-Aligned Bounding Box) 구현
  - Bounding Sphere 구현
  - 오브젝트에 자동 바운딩 볼륨 생성
- 충돌 검사 함수들
  - `intersect(Ray, AABB)` → 탄환과 장애물
  - `intersect(Ray, Sphere)` → 탄환과 타겟
  - `intersect(Ray, Plane)` → 탄환과 바닥
- 충돌 응답
  - 타겟 피격 → 피격 이벤트 발생
  - 장애물 피격 → 무시 또는 착탄점 표시
- 장애물 차단 로직
  - 레이캐스트 시 가장 가까운 교차점 선택
  - 장애물이 타겟보다 앞에 있으면 차단

**핵심 개념**:
- 브로드 페이즈 vs 내로우 페이즈 충돌 검사
- t 파라미터: 레이 상의 교차 위치
- 부동소수점 오차와 epsilon 처리

**C++ 학습 포인트**:
- **C++20 concepts로 제네릭 충돌 검사**
  ```cpp
  template<typename Shape>
  concept Intersectable = requires(const Shape& s, const Ray& r) {
      { intersect(r, s) } -> std::same_as<std::optional<HitResult>>;
  };

  // 어떤 도형이든 intersect()만 구현하면 사용 가능
  template<Intersectable S>
  void checkCollision(const Ray& ray, const S& shape);
  ```
- **`requires` 절**: 함수 오버로딩 대신 제약 조건으로 분기
- **concepts vs 가상 함수**: 컴파일 타임 다형성 vs 런타임 다형성 비교
  - 충돌 검사는 타입이 컴파일 타임에 결정 → concepts가 적합
  - 렌더러 백엔드는 런타임에 교체 가능 → virtual이 적합

**결과물**: 타겟 피격 판정, 장애물 차단이 동작하는 사격

---

#### Chapter 13: 패럴랙스와 엿보기 메카닉

**학습 주제**: 시차 효과, 오클루전 판정, 게임 메카닉으로서의 시점 이동

**내용**:
- 패럴랙스 효과 원리
  - 카메라(가늠자) 위치 변화 → 가까운 물체/먼 물체의 상대 이동량 차이
  - 수학적으로: 가까운 물체일수록 시점 이동에 민감
- 엿보기 메카닉 구현
  - 장애물 뒤에 숨겨진 타겟
  - 키보드로 headOffset 변경 → 카메라 이동 → 장애물 뒤가 보임
  - 시각적 피드백: 엿보기 중일 때 스코프 가장자리 변화
- headOffset 제한
  - 현실적 머리 움직임 범위 (좌우 ±15cm, 상하 ±10cm)
  - 부드러운 이동 (lerp 적용)
  - 되돌아가기: 키 뗐을 때 천천히 원위치
- 디버그 시각화
  - 탑뷰에서 카메라-타겟-장애물 관계 표시
  - 시야선(line of sight) 시각화

**핵심 개념**:
- 패럴랙스가 깊이 인식에 미치는 영향
- 게임 디자인: 제한된 입력으로 공간 탐색의 재미
- 오클루전 쿼리 vs 레이캐스트 기반 가시성 판단

**C++ 학습 포인트**:
- **고차 함수와 lambda**: 이징(easing) 함수를 lambda로 정의
  ```cpp
  using EaseFunc = std::function<float(float)>;
  EaseFunc easeOutQuad = [](float t) { return t * (2 - t); };
  headOffset = std::lerp(current, target, easeOutQuad(t));
  ```
- **`std::invoke`**: 다양한 callable을 균일하게 호출
- 함수를 값으로 다루는 함수형 프로그래밍 패턴

**결과물**: 머리 이동으로 장애물 뒤를 엿보는 핵심 메카닉 완성

---

#### Chapter 14: HUD와 스코프 오버레이

**학습 주제**: 2D 오버레이 렌더링, 포스트 프로세싱 기초, UI 렌더링

**내용**:
- 2D 렌더링 파이프라인
  - 직교 투영으로 HUD 렌더링
  - 깊이 테스트 비활성화하여 항상 최상위 표시
- 스코프 오버레이
  - 원형 스코프 마스크 (스텐실 버퍼 또는 알파)
  - 십자선(crosshair) 렌더링 - 레티클 위치에 표시
  - 비네트 효과 (가장자리 어두움)
- 게임 정보 HUD
  - 남은 탄약 수
  - 현재 점수
  - 피격 타겟 수 / 전체 타겟 수
- 비트맵 폰트 렌더링
  - 텍스처 아틀라스 기반 문자 렌더링
  - 간단한 `drawText(x, y, text)` 함수

**핵심 개념**:
- 3D 씬 위에 2D를 합성하는 렌더링 순서
- 스텐실 버퍼의 용도와 활용법
- 텍스처 아틀라스와 UV 매핑

**C++ 학습 포인트**:
- **`std::string_view`**: 문자열 복사 없이 텍스트 전달
  ```cpp
  void drawText(Vec2 pos, std::string_view text, float scale = 1.0f);
  ```
- **`std::format` (C++20)**: 타입 안전한 문자열 포맷팅
  ```cpp
  auto scoreText = std::format("Score: {:04d}", score);
  auto timerText = std::format("Time: {:.1f}s", remainingTime);
  ```
- **`std::span`**: HUD 정점 데이터를 소유 없이 전달

**결과물**: 스코프 뷰 + 십자선 + 점수 표시 HUD

---

#### Chapter 15: 게임 스테이트와 점수 시스템

**학습 주제**: 유한 상태 기계, 게임 흐름 관리, 점수 산출

**내용**:
- 게임 상태 머신
  ```
  [Ready] → 키 입력 → [Playing] → 모든 타겟 클리어 → [Result]
                          ↓                                  ↓
                     타이머 종료 ──────────────────────────→ [Result]
                                                             ↓
                                                     키 입력 → [Ready]
  ```
- 점수 시스템
  - 거리별 기본 점수 (원거리 > 근거리)
  - 명중 위치 보너스 (타겟 중심 근접도)
  - 소요 시간 보너스
  - 연속 명중(streak) 보너스
- 타이머
  - 제한 시간 (60초 등)
  - HUD에 남은 시간 표시
- 사운드 (선택)
  - 간단한 사격음, 피격음
  - SDL3 Audio API 활용 (추가 의존성 없음)

**핵심 개념**:
- 상태 머신 패턴과 `std::variant` 활용
- 게임 밸런스: 점수 가중치 튜닝
- 게임 흐름(flow)과 피드백 루프

**C++ 학습 포인트**:
- **`std::variant` 기반 상태 머신** (Ch.05의 이벤트 시스템을 상태에 적용)
  ```cpp
  struct ReadyState { };
  struct PlayingState { float timer; int score; };
  struct ResultState { int finalScore; float totalTime; };
  using GameState = std::variant<ReadyState, PlayingState, ResultState>;

  GameState state = ReadyState{};
  ```
- **`std::visit` + overloaded 패턴 심화**: 상태별 update/render 분기
  ```cpp
  std::visit(overloaded{
      [&](ReadyState&)   { renderReadyScreen(); },
      [&](PlayingState& s) { s.timer -= dt; renderGame(s); },
      [&](ResultState& s)  { renderResult(s); }
  }, state);
  ```
- **`std::chrono`**: 게임 타이머, 경과 시간 측정
  ```cpp
  using Clock = std::chrono::steady_clock;
  auto startTime = Clock::now();
  float elapsed = std::chrono::duration<float>(Clock::now() - startTime).count();
  ```

**결과물**: 시작 → 플레이 → 결과 흐름이 동작하는 완성된 프로토타입

---

## Milestone 2: 리소스와 비주얼 개선 (Chapter 16 ~ 22)

> 프로토타입에 시각적 품질을 입힌다. 에셋 매니징, 고급 렌더링 기법을 학습한다.

---

#### Chapter 16: 리소스 매니저

**학습 주제**: 에셋 로딩 아키텍처, 캐싱, 핸들 시스템

**내용**:
- `ResourceManager` 설계
  - 타입별 리소스 캐시 (`std::unordered_map`)
  - 경로 기반 중복 로딩 방지
  - 핸들(Handle) 기반 참조 - 직접 포인터 대신 ID 사용
- 셰이더 매니저: 셰이더 프로그램 로딩/캐싱
- 텍스처 매니저: 이미지 로딩/밉맵 생성/캐싱
- 메시 매니저: 프로시저럴 + 파일 기반 메시

**핵심 개념**:
- 핸들 vs 포인터: 안전성과 유연성
- 리소스 라이프사이클과 메모리 관리
- 비동기 로딩의 필요성 (이 단계에서는 동기로 시작)

**C++ 학습 포인트**:
- **type erasure**: 다양한 리소스 타입을 균일하게 관리
- **`std::any`**: 타입을 지운 리소스 저장소 (간단한 접근)
- **CRTP (Curiously Recurring Template Pattern)**: 리소스 베이스 클래스
  ```cpp
  template<typename Derived>
  class Resource {
  public:
      using Handle = uint32_t;
      static Handle create(/* ... */);
  };
  class Texture : public Resource<Texture> { /* ... */ };
  ```
- **handle/generation 패턴**: 댕글링 포인터 방지
  ```cpp
  struct ResourceHandle {
      uint32_t index;       // 배열 인덱스
      uint32_t generation;  // 재사용 감지용 세대 번호
  };
  ```

**결과물**: 중앙화된 리소스 관리 시스템

---

#### Chapter 17: 모델 로딩

**학습 주제**: 3D 모델 파일 포맷 이해, 간단한 파서 구현

**내용**:
- OBJ 포맷 파서 직접 구현
  - vertex, normal, texcoord, face 파싱
  - MTL(material) 파일 파싱
  - 삼각화(triangulation) 처리
- 총 모델, 타겟 모델 로딩
  - 무료 에셋 사이트에서 간단한 모델 확보
  - 또는 Blender로 간단한 모델 직접 제작
- 메시 최적화
  - 중복 정점 제거 (인덱싱)
  - 바운딩 볼륨 자동 계산

**핵심 개념**:
- OBJ 포맷의 구조 (텍스트 기반, 간단한 파싱)
- 정점 인덱싱과 GPU 효율성
- 모델 좌표계 변환 (Z-up vs Y-up)

**C++ 학습 포인트**:
- **`std::from_chars`**: `stof`/`stoi` 대신 로케일 독립적이고 빠른 숫자 파싱
  ```cpp
  float val;
  auto [ptr, ec] = std::from_chars(begin, end, val);
  if (ec == std::errc{}) { /* 성공 */ }
  ```
- **`std::string_view`로 제로카피 파싱**: 한 줄을 잘라서 토큰화
- **`std::ranges` 파이프라인 (C++20)**: 파일 파싱을 선언적으로
  ```cpp
  auto tokens = line
      | std::views::split(' ')
      | std::views::transform(toStringView)
      | std::views::filter(notEmpty);
  ```

**결과물**: 외부 모델을 로딩하여 렌더링

---

#### Chapter 18: 텍스처와 머터리얼 고급

**학습 주제**: 텍스처 매핑 심화, 멀티텍스처, 기본 PBR 개념

**내용**:
- 텍스처 기법들
  - Diffuse map: 기본 색상
  - Specular map: 반사율 제어
  - Normal map: 표면 디테일 (법선 맵핑)
- Normal Mapping 구현
  - 탄젠트 스페이스(tangent space) 이해
  - TBN 행렬 계산
  - fragment shader에서 법선 맵 적용
- 머터리얼 시스템
  - `Material` 클래스에 텍스처 슬롯 추가
  - 셰이더 유니폼 자동 바인딩

**핵심 개념**:
- 탄젠트 스페이스가 필요한 이유
- 노말 맵으로 저폴리곤 메시의 디테일을 높이는 원리
- 텍스처 유닛과 바인딩

**C++ 학습 포인트**:
- **`enum class` 심화**: 텍스처 슬롯을 타입 안전하게
  ```cpp
  enum class TextureSlot : uint8_t { Diffuse, Specular, Normal, Count };
  ```
- **`std::bitset`**: 머터리얼의 활성 텍스처 플래그
  ```cpp
  std::bitset<static_cast<size_t>(TextureSlot::Count)> activeSlots;
  activeSlots.set(static_cast<size_t>(TextureSlot::Normal));
  ```
- **scoped enumerations**: C 스타일 enum과의 차이, 타입 안전성

**결과물**: 노말 맵이 적용된 사격장 환경

---

#### Chapter 19: 파티클 시스템

**학습 주제**: GPU 기반 파티클, 빌보딩, 이펙트 디자인

**내용**:
- 파티클 시스템 아키텍처
  ```cpp
  struct Particle {
      Vec3 position;
      Vec3 velocity;
      Vec4 color;
      float life;
      float size;
  };

  class ParticleEmitter {
      std::vector<Particle> particles;
      // spawn, update, render
  };
  ```
- 이펙트 유형
  - 총구 화염(muzzle flash) - 짧은 수명, 밝은 색
  - 착탄 먼지(impact dust) - 장애물 피격 시
  - 타겟 파편(debris) - 타겟 피격 시
- 빌보딩
  - 파티클이 항상 카메라를 향하도록 회전
  - 뷰 행렬에서 right/up 벡터 추출
- 알파 블렌딩과 가산 블렌딩

**핵심 개념**:
- 오브젝트 풀링: 동적 할당 최소화
- 빌보딩의 수학 (카메라 방향 벡터 활용)
- 블렌딩 모드와 렌더링 순서 (투명 오브젝트 문제)

**C++ 학습 포인트**:
- **object pool 패턴**: 파티클을 매 프레임 new/delete 하지 않기
  ```cpp
  class ParticlePool {
      std::vector<Particle> pool;  // 고정 크기, 미리 할당
      size_t activeCount = 0;
      // spawn: activeCount++, kill: swap with last + activeCount--
  };
  ```
- **placement new**: 이미 할당된 메모리에 객체 생성
- **`std::pmr` (다형적 메모리 리소스)**: 커스텀 할당기의 표준화된 인터페이스
  ```cpp
  std::pmr::monotonic_buffer_resource arena(1024 * 64);
  std::pmr::vector<Particle> particles(&arena);
  ```
- **`std::ranges`**: 활성 파티클만 업데이트
  ```cpp
  auto alive = particles | std::views::take(activeCount);
  for (auto& p : alive) { p.position += p.velocity * dt; }
  ```

**결과물**: 사격 시 이펙트가 터지는 시각적 피드백

---

#### Chapter 20: 포스트 프로세싱

**학습 주제**: 프레임버퍼 오프스크린 렌더링, 화면 효과

**내용**:
- 프레임버퍼 오브젝트(FBO)
  - 오프스크린 렌더링 개념
  - 컬러/깊이/스텐실 어태치먼트
- 스코프 효과
  - 비네트(vignette): 가장자리 어두움
  - 피사계 심도(DOF) 간이 구현: 가장자리 블러
  - 컬러 그레이딩: 스코프 특유의 색조
- 화면 전체 효과
  - 피격 시 화면 흔들림(screen shake)
  - 사격 시 짧은 화이트 플래시
- 풀스크린 쿼드 렌더링 기법

**핵심 개념**:
- FBO의 원리와 렌더 패스(render pass) 개념
- 포스트 프로세싱 체인 설계
- 커널 기반 이미지 필터 (블러, 샤프닝)

**C++ 학습 포인트**:
- **함수 합성**: 포스트 프로세싱 패스를 체인으로 연결
  ```cpp
  using PostEffect = std::function<void(Framebuffer& src, Framebuffer& dst)>;
  std::vector<PostEffect> postPipeline = { vignette, bloom, colorGrade };
  for (auto& effect : postPipeline) { effect(ping, pong); std::swap(ping, pong); }
  ```
- **`std::function` 활용**: 런타임에 이펙트 체인 변경 가능

**결과물**: 스코프 비네트 + 피격 효과가 적용된 게임

---

#### Chapter 21: 환경 렌더링

**학습 주제**: 스카이박스, 지형, 환경 조명

**내용**:
- 스카이박스
  - 큐브맵 텍스처 로딩
  - 깊이 버퍼 트릭 (가장 뒤에 렌더링)
- 간단한 지형
  - 하이트맵 기반 지형 생성
  - 텍스처 스플래팅 (풀/흙/바위 블렌딩)
  - 또는 심플한 평면 + 텍스처
- 그림자 (선택)
  - 기본 섀도우 맵핑 개념
  - 깊이 맵 렌더링 → 그림자 판정
  - 사격장의 장애물 그림자

**핵심 개념**:
- 큐브맵 텍스처의 구조와 샘플링
- 멀티패스 렌더링 (섀도우 맵 → 메인 렌더)
- 그림자 아크네(shadow acne)와 피터 팬 문제

**C++ 학습 포인트**:
- **`std::filesystem`**: 에셋 경로 관리, 플랫폼 독립적 경로 처리
  ```cpp
  namespace fs = std::filesystem;
  auto texPath = fs::path("assets") / "textures" / "skybox";
  for (auto& entry : fs::directory_iterator(texPath)) { /* 큐브맵 로딩 */ }
  ```
- 주의: WASM에서는 가상 파일시스템이므로 `std::filesystem` 일부 기능 제한

**결과물**: 하늘, 지형, (선택) 그림자가 있는 완성된 사격장

---

#### Chapter 22: 사운드 시스템

**학습 주제**: SDL3 Audio API 활용, 게임 오디오 기초

**내용**:
- SDL3 오디오 시스템
  - `SDL_OpenAudioDevice`, `SDL_AudioStream`
  - WAV 로딩 (`SDL_LoadWAV`) + 커스텀 파서 비교
  - 오디오 콜백 vs 스트림 방식
- 게임 사운드 이벤트
  - 사격음
  - 타겟 피격음
  - 빈 탄창(empty magazine) 소리
  - 배경 환경음 (바람 등)
- 사운드 믹싱
  - 여러 사운드 동시 재생 (간단한 믹서 구현)
  - 볼륨 제어, 거리 기반 감쇄
- WASM 호환
  - 브라우저 오디오 정책: 사용자 인터랙션 후 재생 시작
  - SDL3가 내부적으로 Web Audio API로 매핑

**핵심 개념**:
- 오디오 버퍼와 스트리밍의 차이
- 게임에서 사운드 피드백의 중요성
- WAV 파일 포맷 구조 (직접 파싱 시도 가능)
- SDL3 하나로 Desktop/Web 오디오를 통합하는 이점

**C++ 학습 포인트**:
- **`std::atomic`**: 오디오 스레드와 게임 스레드 간 상태 공유
  ```cpp
  std::atomic<bool> shouldPlayShot{false};
  // 게임 스레드: shouldPlayShot.store(true);
  // 오디오 콜백: if (shouldPlayShot.exchange(false)) { /* 재생 */ }
  ```
- **콜백 패턴**: SDL 오디오 콜백에 `std::function` 또는 C 호환 함수 포인터
- **lock-free 기초**: 오디오는 실시간이므로 mutex 사용을 최소화해야 하는 이유

**결과물**: 사격감이 살아있는 오디오 피드백 (Desktop + Web)

---

## Milestone 3: 시선 추적 연동 (Chapter 23 ~ 28)

> 키보드/마우스 입력을 얼굴/시선 추적으로 대체한다.

---

#### Chapter 23: 시선 추적 기술 개관

**학습 주제**: 시선 추적의 원리, 사용 가능한 모델/라이브러리 조사

**내용**:
- 시선 추적 기술 분류
  - 하드웨어 기반: Tobii, Pupil Labs 등
  - 소프트웨어(웹캠) 기반: MediaPipe, OpenFace, GazeML 등
- 우리가 필요한 것
  - **얼굴 위치/방향** → 가늠자 (headOffset)
  - **시선 방향** → 가늠쇠 (gazeDirection)
- 주요 라이브러리 비교
  | 라이브러리 | 얼굴 위치 | 시선 방향 | 무료 | 비고 |
  |-----------|----------|----------|------|------|
  | MediaPipe Face Mesh | O | △ | O | 468 랜드마크 |
  | OpenFace | O | O | O | 학술용, 정확도 높음 |
  | dlib | O | X | O | 68 랜드마크 |
  | Tobii SDK | O | O | △ | 전용 하드웨어 필요 |
  | GazeML/MPIIGaze | X | O | O | 시선만 특화 |
- 접근 전략
  - Phase 1: MediaPipe + 간단한 시선 추정
  - Phase 2: (선택) 직접 CNN 모델 학습

**핵심 개념**:
- appearance-based vs model-based 시선 추적
- 얼굴 랜드마크 기반 head pose estimation 원리
- PnP(Perspective-n-Point) 알고리즘

**결과물**: 기술 조사 정리 문서, 라이브러리 선택

---

#### Chapter 24: 웹캠 입력과 얼굴 추적

**학습 주제**: 카메라 캡처 (SDL3 Camera / OpenCV), MediaPipe 얼굴 추적 연동

**내용**:
- 웹캠 입력 파이프라인
  - **Desktop**: SDL3 Camera API (`SDL_OpenCamera`) 또는 OpenCV
  - **WASM**: `getUserMedia` (WebRTC) → SDL3가 내부 매핑 또는 JS interop
  - 별도 스레드에서 캡처/처리 (게임 루프 블록 방지, WASM에서는 Web Worker)
- 얼굴 위치/방향 추출
  - MediaPipe Face Mesh → 468개 랜드마크
  - 주요 랜드마크에서 머리 위치/방향 계산
  - solvePnP로 head pose estimation
- 좌표 변환
  - 카메라 좌표 → 게임 좌표 매핑
  - 스무딩: 노이즈 제거를 위한 이동 평균/칼만 필터
  - 캘리브레이션: 기준 위치 설정

**핵심 개념**:
- 멀티스레딩: 카메라 캡처와 게임 루프 분리
- 칼만 필터의 원리와 노이즈 제거
- Head Pose Estimation의 수학 (PnP, 로드리게스 변환)

**C++ 학습 포인트**:
- **`std::jthread` (C++20)**: 자동으로 join되는 스레드
  ```cpp
  std::jthread captureThread([](std::stop_token stoken) {
      while (!stoken.stop_requested()) {
          auto frame = captureFrame();
          processFrame(frame);
      }
  });
  // captureThread 소멸 시 자동 stop 요청 + join
  ```
- **`std::stop_token` (C++20)**: 협력적 스레드 취소
- **`std::mutex` + `std::lock_guard`**: 프레임 데이터 공유 보호
- **`std::condition_variable`**: 새 프레임 도착 알림
- WASM에서의 대안: Web Worker + `SharedArrayBuffer`

**결과물**: 얼굴을 움직이면 headOffset이 변하는 데모

---

#### Chapter 25: 시선 방향 추적

**학습 주제**: 눈 랜드마크 분석, 시선 벡터 추정

**내용**:
- 눈 영역 분석
  - 얼굴 랜드마크에서 눈 영역 추출
  - 동공 위치 검출 (눈 랜드마크 기반)
  - 눈 개폐 감지 (EAR: Eye Aspect Ratio)
- 시선 방향 추정
  - 눈 중심 → 동공 오프셋 → 시선 방향
  - 또는 외부 시선 추정 모델 활용
- 깜빡임 감지 → 사격 트리거
  - 양쪽 눈 동시 깜빡임: 사격
  - 한쪽 눈 깜빡임: 사격 취소 또는 재장전
  - 디바운싱: 자연스러운 깜빡임 무시

**핵심 개념**:
- Eye Aspect Ratio (EAR) 계산법
- 시선 추정의 한계와 정확도
- 깜빡임 인식의 임계값 튜닝

**C++ 학습 포인트**:
- **ring buffer**: `std::array` 기반 고정 크기 순환 버퍼로 시선 히스토리 관리
  ```cpp
  template<typename T, size_t N>
  class RingBuffer {
      std::array<T, N> data;
      size_t head = 0, count = 0;
  public:
      void push(T val) { data[head++ % N] = val; count = std::min(count+1, N); }
      T average() const; // 이동 평균 필터
  };
  RingBuffer<Vec2, 10> gazeHistory;  // 최근 10프레임 시선 평균
  ```
- 신호 처리 패턴: 노이즈 제거를 위한 필터를 C++ 제네릭으로

**결과물**: 시선으로 레티클 이동, 깜빡임으로 사격

---

#### Chapter 26: 캘리브레이션 시스템

**학습 주제**: 시선 추적 보정, 사용자별 최적화

**내용**:
- 캘리브레이션 절차 설계
  - 화면 9점 바라보기 → 시선-화면 매핑 학습
  - 얼굴 기준 위치 설정 (중립 자세)
  - 민감도 조절 인터페이스
- 매핑 함수
  - 선형 매핑 → 2차 다항식 매핑
  - 또는 간단한 호모그래피 변환
- 런타임 보정
  - 드리프트 감지 및 자동 보정
  - 재캘리브레이션 단축키

**핵심 개념**:
- 최소자승법(least squares)으로 매핑 함수 피팅
- 호모그래피와 평면 대 평면 변환
- UX: 캘리브레이션이 사용자 경험에 미치는 영향

**결과물**: 게임 시작 전 캘리브레이션 화면

---

#### Chapter 27: 통합과 튜닝

**학습 주제**: 시스템 통합, 레이턴시 최적화, 게임 밸런스

**내용**:
- 입력 소스 통합
  - 키보드/마우스 ↔ 시선 추적 전환 가능
  - 하이브리드 모드 (시선 + 마우스 보정)
- 레이턴시 최적화
  - 카메라 캡처 → 추적 → 렌더링 파이프라인 지연 측정
  - 예측(prediction): 현재 속도로 다음 프레임 위치 예측
  - 스무딩 vs 반응성 밸런스
- 게임 밸런스 조정
  - 시선 추적 정확도를 감안한 타겟 크기 조정
  - 난이도별 설정 (타겟 크기, 시간, 관용도)
  - 플레이테스트와 피드백 반영

**핵심 개념**:
- end-to-end 레이턴시와 체감 반응성
- 예측 알고리즘 (선형 외삽, 칼만 필터 예측)
- 게임 디자인 이터레이션

**C++ 학습 포인트**:
- **`[[likely]]` / `[[unlikely]]` (C++20)**: 핫 패스 최적화 힌트
  ```cpp
  if (hit.has_value()) [[likely]] {
      processHit(*hit);
  }
  ```
- **프로파일링**: `std::chrono`로 구간 측정, 매크로 래퍼
  ```cpp
  #define PROFILE_SCOPE(name) ScopeTimer _timer##__LINE__(name)
  struct ScopeTimer {
      const char* name;
      std::chrono::steady_clock::time_point start;
      ~ScopeTimer() { /* 소요 시간 출력 */ }
  };
  ```
- 최적화 기법: 분기 예측, 캐시 친화적 데이터 배치 (SoA vs AoS)

**결과물**: 시선 추적으로 플레이 가능한 최종 빌드

---

#### Chapter 28: (보너스) 직접 시선 추적 모델 만들기

**학습 주제**: CNN 기반 시선 추정, 데이터 수집, 학습 파이프라인

**내용**:
- 시선 추정 모델 아키텍처
  - 입력: 눈 영역 이미지 (크롭)
  - 출력: 시선 벡터 (x, y) 또는 화면 좌표
  - 간단한 CNN 구조 (LeNet/ResNet 수준)
- 데이터 수집
  - 캘리브레이션 데이터를 학습 데이터로 활용
  - 공개 데이터셋: MPIIGaze, GazeCapture
- 학습 파이프라인
  - PyTorch/TensorFlow로 학습
  - ONNX 변환 → C++ 추론 (ONNX Runtime)
- 기존 라이브러리 대비 성능 비교

**핵심 개념**:
- CNN의 기본 구조 (Conv → Pool → FC)
- 전이 학습(transfer learning)으로 적은 데이터로 학습
- ONNX Runtime을 C++에서 사용하는 방법

**C++ 학습 포인트**:
- **C 라이브러리 RAII 래핑**: ONNX Runtime의 C API를 안전하게 감싸기
  ```cpp
  struct OrtSessionDeleter {
      void operator()(OrtSession* s) { OrtReleaseSession(s); }
  };
  using OrtSessionPtr = std::unique_ptr<OrtSession, OrtSessionDeleter>;
  ```
- **`std::span`으로 텐서 데이터 전달**: 복사 없이 추론 입출력
  ```cpp
  void infer(std::span<const float> input, std::span<float> output);
  ```

**결과물**: 직접 학습한 시선 추적 모델 통합

---

## 부록

### A. 디버깅 도구

- ImGui 통합으로 런타임 변수 조절
- 와이어프레임 모드
- 바운딩 볼륨 시각화
- 레이캐스트 시각화
- FPS/성능 오버레이

### B. 프로파일링과 최적화

- 렌더링 병목: draw call 최소화, 배칭
- CPU 프로파일링: 크로노 기반 구간 측정
- GPU 프로파일링: OpenGL 쿼리

### C. WASM 빌드 레퍼런스

이 코스는 처음부터 WASM을 고려하여 설계되어 별도 포팅이 불필요하다.
각 챕터에서 WASM 관련 사항이 인라인으로 다뤄지며, 여기에는 핵심 체크리스트만 정리한다.

- **빌드**: `emcmake cmake -B build-web && cmake --build build-web`
- **실행**: `python3 -m http.server` → 브라우저에서 `index.html` 열기
- **에셋**: `--preload-file assets/` 또는 `--embed-file` 으로 가상 FS에 임베딩
- **GLSL**: 300 es 기본, Desktop에서는 전처리기로 `#version 330 core` 자동 주입
- **게임 루프**: `emscripten_set_main_loop` (Ch.05에서 구현)
- **스레딩**: WASM에서는 제한적 → SharedArrayBuffer + Web Worker (시선 추적 시)
- **오디오**: SDL3 Audio → Web Audio API 자동 매핑, 사용자 인터랙션 후 시작
- **카메라**: SDL3 Camera API 또는 JS interop `getUserMedia`
- **디버깅**: `-g` 플래그 + 브라우저 개발자 도구, `-s ASSERTIONS=1`

---

## 예상 학습 시간

| Milestone | 챕터 수 | 예상 학습 시간 |
|-----------|---------|--------------|
| 1. 프로토타이핑 | 15 | 집중적으로 진행 시 챕터당 1~3일 |
| 2. 비주얼 개선 | 7 | 에셋 준비 포함 챕터당 2~3일 |
| 3. 시선 추적 | 6 | 실험/튜닝 포함 챕터당 2~4일 |

---

## 프로젝트 디렉토리 구조 (최종)

```
gazeshot/
├── CMakeLists.txt
├── cmake/
│   └── Emscripten.cmake  # WASM 빌드 헬퍼
├── core/
│   ├── math/              # Vec, Mat, Quat, Transform
│   ├── types/             # 공통 타입 정의
│   └── utils/             # Logger, Timer 등
├── platform/
│   └── sdl/               # SDL3 Window, Input, Audio 래퍼
├── renderer/
│   ├── interface/         # 추상 렌더러 인터페이스
│   ├── opengl/            # OpenGL ES 3.0 백엔드
│   └── shaders/           # GLSL 300 es 셰이더
├── engine/
│   ├── scene/             # Entity, Scene
│   ├── resource/          # ResourceManager
│   └── particle/          # ParticleSystem
├── game/
│   ├── camera/            # SniperCamera
│   ├── shooting/          # Ray, Ballistics, Collision
│   ├── hud/               # Scope, Crosshair, Score
│   ├── state/             # GameState, ScoreSystem
│   └── tracking/          # FaceTracker, GazeTracker
├── assets/
│   ├── models/
│   ├── textures/
│   ├── shaders/
│   └── sounds/
├── tests/                 # doctest 기반 테스트
│   ├── math/
│   ├── collision/
│   └── ...
├── web/
│   ├── index.html         # WASM 호스트 페이지
│   └── shell.html         # Emscripten 셸 템플릿
└── docs/
    └── chapters/          # 학습 자료 (이 코스의 챕터들)
```

---

## 다음 단계

이 코스 아웃라인이 괜찮으시다면, **Chapter 01**부터 상세 학습 자료를 작성합니다.
각 챕터는 다음 구조로 제공됩니다:

1. **학습 목표** - 이 챕터에서 무엇을 배우는가
2. **배경 지식** - 필요한 이론과 개념 설명
3. **설계** - 어떤 구조로 만들 것인가 (다이어그램, 인터페이스)
4. **구현 가이드** - 단계별 코드 작성 안내 (핵심 코드 스니펫 + 설명)
5. **검증** - 동작 확인 방법, 예상 결과
6. **심화** - 더 알아볼 주제, 참고 자료
