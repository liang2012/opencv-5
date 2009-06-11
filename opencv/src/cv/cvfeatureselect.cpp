/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include "_cv.h"

namespace cv
{

template<typename T> struct lessThanPtr
{
    bool operator()(const T* a, const T* b) const { return *a < *b; }
};

void goodFeaturesToTrack( const Mat& image, Vector<Point2f>& corners,
                          int maxCorners, double qualityLevel, double minDistance,
                          const Mat& mask, int blockSize,
                          bool useHarrisDetector, double harrisK )
{
    CV_Assert( qualityLevel > 0 && minDistance >= 0 && maxCorners >= 0 );

    if( mask.data )
        CV_Assert( mask.type() == CV_8UC1 && mask.size() == image.size() );

    Mat eig, tmp;
    if( useHarrisDetector )
        cornerHarris( image, eig, blockSize, 3, harrisK );
    else
        cornerMinEigenVal( image, eig, blockSize, 3 );

    double maxVal = 0;
    minMaxLoc( eig, 0, &maxVal, 0, 0, mask );
    threshold( eig, eig, maxVal*qualityLevel, 0, THRESH_TOZERO );
    dilate( eig, tmp, Mat());

    Size imgsize = image.size();

    Vector<const float*> tmpCorners;

    // collect list of pointers to features - put them into temporary image
    for( int y = 1; y < imgsize.height - 1; y++ )
    {
        const float* eig_data = (const float*)eig.ptr(y);
        const float* tmp_data = (const float*)tmp.ptr(y);
        const uchar* mask_data = mask.data ? mask.ptr(y) : 0;

        for( int x = 1; x < imgsize.width - 1; x++ )
        {
            float val = eig_data[x];
            if( val != 0 && val == tmp_data[x] && (!mask_data || mask_data[x]) )
                tmpCorners.push_back(eig_data + x);
        }
    }

    sort( tmpCorners, lessThanPtr<float>() );
    corners.clear();
    size_t i, j, total = tmpCorners.size(), ncorners = 0;
    
    minDistance *= minDistance;

    // select the strongest features
    for( i = 0; i < total; i++ )
    {
        int ofs = (const uchar*)tmpCorners[i] - eig.data;
        int y = ofs / eig.step;
        int x = (ofs - y*eig.step)/sizeof(float);

        if( minDistance > 0 )
        {
            for( j = 0; j < ncorners; j++ )
            {
                float dx = x - corners[j].x;
                float dy = y - corners[j].y;
                if( dx*dx + dy*dy < minDistance )
                    break;
            }
            if( j < ncorners )
                continue;
        }

        corners.push_back(Point2f((float)x, (float)y));
        ++ncorners;
        if( maxCorners > 0 && (int)ncorners == maxCorners )
            break;
    }
}

}

CV_IMPL void
cvGoodFeaturesToTrack( const void* _image, void*, void*,
                       CvPoint2D32f* _corners, int *_corner_count,
                       double quality_level, double min_distance,
                       const void* _maskImage, int block_size,
                       int use_harris, double harris_k )
{
    cv::Mat image = cv::cvarrToMat(_image), mask;
    cv::Vector<cv::Point2f> corners;

    if( _maskImage )
        mask = cv::cvarrToMat(_maskImage);

    CV_Assert( _corners && _corner_count );
    cv::goodFeaturesToTrack( image, corners, *_corner_count, quality_level,
        min_distance, mask, block_size, use_harris != 0, harris_k );

    size_t i, ncorners = corners.size();
    for( i = 0; i < ncorners; i++ )
        _corners[i] = corners[i];
    *_corner_count = (int)ncorners;
}

/* End of file. */
