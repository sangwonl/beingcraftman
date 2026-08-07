#include <iostream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

int main() {
  double alpha = 1.0;
  int beta = 0;

  Mat image = imread("resources/lena.jpg");
  Mat oimage = Mat::zeros(image.size(), image.type());

  cout << "alpha [1.0-3.0]: ";
  cin >> alpha;
  cout << "beta [0-100]: ";
  cin >> beta;

  // 1. Using for loops to iterate through each pixel and channel
  // for (int y = 0; y < image.rows; y++) {
  //   for (int x = 0; x < image.cols; x++) {
  //     for (int c = 0; c < image.channels(); c++) {
  //       Vec3b const& color = image.at<Vec3b>(y, x);
  //       oimage.at<Vec3b>(y, x)[c] =
  //           saturate_cast<uchar>(alpha * (color[c]) + beta);
  //     }
  //   }
  // }

  // 2. Using OpenCV's convertTo function
  // image.convertTo(oimage, -1, alpha, beta);

  // 3. Using OpenCV's operator overloading
  oimage = image * alpha + beta;

  imshow("Original Image", image);
  imshow("New Image", oimage);
  waitKey();

  return 0;
}
