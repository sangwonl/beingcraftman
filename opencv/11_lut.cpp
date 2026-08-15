#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

void reduceColor(Mat& src, uchar table[]) {
  for (int i = 0; i < src.rows; i++)
    for (int j = 0; j < src.cols; j++)
      // since src is gray image
      // we can use src.at<uchar>(i, j) to access the pixel value
      src.at<uchar>(i, j) = table[src.at<uchar>(i, j)];
}

int main() {
  Mat src = imread("resources/lena.jpg", IMREAD_GRAYSCALE);
  imshow("Original Image", src);

  // manual create a lookup table
  //   uchar table[256];
  //   for (int i = 0; i < 256; i++) {
  //     table[i] = (uchar)((i / 100) * 100);
  //   }
  //   Mat dst = src.clone();
  //   reduceColor(dst, table);

  // or use OpenCV function to create a lookup table
  Mat table(1, 256, CV_8U);
  uchar* p = table.ptr();
  for (int i = 0; i < 256; i++) {
    p[i] = (uchar)((i / 100) * 100);
  }
  Mat dst;
  LUT(src, table, dst);

  imshow("Reduced Color Image", dst);
  waitKey(0);

  return 0;
}