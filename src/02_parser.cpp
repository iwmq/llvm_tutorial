#include <stdio.h>
#include <string>
#include <iostream>
#include <map>
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
        std::vector<std::string> args)
    : name(name), args(std::move(args)) {}

private:
    std::string name;
    std::vector<std::string> args;
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

static std::unique_ptr<ExprAST> ParseExpression();
static std::map<char, int> BinOpPrecedence;

// paren_expr ::= '(' + expr + ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
    get_next_token(); // eat '('
    auto v = ParseExpression();
    if (!v) {
        return nullptr;
    }

    if (cur_token != ')') {
        log_error_p("Expect ')'");
    }
    get_next_token(); // eat ')'
    return v;
}

// identifier_expr
//              :: = indentifier
//              :: = '(' express * ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
    std::string identifier_name = identifier_str;

    get_next_token(); // eat identifier

    if (cur_token != '(') {
        // single variable reference
        return std::make_unique<VariableExprAST>(identifier_name);
    }

    // call
    get_next_token(); // eat '('

    std::vector<std::unique_ptr<ExprAST>> args;
    if ( cur_token != ')') {
        while (1)
        {
            if (auto arg = ParseExpression()) {
                args.push_back(std::move(arg));
            } else {
                return nullptr;
            }

            if (cur_token == ')') {
                break; // end of args
            }

            if (cur_token != ',') {
                log_error_p("Expect ')' or ','");
            }

            get_next_token();
        }
    }

    get_next_token(); // eat ')'

    return std::make_unique<CallableExprAST>(identifier_name, std::move(args));
}

// primary
//      ::= number_expr
//      ::= identifier_expr
//      ::= paren_expr
static std::unique_ptr<ExprAST> ParsePrimary() {
    switch (cur_token) {
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        default:
            return log_error("Unknown token when expecting a primary expression");
    }
}

// get current token's precedence
static int get_token_precedence() {
    if (!isascii(cur_token)) {
        return -1;
    }

    // make sure it is a defined operation
    int token_precedence = BinOpPrecedence[cur_token];
    if (token_precedence <= 0) {
        return -1;
    }
    return token_precedence;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(
    int expr_prec, std::unique_ptr<ExprAST> rhs);

static std::unique_ptr<ExprAST> ParseExpression() {
    auto lhs = ParsePrimary();
    if (!lhs) {
        return nullptr;
    }

    return ParseBinOpRHS(0, std::move(lhs));
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(
    int expr_prec, std::unique_ptr<ExprAST> lhs) {
    while (1) {
        int tok_prec = get_token_precedence();
        if (tok_prec < expr_prec) {
            return lhs;
        }

        // this is a binary op
        int bin_op = cur_token;
        get_next_token(); // eat this binary op

        auto rhs = ParsePrimary();
        if (!rhs) {
            return nullptr;
        }

        // check next op's precedence
        int next_prec = get_token_precedence();
        if (tok_prec < next_prec) {
            rhs = ParseBinOpRHS(tok_prec + 1, std::move(rhs));
            if (!rhs) {
                return nullptr;
            }
        } else {
            // merge current lhs and rhs
            lhs = std::make_unique<BinaryExprAST>(bin_op,
                std::move(lhs), std::move(rhs));
        }
    }   // loop around
    
}

// prototype
//      ::= def id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (cur_token != tok_identifier) {
        return log_error_p("Expected an identifier");
    }

    std::string fn_name = identifier_str;
    get_next_token(); // eat function anem

    if (cur_token != '(') {
        return log_error_p("Expected '(' in prototype");
    }

    // argument list
    std::vector<std::string> args;

    while (get_next_token() == tok_identifier) {
        args.push_back(identifier_str);
    }

    if (cur_token != ')') {
        return log_error_p("Expected ')' in prototype");
    }

    // success
    get_next_token(); // eat ')'

    return std::make_unique<PrototypeAST>(fn_name, std::move(args));
}

// definition_exp
//          :: = proto expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
    get_next_token();   //eat 'def'

    auto proto = ParsePrototype();
    if (!proto) {
        return nullptr;
    }

    auto expression = ParseExpression();
    if (!expression) {
        return nullptr;
    }

    return std::make_unique<FunctionAST>(std::move(proto), std::move(expression));
}

// extern_exp
//          ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
    get_next_token(); // eat extern
    return ParsePrototype();
}

// top-level expr
//          :: = expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    auto expression = ParseExpression();
    if (!expression) {
        return nullptr;
    }

    // anonymous function
    auto proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(proto), std::move(expression));
}

// Top level parsing
static void HandleDefinition() {
    if (ParseDefinition()) {
        fprintf(stderr, "Parsed a function definition.\n");
    } else {
        // Skip token for error recovery.
        get_next_token();
    }
}

static void HandleExtern() {
    if (ParseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
    } else {
        // Skip token for error recovery.
        get_next_token();
    }
}

static void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (ParseTopLevelExpr()) {
        fprintf(stderr, "Parsed a top-level expr\n");
    } else {
        // Skip token for error recovery.
        get_next_token();
    }
}

// top loop
static void MainLoop() {
    while (true) {
        fprintf(stderr, "ready>");
        switch (cur_token) {
        case tok_eof:
            return;
        case ';':   // ignore top level semicolons
            get_next_token();
            break;
        case tok_def:
            HandleDefinition();
            break;
        case tok_extern:
            HandleExtern();
            break;
        default:
            HandleTopLevelExpression();
            break;
        }
    }
    
}

int main(int argc, char* argv[]) {

    // set up binary operation precedence
    // 1 is lowest
    BinOpPrecedence['<'] = 10;
    BinOpPrecedence['+'] = 20;
    BinOpPrecedence['-'] = 20;
    BinOpPrecedence['*'] = 40;  // highest

    // Prime the first token.
    fprintf(stderr, "ready> ");
    get_next_token();

    // Run the main loop
    MainLoop();

    return 0;
}