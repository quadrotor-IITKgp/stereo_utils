#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <stereo_msgs/DisparityImage.h>

class getDepth
{
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_l;
  ros::Subscriber image_sub_d;
  cv::Mat left_img;
  cv::Mat_<uint8_t> disp;

  int LowerH;
  int LowerS;
  int LowerV;
  int UpperH;
  int UpperS;
  int UpperV;
  int erosion_kernel;

public:
  getDepth()
    : it_(nh_)
  {
    image_sub_l = it_.subscribe("/stereo/left/image_rect_color", 1, &getDepth::left_callback, this);
    image_sub_d = nh_.subscribe("/stereo/disparity", 1 ,&getDepth::disparity_callback, this);


    LowerH = 0;
    LowerS = 0;
    LowerV = 0;
    UpperH = 180;
    UpperS = 196;
    UpperV = 170;
    erosion_kernel = 3;
    cv::namedWindow("left");
    cv::namedWindow("disparity");
    cv::namedWindow("mask");
    cv::createTrackbar("LowerH","left",&LowerH,180,NULL);
    cv::createTrackbar("UpperH","left",&UpperH,180,NULL);
    cv::createTrackbar("LowerS","left",&LowerS,256,NULL);
    cv::createTrackbar("UpperS","left",&UpperS,256,NULL);
    cv::createTrackbar("LowerV","left",&LowerV,256,NULL);
    cv::createTrackbar("UpperV","left",&UpperV,256,NULL);
    cv::createTrackbar("ErosionKernel 2n+3","left",&erosion_kernel,6,NULL);

    //cv::namedWindow("masked_disparity")
  }

  ~getDepth()
  {
    cv::destroyWindow("left");
    cv::destroyWindow("disparity");
    cv::destroyWindow("mask");
    //cv::destroyWindow("masked_disparity")
  }

  void left_callback(const sensor_msgs::ImageConstPtr&);
  void disparity_callback(const stereo_msgs::DisparityImagePtr&);
  cv::Mat IsolateColor(const cv::Mat& src);
  cv::Mat reduceNoise(const cv::Mat& src);
};


cv::Mat getDepth::IsolateColor(const cv::Mat& src)
{
    cv::Mat out,img_hsv; 
    cv::cvtColor(src,img_hsv,CV_BGR2HSV);
    cv::inRange(img_hsv,cv::Scalar(LowerH,LowerS,LowerV),cv::Scalar(UpperH,UpperS,UpperV),out);
    return out; 
}

cv::Mat getDepth::reduceNoise(const cv::Mat& src)
{
    cv::Mat dst = src.clone();
    int i, j, k, l, count;

    //-------- Remove Borders ----------//
    for(i=0;i<dst.rows;i++)
    {
        dst.at<uchar>(i,0)=0;
        dst.at<uchar>(i,dst.cols-1)=0;
    }
    for(i=0;i<dst.cols;i++)
    {
        dst.at<uchar>(0,i)=0;
        dst.at<uchar>(dst.rows-1,i)=0;
    }
    //----------------------------------//

    //-------- Perform a variation of Erosion followed by Dilation -----------//
    int kern = 2*erosion_kernel + 3;
    for(i=1;i<src.rows-1;i++)
    {
        for(j=1;j<src.cols-1;j++)
        {
            if(src.at<uchar>(i,j)==255)
            {
                count=0;
                 for(l=i-kern/2;l<=i+kern/2;l++)
                 {
                    for(k=j-kern/2;k<=j+kern/2;k++)
                        {
                            if((!(i==l&&j==k)) && src.at<uchar>(l,k)==255)
                            {
                                count ++;
                            }
                        }
                 }
                 if(count<=(kern*kern)/2) dst.at<uchar>(i,j)=0;
            }

        }
    }
     for(i=1;i<src.rows-1;i++)
    {
        for(j=1;j<src.cols-1;j++)
        {
            if(src.at<uchar>(i,j)==0)
            {
                count=0;
                 for(l=i-kern/2;l<=i+kern/2;l++)
                 {
                    for(k=j-kern/2;k<=j+kern/2;k++)
                        {
                            if((!(i==l&&j==k)) && src.at<uchar>(l,k)==0)
                            {
                                count ++;
                            }
                        }
                 }
                 if(count<=(kern*kern)/2) dst.at<uchar>(i,j)=255;
            }

        }
    }
    return dst;
}

void getDepth::left_callback(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8); 
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }
    this->left_img = cv_ptr->image;
    cv::imshow("left",cv_ptr->image);
    cv::waitKey(30);
  }
void getDepth::disparity_callback(const stereo_msgs::DisparityImagePtr& msg)
  {
    cv::Mat_<float> dMat(msg->image.height, msg->image.width, reinterpret_cast<float*>(&(msg->image.data[0])));
    this->disp = (dMat) * (255/msg->max_disparity);

    cv::Mat img_mask1 = IsolateColor(this->left_img);
    cv::Mat img_mask2 = reduceNoise(img_mask1);

    cv::imshow("disparity",disp);
    cv::imshow("mask",img_mask2);
    cv::waitKey(30);
  }  


int main(int argc, char** argv)
{
  ros::init(argc, argv, "image_converter");
  getDepth obj;
  ros::spin();
  return 0;
}  