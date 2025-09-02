/*
    Problem 1:

    CSV Aggregator: Category Totals
        Goal: From a CSV date,category,amount, compute totals per category and overall.

    CLI: 
        agg input.csv [--sorted] [--top K].

    Output: 
        table category,total (amount is decimal; use long double).
*/


#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <numeric>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>

struct Option {
    Option() :
        sorted(false),
        k(-1)
    {
    };

    void get(int argc, char *argv[])
    {
        if (argc > 1)
        {
            filename = argv[1];

            int c;
            while (1)
            {
                static struct option longOpt[] = {
                    { "sorted", no_argument, 0, 's' },
                    { "top", required_argument, 0, 't'},
                    { 0, 0, 0, 0 }
                };

                int index = 0;

                c = getopt_long(argc, argv, "t:", longOpt, &index);
                if (c == -1)
                    break;

                switch (c)
                {
                case 's':
                    sorted = true;
                    break;
                case 't':
                    k = std::atoi(optarg);
                }
            }
        }
    }

    void print()
    {
        std::cout << "Options   " << std::endl;
        std::cout << "filename : " << filename << std::endl;
        std::cout << "k        : " << k << std::endl;
        std::cout << "sorted   : " << (sorted ? "true" : "false") << std::endl;
    }

    bool sorted;
    int k;
    std::string filename;
};

class CSV
{
public :
    CSV()
    {
    }

    virtual ~CSV()
    {
    }

    struct Data {
        std::vector<std::vector<std::string>> rows;
        bool success = true;
        std::string msg;
    };

    static Data parse(std::string_view content, char delimiter = ',')
    {
        Data data;

        if (content.empty())
        {
            return data;
        }

        const char *ptr = content.data();
        const char *end = ptr + content.length();

        std::vector<std::string> current_row;
        std::string current_field;
        while (ptr < end)
        {
            char ch = (*ptr);

            // case 1: open quote
            if (ch == '"')
            {
                ++ptr;

                // loop until closing found
                while (ptr < end)
                {
                    ch = (*ptr);
                    ++ptr;
                    
                    if (ch == '"')
                    {
                        if ((ptr < end) && *ptr == '"')
                        {
                            current_field += '"';
                            ++ptr;
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        current_field += ch;
                    }
                }

                while (ptr < end && (*ptr == '\t' || *ptr == ' '))
                {
                    ++ptr;
                }
            }
            // case 2: field delimiter
            else if (ch == delimiter)
            {
                current_row.push_back(std::move(current_field));
                current_field.clear();
                ++ptr;
            }
            // case 3: new line
            else if (ch == '\r' || ch == '\n')
            {
                current_row.push_back(std::move(current_field));
                current_field.clear();

                if (!current_row.empty())
                {
                    data.rows.push_back(std::move(current_row));
                    current_row.clear();
                }

                if (ch == '\r' && (ptr+1 < end && (*(ptr+1) == '\n')))
                {
                    ++ptr;
                }

                ++ptr;
            }
            // case 4: normal character            
            else
            {
                current_field += ch;
                ++ptr;
            }
        }

        if (!current_field.empty() || !current_row.empty())
        {
            current_row.push_back(std::move(current_field));
            data.rows.push_back(std::move(current_row));
        }

        return data;
    }

    static Data read(const std::string &filename, char delimiter = ',')
    {
        Data data;

        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            data.success = false;
            data.msg = "Fail open file.";

            return data;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        return parse(content, delimiter);
    }

    typedef std::deque<std::pair<std::string, long double>> AggType;
    static AggType aggregrate(const Data &data)
    {
        AggType result;
        std::unordered_map<std::string, int> order;
        int size = 0;

        auto iter = data.rows.cbegin();
        while (iter != data.rows.cend())
        {
            const std::string &name = (*iter)[1];
            long double value = std::strtold((*iter)[2].c_str(), NULL);
            auto loc = order.find(name);
            if (loc != order.end())
            {
                result[loc->second].second += value;
            }
            else
            {
                order[name] = size++;
                result.push_back(std::make_pair(name, value));
            }
            
            ++iter;
        }

        return result;
    }

    static void print(AggType &agg, bool sorted, int K)
    {
        // copy to vector
        std::vector<int> order(agg.size());
        std::iota(order.begin(), order.end(), 0);

        if (sorted)
        {
            // sort according to agg[i].first and put the result to order
            std::sort(order.begin(), order.end(),
                [&agg](int a, int b)
                {
                    return agg[a].first < agg[b].first;
                }
            );
        }   

        long i = 0;
        while (i < static_cast<long>(agg.size()) && (K == -1 || i < K))
        {
            std::cout << agg[order[i]].first << ":" << agg[order[i]].second << std::endl;
            ++i;
        }
    }

private :
};

int main(int argc, char *argv[])
{
    struct Option opt;
    opt.get(argc, argv);
    
    // read csv
    CSV csv;
    CSV::Data data = csv.read(opt.filename);

    // calculate amount per category
    CSV::AggType agg = csv.aggregrate(data);

    // print
    csv.print(agg, opt.sorted, opt.k);

    return 0;
}
