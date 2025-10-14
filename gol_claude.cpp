/*****************************************************************************

    Goal: Simulate Life on a toroidal grid.

    Input seed from file or random; steps --steps M, size --rows R --cols C
    Print each generation (or only final with --final-only).

*****************************************************************************/

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

struct Option {
    Option() :
        steps(100),
        rows(20),
        cols(20),
        final_only(false),
        detect_cycle(false)
    {}

    bool get(int argc, char *argv[])
    {
        if (argc > 1)
        {
            int c;
            while (1)
            {
                static struct option longOpt[] = {
                    { "steps", required_argument, 0, 's'},
                    { "rows", required_argument, 0, 'r'},
                    { "cols", required_argument, 0, 'c'},
                    { "final-only", no_argument, 0, 'f'},
                    { "detect-cycle", no_argument, 0, 'd'},
                    { 0, 0, 0, 0 }
                };

                int index = 0;
                c = getopt_long(argc, argv, "s:r:c:fd", longOpt, &index);
                if (c == -1)
                    break;

                switch (c)
                {
                case 's':
                    steps = std::atoi(optarg);
                    if (steps <= 0) {
                        std::cerr << "Error: Steps must be > 0" << std::endl;
                        return false;
                    }
                    break;
                case 'r':
                    rows = std::atoi(optarg);
                    if (rows <= 0) {
                        std::cerr << "Error: Rows must be > 0" << std::endl;
                        return false;
                    }
                    break;
                case 'c':
                    cols = std::atoi(optarg);
                    if (cols <= 0) {
                        std::cerr << "Error: Cols must be > 0" << std::endl;
                        return false;
                    }
                    break;
                case 'f':
                    final_only = true;
                    break;
                case 'd':
                    detect_cycle = true;
                    break;
                default:
                    std::cerr << "Unknown option" << std::endl;
                    return false;
                }
            }
        }
        return true;
    }

    void print() const
    {
        std::cout << "Options" << std::endl;
        std::cout << "  steps      : " << steps << std::endl;
        std::cout << "  rows       : " << rows << std::endl;
        std::cout << "  cols       : " << cols << std::endl;
        std::cout << "  final only : " << (final_only ? "true" : "false") << std::endl;
        std::cout << "  detect cyc : " << (detect_cycle ? "true" : "false") << std::endl;
        std::cout << std::endl;
    }

    int steps;
    int rows;
    int cols;
    bool final_only;
    bool detect_cycle;
};

class GameOfLife
{
public:
    GameOfLife() : _row(0), _col(0), _steps(0), _finalOnly(false), _detectCycle(false) {}

    void initBoard(int row, int col, int steps, bool finalOnly, bool detectCycle)
    {
        if (row <= 0 || col <= 0 || steps <= 0) {
            throw std::invalid_argument("Row, col, and steps must be positive");
        }

        _row = row;
        _col = col;
        _steps = steps;
        _finalOnly = finalOnly;
        _detectCycle = detectCycle;

        // Use flat vector for better cache locality
        _cell.resize(_row * _col);
        _nextCycle.resize(_row * _col);

        // Randomize the board
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 1);
        
        for (size_t i = 0; i < _cell.size(); i++) {
            _cell[i] = (distrib(gen) == 1);
        }

        if (_detectCycle) {
            _history.reserve(std::min(10000, _steps + 100));
        }
    }

    void step()
    {
        // Neighbor offsets for 8 directions
        static const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        static const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

        for (int i = 0; i < _row; i++) {
            for (int j = 0; j < _col; j++) {
                int count = 0;
                
                // Count live neighbors
                for (int d = 0; d < 8; d++) {
                    int ni = i + dx[d];
                    int nj = j + dy[d];

                    // Toroidal wrapping
                    if (ni < 0) ni += _row;
                    else if (ni >= _row) ni -= _row;

                    if (nj < 0) nj += _col;
                    else if (nj >= _col) nj -= _col;
                    
                    count += _cell[ni * _col + nj] ? 1 : 0;
                }
                
                // Game of Life rules
                int idx = i * _col + j;
                _nextCycle[idx] = (count == 3) || (_cell[idx] && count == 2);
            }
        }
    }

    void display() const
    {
        for (int i = 0; i < _row; i++) {
            for (int j = 0; j < _col; j++) {
                std::cout << (_cell[i * _col + j] ? 'o' : '.');
            }
            std::cout << '\n';
        }
        std::cout << std::endl;
    }

    void start()
    {
        // Initial state
        if (!_finalOnly) {
            std::cout << "Cycle: 0" << std::endl;
            display();
        }

        bool boardEmpty = false;
        bool boardStatic = false;

        for (int i = 0; i < _steps; i++) {
            // Cycle detection
            if (_detectCycle) {
                size_t hash = hashState();
                if (_history.find(hash) != _history.end()) {
                    std::cout << "Cycle detected at step " << i << ". Stopping." << std::endl;
                    break;
                }
                _history.insert(hash);
            }

            size_t prevHash = hashState();
            step();
            update();
            size_t currHash = hashState();

            // Check for static board
            if (!_detectCycle && prevHash == currHash) {
                if (!boardStatic) {
                    std::cout << "Board became static at step " << (i + 1) << ". Stopping." << std::endl;
                    boardStatic = true;
                    if (_finalOnly) {
                        std::cout << "Cycle: " << (i + 1) << std::endl;
                        display();
                    }
                    break;
                }
            }

            // Check for empty board
            if (currHash == 0 || isEmptyBoard()) {
                std::cout << "Board became empty at step " << (i + 1) << ". Stopping." << std::endl;
                boardEmpty = true;
                if (_finalOnly) {
                    std::cout << "Cycle: " << (i + 1) << std::endl;
                    display();
                }
                break;
            }

            if (!_finalOnly || i == (_steps - 1)) {
                std::cout << "Cycle: " << (i + 1) << std::endl;
                display();
            }
        }
    }

private:
    void update()
    {
        _cell.swap(_nextCycle);
    }

    bool isEmptyBoard() const
    {
        for (bool cell : _cell) {
            if (cell) return false;
        }
        return true;
    }

    static inline uint64_t splitmix64(uint64_t x)
    {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }

    static inline void hash_combine(uint64_t& h, uint64_t k)
    {
        k = splitmix64(k);
        h ^= k + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    size_t hashState() const
    {
        uint64_t h = 0x243f6a8885a308d3ull;
        hash_combine(h, (uint64_t)_row);
        hash_combine(h, (uint64_t)_col);

        uint64_t word = 0;
        int bitpos = 0;

        for (size_t i = 0; i < _cell.size(); ++i) {
            if (_cell[i]) word |= (uint64_t)1 << bitpos;
            ++bitpos;
            if (bitpos == 64) {
                hash_combine(h, word);
                word = 0;
                bitpos = 0;
            }
        }
        if (bitpos) hash_combine(h, word);
        return (size_t)h;
    }

    int _row;
    int _col;
    int _steps;
    bool _finalOnly;
    bool _detectCycle;

    std::vector<bool> _cell;
    std::vector<bool> _nextCycle;
    std::unordered_set<size_t> _history;
};

int main(int argc, char *argv[])
{
    Option option;
    if (!option.get(argc, argv)) {
        return 1;
    }
    option.print();

    try {
        GameOfLife gol;
        gol.initBoard(option.rows, option.cols, option.steps, 
                      option.final_only, option.detect_cycle);
        gol.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
