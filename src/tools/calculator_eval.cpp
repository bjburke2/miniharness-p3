// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

#include <string>
#include <vector>

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind k) {
    return (k == Kind::Times || k == Kind::Divide) ? 2 : 1;
}

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

bool apply_top(std::vector<double>& values, std::vector<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2 || !is_operator(ops.back())) {
        *error = "malformed expression";
        return false;
    }
    Kind op = ops.back();
    ops.pop_back();
    double right = values.back();
    values.pop_back();
    double left = values.back();
    values.pop_back();
    double result = 0.0;
    switch (op) {
        case Kind::Plus:   result = left + right; break;
        case Kind::Minus:  result = left - right; break;
        case Kind::Times:  result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "malformed expression";
            return false;
    }
    values.push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;
    for (const CalcToken& t : tokens) {
        if (t.kind == Kind::Number) {
            values.push_back(t.value);
        } else if (t.kind == Kind::LParen) {
            ops.push_back(Kind::LParen);
        } else if (t.kind == Kind::RParen) {
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_top(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unbalanced parentheses";
                return false;
            }
            ops.pop_back();
        } else {
            while (!ops.empty() && ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(t.kind)) {
                if (!apply_top(values, ops, error)) return false;
            }
            ops.push_back(t.kind);
        }
    }
    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unbalanced parentheses";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }
    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}
