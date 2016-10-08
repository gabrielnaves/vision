#include "calibration/color_calibration.hpp"

ColorCalibration::ColorCalibration(){
    //initial parameters
    hsv_min_h_ = 0;
    hsv_max_h_ = 0;
    hsv_min_s_ = 0;
    hsv_max_s_ = 0;
    hsv_min_v_ = 0;
    hsv_max_v_ = 0;
    hdr_ = 1;
    is_blue_saved_ = false;
    is_yellow_saved_ = false;
    is_orange_saved_ = false;
    window_name_ = "Color Calibration";
    window_name_HDR_ = "HDR Calibration";

    //decide if open or not file to save color calibration
    ros::param::get("/vision/calibration/calibrate_team_color", calibrate_);
    auto sourceDir = ros::package::getPath("vision").append("/data/");
    auto filename = "color_calibration.yaml";

    if (calibrate_)
    {

        //create UI
        cv::namedWindow(window_name_);
        cv::createTrackbar("HMIN", window_name_, &hsv_min_h_, 360);
        cv::createTrackbar("HMAX", window_name_, &hsv_max_h_, 360);
        cv::createTrackbar("SMIN", window_name_, &hsv_min_s_, 256);
        cv::createTrackbar("SMAX", window_name_, &hsv_max_s_, 256);
        cv::createTrackbar("VMIN", window_name_, &hsv_min_v_, 256);
        cv::createTrackbar("VMAX", window_name_, &hsv_max_v_, 256);
        cv::namedWindow(window_name_HDR_);
        cv::createTrackbar("HDR", window_name_HDR_, &hdr_, 300);
        //save previous parameters
        colorHandler_ = cv::FileStorage(sourceDir+filename, cv::FileStorage::READ);
        old_blue = colorHandler_["Blue"];
        old_yellow = colorHandler_["Yellow"];
        old_orange = colorHandler_["Orange"];
        
        //set file calibration to save HSV values
        colorManager_ = cv::FileStorage(sourceDir+filename, cv::FileStorage::WRITE);
    }
    else
        ROS_INFO("Color already calibrated, to recalibrate change config!");

}

ColorCalibration::~ColorCalibration(){
    if (calibrate_)
        if (not is_blue_saved_)
        {
            cv::FileNodeIterator it = old_blue.begin();
            cv::Scalar min = (cv::Scalar)(*it)["Min"];
            cv::Scalar max = (cv::Scalar)(*it)["Max"];
            colorManager_ << "Blue";
            colorManager_ << "{" << "Min" << min;
            colorManager_        << "Max" << max;
            colorManager_ << "}";
        }
        if (not is_yellow_saved_)
        {
            cv::FileNodeIterator it = old_yellow.begin();
            cv::Scalar min = (cv::Scalar)(*it)["Min"];
            cv::Scalar max = (cv::Scalar)(*it)["Max"];
            colorManager_ << "Yellow";
            colorManager_ << "{" << "Min" << min;
            colorManager_        << "Max" << max;
            colorManager_ << "}";
        }
        if(not is_orange_saved_){
            cv::FileNodeIterator it = old_orange.begin();
            cv::Scalar min = (cv::Scalar)(*it)["Min"];
            cv::Scalar max = (cv::Scalar)(*it)["Max"];
            colorManager_ << "Orange";
            colorManager_ << "{" << "Min" << min;
            colorManager_        << "Max" << max;
            colorManager_ << "}";
        }
        colorHandler_.release();
        colorManager_.release();
}

void ColorCalibration::calibrate(cv::Mat rgb_input){
    if (calibrate_)
    {
        equalizeIntensity(rgb_input);
        gammaCorrection(rgb_input);
        cv::imshow("Color changed", rgb_input);
        cv::waitKey(1);
        if (is_blue_saved_ && is_yellow_saved_ && is_orange_saved_)
            cv::destroyWindow(window_name_);
        else{
            cv::Mat hsv_converted;
            cv::cvtColor(rgb_input, hsv_converted, CV_BGR2HSV);

            cv::inRange(hsv_converted, cv::Scalar(hsv_min_h_, hsv_min_s_, hsv_min_v_),
                         cv::Scalar(hsv_max_h_, hsv_max_s_, hsv_max_v_), segmented_image_);

            cv::imshow(window_name_, segmented_image_);
        }

        if (cv::waitKey(10) == 'b' && not is_blue_saved_)
        {
            ROS_INFO("Blue Calibrated!");
            save("Blue");
        }
        if (cv::waitKey(10) == 'y' && not is_yellow_saved_)
        {
            ROS_INFO("Yellow Calibrated!");
            save("Yellow");
        }
        if (cv::waitKey(10) == 'o' && not is_orange_saved_)
        {
            ROS_INFO("Orange Calibrated!");
            save("Orange");
        }
    }
    rgb_calibrated_ = rgb_input;
}

void ColorCalibration::save(std::string color){
    cv::Mat colorMat = cv::Mat_<int>(2, 3, CV_8U) << hsv_min_h_, hsv_min_s_, hsv_min_v_,
                                                     hsv_max_h_, hsv_max_s_, hsv_max_v_;
    colorManager_ << color << colorMat;
    if (color == "Blue")
        is_blue_saved_ = true;
    if (color == "Yellow")
        is_yellow_saved_ = true;
    if (color == "Orange")
        is_orange_saved_ = true;
}

bool ColorCalibration::isCalibrated(){
    return calibrate_;
}

cv::Mat ColorCalibration::getRGBCalibrated(){
    return rgb_calibrated_;
}

void ColorCalibration::gammaCorrection(cv::Mat& rgb_input){
    unsigned char lut[256];
    float fGamma = hdr_/10;
    for (int i = 0; i < 256; i++)
        lut[i] = cv::saturate_cast<uchar>(pow((float)(i / 255.0), fGamma) * 255.0f);
 
    cv::Mat new_image;
    new_image = rgb_input.clone();
 
 
    cv::MatIterator_<cv::Vec3b> it, end;
 
    for (it = new_image.begin<cv::Vec3b>(), end = new_image.end<cv::Vec3b>(); it != end; it++){
 
    (*it)[0] = lut[((*it)[0])];
 
    (*it)[1] = lut[((*it)[1])];
 
    (*it)[2] = lut[((*it)[2])];
    }
    rgb_input = new_image;
}

void ColorCalibration::equalizeIntensity(cv::Mat& rgb_input)
{
    if(rgb_input.channels() >= 3)
    {
        cv::Mat ycrcb;

        cv::cvtColor(rgb_input,ycrcb,CV_BGR2YCrCb);
        std::vector<cv::Mat> channels;
        cv::split(ycrcb,channels);

        cv::equalizeHist(channels[0], channels[0]);

        cv::merge(channels,ycrcb);

        cv::cvtColor(ycrcb,rgb_input,CV_YCrCb2BGR);

    }else
        ROS_ERROR("[COLOR CALIBRATION] 3 channels are needed to >>color<< calibration!");
}