# Chapter 03: 커스텀 수학 라이브러리 (2) — 변환과 투영

## 데모 미리보기

```
┌─────────────────────────────────────┐
│                                     │
│       ╱──╲    3D 큐브가              │
│      │    │   커스텀 MVP 행렬로       │
│       ╲──╱    회전한다               │
│                                     │
│  perspective + lookAt + rotate       │
│  모두 직접 구현                      │
└─────────────────────────────────────┘
```

- **데모**: 원근 투영이 적용된 3D 와이어프레임 큐브가 Y축으로 회전
- **테스트**: lookAt, perspective 결과를 GLM과 비교하는 테스트 통과
- 블로그에 "perspective 행렬의 각 원소가 뜻하는 것" 다이어그램 포함 가능

---

## 학습 목표

1. `translate`, `rotate`, `scale` 변환 함수를 구현한다
2. `lookAt` 뷰 행렬을 직접 유도하고 구현한다
3. `perspective`, `ortho` 투영 행렬을 구현한다
4. 쿼터니언(`Quat`)을 구현하여 짐벌 락 문제를 해결한다
5. `static_assert`, user-defined literal, `[[nodiscard]]`를 실습한다

---

## 1. 배경 지식

### MVP 변환 파이프라인

3D 좌표가 화면 픽셀이 되기까지:

```
로컬 좌표 ──[Model]──→ 월드 좌표 ──[View]──→ 카메라 좌표 ──[Projection]──→ NDC
    (0,1,0)         (5,11,15)          (0,1,-10)           (-0.2, 0.3, 0.95)
```

- **Model**: 오브젝트를 월드에 배치 (이동, 회전, 스케일)
- **View**: 카메라 기준으로 좌표계 변환 (`lookAt`)
- **Projection**: 3D → 2D 투영 (`perspective` 또는 `ortho`)

### Perspective 행렬 유도

시야각(FOV)과 종횡비(aspect)로부터:

```
fov = 45도 (수직 시야각)
aspect = width / height
near = 0.1, far = 100.0

f = 1 / tan(fov / 2)

     ┌ f/aspect  0       0              0          ┐
P  = │ 0         f       0              0          │
     │ 0         0   (far+near)/(near-far)   (2·far·near)/(near-far) │
     └ 0         0      -1              0          ┘
```

각 요소의 의미:
- `f/aspect`: X축을 종횡비에 맞게 스케일
- `f`: Y축을 FOV에 맞게 스케일
- 세 번째 행: Z를 [near, far] → [-1, 1]로 매핑 (depth)
- `-1` (네 번째 행 세 번째 열): **perspective divide** — w에 -z를 넣어서 원근감

### 짐벌 락 (Gimbal Lock)

오일러 각도로 회전할 때, 한 축이 다른 축과 정렬되면 자유도가 하나 사라진다.
스나이퍼 조준에서 위아래로 90도 보려 할 때 문제가 된다.

해결: **쿼터니언** — 4개의 성분(w, x, y, z)으로 임의의 3D 회전을 표현.

---

## 2. 구현 가이드

### Step 1: 변환 함수

```hpp
// core/include/gazeshot/core/math/Transform.hpp

#pragma once

#include <gazeshot/core/math/Mat4.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <cmath>

namespace gazeshot::core::math {

// ── 이동 행렬 ──
// row-major: m[row][col] = 수학 표기 그대로
// [1  0  0  tx]
// [0  1  0  ty]
// [0  0  1  tz]
// [0  0  0  1 ]
template<typename T>
[[nodiscard]] constexpr Mat4<T> translate(const Vec3<T>& offset) {
    Mat4<T> m = Mat4<T>::identity();
    m[0][3] = offset.x;
    m[1][3] = offset.y;
    m[2][3] = offset.z;
    return m;
}

// ── 스케일 행렬 ──
template<typename T>
[[nodiscard]] constexpr Mat4<T> scale(const Vec3<T>& s) {
    Mat4<T> m{};
    m[0].x = s.x;
    m[1].y = s.y;
    m[2].z = s.z;
    m[3].w = T(1);
    return m;
}

// ── 축-각도 회전 행렬 (로드리게스 공식) ──
template<typename T>
[[nodiscard]] Mat4<T> rotate(T radians, Vec3<T> axis) {
    axis = normalize(axis);
    T c = std::cos(radians);
    T s = std::sin(radians);
    T t = T(1) - c;

    T x = axis.x, y = axis.y, z = axis.z;

    return Mat4<T>{
        Vec4<T>{ t*x*x + c,   t*x*y - s*z, t*x*z + s*y, 0 },
        Vec4<T>{ t*x*y + s*z, t*y*y + c,   t*y*z - s*x, 0 },
        Vec4<T>{ t*x*z - s*y, t*y*z + s*x, t*z*z + c,   0 },
        Vec4<T>{ 0,           0,           0,           1 }
    };
}

// ── 개별 축 회전 (최적화된 버전) ──
template<typename T>
[[nodiscard]] Mat4<T> rotateX(T radians) {
    T c = std::cos(radians), s = std::sin(radians);
    Mat4<T> m = Mat4<T>::identity();
    m[1][1] = c;  m[1][2] = -s;
    m[2][1] = s;  m[2][2] =  c;
    return m;
}

template<typename T>
[[nodiscard]] Mat4<T> rotateY(T radians) {
    T c = std::cos(radians), s = std::sin(radians);
    Mat4<T> m = Mat4<T>::identity();
    m[0][0] =  c;  m[0][2] = s;
    m[2][0] = -s;  m[2][2] = c;
    return m;
}

template<typename T>
[[nodiscard]] Mat4<T> rotateZ(T radians) {
    T c = std::cos(radians), s = std::sin(radians);
    Mat4<T> m = Mat4<T>::identity();
    m[0][0] = c;  m[0][1] = -s;
    m[1][0] = s;  m[1][1] =  c;
    return m;
}

// ── 뷰 행렬 (카메라) ──
template<typename T>
[[nodiscard]] Mat4<T> lookAt(const Vec3<T>& eye,
                              const Vec3<T>& target,
                              const Vec3<T>& worldUp) {
    Vec3<T> forward = normalize(eye - target);       // 카메라 → 타겟 (반대 방향)
    Vec3<T> right   = normalize(cross(worldUp, forward));
    Vec3<T> up      = cross(forward, right);

    // row-major: 각 행이 basis 벡터 + translation
    Mat4<T> m{};
    m[0] = Vec4<T>{ right.x,   right.y,   right.z,   -dot(right, eye)   };
    m[1] = Vec4<T>{ up.x,      up.y,      up.z,      -dot(up, eye)      };
    m[2] = Vec4<T>{ forward.x, forward.y, forward.z, -dot(forward, eye) };
    m[3] = Vec4<T>{ 0,         0,         0,          T(1)               };
    return m;
}

// ── 원근 투영 ──
template<typename T>
[[nodiscard]] Mat4<T> perspective(T fovRadians, T aspect, T near, T far) {
    T f = T(1) / std::tan(fovRadians / T(2));

    Mat4<T> m{};
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = (far + near) / (near - far);
    m[2][3] = (T(2) * far * near) / (near - far);
    m[3][2] = T(-1);
    return m;
}

// ── 직교 투영 ──
template<typename T>
[[nodiscard]] constexpr Mat4<T> ortho(T left, T right, T bottom, T top,
                                       T near, T far) {
    Mat4<T> m{};
    m[0][0] = T(2) / (right - left);
    m[1][1] = T(2) / (top - bottom);
    m[2][2] = T(-2) / (far - near);
    m[0][3] = -(right + left) / (right - left);
    m[1][3] = -(top + bottom) / (top - bottom);
    m[2][3] = -(far + near) / (far - near);
    m[3][3] = T(1);
    return m;
}

// ── 유틸리티 ──
template<typename T>
constexpr T radians(T degrees) {
    return degrees * T(3.14159265358979323846) / T(180);
}

template<typename T>
constexpr T degrees(T rad) {
    return rad * T(180) / T(3.14159265358979323846);
}

} // namespace gazeshot::core::math
```

**C++ 학습 포인트: `[[nodiscard]]`**

```cpp
[[nodiscard]] Mat4<T> rotate(T radians, Vec3<T> axis);

// 사용 측:
rotate(0.5f, {0,1,0});             // ⚠️ 컴파일러 경고! 반환값 무시
auto m = rotate(0.5f, {0,1,0});    // ✅
```

변환 함수의 결과를 안 쓰면 버그다. `[[nodiscard]]`가 이를 잡아준다.

### Step 2: 쿼터니언

```hpp
// core/include/gazeshot/core/math/Quat.hpp

#pragma once

#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Mat4.hpp>
#include <cmath>

namespace gazeshot::core::math {

template<typename T = f32>
struct Quat {
    T w{1}, x{}, y{}, z{};  // w가 먼저 (수학 관례)

    constexpr Quat() = default;
    constexpr Quat(T w, T x, T y, T z) : w(w), x(x), y(y), z(z) {}

    // 축-각도로 생성
    static Quat fromAxisAngle(const Vec3<T>& axis, T radians) {
        auto a = normalize(axis);
        T half = radians / T(2);
        T s = std::sin(half);
        return { std::cos(half), a.x * s, a.y * s, a.z * s };
    }

    // 오일러 각도로 생성 (yaw, pitch, roll)
    static Quat fromEuler(T pitch, T yaw, T roll) {
        T cp = std::cos(pitch / T(2)), sp = std::sin(pitch / T(2));
        T cy = std::cos(yaw   / T(2)), sy = std::sin(yaw   / T(2));
        T cr = std::cos(roll  / T(2)), sr = std::sin(roll  / T(2));
        return {
            cr*cp*cy + sr*sp*sy,
            sr*cp*cy - cr*sp*sy,
            cr*sp*cy + sr*cp*sy,
            cr*cp*sy - sr*sp*cy
        };
    }

    // 단위 쿼터니언 여부
    T lengthSquared() const { return w*w + x*x + y*y + z*z; }
    T length() const { return std::sqrt(lengthSquared()); }

    // 회전 행렬로 변환
    [[nodiscard]] Mat4<T> toMat4() const {
        T xx = x*x, yy = y*y, zz = z*z;
        T xy = x*y, xz = x*z, yz = y*z;
        T wx = w*x, wy = w*y, wz = w*z;

        Mat4<T> m{};
        m[0] = Vec4<T>{ 1-2*(yy+zz),  2*(xy-wz),   2*(xz+wy),  0 };
        m[1] = Vec4<T>{ 2*(xy+wz),    1-2*(xx+zz),  2*(yz-wx),  0 };
        m[2] = Vec4<T>{ 2*(xz-wy),    2*(yz+wx),   1-2*(xx+yy), 0 };
        m[3] = Vec4<T>{ 0,            0,            0,           1 };
        return m;
    }
};

// ── 쿼터니언 곱 (회전 합성) ──
template<typename T>
constexpr Quat<T> operator*(const Quat<T>& a, const Quat<T>& b) {
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

// ── 정규화 ──
template<typename T>
Quat<T> normalize(const Quat<T>& q) {
    T len = q.length();
    return { q.w/len, q.x/len, q.y/len, q.z/len };
}

// ── 구면 선형 보간 (Slerp) ──
template<typename T>
Quat<T> slerp(const Quat<T>& a, const Quat<T>& b, T t) {
    T cosHalf = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;

    // 최단 경로 보장
    Quat<T> b2 = b;
    if (cosHalf < 0) {
        b2 = { -b.w, -b.x, -b.y, -b.z };
        cosHalf = -cosHalf;
    }

    // 거의 같으면 lerp로 대체
    if (cosHalf > T(0.9999)) {
        return normalize(Quat<T>{
            a.w + t*(b2.w - a.w),
            a.x + t*(b2.x - a.x),
            a.y + t*(b2.y - a.y),
            a.z + t*(b2.z - a.z)
        });
    }

    T halfAngle = std::acos(cosHalf);
    T sinHalf = std::sin(halfAngle);
    T wa = std::sin((T(1) - t) * halfAngle) / sinHalf;
    T wb = std::sin(t * halfAngle) / sinHalf;

    return {
        wa*a.w + wb*b2.w,
        wa*a.x + wb*b2.x,
        wa*a.y + wb*b2.y,
        wa*a.z + wb*b2.z
    };
}

using Quatf = Quat<f32>;
using Quatd = Quat<f64>;

} // namespace gazeshot::core::math
```

### Step 3: User-defined literal

```hpp
// core/include/gazeshot/core/math/Literals.hpp

#pragma once

namespace gazeshot::core::math::literals {

// 45.0_deg → 0.785398... (라디안)
constexpr float operator""_deg(long double degrees) {
    return static_cast<float>(degrees * 3.14159265358979323846L / 180.0L);
}

constexpr float operator""_deg(unsigned long long degrees) {
    return static_cast<float>(static_cast<long double>(degrees)
                              * 3.14159265358979323846L / 180.0L);
}

} // namespace gazeshot::core::math::literals
```

사용:

```cpp
using namespace gazeshot::core::math::literals;

auto rot = rotateY(45_deg);       // 45도를 라디안으로 변환할 필요 없음
auto fov = perspective(60.0_deg, aspect, 0.1f, 100.0f);
```

### Step 4: static_assert로 레이아웃 검증

```hpp
// Math.hpp 하단에 추가

static_assert(sizeof(Vec3f) == 12, "Vec3f must be 12 bytes (3 floats)");
static_assert(sizeof(Vec4f) == 16, "Vec4f must be 16 bytes (4 floats)");
static_assert(sizeof(Mat4f) == 64, "Mat4f must be 64 bytes (16 floats)");
static_assert(sizeof(Quatf) == 16, "Quatf must be 16 bytes (4 floats)");

// 컴파일 타임에 메모리 레이아웃이 GPU 호환인지 보장
// 이것이 실패하면 패딩이 들어갔다는 뜻 → 구조체 재설계 필요
```

### Step 5: 테스트

```cpp
// tests/test_math_transform.cpp

#include <doctest/doctest.h>
#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;
using namespace gazeshot::core::math::literals;

constexpr float EPS = 1e-5f;

TEST_CASE("translate") {
    auto m = translate(Vec3f{5, 10, 15});
    auto p = m * Vec4f{0, 0, 0, 1};
    CHECK(p.x == doctest::Approx(5.0f));
    CHECK(p.y == doctest::Approx(10.0f));
    CHECK(p.z == doctest::Approx(15.0f));
}

TEST_CASE("translate은 방향벡터에 영향 없음") {
    auto m = translate(Vec3f{5, 10, 15});
    auto d = m * Vec4f{1, 0, 0, 0};  // w=0
    CHECK(d.x == doctest::Approx(1.0f));
    CHECK(d.y == doctest::Approx(0.0f));
}

TEST_CASE("scale") {
    auto m = scale(Vec3f{2, 3, 4});
    auto p = m * Vec4f{1, 1, 1, 1};
    CHECK(p.x == doctest::Approx(2.0f));
    CHECK(p.y == doctest::Approx(3.0f));
    CHECK(p.z == doctest::Approx(4.0f));
}

TEST_CASE("rotateY 90도") {
    auto m = rotateY(90.0_deg);
    auto p = m * Vec4f{1, 0, 0, 1};  // X축 위 점
    // Y축 90도 회전 → Z축 음방향으로 이동
    CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
    CHECK(p.z == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("rotate 축-각도") {
    auto m = rotate(90.0_deg, Vec3f{0, 1, 0});
    auto p = m * Vec4f{1, 0, 0, 1};
    CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
    CHECK(p.z == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("lookAt 기본") {
    auto v = lookAt(Vec3f{0, 0, 5}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});
    // 카메라가 (0,0,5)에서 원점을 봄
    // 원점의 점은 카메라 앞 5만큼에 있어야 함
    auto p = v * Vec4f{0, 0, 0, 1};
    CHECK(p.z == doctest::Approx(-5.0f).epsilon(EPS));
}

TEST_CASE("perspective 기본") {
    auto p = perspective(90.0_deg, 1.0f, 0.1f, 100.0f);
    // near plane 위의 점
    auto result = p * Vec4f{0, 0, -0.1f, 1};
    // perspective divide 후 z가 -1에 가까워야 함
    float ndcZ = result.z / result.w;
    CHECK(ndcZ == doctest::Approx(-1.0f).epsilon(0.01f));
}

TEST_CASE("Quat fromAxisAngle → toMat4 일관성") {
    auto q = Quatf::fromAxisAngle({0, 1, 0}, 90.0_deg);
    auto mq = q.toMat4();
    auto mr = rotateY(90.0_deg);

    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK(mq[r][c] == doctest::Approx(mr[r][c]).epsilon(EPS));
}

TEST_CASE("Quat slerp 중간점") {
    auto a = Quatf::fromAxisAngle({0,1,0}, 0.0_deg);
    auto b = Quatf::fromAxisAngle({0,1,0}, 90.0_deg);
    auto mid = slerp(a, b, 0.5f);
    auto m = mid.toMat4();

    // 45도 회전과 동일해야 함
    auto expected = rotateY(45.0_deg);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK(m[r][c] == doctest::Approx(expected[r][c]).epsilon(EPS));
}

TEST_CASE("user-defined literal _deg") {
    CHECK(radians(90.0f) == doctest::Approx(90.0_deg).epsilon(EPS));
    CHECK(radians(45.0f) == doctest::Approx(45.0_deg).epsilon(EPS));
}
```

### Step 6: 데모 — 3D 회전 큐브

```cpp
// game/src/main.cpp  (Ch.03 변경사항만 발췌)
// Ch.02의 구조를 유지하면서 다음을 변경:

// 삼각형 대신 큐브 정점
float vertices[] = {
    // 앞면                // 뒷면
    -0.5f, -0.5f,  0.5f,   -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,    0.5f, -0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,    0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,   -0.5f,  0.5f, -0.5f,
};

unsigned int indices[] = {
    // 앞면             뒷면
    0,1,2, 2,3,0,      4,5,6, 6,7,4,
    // 좌면             우면
    4,0,3, 3,7,4,      1,5,6, 6,2,1,
    // 상면             하면
    3,2,6, 6,7,3,      4,5,1, 1,0,4,
};

// oneFrame 안에서:
void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);
    // ...

    using namespace gazeshot::core::math;
    using namespace gazeshot::core::math::literals;

    app->time += 1.0f / 60.0f;

    // ── 커스텀 MVP 행렬 ──
    Mat4f model = rotateY(app->time)
                * rotateX(app->time * 0.7f);

    Mat4f view = lookAt(
        Vec3f{0, 0, 3},          // 카메라 위치
        Vec3f{0, 0, 0},          // 바라보는 곳
        Vec3f{0, 1, 0}           // 월드 업
    );

    float aspect = static_cast<float>(app->window.width())
                 / static_cast<float>(app->window.height());

    Mat4f proj = perspective(45.0_deg, aspect, 0.1f, 100.0f);

    Mat4f mvp = proj * view * model;

    // uniform 전달
    int loc = glGetUniformLocation(app->shaderProgram, "uTransform");
    glUniformMatrix4fv(loc, 1, GL_TRUE, mvp.data());

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(app->vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

    app->window.swapBuffers();
}
```

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 테스트 PASS | lookAt, perspective, Quat 테스트 전부 통과 |
| 큐브 보임 | 3D 원근감이 있는 큐브가 회전 |
| 깊이 동작 | 뒷면이 앞면에 가려짐 (depth test) |
| WASM 동작 | 브라우저에서도 동일한 3D 큐브 |
| static_assert | sizeof 검증 컴파일 통과 |
| _deg 리터럴 | `45_deg` 사용 가능 |

---

## 5. 블로그 데모 아이디어

1. **3D 큐브 GIF**: 원근감 있는 회전 큐브
2. **perspective 행렬 다이어그램**: 각 원소가 뜻하는 것
3. **짐벌 락 시연**: 오일러 각도로 회전 시 문제 → 쿼터니언으로 해결
4. **코드 비교**: `glm::lookAt` vs 우리의 `lookAt` — 결과 동일
5. **user-defined literal**: `45.0_deg`가 코드를 얼마나 읽기 좋게 하는지

---

## 다음 챕터 예고

**Chapter 04: 렌더링 추상화 레이어**

OpenGL 직접 호출을 추상화하여 `Renderer`, `VertexBuffer`, `ShaderProgram` 등을 만든다.
데모: 추상화된 API로 동일한 큐브를 렌더링 — 코드에서 `gl*` 호출이 사라진다.
