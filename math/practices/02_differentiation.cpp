#include <iostream>
#include <numeric>
#include <ranges>
#include <vector>

template <typename TX>
std::vector<double> n_average(const std::vector<TX> &x, int n) {
  std::vector<double> avgs;

  int size = x.size();
  for (int i = 0; i <= size - n; i++) {
    int e = i + n;
    double sum = std::accumulate(x.begin() + i, x.begin() + e, 0.0);
    avgs.push_back(sum / n);
  }
  return avgs;
}

template <typename TX, typename TY>
std::vector<double> derivative_central(const std::vector<TX> &x,
                                       const std::vector<TY> &y) {
  std::vector<double> derivatives;

  int n = x.size();
  for (int i = 1; i < n - 1; i++) {
    double dx = x[i + 1] - x[i - 1];
    double dy = y[i + 1] - y[i - 1];
    derivatives.push_back(dy / dx);
  }

  return derivatives;
}

template <typename TX, typename TY>
std::vector<double> derivative(const std::vector<TX> &x,
                               const std::vector<TY> &y) {
  std::vector<double> derivatives;

  int n = x.size();
  for (int i = 0; i < n - 1; i++) {
    double dx = x[i + 1] - x[i];
    double dy = y[i + 1] - y[i];
    derivatives.push_back(dy / dx);
  }

  return derivatives;
}

std::string to_string(const std::vector<double> &vec) {
  auto transformed =
      vec | std::views::transform([](double v) { return std::to_string(v); });

  auto it = transformed.begin();
  if (it != transformed.end()) {
    std::cout << *it;
    ++it;
  }

  std::string result;
  for (; it != transformed.end(); ++it) {
    result += ", " + *it;
  }

  return result;
}

int main(void) {
  std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
  std::vector<double> y = {0.0, 1.0, 4.0, 9.0, 16.0}; // y = x^2

  auto der = derivative_central(x, y);
  std::cout << "Derivatives (central): " << to_string(der) << std::endl;

  der = derivative(x, y);
  std::cout << "Derivatives: " << to_string(der) << std::endl;

  y = {4.0, 12.0, 8.0, 6.0, 4.0};
  auto avgs = n_average(y, 3);
  auto iota_view = std::views::iota(0, (int)avgs.size());
  x = std::vector<double>(iota_view.begin(), iota_view.end());
  std::cout << "Averages: " << to_string(avgs) << std::endl;

  der = derivative(x, avgs);
  std::cout << "Derivatives: " << to_string(der) << std::endl;
  return 0;
}