/*****************************************************************************

    Arithmetic Expression Evaluator
    Goal: Parse & evaluate strings like "3 + 4*2/(1-5)^2^3".

    Operators: + - * / ^, parentheses, unary minus, spaces.
    Precedence & right-associativity for ^.
    Errors → exceptions (e.g., division by zero, bad token).
    Hints: Tokenize → shunting-yard → RPN evaluate; or recursive-descent.
    Done when: Matches expected results on ~15 varied test cases.

*****************************************************************************/

#include <cmath>
#include <cctype>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static inline bool is_op_token(const std::string& t) {
    return t == "+" || t == "-" || t == "*" || t == "/" || t == "^" || t == "(" || t == "u+" || t == "u-";
}

bool tokenize(const std::string& input, std::vector<std::string>& infix)
{
    infix.clear();

    const std::unordered_set<char> ops{ '+', '-', '*', '/', '^', '(', ')' };
    const size_t n = input.size();
    size_t i = 0;

    auto peek = [&](size_t k) -> char {
        return (k < n) ? input[k] : '\0';
    };

    while (i < n) {
        char ch = input[i];

        // skip whitespace
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++i;
            continue;
        }

        // Number (possibly starting with '.' like ".5")
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
            std::string num;
            bool has_digit = false;
            bool has_dot = false;

            while (i < n) {
                char c = input[i];
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    has_digit = true;
                    num.push_back(c);
                    ++i;
                } else if (c == '.') {
                    if (has_dot) return false;  // multiple dots
                    has_dot = true;
                    num.push_back(c);
                    ++i;
                } else {
                    break;
                }
            }

            // Must contain at least one digit (reject lone ".")
            if (!has_digit) return false;

            infix.push_back(std::move(num));
            continue;
        }

        // '-' could be unary minus (part of a number) or a binary operator
        // inside tokenize()
        if (ch == '-' || ch == '+') {
            bool unary = infix.empty() || is_op_token(infix.back());
            if (unary) {
                char nxt = peek(i + 1);
                // numeric literal with sign (fast path)
                if (std::isdigit((unsigned char)nxt) || nxt == '.') {
                    std::string num(1, ch);   // "+" or "-"
                    ++i;
                    bool has_digit = false, has_dot = false;
                    while (i < n) {
                        char c = input[i];
                        if (std::isdigit((unsigned char)c)) { has_digit = true; num.push_back(c); ++i; }
                        else if (c == '.') { if (has_dot) return false; has_dot = true; num.push_back(c); ++i; }
                        else break;
                    }
                    if (!has_digit) return false;
                    infix.push_back(std::move(num));
                    continue;
                }
                // otherwise unary operator token
                infix.push_back(ch == '-' ? "u-" : "u+");
                ++i;
                continue;
            }
            // binary operator
            infix.emplace_back(1, ch);
            ++i;
            continue;
        }


        // Other operators and parentheses
        if (ops.count(ch)) {
            infix.emplace_back(1, ch);
            ++i;
            continue;
        }

        // Unknown character
        return false;
    }

    return true;
}

static inline int precedence(const std::string& op) {
    if (op == "^") return 4;
    if (op == "u+" || op == "u-") return 3;
    if (op == "*" || op == "/") return 2;
    if (op == "+" || op == "-") return 1;
    return -1; // not an operator
}

static inline bool isRightAssociative(const std::string& op) {
    return op == "^" || op == "u+" || op == "u-";
}

bool convertInfixToPostfix(const std::vector<std::string>& infix,
                           std::vector<std::string>& postfix)
{
    postfix.clear();
    postfix.reserve(infix.size());

    const std::unordered_set<std::string> ops{"+","-","*","/","^", "u+", "u-"};
    std::stack<std::string> st;

    for (const std::string& tok : infix) {
        if (tok == "(") {
            st.push(tok);
        } else if (tok == ")") {
            // Pop until "("
            bool foundLeft = false;
            while (!st.empty()) {
                if (st.top() == "(") { foundLeft = true; st.pop(); break; }
                postfix.push_back(st.top());
                st.pop();
            }
            if (!foundLeft) return false; // mismatched right parenthesis
        } else if (ops.count(tok)) {
            // Operator: pop while (top has higher precedence) OR
            // (equal precedence and current op is left-assoc), stopping at "("
            while (!st.empty()) {
                const std::string& top = st.top();
                if (top == "(") break;
                int pTop = precedence(top);
                int pCur = precedence(tok);
                if (pTop > pCur || (pTop == pCur && !isRightAssociative(tok))) {
                    postfix.push_back(top);
                    st.pop();
                } else break;
            }
            st.push(tok);
        } else {
            // Operand
            postfix.push_back(tok);
        }
    }

    // Drain stack
    while (!st.empty()) {
        if (st.top() == "(") return false; // mismatched left parenthesis
        postfix.push_back(st.top());
        st.pop();
    }

    return true;
}

double evaluate(std::vector<std::string> &exp)
{
    std::cout << std::endl;
    using Fn2 = double(*)(double,double);
    using Fn1 = double(*)(double);

    static const std::unordered_map<std::string, Fn2> OPS2 = {
        {"+", [](double a,double b){return a+b;} },
        {"-", [](double a,double b){return a-b;} },
        {"*", [](double a,double b){return a*b;} },
        {"/", [](double a,double b){ if(b==0.0) throw std::domain_error("div by 0"); return a/b; } },
        {"^", [](double a,double b){ return std::pow(a,b); } },
    };
    static const std::unordered_map<std::string, Fn1> OPS1 = {
        {"u+", [](double x){ return  x; }},
        {"u-", [](double x){ return -x; }},
    };

    std::stack<double> s;
    for (const std::string& t : exp) {
        if (auto it1 = OPS1.find(t); it1 != OPS1.end()) {
            if (s.size() < 1) throw std::runtime_error("not enough operands for unary op");
            double x = s.top(); s.pop();
            s.push(it1->second(x));
        } else if (auto it2 = OPS2.find(t); it2 != OPS2.end()) {
            if (s.size() < 2) throw std::runtime_error("not enough operands");
            double rhs = s.top(); s.pop();
            double lhs = s.top(); s.pop();
            s.push(it2->second(lhs, rhs));
        } else {
            s.push(std::stod(t)); // throws on bad token
        }
    }
    if (s.size() != 1) throw std::runtime_error("invalid expression");
    return s.top();
}

int main(int argc, char *argv[])
{
    std::string input;
    input.reserve(256);

    if (argc >= 2)
    {
        for (int i = 1; i < argc; i++)
        {
            if (i > 1) input.push_back(' ');
            input += argv[i];
        }
    }
    else
    {
        if (!std::getline(std::cin, input)) {
            std::cerr << "Usage: eval <expression>\n";
            return 1;
        }
    }

    std::vector<std::string> postfix;
    std::vector<std::string> infix;

    if (!tokenize(input, infix))
    {
        std::cerr << "Tokenization error\n";
        return -1;
    }

    if (!convertInfixToPostfix(infix, postfix))
    {
        std::cerr << "Parse error (mismatched parentheses?)\n";
        return -1;
    }

    try {
        double value = evaluate(postfix);
        std::cout << value << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Evaluation error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
