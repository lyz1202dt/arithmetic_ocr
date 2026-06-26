#pragma once

#include <string>
#include <vector>

namespace arithmetic {

inline bool IsBinaryOperator(char ch) {
  return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

inline bool NormalizeParentheses(std::string &expr) {
  std::vector<std::size_t> paren_positions;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    if (expr[i] == '(' || expr[i] == ')') {
      paren_positions.push_back(i);
    }
  }

  if (paren_positions.size() % 2 == 1) {
    return false;
  }

  for (std::size_t k = 0; k < paren_positions.size(); ++k) {
    expr[paren_positions[k]] = (k % 2 == 0) ? '(' : ')';
  }
  return true;
}

inline bool IsAcceptableExpression(const std::string &expr) {
  if (expr.empty()) {
    return false;
  }

  int balance = 0;
  char prev = '\0';

  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char ch = expr[i];
    const bool is_digit = ch >= '0' && ch <= '9';
    const bool is_left_paren = ch == '(';
    const bool is_right_paren = ch == ')';
    const bool is_op = IsBinaryOperator(ch);

    if (!is_digit && !is_left_paren && !is_right_paren && !is_op) {
      return false;
    }

    if (i == 0) {
      if (is_op || is_right_paren) {
        return false;
      }
    } else {
      const bool prev_is_digit = prev >= '0' && prev <= '9';
      const bool prev_is_left_paren = prev == '(';
      const bool prev_is_right_paren = prev == ')';
      const bool prev_is_op = IsBinaryOperator(prev);

      if (prev_is_op && (is_op || is_right_paren)) {
        return false;
      }
      if (prev_is_left_paren && (ch == '*' || ch == '/' || is_right_paren || ch == '-')) {
        return false;
      }
      if ((prev_is_digit || prev_is_right_paren) && is_left_paren) {
        return false;
      }
      if (prev_is_right_paren && is_digit) {
        return false;
      }
    }

    if (is_left_paren) {
      ++balance;
    } else if (is_right_paren) {
      --balance;
      if (balance < 0) {
        return false;
      }
    }

    prev = ch;
  }

  if (balance != 0) {
    return false;
  }
  if (IsBinaryOperator(prev) || prev == '(') {
    return false;
  }
  return true;
}

inline bool TryCalcExpression(const std::string &expr, long long &result) {
  std::vector<long long> nums;
  std::vector<char> ops;

  auto precedence = [](char op) {
    if (op == '+' || op == '-') {
      return 1;
    }
    if (op == '*' || op == '/') {
      return 2;
    }
    return 0;
  };

  auto apply_top = [&]() -> bool {
    if (ops.empty() || nums.size() < 2) {
      return false;
    }

    const char op = ops.back();
    ops.pop_back();
    const long long b = nums.back();
    nums.pop_back();
    const long long a = nums.back();
    nums.pop_back();

    long long value = 0;
    switch (op) {
      case '+':
        value = a + b;
        break;
      case '-':
        value = a - b;
        break;
      case '*':
        value = a * b;
        break;
      case '/':
        if (b == 0) {
          return false;
        }
        value = a / b;
        break;
      default:
        return false;
    }

    nums.push_back(value);
    return true;
  };

  std::size_t i = 0;
  while (i < expr.size()) {
    const char ch = expr[i];
    if (ch >= '0' && ch <= '9') {
      long long value = 0;
      while (i < expr.size() && expr[i] >= '0' && expr[i] <= '9') {
        value = value * 10 + (expr[i] - '0');
        ++i;
      }
      nums.push_back(value);
      continue;
    }

    if (ch == '(') {
      ops.push_back(ch);
      ++i;
      continue;
    }

    if (ch == ')') {
      while (!ops.empty() && ops.back() != '(') {
        if (!apply_top()) {
          return false;
        }
      }
      if (ops.empty() || ops.back() != '(') {
        return false;
      }
      ops.pop_back();
      ++i;
      continue;
    }

    if (IsBinaryOperator(ch)) {
      while (!ops.empty() && ops.back() != '(' && precedence(ops.back()) >= precedence(ch)) {
        if (!apply_top()) {
          return false;
        }
      }
      ops.push_back(ch);
      ++i;
      continue;
    }

    return false;
  }

  while (!ops.empty()) {
    if (ops.back() == '(') {
      return false;
    }
    if (!apply_top()) {
      return false;
    }
  }

  if (nums.size() != 1) {
    return false;
  }

  result = nums.back();
  return true;
}

}  // namespace arithmetic
