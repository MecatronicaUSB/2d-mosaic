// FCOMP MODULE
/**
 * @file main.h
 * @brief Contais the main code for feature comparison module
 * @version 1.0
 * @date 20/01/2018
 * @author Victor Garcia
 * @title Main code
 */

#include "../../common/utils.h"
#include "../include/options.h"
#include "../include/detector.h"

/// Dimensions to resize images
#define TARGET_WIDTH	640   
#define TARGET_HEIGHT	480 

const std::string green("\033[1;32m");
const std::string reset("\033[0m");

/// User namespaces
using namespace std;
using namespace cv;
using namespace cv::xfeatures2d;

/*
 * @function main
 * @brief Main function
 * @detail Compute the feature extraction and match using the desired algorithm.
 * Can perform the algorithm with:
 * - Two images
 * - Video
 * - A set of frames
 * Using the following extractors: 
 * - Sift
 * - Surf
 * - Orb
 * - Kaze
 * And the folowing matchers: 
 * - Brute force
 * - Flann
 */
int main( int argc, char** argv ) {

    double tot_matches=0, tot_good =0;
    double t;
    int n_matches=0, n_good=0;
    int i=0, n_img=0;
    int n_iter = 0, step_iter = 0;
    int fps =0, fcnt=0;

    parser.Prog(argv[0]);

    vector<string> file_names;
    vector<vector<DMatch> > matches;
    vector<DMatch>  good_matches;
    vector<KeyPoint> keypoints[2];
    Mat result;
    Mat descriptors[2];
    Mat img[2], img_ori[2];

    try{
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help){
        std::cout << parser;
        return 1;
    }
    catch (args::ParseError e){
        std::cerr << e.what() << std::endl;
        std::cerr << "Use -h, --help command to see usage" << std::endl;
        //std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError e){
        std::cerr << "Bad imput commands" << std::endl;
        std::cerr << "Use -h, --help command to see usage" << std::endl;
        return 1;
    }
   
    int minHessian = 400;
    Ptr<Feature2D> detector;
    if(op_sift){
        detector = SIFT::create();
    }else if(op_surf){
        detector = SURF::create(minHessian);
    }else if(op_orb){
        detector = ORB::create();
    }else if(op_kaze){
        detector = KAZE::create();
    }else if(op_akaze){
        detector = AKAZE::create();
    }
    Ptr<DescriptorMatcher> matcher;
    if(op_flann){
        matcher = FlannBasedMatcher::create();
    }else{
        matcher = BFMatcher::create();
    }
    
    // Two images as imput
    if (op_img){
        n_iter = 2;
        step_iter = 1;
        t = (double) getTickCount();
        // Check for two image flags and patchs (-i imageName)
        for(const auto img_name: args::get(op_img)){
            img[i++] = imread(img_name, IMREAD_UNCHANGED);
            if( !img[i-1].data){
                cout<< " --(!) Error reading image "<< i << endl; 
                cerr << parser;
                return -1;
            }
            cout<< " -- Loaded image "<< img_name << endl;
            // Two images loaded successfully
        }
        if(i<2){
            cout<< " -- Insuficient imput data " << endl;
            std::cerr << "Use -h, --help command to see usage" << std::endl;
            return -1;
        }
    }
    string dir_ent;
    if(op_dir){
        step_iter = 1;
        dir_ent = args::get(op_dir);
        file_names = read_filenames(dir_ent);
        n_iter = file_names.size();
    } 
    VideoCapture vid;
    if(op_vid){
        vid.open(args::get(op_vid));
        if(!vid.isOpened()){
            cout << "Couldn't open Video " << endl;
            return -1;
        }
        double fps = vid.get(CAP_PROP_FPS);
        double fcnt = vid.get(CAP_PROP_FRAME_COUNT);
        cout << "Video opened \nFrames per second: "<< fps << "\nFrames in video:   "<<fcnt<< endl;
        step_iter = fps;
        if(op_frate){
            step_iter = args::get(op_frate);
        }
        cout << "Pick images each "<<step_iter<< " frames"<< endl;
        n_iter = fcnt;
    }
    t = (double) getTickCount();
    for(i=0; i<n_iter-step_iter; i+=step_iter){
        if(op_dir){
            img[0] = imread(file_names[i++],IMREAD_COLOR);
            img[1] = imread(file_names[i],IMREAD_COLOR);
        }
        if(op_vid){
            vid.set(CAP_PROP_POS_FRAMES,i);
            vid >> img[0];
            vid.set(CAP_PROP_POS_FRAMES,i+=step_iter);
            vid >> img[1];
        }
        // Resize the images to 640 x 480
        resize(img[0], img[0], Size(TARGET_WIDTH, TARGET_HEIGHT), 0, 0, CV_INTER_LINEAR);
        resize(img[1], img[1], Size(TARGET_WIDTH, TARGET_HEIGHT), 0, 0, CV_INTER_LINEAR);

        img_ori[0] = img[0].clone();
        img_ori[1] = img[1].clone();

        // Conver images to gray
        cvtColor(img[0],img[0],COLOR_BGR2GRAY);
        cvtColor(img[1],img[1],COLOR_BGR2GRAY);

        // Apply pre-processing algorithm if selected (histogram stretch)
        if(op_pre){
            imgChannelStretch(img[0], img[0], 1, 99);
            imgChannelStretch(img[1], img[1], 1, 99);
        }

        // Detect the keypoints using desired Detector and compute the descriptors
        detector->detectAndCompute( img[0], Mat(), keypoints[0], descriptors[0] );
        detector->detectAndCompute( img[1], Mat(), keypoints[1], descriptors[1] );

        if(!keypoints[0].size() || !keypoints[1].size()){
            cout << "No Key points Found" <<  endl;
            return -1;
        }
        // Flann needs the descriptors to be of type CV_32F (case for binary descriptors)
        if(descriptors[0].type()!=CV_32F)
        {
            descriptors[0].convertTo(descriptors[0], CV_32F);
            descriptors[1].convertTo(descriptors[1], CV_32F);
        }
        // Match the keypoints for input images
        matcher->knnMatch( descriptors[0], descriptors[1], matches, 2);
        n_matches = descriptors[0].rows;
        // Discard the bad matches (outliers)
        good_matches = getGoodMatches(matches);
        if(op_grid){
            good_matches = gridDetector(keypoints[0], good_matches);
        }

        n_good = good_matches.size();
        tot_matches+=n_matches;
        tot_good+=n_good;

        cout << "Pair  "<< n_img++ <<" -- -- -- -- -- -- -- -- -- --"  << endl;
        cout << "-- Possible matches  ["<< n_matches <<"]"  << endl;
        cout << "-- Good Matches      ["<<green<<n_good<<reset<<"]"  << endl;
        if(op_out){
            Mat img_matches;
            // Draw only "good" matches
            drawMatches( img_ori[0], keypoints[0], img_ori[1], keypoints[1],
                        good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
                        vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );
            // Show matches
            namedWindow("Good Matches", WINDOW_NORMAL);
            imshow( "Good Matches", img_matches );
            waitKey(0);
        }
        matches.clear();
        img[0].release();
        img[1].release();
        img_ori[0].release();
        img_ori[1].release();
        keypoints[0].clear();
        keypoints[1].clear();
        descriptors[0].release();
        descriptors[1].release();
    }
    cout << "\nTotal "<< n_img <<" -- -- -- -- -- -- -- -- -- --"  << endl;
    cout << "-- Total Possible matches  ["<< tot_matches <<"]"  << endl;
    cout << "-- Total Good Matches      ["<<green<<tot_good<<reset<<"]"  << endl;
    t = ((double) getTickCount() - t) / getTickFrequency();        
    cout << "   Execution time: " << t << " s" <<endl;

    return 0;
}