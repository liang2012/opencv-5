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

#include "cvtest.h"

void InitAAcc();
void InitAApproxPoly();
void InitAArrayIterator();
void InitABackProject();
void InitACalibration();
void InitACamShift();
void InitACanny();
void InitAChessCorners();
void InitAConDens();
void InitAContours();
void InitAConvexHull();
void InitACorner();
void InitACvCalculate();
void InitADerv();
void InitADistanceTransform();
void InitAEMD();
void InitAFitEllipse();
void InitAFitLine();
void InitAFloodFill();
void InitAHaar();
void InitAHistogram();
void InitACalcContrastHist();
void InitACalcOpticalFlowLK();
void InitACalcOpticalFlowHS(); 
void InitAHistogramOperations();
void InitAHistograms();
void InitAHoughLines();
void InitAImage();
void InitAImageToHMMObs();
void InitAKalman();
void InitAKMeans();
void InitAMaskAcc();
void InitAMatchTemplate();
void InitAMathUtils();
void InitAMeanShift();
void InitAMinAreaRect();
void InitAMinEVal();
void InitAMoments();
void InitAMorphology();
void InitAMotionTemplates();
void InitAMotSeg();
void InitANodeIterator();
void InitAOptFlowPyrLK();
void InitAPixelAccess();
void InitAPyramids();
void InitASamplers();
void InitASequence();
void InitASnakes();
void InitAStorageArray();
void InitASubdiv();
void InitAThreshold();
void InitAAdaptThreshold();
void InitATree();
void InitATreeIterator();
void InitAFloodFill();
void InitAFloodFill8();
void InitAUnDistort();
void InitAEigenObjects();
void InitAContourMoments();
void InitAMatchContours();
void InitACreateContourTree();
void InitAMatchContourTrees();
void InitAPreCorner();
void InitAPyrSegmentation();
void InitAGestureRecognition();
void InitAPOSIT();

CvTS test_system;

/*============================== Algorithm Tests =============================*/
int main(int argC,char *argV[])
{
    char** argv = (char**)malloc( 10*sizeof(argv[0]));
    int argc = 0;
    argv[argc++] = argV[0];
    argv[argc++] = "-A";
    argv[argc++] = "-l";
    argv[argc++] = "-s";
    argv[argc++] = "-m";
    argv[argc++] = "-B";

#ifdef WIN32
    atsInitModuleTestData( argV[0], "../tests/cv/testdata" );
#else
    atsInitModuleTestData( argV[0], "../testdata" );
#endif
    //atsLoadPrimitives( 1 );

    /*InitAAcc();
    // InitAAdaptThreshold(); // test is not up-to-date
    InitAApproxPoly();
    InitACalcOpticalFlowLK();
    InitACalcOpticalFlowHS();
    InitACalibration();
    InitACamShift();
    InitACanny();
    InitAChessCorners();
    InitAConDens();
    InitAContours();
    InitAContourMoments();
    InitAConvexHull();
    InitACorner();
    InitACreateContourTree();
    InitADerv();
    InitADistanceTransform();
    InitAEigenObjects();
    InitAEMD();

    InitAFitEllipse();
    InitAFitLine();
    InitAFloodFill();
    InitAFloodFill8();
    InitAHaar();
    //InitACalcContrastHist(); // the function is not available
    InitAHistograms();
    InitAHoughLines();
    InitAImage();
    //InitAImageToHMMObs(); // test uses IPL DCT
    InitAKalman();
    //InitAKMeans(); // test is not up-to-date
    InitAMaskAcc();
    InitAMatchContours();
    InitAMatchContourTrees();
    InitAMatchTemplate();
    InitAMeanShift();
    InitAMinAreaRect();
    InitAMoments();
    InitAMorphology();
    InitAMotionTemplates();
    // InitAMotSeg(); // test is not up-to-date
    InitAMinEVal ();
    InitAOptFlowPyrLK();
    InitAPixelAccess();

    InitAPOSIT();
    InitAPyramids();
    InitASamplers();
    InitASnakes();
    InitASubdiv();
    InitAThreshold();
    InitAUnDistort();
    InitAPreCorner();
    InitAPyrSegmentation();
    //InitAGestureRecognition(); // some functionality has been removed
    */
    test_system.run( argC, argV );
    fflush( stdout );
    printf( "Now running the old-style tests...\n" );

    trsRun( argc, argv );
    printf("Done\n");
    free( argv );
    return 0;
}

/* End of file. */
