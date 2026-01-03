#include <functional>
#include <iostream>

double f(double x) {
  return x * x; // y = x^2
}

double derivative(double x) {
  double h = 1e-6;
  return (f(x + h) - f(x)) / h;
}

double derivative2(double x) {
  double h = 1e-6;
  return (f(x + h) - f(x - h)) / (2 * h);
}

double derivative_f(double (*f)(double), double x) {
  double h = 1e-6;
  return (f(x + h) - f(x)) / h;
}

double derivative_f2(std::function<double(double)> f, double x) {
  double h = 1e-6;
  return (f(x + h) - f(x - h)) / (2 * h);
}

int main() {
  double x = 2.0;

  std::cout << "The derivative of f at x = " << x << " is approximately "
            << derivative(x) << std::endl;
  std::cout << "The derivative of f at x = " << x << " is approximately "
            << derivative2(x) << std::endl;
  std::cout << "The derivative of f at x = " << x << " is approximately "
            << derivative_f(f, x) << std::endl;

  auto lambda_f = [](double x) { return x * x; };
  std::cout << "The derivative of f at x = " << x << " is approximately "
            << derivative_f2(lambda_f, x) << std::endl;

  return 0;
}