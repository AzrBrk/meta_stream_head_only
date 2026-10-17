# 元对象：状态、步进规则与编译期循环

> 本库仍在开发中，公开 API 可能变化；使用时请关注后续更新，并以当前头文件实现为准。此检出版本的相关实现集中在根目录 `meta_stream.hpp`，并非拆分为 `meta_objects.hpp`、`meta_states.hpp` 等独立头文件。

本文介绍元对象，以及它们如何成为编译期类型流和循环的基础。重点是区分**承载状态的元对象**与**驱动递归的循环器**：元对象封装一个可推进的过程；只有循环器反复调用它，才形成循环。

## 1. 心智模型：payload、function、apply、meta_set

一个元对象可先用以下四项来理解：

| 概念 | 对应接口 | 含义 |
|---|---|---|
| 当前状态（payload） | `::type` | 对象此刻持有的类型，可以是类型列表，也可以是计数器、用户状态或任意其他类型 |
| 步进规则 | `::function`（普通对象、返回对象）或模板参数 `F` | 接收当前状态及本步参数，决定如何推进 |
| 下一状态 | `apply<Args...>` | 按步进规则生成一个新的元对象 |
| 替换状态 | `meta_set<Another>` | 保留原步进规则和其他配置，仅换掉当前状态 |

例如，`meta_object<OBJ, F>` 表示“当前状态是 `OBJ`，后续用 `F` 推进”。`apply` 不会在对象声明处自行启动无限递归；它只描述**一步**。连续对 `apply` 的结果再调用 `apply`，才形成状态链。

还要区分下列几样东西：

* **元对象**是过程的封装，通常带有状态和推进规则。
* **payload 类型列表**只是某种可能的状态，例如 `exp_list<int, char>`；它本身不是元对象。
* **产出投影**是从对象当前状态取出的值，例如输入对象的 `::ret` 或计时对象的 `::timer`；它们不等于整个对象，也不必等于 `::type`。
* **递归驱动器**（如 `meta_loop::meta_looper`）负责检查继续条件、调用步进、组织后续实例化以及终止。循环器调度对象，对象声明本身不负责循环。

## 2. `meta_invoke` 调用约定

当前实现中的调用入口为：

```cpp
template<class F, class... Args>
using meta_invoke = typename F::template apply<Args...>;
```

因此，元函数的 `apply` 通常写成返回目标类型的别名模板：

```cpp
struct next_state {
  template<class Current, class Input>
  using apply = /* 直接写出下一状态类型 */;
};
```

`meta_invoke<F, Args...>` 已经取得 `F::apply<Args...>` 的类型；它**不会再自动取这个结果的 `::type`**。若 `apply` 写成嵌套类模板而非别名模板，并且该类里有 `using type = ...`，则是否取 `::type` 必须由使用处明确处理，不能把两种协议混为一谈。`meta_object::apply`、`meta_ret_object::apply` 和 `meta_states_object::apply` 都直接给出下一元对象类型。

## 3. 普通元对象：`meta_object`

当前模板形态可简写为：

```cpp
template<class OBJ, class F>
struct meta_object {
  using function = F;
  using type = OBJ;

  template<class... Args>
  using apply = meta_object<meta_invoke<F, OBJ, Args...>, F>;

  template<class ANOTHER_OBJ>
  using meta_set = meta_object<ANOTHER_OBJ, F>;
};
```

`OBJ` 是 payload，`F` 是递归步进规则。例如：

```cpp
struct increment {
  template<class N>
  using apply = std::integral_constant<int, N::value + 1>;
};

using counter = meta_objects::meta_object<
    std::integral_constant<int, 0>, increment>;
using counter_1 = counter::apply<>;
using counter_2 = counter_1::apply<>;
static_assert(counter_2::type::value == 2);
```

这里的两次 `apply` 是显式的两个类型变换步骤；没有循环器时，并不会自动生成整条链。

### 3.1 已绑定与需要初始化的形式

已绑定对象直接提供 `OBJ` 和 `F`，例如 `meta_object<MyState, MyStep>`。需要在首次调用时初始化的对象可使用：

```cpp
template<class F>
using meta_object_construct =
    meta_object<meta_objects_details::meta_uninitialized, with_constructor<F>>;
```

这个形式以 `meta_objects_details::meta_uninitialized` 作为未构造标记。第一次 `apply` 会调用 `F::initialize<Uninit, Args...>`，后续才调用 `F::apply<Obj, Args...>`。构造行为来自 `with_constructor` 包装器，并非普通 `meta_object` 自带的自动初始化。`meta_object_init<F>` 是它的兼容别名。

## 4. 带返回投影的元对象：`meta_ret_object`

`meta_ret_object<OBJ, F, Ret>` 保留 `meta_object` 的状态推进模型，并增加 `::ret`：

```cpp
template<class OBJ, class F, class Ret>
struct meta_ret_object {
  using function = F;
  using ret = meta_invoke<Ret, OBJ>;
  using type = OBJ;

  template<class... Args>
  using apply = meta_ret_object<meta_invoke<F, OBJ, Args...>, F, Ret>;

  template<class ANOTHER_OBJ>
  using meta_set = meta_ret_object<ANOTHER_OBJ, F, Ret>;
};
```

* `::type` 是当前 payload。
* `::ret` 是用 `Ret` 从当前 payload 计算出的本步产出。
* `apply<Args...>` 推进 payload，并保留相同的 `F`、`Ret`。
* `meta_set<Another>` 替换 payload，但仍保留规则和返回投影。

`meta_ret_object` 没有独立的构造别名；其初始化需求可通过给 `F` 使用 `with_constructor` 来组合处理。

### 4.1 `meta_istream_list` 为什么不是“类型列表”

```cpp
using source = meta_ios::meta_istream_list<int, char, double>;
```

`meta_istream_list<Ts...>` 实际展开为 `meta_istream<exp_list<Ts...>>`，而 `meta_istream<TL>` 是 `meta_ret_object`。因此列表 `exp_list<int, char, double>` 是它的 payload；`source` 本身则是一个带步进规则和返回投影的元对象：

```cpp
using payload = typename source::type;  // exp_list<int, char, double>
using first = typename source::ret;     // int
using after_first = source::apply<>;
using remaining = typename after_first::type; // exp_list<char, double>
using second = typename after_first::ret;      // char
```

每次 `apply` 都通过 `pop_front` 消去当前列表头，再从新的 payload 产生下一个 `ret`。所以这是一种沿列表逐步 **reduce（消费/消去列表头）** 的推进过程；这里的 “reduce” 不意味着必须将元素折叠累积成单一结果。若需累积，通常由输出元对象的步进规则完成。

## 5. 状态元对象：`meta_states_object`

当前模板形态为：

```cpp
template<class OBJ, class F, class Changed_Pred,
         std::uint64_t byte_flag = 0, std::size_t flag_index = 0>
struct meta_states_object;
```

它额外跟踪“本步是否改变”的历史和操作码：

* `::type`：当前 payload。
* `::function`：状态更新规则 `F`。
* `::changed_pred`：本步变化判定 `Changed_Pred`。
* `::flags`：原始 `std::uint64_t` 标志字。低 56 位记录变化历史，高 8 位保留操作码。
* `::size`：标志记录位置/步数；达到 56 后低位历史会按实现规则重置。
* `::last_changed`：最近一步的变化位。
* `::at<I>()`：读取有效历史范围内第 `I` 个变化位。
* `meta_set<Another>`：替换 payload，保留规则、标志值和索引。
* `apply<Args...>`：计算新 payload 和标志，并直接产出下一 `meta_states_object`。

直接写 `meta_states_object<OBJ, F, Pred>` 时，初始 `byte_flag` 是数值 `0`、`flag_index` 是 `0`。而便捷包装器 `meta_make_states<States>` 会把用户 `States` 转为这个对象：payload 来自 `States::type`，更新函数包装 `States::apply` / `States::on_changed`，变化判定包装 `States::pred`；初始标志值则取 `States::opr_code`，若没有该成员，就取 `stream_op<>::opr_code`。因此不能把经 `meta_make_states` 构造的初始 `flags` 一概说成零。

对直接传给 `meta_states_object` 的 `F`，实现会先尝试在“本步已改变且存在 `on_changed`”时调用 `F::on_changed<OBJ, Args...>`；否则，如果 `F::apply` 接受额外的标志参数，就传入 `std::integral_constant<std::uint64_t, new_flags>`，不接受时则按普通参数调用 `F::apply<OBJ, Args...>`。**当前这个检出版本没有名为 `flags_operator` 的类型**；不要将它误写为这里的调用参数。经 `meta_make_states` 包装的用户 `States::apply` / `on_changed` 则是二参数转发接口。

## 6. 计时元对象：`meta_timer_object`

计时对象的实际模板形态为：

```cpp
template<std::size_t times, class OBJ, class F,
         class break_f = meta_always_continue>
struct meta_timer_object;
```

可以直接构造 `meta_timer_object<N, OBJ, F, Break>`，也可以将普通元对象转换为计时对象：

```cpp
using timed = meta_objects::to_timer<some_meta_object, 3>;
using stopped_by = meta_objects::break_if<timed, my_break_predicate>;
```

其中 `to_timer<OBJ, N, BF>` 从普通 `meta_object<obj, F>` 取出 `obj` 与 `F`，构造 `meta_timer_object<N, obj, F, BF>`；`break_if` 设置其终止条件。

* `::type` 仍是当前 payload `OBJ`，不是计时状态。
* `::timer` 是终止/继续状态的投影。断言 `break_f` 为真时，它是内部的 `meta_break_signal`；否则是 `std::integral_constant<bool, (times > 0)>`。
* `apply<Args...>` 在 break 信号出现时保持当前计时对象不变；否则调用 `F` 推进 payload，并把计数减一。
* `meta_set<Another>` 只替换 payload，保留次数、规则和终止条件；`reset<N>` 产生使用新次数的对象。

计时对象不提供构造初始化协议；库内也有注释说明它没有 initializer。

## 7. 循环器如何驱动元对象

在当前实现中，通用编译期驱动器是 `meta_loop::meta_looper`。它每一步大致执行：

1. 把当前对象投影到条件对象并检查是否继续；
2. 条件成立时运行生成器；
3. 对当前对象推进一次；
4. 递归处理新阶段，直到条件不成立。

`meta_for<To, From>` 使用这个循环器和计时对象，将输入流元素送入输出流；它不是 `meta_istream_list` 自己执行的递归。另一路状态驱动接口是 `meta_ios::meta_transfer_until` / `transfer_until`：它将 `To`、`From` 包装成 `meta_states_object`，在循环步骤中根据 `pred`、操作码和停止条件更新两条流。这样可以将“如何描述一步状态更新”与“何时继续、何时停止”的调度分开。

问题描述中提到的 `meta_objects.hpp`、`meta_states.hpp`、`meta_states_looper.hpp` 在当前检出版本中并非独立文件；以上两类用法均按当前根目录 `meta_stream.hpp` 的实现说明。本文只说明它们的关系，不将它们说成两个互不相关、由对象声明自动递归的机制。

## 8. 选择接口时的检查清单

1. 先问清楚 payload 是什么；不要仅凭“内部包含类型列表”就把整个对象当作类型列表。
2. 指明一步由哪个函数模板完成，以及它返回的是**下一状态类型**还是带 `::type` 的包装类。
3. `apply` 负责一步，`meta_set` 负责换 payload；需要连续步骤时再交给循环器或显式链式调用。
4. 分清 `::type`（当前状态）、`::ret`（当前输入产出）、`::flags`（状态标志）和 `::timer`（计时终止/继续投影）。
5. 在文档和调用处遵循当前 `meta_invoke` 协议：不要对已经是目标类型的 `apply` 结果再多取一次 `::type`。
