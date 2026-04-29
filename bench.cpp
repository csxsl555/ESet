#include "test/src.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <iterator>
#include <random>
#include <set>
#include <string>
#include <vector>

enum class OpType {
    Insert,
    Erase,
    Find,
    Range,
    IterNext,
    IterPrev,
    Count
};

struct Op {
    OpType type;
    int key;
    int l;
    int r;
};

struct WorkloadConfig {
    std::string name;
    int n_ops;
    int key_range;
    int prefill;
    int p_insert;
    int p_erase;
    int p_find;
    int p_range;
    int p_iter;
};

struct ModelSet {
    std::vector<int> keys;
    std::vector<int> pos;

    explicit ModelSet(int key_range) : pos(key_range, -1) {}

    bool contains(int key) const { return pos[key] != -1; }

    void insert(int key) {
        if (pos[key] != -1)
            return;
        pos[key] = static_cast<int>(keys.size());
        keys.push_back(key);
    }

    void erase(int key) {
        int idx = pos[key];
        if (idx == -1)
            return;
        int last = keys.back();
        keys[idx] = last;
        pos[last] = idx;
        keys.pop_back();
        pos[key] = -1;
    }

    bool empty() const { return keys.empty(); }

    int random_existing(std::mt19937 &rng) const {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(keys.size() - 1));
        return keys[dist(rng)];
    }
};

static std::vector<int> make_prefill(ModelSet &model, std::mt19937 &rng, int prefill, int key_range) {
    std::vector<int> keys;
    keys.reserve(prefill);
    std::uniform_int_distribution<int> dist(0, key_range - 1);
    while (static_cast<int>(keys.size()) < prefill) {
        int k = dist(rng);
        if (!model.contains(k)) {
            model.insert(k);
            keys.push_back(k);
        }
    }
    return keys;
}

static std::vector<Op> generate_ops(const WorkloadConfig &cfg, std::mt19937 &rng, ModelSet &model) {
    std::vector<Op> ops;
    ops.reserve(cfg.n_ops);
    std::uniform_int_distribution<int> dist_key(0, cfg.key_range - 1);
    std::uniform_int_distribution<int> dist_roll(0, 999);

    int total_p = cfg.p_insert + cfg.p_erase + cfg.p_find + cfg.p_range + cfg.p_iter;
    int p_insert = cfg.p_insert;
    int p_erase = p_insert + cfg.p_erase;
    int p_find = p_erase + cfg.p_find;
    int p_range = p_find + cfg.p_range;

    for (int i = 0; i < cfg.n_ops; ++i) {
        int roll = dist_roll(rng) % total_p;
        Op op{};
        if (roll < p_insert) {
            op.type = OpType::Insert;
            int key = dist_key(rng);
            if (model.keys.size() < static_cast<size_t>(cfg.key_range) && (roll % 10) < 7) {
                int tries = 0;
                while (model.contains(key) && tries < 8) {
                    key = dist_key(rng);
                    ++tries;
                }
            }
            op.key = key;
            if (!model.contains(key))
                model.insert(key);
        } else if (roll < p_erase) {
            op.type = OpType::Erase;
            int key = dist_key(rng);
            if (!model.empty() && (roll % 10) < 8)
                key = model.random_existing(rng);
            op.key = key;
            if (model.contains(key))
                model.erase(key);
        } else if (roll < p_find) {
            op.type = OpType::Find;
            int key = dist_key(rng);
            if (!model.empty() && (roll % 2) == 0)
                key = model.random_existing(rng);
            op.key = key;
        } else if (roll < p_range) {
            op.type = OpType::Range;
            int a = dist_key(rng);
            int b = dist_key(rng);
            if (a <= b) {
                op.l = a;
                op.r = b;
            } else {
                op.l = b;
                op.r = a;
            }
        } else {
            op.type = (roll % 2 == 0) ? OpType::IterNext : OpType::IterPrev;
        }
        ops.push_back(op);
    }

    return ops;
}

struct RunResult {
    long long total_ns = 0;
    std::vector<long long> op_ns;
    std::vector<size_t> op_cnt;
};

template <typename SetType>
static size_t range_count(SetType &s, int l, int r) {
    auto lb = s.lower_bound(l);
    auto ub = s.upper_bound(r);
    return static_cast<size_t>(std::distance(lb, ub));
}

static size_t range_count(sjtu::ESet<int> &s, int l, int r) {
    return s.range(l, r);
}

template <typename SetType>
static RunResult run_ops(const std::vector<int> &prefill, const std::vector<Op> &ops) {
    SetType s;
    for (int k : prefill)
        s.emplace(k);

    typename SetType::iterator it = s.end();
    bool it_valid = false;
    volatile size_t sink = 0;

    RunResult result;
    result.op_ns.assign(static_cast<int>(OpType::Count), 0);
    result.op_cnt.assign(static_cast<int>(OpType::Count), 0);

    auto total_start = std::chrono::steady_clock::now();
    for (const auto &op : ops) {
        auto t0 = std::chrono::steady_clock::now();
        switch (op.type) {
        case OpType::Insert: {
            auto res = s.emplace(op.key);
            if (res.second) {
                it = res.first;
                it_valid = true;
            }
            break;
        }
        case OpType::Erase: {
            if (it_valid && it != s.end() && *it == op.key)
                it_valid = false;
            s.erase(op.key);
            break;
        }
        case OpType::Find: {
            auto it2 = s.find(op.key);
            if (it2 != s.end()) {
                it = it2;
                it_valid = true;
            }
            break;
        }
        case OpType::Range: {
            sink += range_count(s, op.l, op.r);
            break;
        }
        case OpType::IterNext: {
            if (it_valid) {
                ++it;
                if (it == s.end())
                    it_valid = false;
            }
            break;
        }
        case OpType::IterPrev: {
            if (it_valid && it != s.begin())
                --it;
            break;
        }
        default:
            break;
        }
        auto t1 = std::chrono::steady_clock::now();
        int idx = static_cast<int>(op.type);
        result.op_ns[idx] += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        result.op_cnt[idx] += 1;
    }
    auto total_end = std::chrono::steady_clock::now();
    result.total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(total_end - total_start).count();
    (void)sink;
    return result;
}

static double mean(const std::vector<double> &vals) {
    double sum = 0.0;
    for (double v : vals)
        sum += v;
    return vals.empty() ? 0.0 : sum / vals.size();
}

static double stddev(const std::vector<double> &vals, double m) {
    if (vals.size() < 2)
        return 0.0;
    double acc = 0.0;
    for (double v : vals) {
        double d = v - m;
        acc += d * d;
    }
    return std::sqrt(acc / (vals.size() - 1));
}

static void print_summary(const std::string &label, const std::vector<RunResult> &runs) {
    int op_count = static_cast<int>(OpType::Count);
    std::vector<double> totals;
    totals.reserve(runs.size());
    for (const auto &r : runs)
        totals.push_back(static_cast<double>(r.total_ns));
    double total_mean = mean(totals);
    double total_std = stddev(totals, total_mean);

    std::cout << label << " total_ns mean=" << static_cast<long long>(total_mean)
              << " std=" << static_cast<long long>(total_std) << "\n";

    for (int i = 0; i < op_count; ++i) {
        std::vector<double> per;
        per.reserve(runs.size());
        for (const auto &r : runs) {
            double avg = r.op_cnt[i] == 0 ? 0.0 : static_cast<double>(r.op_ns[i]) / r.op_cnt[i];
            per.push_back(avg);
        }
        double m = mean(per);
        double s = stddev(per, m);
        std::cout << "  op" << i << " avg_ns=" << m << " std=" << s << "\n";
    }
}

static void print_counts(const std::vector<Op> &ops) {
    std::vector<size_t> cnt(static_cast<int>(OpType::Count), 0);
    for (const auto &op : ops)
        cnt[static_cast<int>(op.type)]++;

    std::cout << "Counts: insert=" << cnt[static_cast<int>(OpType::Insert)]
              << " erase=" << cnt[static_cast<int>(OpType::Erase)]
              << " find=" << cnt[static_cast<int>(OpType::Find)]
              << " range=" << cnt[static_cast<int>(OpType::Range)]
              << " iter_next=" << cnt[static_cast<int>(OpType::IterNext)]
              << " iter_prev=" << cnt[static_cast<int>(OpType::IterPrev)] << "\n";
}

int main() {
    std::mt19937 rng(12345);

    WorkloadConfig mixed{
        "mixed",
        100000,
        1000000,
        20000,
        500,
        166,
        111,
        55,
        168
    };

    WorkloadConfig no_range = mixed;
    no_range.name = "no_range";
    no_range.p_range = 0;
    no_range.p_find += 55;

    std::vector<WorkloadConfig> workloads{mixed, no_range};

    for (const auto &cfg : workloads) {
        std::mt19937 rng_cfg(rng());
        ModelSet model(cfg.key_range);
        auto prefill = make_prefill(model, rng_cfg, cfg.prefill, cfg.key_range);
        auto ops = generate_ops(cfg, rng_cfg, model);

        std::cout << "Workload: " << cfg.name << " ops=" << cfg.n_ops
                  << " prefill=" << cfg.prefill << " key_range=" << cfg.key_range << "\n";
        print_counts(ops);

        std::vector<RunResult> eset_runs;
        std::vector<RunResult> stl_runs;

        const int trials = 5;
        for (int t = 0; t < trials; ++t) {
            eset_runs.push_back(run_ops<sjtu::ESet<int>>(prefill, ops));
            stl_runs.push_back(run_ops<std::set<int>>(prefill, ops));
        }

        print_summary("ESet", eset_runs);
        print_summary("std::set", stl_runs);
        std::cout << "---\n";
    }

    return 0;
}
