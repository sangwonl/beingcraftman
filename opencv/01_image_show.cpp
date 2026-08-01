#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

int main() {
  constexpr char image_path[] = "resources/lena.jpg";
  const cv::Mat image = cv::imread(image_path);
  if (image.empty()) {
    std::cerr << "Failed to read image: " << image_path << '\n';
    return 1;
  }

  cv::imshow("OpenCV Tutorial", image);
  cv::waitKey();
}
