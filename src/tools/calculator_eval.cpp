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

int rank(Kind k) {
    switch (k) {
        case Kind::Times:
        case Kind::Divide: return 2;
        case Kind::Plus:
        case Kind::Minus:  return 1;
        default:           return 0;
    }
}

struct Machine {
    std::vector<double> nums;
    std::vector<Kind> pending;
    std::string problem;

    bool reduce_once() {
        if (pending.empty() || rank(pending.back()) == 0 || nums.size() < 2) {
            problem = "malformed expression";
            return false;
        }
        const Kind op = pending.back();
        pending.pop_back();
        const double rhs = nums.back();
        nums.pop_back();
        const double lhs = nums.back();
        nums.pop_back();
        if (op == Kind::Divide && rhs == 0.0) {
            problem = "division by zero";
            return false;
        }
        double value = 0.0;
        if (op == Kind::Plus) value = lhs + rhs;
        else if (op == Kind::Minus) value = lhs - rhs;
        else if (op == Kind::Times) value = lhs * rhs;
        else value = lhs / rhs;
        nums.push_back(value);
        return true;
    }

    bool reduce_while(int floor) {
        while (!pending.empty() && rank(pending.back()) >= floor) {
            if (!reduce_once()) return false;
        }
        return true;
    }
};

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    Machine m;
    auto fail = [&]() {
        *error = m.problem;
        return false;
    };

    for (const CalcToken& tok : tokens) {
        switch (tok.kind) {
            case Kind::Number:
                m.nums.push_back(tok.value);
                break;
            case Kind::LParen:
                m.pending.push_back(Kind::LParen);
                break;
            case Kind::RParen:
                if (!m.reduce_while(1)) return fail();
                if (m.pending.empty()) {
                    m.problem = "unbalanced parentheses";
                    return fail();
                }
                m.pending.pop_back();
                break;
            default:
                if (!m.reduce_while(rank(tok.kind))) return fail();
                m.pending.push_back(tok.kind);
                break;
        }
    }

    if (!m.reduce_while(1)) return fail();
    if (!m.pending.empty()) {
        m.problem = "unbalanced parentheses";
        return fail();
    }
    if (m.nums.size() != 1) {
        m.problem = "malformed expression";
        return fail();
    }
    *out = m.nums.back();
    return true;
}
