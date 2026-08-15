#include <iostream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

int main() {
  Mat src = imread("resources/lena.jpg");
  imshow("Original Image", src);

  Mat dst;
  dst = 255 - src;  // Invert the image
  imshow("Inverted Image", dst);

  waitKey(0);
  return 0;
}