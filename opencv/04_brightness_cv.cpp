#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

int main() {
  Mat img = imread("resources/lena.jpg", IMREAD_GRAYSCALE);
  imshow("Original Image", img);

  Mat oimage;
  img.convertTo(oimage, -1, 1, 30);

  imshow("New Image", oimage);
  waitKey(0);

  return 0;
}