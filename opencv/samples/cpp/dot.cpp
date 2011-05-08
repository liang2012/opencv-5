#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <iostream>
#include <fstream>

using namespace cv;
using namespace std;

#define SHOW_ALL_RECTS_BY_ONE 1
static void fillColors( vector<Scalar>& colors )
{
    cv::RNG rng = theRNG();

    for( size_t ci = 0; ci < colors.size(); ci++ )
        colors[ci] = Scalar( rng(256), rng(256), rng(256) );
}

static void readTestImageNames( const string& descrFilename, vector<string>& names )
{
    names.clear();

    ifstream file( descrFilename.c_str() );
    if ( !file.is_open() )
        return;

    while( !file.eof() )
    {
        string str; getline( file, str );
        if( str.empty() ) break;
        if( str[0] == '#' ) continue; // comment
        names.push_back(str);
    }
    file.close();
}

int main( int argc, char **argv )
{
    if( argc != 1 && argc != 3 )
    {
        cout << "Format: base; " << endl << "or without arguments to use default data" << endl;
        return -1;
    }

    string baseDirName, testDirName;
    if( argc == 1 )
    {
        baseDirName = "../MultiScaleDOT/Data/train/";
        testDirName = "../MultiScaleDOT/Data/test/";
    }
    else
    {
        baseDirName = argv[1];
        testDirName = argv[2];
        baseDirName += (*(baseDirName.end()-1) == '/' ? "" : "/");
        testDirName += (*(testDirName.end()-1) == '/' ? "" : "/");
    }

    DOTDetector::TrainParams trainParams;
    trainParams.winSize = Size(84, 84);
    trainParams.regionSize = 7;
    trainParams.minMagnitude = 60; // we ignore pixels with magnitude less then minMagnitude
    trainParams.maxStrongestCount = 7; // we find such count of strongest gradients for each region
    trainParams.maxNonzeroBits = 6; // we filter very textured regions (that have more then maxUnzeroBits count of 1s (ones) in the template)
    trainParams.minRatio = 0.85f;

    // 1. Train detector
    DOTDetector dotDetector;
    dotDetector.train( baseDirName, trainParams, true );

    const vector<string>& classNames = dotDetector.getClassNames();
    const vector<DOTDetector::DOTTemplate>& dotTemplates = dotDetector.getDOTTemplates();

    vector<Scalar> colors( classNames.size() );
    fillColors( colors );
    cout << "Templates count " << dotTemplates.size() << endl;

    vector<string> testFilenames;
    readTestImageNames( testDirName + "images.txt", testFilenames );
    if( testFilenames.empty() )
    {
        cout << "Can not read no one test images" << endl;
        return -1;
    }

    // 2. Detect objects
    DOTDetector::DetectParams detectParams;
    detectParams.minRatio = 0.8f;
    detectParams.minRegionSize = 5;
    detectParams.maxRegionSize = 11;
#if SHOW_ALL_RECTS_BY_ONE
    detectParams.isGroup = false;
#endif

    for( size_t imgIdx = 0; imgIdx < testFilenames.size(); imgIdx++ )
    {
        string curFilename = testDirName + testFilenames[imgIdx];
        cout << curFilename << endl;
        Mat queryImage = imread( curFilename, 0 );

        if( queryImage.empty() )
            continue;

        cout << "Detection start ..." << endl;
        vector<vector<Rect> > rects;
        vector<vector<float> > ratios;
        vector<vector<int> > trainTemlateIdxs;
        dotDetector.detectMultiScale( queryImage, rects, detectParams, &ratios, &trainTemlateIdxs );
        cout << "end" << endl;

        Mat draw;
        cvtColor( queryImage, draw, CV_GRAY2BGR );

#if SHOW_ALL_RECTS_BY_ONE
        DOTDetector::groupRectanglesList( rects, 3, 0.2 );
#endif

        const int textStep = 25;
        for( size_t ci = 0; ci < classNames.size(); ci++ )
        {
            putText( draw, classNames[ci], Point(textStep, textStep*(1+ci)), 1, 2, colors[ci], 3 );
            for( size_t ri = 0; ri < rects[ci].size(); ri++ )
            {
                rectangle( draw, rects[ci][ri], colors[ci], 3 );
            }
        }
        Mat scaledDraw;
        cv::resize( draw, scaledDraw, Size(640, 480) );
        imshow( "detection result", scaledDraw );

        cv::waitKey();
    }

}