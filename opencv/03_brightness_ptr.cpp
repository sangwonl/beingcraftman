#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

int main() {
  Mat img = imread("resources/lena.jpg", IMREAD_GRAYSCALE);
  imshow("Original Image", img);

  for (int r = 0; r < img.rows; r++) {
    uchar* p = img.ptr<uchar>(r);
    for (int c = 0; c < img.cols; c++) {
      p[c] = saturate_cast<uchar>(p[c] + 30);
    }
  }

  imshow("New Image", img);
  waitKey(0);

  return 0;
}
