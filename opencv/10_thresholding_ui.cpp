#include <iostream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

Mat src, src_gray, dst;
int threshold_value = 0;
int threshold_type = 0;

void onThresholdChange(int, void*) {
  threshold(src_gray, dst, threshold_value, 255, threshold_type);
  imshow("Output Image", dst);
}

int main() {
  src = imread("resources/lena.jpg");
  cvtColor(src, src_gray, COLOR_BGR2GRAY);
  namedWindow("Output Image", WINDOW_AUTOSIZE);

  createTrackbar(
      "Threshold Value", "Output Image", &threshold_value, 255,
      onThresholdChange);

  onThresholdChange(0, 0);

  while (true) {
    int c = waitKey(20);
    if ((char)c == 27) {
      break;
    }  // ESC key to exit
  }

  return 0;
}