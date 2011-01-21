#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;
using namespace cv::gpu;

void help()
{
    cout << "\nThis program demonstrates using SURF_GPU features detector, descriptor extractor and BruteForceMatcher_GPU" << endl;
    cout << "\nUsage:\n\tmatcher_simple_gpu <image1> <image2>" << endl;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        help();
        return -1;
    }

    GpuMat img1(imread(argv[1], CV_LOAD_IMAGE_GRAYSCALE));
    GpuMat img2(imread(argv[2], CV_LOAD_IMAGE_GRAYSCALE));
    if (img1.empty() || img2.empty())
    {
        cout << "Can't read one of the images" << endl;
        return -1;
    }

    SURF_GPU surf;

    // detecting keypoints & computing descriptors
    GpuMat keypoints1GPU, keypoints2GPU;
    GpuMat descriptors1GPU, descriptors2GPU;
    surf(img1, GpuMat(), keypoints1GPU, descriptors1GPU);
    surf(img2, GpuMat(), keypoints2GPU, descriptors2GPU);

    // matching descriptors
    BruteForceMatcher_GPU< L2<float> > matcher;
    GpuMat trainIdx, distance;
    matcher.matchSingle(descriptors1GPU, descriptors2GPU, trainIdx, distance);
    
    // downloading results
    vector<KeyPoint> keypoints1, keypoints2;
    vector<float> descriptors1, descriptors2;
    vector<DMatch> matches;
    SURF_GPU::downloadKeypoints(keypoints1GPU, keypoints1);
    SURF_GPU::downloadKeypoints(keypoints2GPU, keypoints2);
    SURF_GPU::downloadDescriptors(descriptors1GPU, descriptors1);
    SURF_GPU::downloadDescriptors(descriptors2GPU, descriptors2);
    BruteForceMatcher_GPU< L2<float> >::matchDownload(trainIdx, distance, matches);

    // drawing the results
    Mat img_matches;
    drawMatches(img1, keypoints1, img2, keypoints2, matches, img_matches);
    imshow("matches", img_matches);
    waitKey(0);

    return 0;
}
