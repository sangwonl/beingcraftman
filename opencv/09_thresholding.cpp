#include <iostream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

int main() {
  Mat image = imread("resources/lena.jpg", IMREAD_GRAYSCALE);
  Mat dst;

  int threadhold_value = 127;
  threshold(image, dst, threadhold_value, 255, THRESH_BINARY);

  imshow("Original Image", image);
  imshow("Thresholded Image", dst);
  waitKey(0);

  return 0;
}