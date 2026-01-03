#include <iostream>
#include <random>

namespace Random {
using namespace std;

random_device seeder;
unsigned int seed{seeder()};
mt19937 engine{seed};

void PrintSeed() { cout << "Seed: " << seed << endl; }

void Reseed(unsigned int NewSeed) {
  seed = NewSeed;
  engine.seed(seed);
}

int Int(int min, int max) {
  uniform_int_distribution get{min, max};
  return get(engine);
}

int Bool(float probability) {
  uniform_real_distribution get{0.0, 1.0};
  return probability > get(engine);
}
} // namespace Random

int main() {
  using std::cout, std::endl;

  cout << Random::Int(1, 100) << ", " << Random::Int(1, 100) << ", "
       << Random::Int(1, 100) << endl;

  Random::Reseed(142341);
  Random::PrintSeed();
  cout << Random::Int(1, 100) << ", " << Random::Int(1, 100) << ", "
       << Random::Int(1, 100) << endl;

  Random::Reseed(142341);
  Random::PrintSeed();
  cout << Random::Int(1, 100) << ", " << Random::Int(1, 100) << ", "
       << Random::Int(1, 100) << endl;

  for (int i{0}; i < 10; i++) {
    cout << Random::Bool(0.3) << " ";
  }
  cout << endl;

  return 0;
}