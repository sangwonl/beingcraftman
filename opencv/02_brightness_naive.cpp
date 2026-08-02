#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

int main() {
  Mat img = imread("resources/lena.jpg", IMREAD_GRAYSCALE);
  imshow("original Image", img);

  for (int r = 0; r < img.rows; r++) {
    for (int c = 0; c < img.cols; c++) {
      img.at<uchar>(r, c) = img.at<uchar>(r, c) + 50;
    }
  }

  imshow("New Image", img);
  waitKey(0);
  return 0;
}
