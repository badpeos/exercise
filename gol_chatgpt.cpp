// Game of Life — optimized C++ implementation
// - Flat buffers (std::vector<uint8_t>) for cache-friendliness
// - Precomputed neighbor indices (torus wrap) to avoid per-cell branches
// - Fast display (no per-cell I/O; builds lines, avoids endl flushes)
// - Robust input validation
// - Optional cycle detection via 64-bit hashing
// - Single-lookup unordered_set insert for cycle detection
// - Per-instance base hash cached in init (no static sharing bugs)
// - Hash packs bits into 64-bit words for fewer combines
// - Optional deterministic seeding via --seed
//
// Build (Linux/Clang/GCC):
//   g++ -O3 -std=c++17 -Wall -Wextra -pedantic gol_opt.cpp -o gol
//
// Usage:
//   ./gol -s <steps> -r <rows> -c <cols> [-f] [-d] [--seed N]
//     -s, --steps         number of generations (>=0)
//     -r, --rows          grid rows (>0)
//     -c, --cols          grid cols (>0)
//     -f, --final-only    print only the final board
//     -d, --detect-cycle  stop when a repeated state is detected
//         --seed N        use fixed RNG seed (uint64)

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

struct Options {
    int steps = -1;
    int rows  = -1;
    int cols  = -1;
    bool finalOnly = false;
    bool detectCycle = false;
    bool hasSeed = false;
    uint64_t seed = 0;
};

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " -s <steps> -r <rows> -c <cols> [-f] [-d] [--seed N]\n";
}

static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

static inline void hash_combine(uint64_t& h, uint64_t k) {
    k = splitmix64(k);
    // Similar spirit to boost::hash_combine
    h ^= k + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
}

class GameOfLife {
public:
    GameOfLife() = default;

    void init(int rows, int cols, bool finalOnly, bool detectCycle, std::mt19937_64& rng) {
        _row = rows; _col = cols;
        _finalOnly = finalOnly; _detectCycle = detectCycle;

        const size_t N = static_cast<size_t>(_row) * static_cast<size_t>(_col);
        _grid.assign(N, 0);
        _next.assign(N, 0);

        // Random initialize ~50% density
        std::uniform_int_distribution<int> bit(0, 1);
        for (size_t i = 0; i < N; ++i) _grid[i] = static_cast<uint8_t>(bit(rng));

        // Precompute neighbor indices (wrap-around torus)
        _prevR.resize(_row); _nextR.resize(_row);
        _prevC.resize(_col); _nextC.resize(_col);
        for (int i = 0; i < _row; ++i) {
            _prevR[i] = (i + _row - 1) % _row;
            _nextR[i] = (i + 1) % _row;
        }
        for (int j = 0; j < _col; ++j) {
            _prevC[j] = (j + _col - 1) % _col;
            _nextC[j] = (j + 1) % _col;
        }

        // Precompute base hash once per instance
        _baseHash = 0x243f6a8885a308d3ull;
        hash_combine(_baseHash, static_cast<uint64_t>(_row));
        hash_combine(_baseHash, static_cast<uint64_t>(_col));

        if (_detectCycle) {
            _history.clear();
            // Since we hash once per step, reserve ~steps
            _history.reserve(std::max(8, _expectedSteps + 4));
        }
    }

    void setExpectedSteps(int steps) { _expectedSteps = steps; }

    void display() const {
        std::string line; line.reserve(static_cast<size_t>(_col));
        const uint8_t* g = _grid.data();
        for (int i = 0; i < _row; ++i) {
            line.clear();
            const size_t base = static_cast<size_t>(i) * static_cast<size_t>(_col);
            for (int j = 0; j < _col; ++j) {
                line.push_back(g[base + static_cast<size_t>(j)] ? 'o' : '.');
            }
            std::cout << line << '\n';
        }
        std::cout << '\n';
    }

    void step() {
        const uint8_t* g = _grid.data();
        uint8_t* n = _next.data();
        const int rows = _row, cols = _col;

        for (int i = 0; i < rows; ++i) {
            const int i0 = _prevR[i], i1 = i, i2 = _nextR[i];
            const size_t base0 = static_cast<size_t>(i0) * cols;
            const size_t base1 = static_cast<size_t>(i1) * cols;
            const size_t base2 = static_cast<size_t>(i2) * cols;
            for (int j = 0; j < cols; ++j) {
                const int j0 = _prevC[j], j1 = j, j2 = _nextC[j];
                int count = 0;
                count += g[base0 + j0];
                count += g[base0 + j1];
                count += g[base0 + j2];
                count += g[base1 + j0];
                count += g[base1 + j2];
                count += g[base2 + j0];
                count += g[base2 + j1];
                count += g[base2 + j2];

                const uint8_t alive = g[base1 + j1];
                n[base1 + j1] = static_cast<uint8_t>((count == 3) || (alive && count == 2));
            }
        }
        _grid.swap(_next);
    }

    // Packs the current grid bits into 64-bit words and mixes them.
    size_t hashState() const {
        uint64_t h = _baseHash;
        uint64_t word = 0;
        int bitpos = 0;
        const size_t N = _grid.size();
        for (size_t idx = 0; idx < N; ++idx) {
            if (_grid[idx]) word |= (uint64_t)1 << bitpos;
            ++bitpos;
            if (bitpos == 64) {
                hash_combine(h, word);
                word = 0; bitpos = 0;
            }
        }
        if (bitpos) hash_combine(h, word);
        return (size_t)h;
    }

    void run(int steps, bool printFinalOnly) {
        if (!printFinalOnly) {
            std::cout << "Cycle: 0\n";
            display();
        }

        if (_detectCycle) {
            size_t s0 = hashState();
            _history.insert(s0);
        }

        for (int t = 1; t <= steps; ++t) {
            step();

            if (_detectCycle) {
                size_t s = hashState();
                auto [it, inserted] = _history.insert(s);
                if (!inserted) {
                    std::cout << "Cycle detected at generation " << t << ". Stopping.\n";
                    if (printFinalOnly) display();
                    return;
                }
            }

            if (!printFinalOnly) {
                std::cout << "Cycle: " << t << "\n";
                display();
            }
        }

        if (printFinalOnly) {
            std::cout << "Final cycle: " << steps << "\n";
            display();
        }
    }

private:
    int _row = 0, _col = 0;
    bool _finalOnly = false, _detectCycle = false;
    int _expectedSteps = 0;

    std::vector<uint8_t> _grid, _next;
    std::vector<int> _prevR, _nextR, _prevC, _nextC;

    uint64_t _baseHash = 0;
    std::unordered_set<size_t> _history;
};

static Options parse_options(int argc, char** argv) {
    Options opt;
    const char* shortOpt = "s:r:c:fd"; // -d has no argument
    const option longOpt[] = {
        {"steps",        required_argument, nullptr, 's'},
        {"rows",         required_argument, nullptr, 'r'},
        {"cols",         required_argument, nullptr, 'c'},
        {"final-only",   no_argument,       nullptr, 'f'},
        {"detect-cycle", no_argument,       nullptr, 'd'},
        {"seed",         required_argument, nullptr,  1 },
        {nullptr,         0,                 nullptr,  0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, shortOpt, longOpt, nullptr)) != -1) {
        switch (c) {
            case 's': opt.steps = std::atoi(optarg); break;
            case 'r': opt.rows  = std::atoi(optarg); break;
            case 'c': opt.cols  = std::atoi(optarg); break;
            case 'f': opt.finalOnly = true; break;
            case 'd': opt.detectCycle = true; break;
            case 1:   opt.hasSeed = true; opt.seed = static_cast<uint64_t>(std::strtoull(optarg, nullptr, 10)); break;
            default: print_usage(argv[0]); std::exit(EXIT_FAILURE);
        }
    }

    if (opt.steps < 0 || opt.rows <= 0 || opt.cols <= 0) {
        print_usage(argv[0]);
        std::cerr << "Error: steps must be >= 0, rows/cols must be > 0.\n";
        std::exit(EXIT_FAILURE);
    }

    return opt;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Options opt = parse_options(argc, argv);

    std::mt19937_64 rng;
    if (opt.hasSeed) rng.seed(opt.seed); else rng.seed(std::random_device{}());

    GameOfLife gol;
    gol.setExpectedSteps(opt.steps);
    gol.init(opt.rows, opt.cols, opt.finalOnly, opt.detectCycle, rng);
    gol.run(opt.steps, opt.finalOnly);
    return 0;
}
