# meta_stream 用户手册

一个头文件、零依赖（C++23 起）的类型流处理库。它把"遍历一组类型"这件事写成和写 STL 流相似的 API：一侧是**输入流（istream）**负责吐出类型，另一侧是**输出流（ostream）**负责接收类型，中间用 `transfer` / `meta_for` 把类型从 istream 搬到 ostream。

整个库分两层：

| 层 | 命名空间 | 职责 |
|---|---|---|
| 基础工具层 | `exp_utilities` | `exp_list`、`meta_object`、`meta_looper` 等类型级原语 |
| 流层（本文重点） | `exp_utilities::meta_ios` | 预置的 istream / ostream、`transfer`、`meta_for`、协议、pipe |

> 下文默认你已经 `using namespace meta_ios;`。

---

## 1. 核心概念

### 1.1 `exp_list<Ts...>`

类型列表，库内所有流的"状态"本质上都是一个 `exp_list`。

```cpp
using L = exp_list<int, char, double>;

L::length;                    // 3
L::front;                     // int
L::back;                      // double
L::at<1>;                     // char
L::pop_front;                 // exp_list<char, double>
L::pop_back;                  // exp_list<int, char>
L::push_back<float>;          // exp_list<int, char, double, float>
L::push_front<void>;          // exp_list<void, int, char, double>

L::for_each<std::add_pointer>; // exp_list<int*, char*, double*>
L::to<std::tuple>;            // std::tuple<int, char, double>
```

### 1.2 `meta_object` 与 `meta_ret_object`

流不是普通的类，而是一个**带状态的纯函数**。库里用两个模板表示：

```cpp
// 状态 OBJ，更新函数 F；apply<Args...> 让状态往前走一步
template<class OBJ, class F> struct meta_object {
    using type = OBJ;                          // 当前状态
    template<class... Args> using apply = ...;  // 下一步
    template<class X>       using meta_set = ...; // 直接换状态
};

// 多一个 ::ret，用于 istream——"当前吐出的元素"
template<class OBJ, class F, class Ret> struct meta_ret_object {
    using type = OBJ;   // 状态（剩余未读类型）
    using ret  = ...;   // 当前元素（即将被读出的那个）
    template<class... Args> using apply = ...;  // 读完当前，推进一步
};
```

- **istream** 是 `meta_ret_object`：`::ret` 给当前元素，`apply` 推到下一个。
- **ostream** 是 `meta_object`：`::type` 是累积结果，`apply(this_state, from_ins)` 吸收一个新元素。

`meta_invoke<F, Args...>` 是"调用一个元函数"的统一入口（等价于 `F::template apply<Args...>::type`）。

### 1.3 `meta_looper`（原理，不必深入）

`meta_looper<Cond, OBJ, Gen>` 是一个编译期递归引擎：每一步用 `Cond` 判断要不要继续，继续就对 `OBJ` 应用一次 `F`，再递归。`transfer` / `meta_for` 都是它的特化。运行期的 `for_each` 只是把这条递归链上的每一步回调给一个用户函数——**类型在编译期走完，回调在运行期执行**。

你日常不需要直接碰 `meta_looper`，只要知道它是 `transfer` 和 `meta_for` 背后的引擎即可。

---

## 2. 纯元编程传输

### 2.1 `transfer<N, To, From, break_f>`

把 `From` 的前 N 个元素搬到 `To`，全部在编译期完成，得到一个最终的 `meta_stream`：

```cpp
using result = transfer<3,
                        meta_ostream<exp_list<>>,     // 空的输出流
                        meta_istream_list<int, char, double, float>
                       >;
// result::to::type  == exp_list<int, char, double>
// result::from::type == exp_list<float>          （剩下没读的）
```

- `N`：传多少个。
- `To`：ostream。
- `From`：istream。
- `break_f`（可选）：提前中断条件，默认 `meta_always_continue`。

### 2.2 `meta_all_transfer<To, From, break_f>`

等价于"把 `From` 全部传完"，即 `N = From` 的长度。

```cpp
using result = meta_all_transfer<meta_ostream<exp_list<>>,
                                 meta_istream_list<int, char, double>>;
// result::to::type == exp_list<int, char, double>
// result::from::type == exp_list<>   （读完了）
```

### 2.3 `transfer_until<To, From, break_f>`

一直传，直到 `break_f` 对当前流状态判定为真。`N` 被设为"无穷大"，靠 `break_f` 停下。

```cpp
// 读到一个对齐地址就停（meta_aligned_iterator 内部就是这么用的）
using result = transfer_until<meta_aligned_iterator,
                              meta_index_istream<0>,
                              protocols::only_stream_to_unref<...>>;
```

> 警告：如果 `break_f` 永远不为真，会触发无限递归 / 模板实例化爆炸。务必保证它能在有限步内命中。

---

## 3. 运行时桥接：`meta_for`

`transfer` 系列在编译期把类型搬完，但你往往想在运行期对每一步做点事（写内存、打印、调度）。`meta_for` 就是这个桥：

```cpp
template<class To, class From, class break_f = meta_always_continue>
struct meta_for {
    template<class F, class... Args>
    static constexpr void for_each(F&& f, Args&&... args);
};
```

每次迭代，`f` 收到一个 `meta_stream` 对象：

```cpp
meta_for<meta_aligned_iterator,
         meta_istream_list<char, double, int>>::for_each(
    [](auto stream) {
        std::cout << stream.value()        // 编译期常量（consteval），当前元素的位置
                  << " "
                  << stream.target_type(); // std::type_info，当前元素的类型
        stream.object();                   // ostream 当前状态对象（to_t{}）
        stream.left();                     // 剩余未读元素个数（运行期 size_t）
    });
```

`meta_stream` 上可用的成员：

| 成员 | 说明 |
|---|---|
| `.object()` | 返回 `to_t{}`，即 ostream 当前状态的一个实例 |
| `.value()` | `consteval`，取 `to_t::value`（若有），否则 `no_exist_type` |
| `.target_type()` | `typeid(to_t)` |
| `.left()` | 剩余元素个数（`exp_size<from_t>`） |
| `::to_t` / `::from_t` / `::cache` | 类型别名，分别是 ostream 状态 / istream 剩余 / istream 的 cache |

---

## 4. 预置输入流（istream）

所有 istream 都满足：有 `::type`（剩余状态）、`::ret`（当前元素）、`apply`（推进）。

### 4.1 `meta_istream<TL>` / `meta_istream_list<Tys...>`

最普通的类型列表输入流：

```cpp
using is = meta_istream_list<int, char, double>;
// is::type == exp_list<int, char, double>
// is::ret  == int
// is::apply == 推进一步的新 istream
```

### 4.2 `meta_index_istream<start>`

**永不结束**，每次吐出一个递增的索引 `Idx<i>`：

```cpp
using is = meta_index_istream<0>;
// is::ret       == Idx<0>
// is::apply::ret == Idx<1>
// ... 永远有下一个
```

适合配合 ostream 做"按编号生成"。

### 4.3 `meta_count<start, count>` / `meta_count_istream<start, count>`

从 `start` 起、数 `count` 个就停：

```cpp
using is = meta_count_istream<10, 3>;
// 依次给 Idx<10>, Idx<11>, Idx<12>，然后结束
```

### 4.4 `meta_repeat_istream<T>`

**永不结束**，每次都吐出同一个 `T`：

```cpp
using is = meta_repeat_istream<int>;
// 每次 ::ret == int
```

### 4.5 `meta_char_istream<static_str>`

把一个编译期字符串逐字符读出，`'0'` 终止。`static_str` 是库提供的编译期字符串字面量包装。

### 4.6 `meta_transform_istream<TL, F>`

读元素时先过一道 `F` 变换再吐出来：

```cpp
using is = meta_transform_istream<exp_list<int, char>, std::add_pointer>;
// is::ret == int*
```

---

## 5. 预置输出流（ostream）

所有 ostream 都满足：有 `::type`（累积结果）、`apply(this_state, from_ins)`（吸收一个元素）。

### 5.1 `meta_ostream<TL>`

最普通的输出流：把吸收到的元素 `push_back` 到一个 `exp_list`。

```cpp
using os = meta_ostream<exp_list<>>;
// 吸收 int, char 后，::type == exp_list<int, char>
```

### 5.2 `meta_transform_ostream<TL, F>`

吸收时用 `F(this_list, from_ins)` 变换，再把结果放进列表：

```cpp
// 例：把每个读到的类型 T 变成 T* 放进列表
struct to_ptr_f {
    template<class this_list, class T>
    using apply = T*;
};
using os = meta_transform_ostream<exp_list<>, to_ptr_f>;
```

### 5.3 `meta_filter_ostream<TL, filter>`

`filter(this_list, from_ins)::value` 为真才收下，否则跳过：

```cpp
// 只收 int
struct only_int_f {
    template<class this_list, class T>
    struct apply : std::is_same<T, int> {};
};
using os = meta_filter_ostream<exp_list<>, only_int_f>;
```

### 5.4 `meta_jostream<TL>`

"展开"流：如果 istream 读出来的本身是个类型列表，就把它的元素逐个并入 ostream（而不是把列表本身当成一个元素）。

### 5.5 `meta_forward_ostream<TL, F>`

按索引前进。`F` 接收"当前 ostream 列表"和"istream 刚给的元素"，决定下一步。默认 `F = binary<default_combine>`，即 `exp_list<T1, T2>`。

### 5.6 `meta_iterator` / `meta_transform_iterator<F, init>`

- `meta_iterator`：一个"可替换状态"的 ostream，内部状态初始为 `no_exist_type`，每次 `apply(this_obj, T)` 直接把状态换成 `T`。常用于"把当前流状态换成另一个流对象"的场景。
- `meta_transform_iterator<F, init>`：状态由 `F(this_obj, from_ins)` 计算得到；若 `F` 提供 `initialize`，则初始化阶段也会走它。

### 5.7 `meta_aligned_iterator`

运行期字节流对齐迭代器。它不是把类型累积成 `exp_list`，而是在一个 `std::byte*` 缓冲区里为每个类型算出对齐后的偏移，并负责 placement new / destroy：

```cpp
meta_for<meta_aligned_iterator, meta_istream_list<char, double, int>>::for_each(
    [&](auto stream) {
        using seek_t = decltype(stream.object());
        // stream.value()  == 该类型在 buffer 中的对齐偏移
        seek_t s = stream.object();
        s.emplace(buf, T{...});            // const& 版本
        s.emplace(buf, std::move(t));      // && 版本（内部 std::move）
        s.get(buf);                        // 取引用
        s.destroy(buf);                    // 析构
        s.address_of(buf);                 // 返回 buf + offset
    });
```

对齐计算是 O(1)：`(start + alignof(T) - 1) & ~(alignof(T) - 1)`。

---

## 6. 协议（protocols）

ostream 的 `apply` 函数在被调用时，库里会传给它"哪些参数"——当前流状态 `this`、istream 刚给的元素 `from`、istream 的 `cache`。协议（protocol）就是用来**固定把哪个位置传给元函数**的一层包装：

| 协议 | 等价调用 | 用途 |
|---|---|---|
| `only_this<F>` | `F(this)` | 只看 ostream 当前状态 |
| `only_arg<F>` | `F(from)` | 只看刚读到的元素 |
| `stream_to_unref<F>` | `F(this::type, from)` | 二参：当前状态 + 新元素 |
| `only_stream_to_unref<F>` | `F(from)` | 只读 from |
| `only_stream_from_unref<F>` | `F(from)` | 只读 from |
| `stream_cache_unref<F>` | `F(cache, from)` | 二参：istream 缓存 + 新元素 |
| `only_stream_cache_unref<F>` | `F(cache)` | 只读 istream 缓存 |

协议用在 `transfer` / `meta_all_transfer` 的最后一个模板参数 `break_f` 上，也用于 `meta_filter_ostream` 的判定函数。

辅助类型：

- `protocols::stream_to_t<meta_stream_type>` / `stream_from_t<...>` / `stream_cache_t<...>`：取当前流的三部分类型。
- `protocols::forward_last<TL>`：取类型列表的最后一个元素。
- `protocols::meta_stream_skip_signal` / `is_skip_signal<T>`：在协议折叠时用于"跳过本次"的信号。

---

## 7. pipe 管道

`pipe::transfer<is>` 把一个 istream 包成管道起点，再依次接上 ostream，形成"管道"。它返回一个 `transfer_pipe`：

```cpp
using pipe = pipe::transfer< meta_istream_list<int, char, double> >
                 ::to< 2, meta_ostream<exp_list<>> >;
// pipe::from   —— 组合后的 istream
// pipe::transfer —— 等价的 transfer<2, os, is>
```

`transfer_pipe` 上的方法：

| 方法 | 作用 |
|---|---|
| `::to<Nc, another_os, ps...>` | 把另一个 ostream 接到现有管道之后，传 Nc 个 |
| `::all_to<another_os, ps...>` | 把另一个 ostream 接上，传完所有剩余 |
| `::skip_to<Nc, another_os, break_f, reset_f, ps...>` | 带 break / reset 的管道分支 |

`ps...` 是一组协议（见第 6 节），决定每一跳如何把流状态传给下一个 ostream。

---

## 8. 自定义输入流 / 输出流

只要满足"是一个 meta_object / meta_ret_object"，就能和 `transfer` / `meta_for` / `pipe` 配套使用（`is_meta_object_v` concept 会自动识别）。

### 8.1 自定义 ostream

做一个 `meta_object<State, F>`：`State` 是你累积的结果类型，`F` 提供 `apply<this_state, from_ins>::type` 返回新状态。

```cpp
// 例：把读到的类型都包成 std::vector<T>::value_type（伪）
struct my_append_f {
    template<class this_list, class T>
    using apply = exp_list<typename this_list::template push_back<T>>;
};
using my_ostream = meta_object<exp_list<>, my_append_f>;
```

如果你的 `F` 需要初始化阶段，可以额外提供 `initialize`，库会自动走 `meta_object` 的另一条偏特化。

### 8.2 自定义 istream

做一个 `meta_ret_object<State, F, Ret>`：`State` 是剩余未读内容，`F::apply` 推进一步，`Ret` 返回当前要吐出的元素。

```cpp
// 例：从 exp_list 里每次吐一个（等价于 meta_istream，但展示机制）
struct my_dec_f {
    template<class this_list> using apply = typename this_list::pop_front;
};
struct my_pop_f {
    template<class this_list> using apply = typename this_list::front;
};
using my_istream = meta_ret_object<exp_list<int, char, double>,
                                   my_dec_f, my_pop_f>;
```

`meta_index_istream`、`meta_repeat_istream` 都是这个模式的实例化——直接照着抄即可。

### 8.3 运行期行为

如果你希望流对象在运行期也携带行为（像 `meta_aligned_iterator` 那样有 `emplace` / `get` / `destroy`），就在 ostream 的状态类型（即 `to_t`，也就是 `meta_object` 的第一个模板参数）上直接加成员函数和 `static constexpr` 常量。`meta_for` 的回调里通过 `stream.object()` 拿到它的实例。

---

## 9. C++26 反射适配（可选）

在 GCC 16+（`-std=c++26 -freflection`）下，头文件会额外提供：

```cpp
template<class T, auto Ctx = ::std::meta::access_context::unprivileged()>
struct reflected_member_types {
    using type = exp_list< /* T 的每个非静态数据成员的类型 */ >;
};
```

把一个 struct 的字段类型自动反射成 `exp_list`，直接喂给 `meta_istream`：

```cpp
#include <iostream>
#include <meta>            // GCC 16.2 的 include 顺序 bug：必须先于本头文件
#include "meta_stream.hpp"
using namespace meta_ios;

struct Demo { char a; double b; int c; };

meta_for<meta_aligned_iterator,
         meta_istream<reflected_member_types<Demo>::type>>::for_each(
    [](auto stream) {
        std::cout << stream.value() << " " << stream.target_type() << "\n";
    });
```

> **GCC 16.2 已知问题**：必须在翻译单元最前面先 `#include <meta>`，否则它展开的 `<source_location>` 会报 `std::source_location is not a member of std`。这是标准库 bug，等上游修复后即可去掉。

在 C++23 下，`reflected_member_types` 整段被 `#if defined(__cpp_impl_reflection)` 屏蔽，库其余部分不受影响。

---

## 10. 快速上手清单

```cpp
#include "meta_stream.hpp"
using namespace meta_ios;

int main() {
    // 1. 纯编译期：把一组类型收集进一个 exp_list
    using collected = meta_all_transfer<meta_ostream<exp_list<>>,
                                        meta_istream_list<int, char, double>>::to::type;

    // 2. 运行期：每步回调一个 meta_stream
    meta_for<meta_ostream<exp_list<>>, meta_istream_list<int, char, double>>
        ::for_each([](auto s) {
            std::cout << "left=" << s.left()
                      << " type=" << s.target_type().name() << "\n";
        });
}
```
