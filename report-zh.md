# Subtask 2 报告：ESet vs std::set

## 环境

- 操作系统：Ubuntu 20.04.6 LTS（WSL2）
- 内核：6.6.87.2-microsoft-standard-WSL2
- 内存：15 GiB
- 编译器：g++ 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)
- 编译参数：-O2 -std=c++17
- CPU：Intel(R) Core(TM) Ultra 9 285H
  - 基准速度：2.90 GHz
  - 插槽：1
  - 内核：16
  - 逻辑处理器：16
  - 虚拟化：已启用
  - L1 缓存：1.6 MB
  - L2 缓存：28.0 MB
  - L3 缓存：24.0 MB

## 方法与工作负载设计

### 设计原则

- 使用固定随机种子生成操作序列，保证 ESet 与 std::set 使用同一条操作轨迹。
- 统计每种操作的平均耗时与总耗时，并进行 5 次重复实验取均值与标准差。

### 参数

- Key 范围：[0, 1,000,000)
- 预填充：20,000 个不重复键
- 总操作数：100,000
- 试验次数：5

### 工作负载

1) mixed
- insert：50.0%
- erase：16.6%
- find：11.1%
- range：5.5%
- 迭代器（++/--）：16.8%

2) no_range（消融）
- 将 range 比例置为 0，并加到 find 上

### range 查询实现差异

- ESet：`range(l, r)` 使用子树大小，时间复杂度约 $O(\log n)$
- std::set：`distance(lower_bound(l), upper_bound(r))`，时间复杂度约 $O(k)$

## 结果

### mixed

操作计数：
- insert=50190, erase=16473, find=11186, range=5466, iter_next=8479, iter_prev=8206

总耗时（ns）：
- ESet：24,084,066（std 5,664,351）
- std::set：746,201,811（std 39,709,232）

平均耗时（ns）：

| 操作 | ESet | std::set |
|---|---:|---:|
| insert | 266.8 | 181.5 |
| erase | 259.4 | 206.0 |
| find | 185.4 | 149.1 |
| range | 351.5 | 133,506.0 |
| iter_next | 34.5 | 26.2 |
| iter_prev | 48.5 | 25.9 |

### no_range（消融）

操作计数：
- insert=50207, erase=16708, find=16560, range=0, iter_next=8261, iter_prev=8264

总耗时（ns）：
- ESet：22,392,553（std 3,323,349）
- std::set：14,551,204（std 724,042）

平均耗时（ns）：

| 操作 | ESet | std::set |
|---|---:|---:|
| insert | 252.8 | 152.7 |
| erase | 255.3 | 157.6 |
| find | 186.4 | 131.7 |
| iter_next | 32.8 | 25.8 |
| iter_prev | 44.6 | 22.5 |

## 分析

1) mixed 中 ESet 明显更快，主要原因是 range 的复杂度差异。
- std::set 的 range 计数需要遍历区间元素，range 操作耗时远高于其他操作。
- ESet 使用子树大小，range 为 $O(\log n)$，因此在 mixed 工作负载下占据优势。

2) no_range 下 std::set 更快。
- 当移除 range 操作后，ESet 每次插入/删除维护 `sz` 的成本会带来额外开销。
- ESet 的节点为 `Key*`，存在额外分配与指针间接访问，缓存局部性也更差。

3) 迭代器操作仍是 std::set 略快。
- std::set 的迭代器实现经过高度优化；ESet 仍为直接树上爬升。

## 消融结论

- ESet 的性能优势来自 order-statistics（range 查询）。
- 一旦移除 range 查询，std::set 更快，说明 ESet 的维护成本大于其在常规集合操作上的收益。

## 复现步骤

```
cd /home/csxsi/ESet

g++ -std=c++17 -O2 bench.cpp -o bench
./bench
```

## 可行优化方向

- 将 `Key` 内联存储，减少额外的 `new Key` 分配开销。
- 使用内存池或 `std::pmr` 分配器改善节点分配效率。
- 在不需要 range 的模式下关闭 `sz` 维护。
- 优化迭代器路径（例如缓存常用方向或使用线索化结构）。
