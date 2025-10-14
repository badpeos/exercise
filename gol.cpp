/*****************************************************************************

    Goal: Simulate Life on a toroidal grid.

    Input seed from file or random; steps –steps M, size –rows R –cols C
    Print each generation (or only final with –final-only).

*****************************************************************************/

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

struct Option {
    Option() :
        steps(-1),
        rows(-1),
        cols(-1),
        final_only(false),
        detect_cycle(false)
    {
    };

    void get(int argc, char *argv[])
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
                    if (steps <= 0) std::cerr << "Steps <= 0" << std::endl;
                    break;
                case 'r':
                    rows = std::atoi(optarg);
                    if (rows <= 0) std::cerr << "Rows <= 0" << std::endl;
                    break;
                case 'c':
                    cols = std::atoi(optarg);
                    if (cols <= 0) std::cerr << "Cols <= 0" << std::endl;
                    break;
                case 'f':
                    final_only = true;
                    break;
                case 'd':
                    detect_cycle = true;
                    break;
                default:
                    std::cerr << "Unknown option" << std::endl;
                    break;
                }
            }
        }
    }

    void print()
    {
        std::cout << "Options   " << std::endl;
        std::cout << "steps      : " << steps << std::endl;
        std::cout << "rows       : " << rows << std::endl;
        std::cout << "cols       : " << cols << std::endl;
        std::cout << "final only : " << (final_only ? "true" : "false") << std::endl;
        std::cout << "detect cyc : " << (detect_cycle ? "true" : "false") << std::endl;
    }

    int steps;
    int rows;
    int cols;
    bool final_only;
    bool detect_cycle;
};

/* currently random board */
class GameOfLife
{
public :
    GameOfLife() :
        _row(1),
        _col(1),
        _steps(1),
        _finalOnly(false),
        _detectCycle(false),
        _cell(nullptr),
        _nextCycle(nullptr)
    {
    }

    virtual ~GameOfLife()
    {
        for (int i = 0; i < _row; i++)
        {
            delete [] _cell[i];
            delete [] _nextCycle[i];
        }
        delete [] _cell;
        delete [] _nextCycle;
    }

    void initBoard(int row, int col, int steps, bool finalOnly, bool detectCycle)
    {
        if (row <= 0 || col <= 0 || steps <= 0)
        {
            throw std::invalid_argument("Row, col, and steps must be positive");
        }

        _row = row;
        _col = col;
        _steps = steps;
        _finalOnly = finalOnly;
        _detectCycle = detectCycle;

        _cell = new bool*[row];
        _nextCycle = new bool*[row];
        for (int i = 0; i < row; i++)
        {
            _cell[i] = new bool[col];
            _nextCycle[i] = new bool[col];
        }

        _neighborOffset = {
            { -1, -1 }, {  0, -1 }, { 1, -1 },
            { -1,  0 },             { 1,  0 },
            { -1,  1 }, {  0,  1 }, { 1,  1 }
        };

        // randomize the board
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 1);
        for (int i = 0; i < _row; i++)
        {
            for (int j = 0; j < _col; j++)
            {
                _cell[i][j] = distrib(gen) == 1;
            }
        }

        if (detectCycle)
        {
            _history.reserve(60000);
        }
    }

    void step()
    {
        // for each cell, life when nb = 3 || isLife && nb = 2
        for (int i = 0; i < _row; i++)
        {
            for (int j = 0; j < _col; j++)
            {
                // get adjustent
                int count = 0;
                for (auto &n : _neighborOffset)
                {
                    int ni = i+n.first, nj = j+n.second;

                    if (ni < 0) ni += _row;
                    else if (ni >= _row) ni -= _row;

                    if (nj < 0) nj += _col;
                    else if (nj >= _col) nj -= _col;
                    
                    count += (_cell[ni][nj] ? 1 : 0);
                }
                _nextCycle[i][j] = (count == 3) || (_cell[i][j] && count == 2);
            }
        }
    }

    void display()
    {
        for (int i = 0; i < _row; i++)
        {
            for (int j = 0; j < _col; j++)
            {
                std::cout << (_cell[i][j] ? 'o' : '.');
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void start()
    {
        // initial state
        if (!_finalOnly)
        {
            std::cout << "Cycle: 0" << std::endl;
            display();
        }

        for (int i = 0; i < _steps; i++)
        {
            // cycle detection
            if (_detectCycle)
            {
                size_t s = hashState();
                if (_history.find(s) != _history.end())
                {
                    std::cout << "Cycle detected. Stop calculating." << std::endl;
                    break;
                }
                _history.insert(s);
            }

            step();
            update();
            if (!_finalOnly || i == (_steps - 1))
            {
                std::cout << "Cycle: " << (i+1) << std::endl;
                display();
            }
        }
    }

private :
    void update()
    {
        bool **temp = _cell;
        _cell = _nextCycle;
        _nextCycle = temp;
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
        // similar spirit to boost::hash_combine
        k = splitmix64(k);
        h ^= k + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    }

    size_t hashState()
    {
        uint64_t h = 0x243f6a8885a308d3ull;
        hash_combine(h, (uint64_t)_row);
        hash_combine(h, (uint64_t)_col);

        uint64_t word = 0;
        int bitpos = 0;

        for (int i = 0; i < _row; ++i) {
            for (int j = 0; j < _col; ++j) {
                if (_cell[i][j]) word |= (uint64_t)1 << bitpos;
                ++bitpos;
                if (bitpos == 64)
                {
                    hash_combine(h, word);
                    word = 0;
                    bitpos = 0;
                }
            }
        }
        if (bitpos) hash_combine(h, word); // flush partial word
        return (size_t)h;
    }

    int _row;
    int _col;
    int _steps;
    bool _finalOnly;
    bool _detectCycle;

    bool **_cell;
    bool **_nextCycle;
    std::vector<std::pair<int, int>> _neighborOffset;
    std::unordered_set<size_t> _history;
};

int main(int argc, char *argv[])
{
    Option option;
    option.get(argc, argv);
    option.print();

    GameOfLife gol;
    gol.initBoard(option.rows, option.cols, option.steps, option.final_only, option.detect_cycle);
    gol.start();

    return 0;
}
