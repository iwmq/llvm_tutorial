#include <stdio.h>
#include <string>
#include <iostream>
#include <memory>
#include <vector>


enum token {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,

    //unknown character is in [0, 255] range
    tok_unknown = 255
};

static std::string identifier_str; // fill in if tok_identifier
static double num_val; // fill in if tok_number


static int get_token() {
    static char last_char = ' ';

    // skip white space
    while (isspace(last_char)) {
        last_char = getchar();
    }

    if (isalpha(last_char)) {
        identifier_str = last_char;
        while (isalnum((last_char = getchar()))) {
            identifier_str += last_char;
        }

        if (identifier_str == "def") {
            return tok_def;
        }
        if (identifier_str == "extern") {
            return tok_extern;
        }

        return tok_identifier;
    }

    if (isdigit(last_char) || last_char == '.') {
        std::string NumStr;
        do {
            NumStr += last_char;
            last_char = getchar();
        } while (isdigit(last_char) || last_char == '.');

        num_val = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    // Ignore comment
    if (last_char == '#') {
        do {
            last_char = getchar();
        } while (last_char != EOF && last_char != '\n' && last_char != '\r');
    }

    // End of file
    if (last_char == EOF) {
        return tok_eof;
    }

    // Otherwisie, return the char itself
    int this_char = last_char;
    last_char = getchar();
    return this_char;
}

// Abstract sytax tree
namespace { // anonymous namespace as in LLVM

// ExpreAST - Base class for all express nodes
class ExprAST {
public:
    virtual ~ExprAST() = default;
};

// NumberExprAST -- For numerical node like 1.0

class NumberExprAST : public ExprAST {
public:
    NumberExprAST(double val): val(val) {}

private:
    double val;
};


class VariableExprAST : public ExprAST {
public:
    VariableExprAST(const std::string &name): name(name) {}

private:
    std::string name;
};


class BinaryExprAST : public ExprAST {
public:
    BinaryExprAST(char op,
        std::unique_ptr<ExprAST> lhs,
        std::unique_ptr<ExprAST> rhs)
    : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

private:
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;
};


class CallableExprAST : public ExprAST {
public:
    CallableExprAST(const std::string& callee, 
        std::vector<std::unique_ptr<ExprAST>> args)
    : callee(callee), args(std::move(args)) {}

private:
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;
};


class PrototypeAST {
public:
    PrototypeAST(const std::string& name, 
        std::vector<std::unique_ptr<ExprAST>> args)
    : name(name), args(std::move(args)) {}

private:
    std::string name;
    std::vector<std::unique_ptr<ExprAST>> args;
};


class FunctionAST {
public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto,
        std::unique_ptr<ExprAST> body)
    : proto(std::move(proto)), body(std::move(body)) {}

private:
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;
};


}   // end anonymous namespace


static int cur_token;
static int get_next_token() {
    return cur_token = get_token();
}

// helper functions for reporting errors
std::unique_ptr<ExprAST> log_error(const char *err) {
    fprintf(stderr, "log_error: %s\n", err);
    return nullptr;
}

std::unique_ptr<PrototypeAST> log_error_p(const char *err) {
    log_error(err);
    return nullptr;
}

// Parse number expression
std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(num_val);
    get_next_token();   // consumer the number
    return std::move(result);
}

int main(int argc, char* argv[]) {
    auto t = get_next_token();

    std::cout<<static_cast<int>(t)<<std::endl;


    return 0;
}