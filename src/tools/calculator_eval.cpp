// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"
#include <stack>

static bool apply(std::stack<double>& values,
                  std::stack<CalcToken::Kind>& ops,
                  std::string* error) {
    if (ops.empty() || values.size() < 2) {
        if (error) *error = "malformed expression";
        return false;
    }

    CalcToken::Kind op = ops.top();
    ops.pop();

    double right = values.top();
    values.pop();
    double left = values.top();
    values.pop();

    double result = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus:
            result = left + right;
            break;
        case CalcToken::Kind::Minus:
            result = left - right;
            break;
        case CalcToken::Kind::Times:
            result = left * right;
            break;
        case CalcToken::Kind::Divide:
            if (right == 0) {
                if (error) *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            if (error) *error = "unknown operator";
            return false;
    }

    values.push(result);
    return true;
}

static int precedence(CalcToken::Kind kind) {
    switch (kind) {
        case CalcToken::Kind::Times:
        case CalcToken::Kind::Divide:
            return 2;
        default:
            return 1;
    }
}

bool calc_evaluate(const std::vector<CalcToken>& tokens,
                   double* out,
                   std::string* error) {
    std::stack<double> values;
    std::stack<CalcToken::Kind> ops;

    for (const CalcToken& t : tokens) {
        switch (t.kind) {
            case CalcToken::Kind::Number:
                values.push(t.value);
                break;

            case CalcToken::Kind::LParen:
                ops.push(t.kind);
                break;

            case CalcToken::Kind::RParen:
                while (!ops.empty() && ops.top() != CalcToken::Kind::LParen) {
                    if (!apply(values, ops, error)) return false;
                }
                if (ops.empty()) {
                    if (error) *error = "mismatched parentheses";
                    return false;
                }
                ops.pop();
                break;

            default:
                while (!ops.empty() &&
                       ops.top() != CalcToken::Kind::LParen &&
                       precedence(ops.top()) >= precedence(t.kind)) {
                    if (!apply(values, ops, error)) return false;
                }
                ops.push(t.kind);
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.top() == CalcToken::Kind::LParen) {
            if (error) *error = "mismatched parentheses";
            return false;
        }
        if (!apply(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        if (error) *error = "malformed expression";
        return false;
    }

    *out = values.top();
    return true;
}