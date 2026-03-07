# Chapter 02: 커스텀 수학 라이브러리 (1) — 벡터와 행렬

## 데모 미리보기

이 챕터를 마치면 다음을 얻는다:

```
┌─────────────────────────────────────┐
│                                     │
│         ◇ (회전하는 삼각형)           │
│        / \                          │
│       /   \  ← 커스텀 Vec3, Mat4로   │
│      /_____\    변환 적용            │
│                                     │
│  GLM 제거 완료, 커스텀 수학만 사용     │
└─────────────────────────────────────┘
```

- **데모**: 커스텀 `Mat4`의 회전 행렬로 삼각형이 시계 방향으로 회전
- **테스트**: GLM과 결과를 비교하는 doctest 30+ 케이스가 모두 PASS
- 블로그에 "GLM 없이 직접 만든 수학으로 렌더링" 영상/GIF를 올릴 수 있다

---

## 학습 목표

1. `Vec2`, `Vec3`, `Vec4` 클래스 템플릿을 직접 구현한다
2. `Mat3`, `Mat4`를 row-major로 구현한다
3. `constexpr`로 컴파일 타임 벡터/행렬 연산을 만든다
4. operator overloading 패턴을 익힌다
5. doctest로 부동소수점 비교 테스트를 작성한다
6. GLM과 결과를 비교 검증한 후 GLM을 제거한다

---

## 1. 배경 지식

### 왜 직접 만드는가?

GLM은 훌륭한 라이브러리지만, 직접 만들면:
- 행렬 곱셈의 메모리 레이아웃을 체감한다
- GPU에 데이터를 보낼 때 왜 column-major인지 이해한다
- 디버깅할 때 "이 숫자가 왜 이렇게 나오지?"를 추적할 수 있다
- 프로젝트 전체에서 외부 의존성이 하나 줄어든다

### Column-Major vs Row-Major

행렬을 메모리에 나열하는 방법은 두 가지가 있다:

```
수학 표기:
┌ m00  m01  m02  m03 ┐
│ m10  m11  m12  m13 │
│ m20  m21  m22  m23 │
└ m30  m31  m32  m33 ┘

Column-major (열 우선):                  Row-major (행 우선):
[m00, m10, m20, m30,  ← column 0       [m00, m01, m02, m03,  ← row 0
 m01, m11, m21, m31,  ← column 1        m10, m11, m12, m13,  ← row 1
 m02, m12, m22, m32,  ← column 2        m20, m21, m22, m23,  ← row 2
 m03, m13, m23, m33]  ← column 3        m30, m31, m32, m33]  ← row 3
```

**Column-major를 사용하는 진영:**
- OpenGL / GLSL — GPU에 전달할 때 column-major를 기본으로 기대한다
- GLM, cglm, Handmade Math — OpenGL 관례를 따르는 수학 라이브러리들
- Fortran — 역사적으로 column-major의 원조

**Row-major를 사용하는 진영:**
- DirectX / HLSL — DirectXMath가 row-major
- Unreal Engine (FMatrix) — DirectX 기반
- Eigen — 범용 선형대수 라이브러리
- C/C++ 다차원 배열 — `float m[4][4]`는 row-major

**우리의 선택: Row-major**

코드에서 행렬을 다룰 때의 직관성을 우선했다:

```cpp
// Row-major: m[row][col] — 수학 표기와 코드가 일치
// 이동 행렬을 만들면:
Mat4f m = Mat4f::identity();
m[0][3] = tx;   // 첫 번째 행, 네 번째 열 — 교과서와 동일
m[1][3] = ty;
m[2][3] = tz;
// 눈에 보이는 그대로:
// [1  0  0  tx]
// [0  1  0  ty]
// [0  0  1  tz]
// [0  0  0  1 ]

// Column-major: m[col][row] — 머릿속에서 전치해야 함
m[3] = Vec4f{tx, ty, tz, 1};  // "3번째 열"이 translation
// 코드만 보면 어디가 행이고 열인지 헷갈린다
```

OpenGL은 column-major를 기대하지만, `glUniformMatrix4fv`의 `transpose` 파라미터를
`GL_TRUE`로 설정하면 row-major 데이터를 GPU가 알아서 전치해서 받는다.
성능 차이는 없다.

### `constexpr`의 의미

`constexpr`은 "이 함수/변수는 컴파일 타임에 평가될 수 있다"는 뜻이다:

```cpp
constexpr Vec3 up{0, 1, 0};              // 컴파일 타임에 생성
constexpr float len = length(up);         // 컴파일 타임에 계산
static_assert(len == 1.0f);              // 컴파일 타임에 검증!
```

런타임 오버헤드 제로. 수학 라이브러리에 이상적이다.

---

## 2. 설계

### 타입 구조

```
Vec<N, T>        ← 제네릭 벡터 (사용하지 않는 기저)
├── Vec2<T>      ← 특수화: x, y 접근
├── Vec3<T>      ← 특수화: x, y, z 접근
└── Vec4<T>      ← 특수화: x, y, z, w 접근

Mat<C, R, T>     ← 제네릭 행렬
├── Mat3<T>      ← 3x3
└── Mat4<T>      ← 4x4

별칭:
Vec2f = Vec2<float>   Vec3f = Vec3<float>   Vec4f = Vec4<float>
Mat3f = Mat3<float>   Mat4f = Mat4<float>
```

**설계 결정**: N차원 제네릭 `Vec<N>` 대신 `Vec2/3/4` 각각 특수화한다.
이유: `.x`, `.y`, `.z` 접근이 `.data[0]` 보다 가독성이 좋고, 게임에서 5차원 이상은 쓸 일이 없다.

### 파일 구조

```
core/include/gazeshot/core/math/
├── Vec2.hpp
├── Vec3.hpp
├── Vec4.hpp
├── Mat3.hpp
├── Mat4.hpp
└── Math.hpp     ← 전체 include 편의 헤더
```

---

## 3. 구현 가이드

### Step 1: Vec3 (핵심)

Vec3가 가장 많이 쓰이므로 먼저 구현한다. Vec2, Vec4는 같은 패턴이다.

```hpp
// core/include/gazeshot/core/math/Vec3.hpp

#pragma once

#include <gazeshot/core/Types.hpp>

#include <cmath>
#include <cassert>

namespace gazeshot::core::math {

template<typename T = f32>
struct Vec3 {
    T x{}, y{}, z{};

    // ── 생성자 ──
    constexpr Vec3() = default;
    constexpr Vec3(T x, T y, T z) : x(x), y(y), z(z) {}
    constexpr explicit Vec3(T scalar) : x(scalar), y(scalar), z(scalar) {}

    // ── 인덱스 접근 ──
    constexpr T& operator[](usize i) {
        assert(i < 3);
        return (&x)[i];   // x, y, z가 연속 메모리라는 보장 (표준 layout)
    }
    constexpr const T& operator[](usize i) const {
        assert(i < 3);
        return (&x)[i];
    }

    // ── 산술 연산자 (멤버) ──
    constexpr Vec3& operator+=(const Vec3& rhs) {
        x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }
    constexpr Vec3& operator-=(const Vec3& rhs) {
        x -= rhs.x; y -= rhs.y; z -= rhs.z;
        return *this;
    }
    constexpr Vec3& operator*=(T scalar) {
        x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }
    constexpr Vec3& operator/=(T scalar) {
        x /= scalar; y /= scalar; z /= scalar;
        return *this;
    }

    // ── 단항 연산자 ──
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }

    // ── 비교 ──
    constexpr bool operator==(const Vec3&) const = default;  // C++20!

    // ── 포인터 변환 (OpenGL uniform 전달용) ──
    constexpr const T* data() const { return &x; }
    constexpr T* data() { return &x; }
};

// ── 이항 연산자 (비멤버, friend 불필요 — ADL이 찾아줌) ──
template<typename T>
constexpr Vec3<T> operator+(Vec3<T> a, const Vec3<T>& b) { return a += b; }

template<typename T>
constexpr Vec3<T> operator-(Vec3<T> a, const Vec3<T>& b) { return a -= b; }

template<typename T>
constexpr Vec3<T> operator*(Vec3<T> v, T scalar) { return v *= scalar; }

template<typename T>
constexpr Vec3<T> operator*(T scalar, Vec3<T> v) { return v *= scalar; }

template<typename T>
constexpr Vec3<T> operator/(Vec3<T> v, T scalar) { return v /= scalar; }

// ── 자유 함수 ──
template<typename T>
constexpr T dot(const Vec3<T>& a, const Vec3<T>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template<typename T>
constexpr Vec3<T> cross(const Vec3<T>& a, const Vec3<T>& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

template<typename T>
constexpr T lengthSquared(const Vec3<T>& v) {
    return dot(v, v);
}

template<typename T>
T length(const Vec3<T>& v) {
    return std::sqrt(lengthSquared(v));
}

template<typename T>
Vec3<T> normalize(const Vec3<T>& v) {
    T len = length(v);
    assert(len > T(0));
    return v / len;
}

template<typename T>
constexpr Vec3<T> lerp(const Vec3<T>& a, const Vec3<T>& b, T t) {
    return a + (b - a) * t;
}

// ── 별칭 ──
using Vec3f = Vec3<f32>;
using Vec3d = Vec3<f64>;
using Vec3i = Vec3<i32>;

} // namespace gazeshot::core::math
```

**C++ 학습 포인트: 이항 연산자를 비멤버로 만드는 이유**

```cpp
// 멤버로 만들면:
Vec3 a{1,2,3};
auto b = a * 2.0f;   // ✅ a.operator*(2.0f)
auto c = 2.0f * a;   // ❌ float에는 operator*(Vec3)가 없다!

// 비멤버로 만들면:
auto c = 2.0f * a;   // ✅ operator*(float, Vec3) 자유 함수를 찾는다
```

**C++ 학습 포인트: `operator==(const Vec3&) const = default`**

C++20에서는 `==`를 `= default`로 선언하면:
- 모든 멤버를 순서대로 비교하는 코드가 자동 생성된다
- `!=`도 자동으로 만들어진다 (C++20 rewritten candidates)

### Step 2: Vec2, Vec4

Vec3와 동일한 패턴이므로 차이점만 짚는다:

```hpp
// core/include/gazeshot/core/math/Vec2.hpp  (골격)

template<typename T = f32>
struct Vec2 {
    T x{}, y{};
    // ... (Vec3와 동일 패턴, z 관련만 제거)
};

// dot은 있지만 cross는 없다 (2D cross는 스칼라 반환이므로 별도)
template<typename T>
constexpr T cross2D(const Vec2<T>& a, const Vec2<T>& b) {
    return a.x * b.y - a.y * b.x;  // 외적의 z 성분
}

using Vec2f = Vec2<f32>;
```

```hpp
// core/include/gazeshot/core/math/Vec4.hpp  (골격)

template<typename T = f32>
struct Vec4 {
    T x{}, y{}, z{}, w{};
    // ... (Vec3 패턴 + w 추가)
};

using Vec4f = Vec4<f32>;
```

> **전체 코드**: Vec2, Vec4는 Vec3를 따라 직접 작성해 보라.
> 패턴이 완전히 동일하므로 "따라 치기"가 아니라 "이해하고 만들기"가 된다.

### Step 3: Mat4 (핵심)

```hpp
// core/include/gazeshot/core/math/Mat4.hpp

#pragma once

#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/core/Types.hpp>

#include <cmath>
#include <cassert>

namespace gazeshot::core::math {

template<typename T = f32>
struct Mat4 {
    // Row-major: rows[0] = 첫 번째 행
    Vec4<T> rows[4] = {};

    // ── 생성자 ──
    constexpr Mat4() = default;

    // 대각 행렬 (identity는 Mat4(1.0f))
    constexpr explicit Mat4(T diagonal) {
        rows[0].x = diagonal;
        rows[1].y = diagonal;
        rows[2].z = diagonal;
        rows[3].w = diagonal;
    }

    // 4개 행 벡터로 구성
    constexpr Mat4(const Vec4<T>& r0, const Vec4<T>& r1,
                   const Vec4<T>& r2, const Vec4<T>& r3)
        : rows{r0, r1, r2, r3} {}

    // ── 행 접근: m[row][col] — 수학 표기와 동일 ──
    constexpr Vec4<T>& operator[](usize i) {
        assert(i < 4);
        return rows[i];
    }
    constexpr const Vec4<T>& operator[](usize i) const {
        assert(i < 4);
        return rows[i];
    }

    // ── 단위 행렬 ──
    static constexpr Mat4 identity() { return Mat4(T(1)); }

    // ── OpenGL 전달용 ──
    constexpr const T* data() const { return rows[0].data(); }

    // ── 비교 ──
    constexpr bool operator==(const Mat4&) const = default;
};

// ── 행렬 × 행렬: result[i][j] = sum_k(a[i][k] * b[k][j]) ──
template<typename T>
constexpr Mat4<T> operator*(const Mat4<T>& a, const Mat4<T>& b) {
    Mat4<T> result;
    for (usize row = 0; row < 4; ++row) {
        for (usize col = 0; col < 4; ++col) {
            T sum = T(0);
            for (usize k = 0; k < 4; ++k) {
                sum += a[row][k] * b[k][col];
            }
            result[row][col] = sum;
        }
    }
    return result;
}

// ── 행렬 × 벡터: result[i] = dot(row_i, v) ──
template<typename T>
constexpr Vec4<T> operator*(const Mat4<T>& m, const Vec4<T>& v) {
    return {
        m[0].x * v.x + m[0].y * v.y + m[0].z * v.z + m[0].w * v.w,
        m[1].x * v.x + m[1].y * v.y + m[1].z * v.z + m[1].w * v.w,
        m[2].x * v.x + m[2].y * v.y + m[2].z * v.z + m[2].w * v.w,
        m[3].x * v.x + m[3].y * v.y + m[3].z * v.z + m[3].w * v.w,
    };
}

// ── 전치 ──
template<typename T>
constexpr Mat4<T> transpose(const Mat4<T>& m) {
    return {{m[0].x, m[1].x, m[2].x, m[3].x},
            {m[0].y, m[1].y, m[2].y, m[3].y},
            {m[0].z, m[1].z, m[2].z, m[3].z},
            {m[0].w, m[1].w, m[2].w, m[3].w}};
}

// ── 역행렬 (코팩터 방식, m[row][col] 접근) ──
template<typename T>
Mat4<T> inverse(const Mat4<T>& m) {
    // 상위 2행의 2x2 소행렬식
    T s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
    T s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
    T s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
    T s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
    T s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
    T s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

    // 하위 2행의 2x2 소행렬식
    T c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    T c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    T c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    T c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    T c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    T c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

    T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    assert(std::abs(det) > T(1e-8));
    T inv = T(1) / det;

    Mat4<T> r;
    r[0][0] = ( m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * inv;
    r[0][1] = (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * inv;
    r[0][2] = ( m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * inv;
    r[0][3] = (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * inv;

    r[1][0] = (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * inv;
    r[1][1] = ( m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * inv;
    r[1][2] = (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * inv;
    r[1][3] = ( m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * inv;

    r[2][0] = ( m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * inv;
    r[2][1] = (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * inv;
    r[2][2] = ( m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * inv;
    r[2][3] = (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * inv;

    r[3][0] = (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * inv;
    r[3][1] = ( m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * inv;
    r[3][2] = (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * inv;
    r[3][3] = ( m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * inv;

    return r;
}

// ── 별칭 ──
using Mat4f = Mat4<f32>;
using Mat4d = Mat4<f64>;

} // namespace gazeshot::core::math
```

**row-major 접근 방식 시각화**:

```
Mat4 m = identity();

m[0]  →  첫 번째 행 = {1, 0, 0, 0}
m[1]  →  두 번째 행 = {0, 1, 0, 0}
m[2]  →  세 번째 행 = {0, 0, 1, 0}
m[3]  →  네 번째 행 = {0, 0, 0, 1}

m[0][3], m[1][3], m[2][3]  →  이동(translation) 성분!
```

이동 행렬에서 translation이 **각 행의 마지막 열(w 위치)** 에 들어간다.
수학 교과서의 표기와 완전히 일치하므로 코드를 읽을 때 머릿속 변환이 필요 없다.

### Step 4: 편의 헤더

```hpp
// core/include/gazeshot/core/math/Math.hpp

#pragma once

#include <gazeshot/core/math/Vec2.hpp>
#include <gazeshot/core/math/Vec3.hpp>
#include <gazeshot/core/math/Vec4.hpp>
#include <gazeshot/core/math/Mat4.hpp>
// Mat3는 Ch.03에서 추가
```

### Step 5: 테스트

```cpp
// tests/test_math_vec.cpp

#include <doctest/doctest.h>
#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;

// ── Vec3 기본 ──

TEST_CASE("Vec3 기본 생성") {
    Vec3f v;
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
    CHECK(v.z == 0.0f);
}

TEST_CASE("Vec3 값 생성") {
    Vec3f v{1.0f, 2.0f, 3.0f};
    CHECK(v.x == 1.0f);
    CHECK(v.y == 2.0f);
    CHECK(v.z == 3.0f);
}

TEST_CASE("Vec3 스칼라 생성") {
    Vec3f v(5.0f);
    CHECK(v.x == 5.0f);
    CHECK(v.y == 5.0f);
    CHECK(v.z == 5.0f);
}

TEST_CASE("Vec3 인덱스 접근") {
    Vec3f v{10, 20, 30};
    CHECK(v[0] == 10.0f);
    CHECK(v[1] == 20.0f);
    CHECK(v[2] == 30.0f);
}

// ── Vec3 산술 ──

TEST_CASE("Vec3 덧셈") {
    Vec3f a{1, 2, 3};
    Vec3f b{4, 5, 6};
    auto c = a + b;
    CHECK(c.x == doctest::Approx(5.0f));
    CHECK(c.y == doctest::Approx(7.0f));
    CHECK(c.z == doctest::Approx(9.0f));
}

TEST_CASE("Vec3 뺄셈") {
    auto c = Vec3f{5, 7, 9} - Vec3f{4, 5, 6};
    CHECK(c == Vec3f{1, 2, 3});
}

TEST_CASE("Vec3 스칼라 곱") {
    Vec3f v{1, 2, 3};
    CHECK(v * 2.0f == Vec3f{2, 4, 6});
    CHECK(2.0f * v == Vec3f{2, 4, 6});  // 교환법칙
}

TEST_CASE("Vec3 단항 마이너스") {
    Vec3f v{1, -2, 3};
    CHECK(-v == Vec3f{-1, 2, -3});
}

// ── Vec3 기하 ──

TEST_CASE("Vec3 dot product") {
    Vec3f a{1, 0, 0};
    Vec3f b{0, 1, 0};
    CHECK(dot(a, b) == doctest::Approx(0.0f));   // 직교

    CHECK(dot(a, a) == doctest::Approx(1.0f));    // 자기 자신
}

TEST_CASE("Vec3 cross product") {
    Vec3f x{1, 0, 0};
    Vec3f y{0, 1, 0};
    Vec3f z = cross(x, y);
    CHECK(z.x == doctest::Approx(0.0f));
    CHECK(z.y == doctest::Approx(0.0f));
    CHECK(z.z == doctest::Approx(1.0f));

    // 반교환법칙: cross(y, x) = -cross(x, y)
    Vec3f neg_z = cross(y, x);
    CHECK(neg_z == -z);
}

TEST_CASE("Vec3 length & normalize") {
    Vec3f v{3, 4, 0};
    CHECK(length(v) == doctest::Approx(5.0f));

    Vec3f n = normalize(v);
    CHECK(length(n) == doctest::Approx(1.0f));
    CHECK(n.x == doctest::Approx(0.6f));
    CHECK(n.y == doctest::Approx(0.8f));
}

TEST_CASE("Vec3 lerp") {
    Vec3f a{0, 0, 0};
    Vec3f b{10, 20, 30};
    auto mid = lerp(a, b, 0.5f);
    CHECK(mid == Vec3f{5, 10, 15});
}

// ── Vec3 constexpr ──

TEST_CASE("Vec3 constexpr 검증") {
    // 이것들이 컴파일된다는 것 자체가 constexpr 동작 증명
    constexpr Vec3f a{1, 2, 3};
    constexpr Vec3f b{4, 5, 6};
    constexpr auto c = a + b;
    constexpr auto d = dot(a, b);

    CHECK(c == Vec3f{5, 7, 9});
    CHECK(d == doctest::Approx(32.0f));
}
```

```cpp
// tests/test_math_mat.cpp

#include <doctest/doctest.h>
#include <gazeshot/core/math/Math.hpp>

using namespace gazeshot::core::math;

TEST_CASE("Mat4 identity") {
    auto I = Mat4f::identity();
    CHECK(I[0].x == 1.0f);
    CHECK(I[1].y == 1.0f);
    CHECK(I[2].z == 1.0f);
    CHECK(I[3].w == 1.0f);

    // 비대각 원소는 0
    CHECK(I[0].y == 0.0f);
    CHECK(I[1].x == 0.0f);
}

TEST_CASE("Mat4 identity * identity = identity") {
    auto I = Mat4f::identity();
    auto result = I * I;
    CHECK(result == I);
}

TEST_CASE("Mat4 * Vec4") {
    auto I = Mat4f::identity();
    Vec4f v{1, 2, 3, 1};
    auto result = I * v;
    CHECK(result == v);
}

TEST_CASE("Mat4 이동 행렬") {
    // row-major: m[row][col] — 수학 표기 그대로
    // [1  0  0  tx]
    // [0  1  0  ty]
    // [0  0  1  tz]
    // [0  0  0  1 ]
    auto m = Mat4f::identity();
    m[0][3] = 5;
    m[1][3] = 10;
    m[2][3] = 15;

    Vec4f point{0, 0, 0, 1};     // w=1: 점(이동 영향 받음)
    auto moved = m * point;
    CHECK(moved.x == doctest::Approx(5.0f));
    CHECK(moved.y == doctest::Approx(10.0f));
    CHECK(moved.z == doctest::Approx(15.0f));

    Vec4f dir{1, 0, 0, 0};       // w=0: 방향(이동 영향 안 받음)
    auto same = m * dir;
    CHECK(same.x == doctest::Approx(1.0f));
    CHECK(same.y == doctest::Approx(0.0f));
}

TEST_CASE("Mat4 inverse") {
    auto m = Mat4f::identity();
    m[0][3] = 5;
    m[1][3] = 10;
    m[2][3] = 15;

    auto inv = inverse(m);
    auto result = m * inv;

    auto I = Mat4f::identity();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            CHECK(result[r][c] == doctest::Approx(I[r][c]).epsilon(1e-5));
        }
    }
}

TEST_CASE("Mat4 transpose") {
    // row-major: 각 Vec4가 행
    Mat4f m{
        Vec4f{1, 2, 3, 4},
        Vec4f{5, 6, 7, 8},
        Vec4f{9, 10, 11, 12},
        Vec4f{13, 14, 15, 16}
    };
    auto t = transpose(m);
    CHECK(t[0] == Vec4f{1, 5, 9, 13});
    CHECK(t[1] == Vec4f{2, 6, 10, 14});
    CHECK(t[2] == Vec4f{3, 7, 11, 15});
    CHECK(t[3] == Vec4f{4, 8, 12, 16});
}

TEST_CASE("Mat4 data() 메모리 레이아웃") {
    auto m = Mat4f::identity();
    const float* ptr = m.data();

    // row-major: 첫 4개 = 첫 번째 행
    CHECK(ptr[0]  == 1.0f);  // m[0][0]
    CHECK(ptr[1]  == 0.0f);  // m[0][1]
    CHECK(ptr[4]  == 0.0f);  // m[1][0]
    CHECK(ptr[5]  == 1.0f);  // m[1][1]
    CHECK(ptr[15] == 1.0f);  // m[3][3]
}
```

CMake에 테스트 추가:

```cmake
# tests/CMakeLists.txt (수정)

add_executable(gazeshot_tests
    test_types.cpp
    test_math_vec.cpp
    test_math_mat.cpp
)

target_link_libraries(gazeshot_tests
    PRIVATE
        gazeshot_core
        doctest::doctest
)

target_compile_definitions(gazeshot_tests
    PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

add_test(NAME gazeshot_tests COMMAND gazeshot_tests)
```

### Step 6: 데모 — 회전하는 삼각형

main.cpp를 수정하여 커스텀 수학으로 삼각형을 회전시킨다.

> 아직 렌더러 추상화 전이므로 직접 GL 호출을 사용한다.
> Ch.04에서 정리할 예정.

```cpp
// game/src/main.cpp (Ch.02 데모)

#include <gazeshot/platform/Window.hpp>
#include <gazeshot/core/math/Math.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <cmath>
#include <cstdio>

using namespace gazeshot::core::math;

// ── 인라인 셰이더 (GLSL 300 es / 330 core 호환) ──
#ifdef __EMSCRIPTEN__
static const char* VERT_SRC = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() {
    gl_Position = uTransform * vec4(aPos, 1.0);
}
)";
static const char* FRAG_SRC = R"(#version 300 es
precision mediump float;
out vec4 FragColor;
void main() {
    FragColor = vec4(0.95, 0.55, 0.15, 1.0);  // 오렌지
}
)";
#else
static const char* VERT_SRC = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uTransform;
void main() {
    gl_Position = uTransform * vec4(aPos, 1.0);
}
)";
static const char* FRAG_SRC = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.95, 0.55, 0.15, 1.0);
}
)";
#endif

// ── Z축 회전 행렬 (커스텀 수학!) ──
Mat4f rotateZ(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Mat4f m = Mat4f::identity();
    // row-major: m[row][col] = 수학 표기 그대로
    // [ c  -s  0  0 ]
    // [ s   c  0  0 ]
    // [ 0   0  1  0 ]
    // [ 0   0  0  1 ]
    m[0][0] = c;   m[0][1] = -s;
    m[1][0] = s;   m[1][1] =  c;
    return m;
}

struct App {
    gazeshot::platform::Window window;
    unsigned int shaderProgram = 0;
    unsigned int vao = 0;
    float time = 0.0f;
};

unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return shader;
}

void initGL(App& app) {
    // 셰이더
    auto vs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
    auto fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
    app.shaderProgram = glCreateProgram();
    glAttachShader(app.shaderProgram, vs);
    glAttachShader(app.shaderProgram, fs);
    glLinkProgram(app.shaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 삼각형 정점
    float vertices[] = {
         0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
    };

    unsigned int vbo;
    glGenVertexArrays(1, &app.vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(app.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
}

void oneFrame(void* arg) {
    auto* app = static_cast<App*>(arg);
    app->window.pollEvents();

    if (app->window.shouldClose()) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    app->time += 1.0f / 60.0f;  // 간이 타이머

    // ── 커스텀 수학으로 회전 행렬 생성 ──
    Mat4f transform = rotateZ(app->time);

    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(app->shaderProgram);

    // ── 커스텀 Mat4의 data()로 uniform 전달 ──
    // GL_TRUE: row-major 데이터를 GPU가 전치해서 받도록 지시
    int loc = glGetUniformLocation(app->shaderProgram, "uTransform");
    glUniformMatrix4fv(loc, 1, GL_TRUE, transform.data());

    glBindVertexArray(app->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    app->window.swapBuffers();
}

int main(int, char*[]) {
    App app{
        .window = gazeshot::platform::Window({
            .title = "GazeShot — Ch.02 Rotating Triangle",
        }),
    };

    initGL(app);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(oneFrame, &app, 0, true);
#else
    while (!app.window.shouldClose()) {
        oneFrame(&app);
    }
#endif
    return 0;
}
```

**핵심 순간**: `glUniformMatrix4fv(loc, 1, GL_TRUE, transform.data())`

- `GL_TRUE` = "이 데이터는 row-major이니 전치해서 받아라"
- `transform.data()` = `&rows[0].x` → 16개 float의 시작 주소

OpenGL은 내부적으로 column-major를 기대하지만, transpose 플래그 하나로 해결된다.
우리가 row-major를 선택한 이유는 코드의 직관성 때문이다:
`m[0][1]`이 "0행 1열"이라는 수학적 의미와 정확히 일치한다.

---

## 4. 검증 체크리스트

| 항목 | 확인 방법 |
|------|----------|
| 테스트 PASS | `ctest --output-on-failure` 전체 통과 |
| 회전 방향 | 반시계 방향으로 삼각형이 돈다 |
| WASM 동작 | 브라우저에서도 동일하게 회전 |
| constexpr | `static_assert` 또는 `constexpr auto` 컴파일 성공 |
| GLM 미사용 | `#include <glm>` 없이 빌드 |

---

## 5. 블로그 데모 아이디어

1. **회전 삼각형 GIF**: Desktop에서 캡처
2. **코드 하이라이트**: `rotateZ()` 함수 — "이 5줄이 GLM::rotate()를 대체한다"
3. **테스트 출력**: doctest 결과 스크린샷 (30+ 케이스 PASS)
4. **메모리 레이아웃 다이어그램**: row-major vs column-major 비교 그림 포함
5. **브라우저 데모**: WASM 빌드를 블로그에 임베딩하면 독자가 직접 돌려볼 수 있다

---

## 6. 심화 읽기

- [GLM 소스 코드](https://github.com/g-truc/glm) — 우리 구현과 비교
- [Handmade Math](https://github.com/HandmadeMath/HandmadeMath) — 유사한 싱글헤더 수학 라이브러리
- [OpenGL Matrix FAQ](https://www.opengl.org/archives/resources/faq/technical/transformations.htm)

---

## 다음 챕터 예고

**Chapter 03: 커스텀 수학 라이브러리 (2) — 변환과 투영**

`translate()`, `rotate()`, `scale()`, `lookAt()`, `perspective()`를 구현한다.
데모: 커스텀 MVP 행렬로 3D 큐브가 화면에서 회전한다.
