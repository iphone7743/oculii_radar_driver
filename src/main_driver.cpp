#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include "radar_system.h"
#include <sys/types.h>

#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#define USE_FUSION_TRACKER false


namespace pcl{

struct PointRADAR
  {
    PCL_ADD_POINT4D                     // Macro quad-word XYZ
    float alpha;                        /*!< Detection horizontal angle */
    float beta;                         /*!< Detection vertical angle */
    float range;                        /*!< Detection range to radar center */
    float doppler;                      /*!< Current doppler speed */
    float power;                        /*!< Power received by radar */
    float recoveredSpeed;               /*!< Target true speed segment along vehicle moving direction*/
    uint16_t dotFlags;                  /*!< Point status flag */
    uint16_t denoiseFlag;               /*!< Mark of noise point */
    uint16_t historyFrameFlag;          /*!< Mark of the last frame detections */
    uint16_t dopplerCorrectionFlag;     /*!< Mark of doppler correction feature */

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW     // Ensure proper alignment
  } EIGEN_ALIGN16;
}

POINT_CLOUD_REGISTER_POINT_STRUCT(PointRADAR,
                                  (float, x, x)
                                  (float, y, y)
                                  (float, z, z)
                                  (float, alpha, alpha)
                                  (float, beta, beta)
                                  (float, range, range)
                                  (float, doppler, doppler)
                                  (float, power, power)
                                  (float, recoveredSpeed, recoveredSpeed)
                                  (uint16_t, dotFlags, dotFlags)
                                  (uint16_t, denoiseFlag, denoiseFlag)
                                  (uint16_t, historyFrameFlag, historyFrameFlag)
                                  (uint16_t, dopplerCorrectionFlag, dopplerCorrectionFlag)
)





ros::Publisher radar_pub;
std::mutex mutexLock;
std::string configPath="./src/oculii_radar_driver/config.xml";



// ======================================== Create an instance of the system =======================================================
oculii::RadarSystem *mySystem_ = oculii::RadarSystem::GetRadarSystemInstance(configPath);


std::atomic<bool> hasNewData_;
std::atomic<bool> bufferSwapped_;
std::map< int, std::vector<oculii::RadarDetectionPacket> >* enhancedPclPacketsReadPt_;
std::map< int, std::vector<oculii::RadarDetectionPacket> >* enhancedPclPacketsDispPt_;
std::map< int, std::vector<oculii::RadarDetectionPacket> > enhancedPclPacketsSwapBuffer_[2];
std::map< int, oculii::RadarDetectionPacket >* pclPacketsReadPt_;
std::map< int, oculii::RadarDetectionPacket >* pclPacketsDispPt_;
std::map< int, oculii::RadarDetectionPacket > pclPacketsSwapBuffer_[2];




// ======================================== Function to display data =======================================================
void DispFrame()
{   
    const int powerThreshold = 0;


    while(true)
    {
        pcl::PointCloud<pcl::PointRADAR> radar_pcl;
        
        if(!hasNewData_)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            continue;
        }

        // swap buffer

        
        if (mySystem_->MeetEnhancedPclCondition())
        {
            // swap buffer
            auto tmpPt = enhancedPclPacketsReadPt_;
            enhancedPclPacketsReadPt_ = enhancedPclPacketsDispPt_;
            enhancedPclPacketsDispPt_ = tmpPt;
            bufferSwapped_ = true;
            hasNewData_ = false;

            for(auto it = enhancedPclPacketsDispPt_->begin(); it != enhancedPclPacketsDispPt_->end(); ++it)
                for (auto& packet : it->second)
                    for (auto& det : packet.data)
                    {
                        if (det.power < powerThreshold)
                            continue;
                        
                        pcl::PointRADAR radar_pt;
                        radar_pt.x = det.x;
                        radar_pt.y = det.y;
                        radar_pt.z = det.z;
                        radar_pt.alpha = det.alpha;
                        radar_pt.beta = det.beta;
                        radar_pt.range = det.range;
                        radar_pt.doppler = det.doppler;
                        radar_pt.power = det.power;
                        radar_pt.recoveredSpeed = det.recoveredSpeed;
                        radar_pt.dotFlags = det.dotFlags;
                        radar_pt.denoiseFlag = det.denoiseFlag;
                        radar_pt.historyFrameFlag = det.historyFrameFlag;
                        radar_pt.dopplerCorrectionFlag = det.dopplerCorrectionFlag;

                        radar_pcl.push_back(radar_pt);

                    }
        }
        else
        {
            // swap buffer
            auto tmpPt = pclPacketsReadPt_;
            pclPacketsReadPt_ = pclPacketsDispPt_;
            pclPacketsDispPt_ = tmpPt;
            bufferSwapped_ = true;
            hasNewData_ = false;

            for(auto it = pclPacketsDispPt_->begin(); it != pclPacketsDispPt_->end(); ++it)
                for (auto& det : it->second.data)
                {
                    if (det.power < powerThreshold)
                        continue;


                        pcl::PointRADAR radar_pt;
                        radar_pt.x = det.x;
                        radar_pt.y = det.y;
                        radar_pt.z = det.z;
                        radar_pt.alpha = det.alpha;
                        radar_pt.beta = det.beta;
                        radar_pt.range = det.range;
                        radar_pt.doppler = det.doppler;
                        radar_pt.power = det.power;
                        radar_pt.recoveredSpeed = det.recoveredSpeed;
                        radar_pt.dotFlags = det.dotFlags;
                        radar_pt.denoiseFlag = det.denoiseFlag;
                        radar_pt.historyFrameFlag = det.historyFrameFlag;
                        radar_pt.dopplerCorrectionFlag = det.dopplerCorrectionFlag;

                        radar_pcl.push_back(radar_pt);
                }
        }

        sensor_msgs::PointCloud2 radar_msg;
        pcl::toROSMsg(radar_pcl, radar_msg);
        radar_msg.header.stamp = ros::Time::now();
        radar_msg.header.frame_id = "oculii";
        
        radar_pub.publish(radar_msg);
    }
}





// =========================================== Main Function ====================================================

int main(int argc, char** argv)
{
    std::cout << "Initializing ROS nodes" << std::endl;
    ros::init (argc, argv, "oculii_radar");
    ros::NodeHandle nh;
    radar_pub = nh.advertise<sensor_msgs::PointCloud2>("/oculii_radar/point_cloud", 100);


    std::cout << "Testing application layer stuff" << std::endl;
    oculii::RadarErrorCode status = mySystem_->StartSystem();
    // validate the config file and instantiate radar objects in the system
    if(status == oculii::RadarErrorCode::SUCCESS)
        std::cout << "Start Radar System success! " << std::endl;                               
    else
    {   std::cout << "Start Radar System fail with error " << oculii::ErrorToString(status) << std::endl;
        return 0;}
    



    uint32_t oldFrameNumber=0;


    


    // ============ Send Commands to Radar =========================
    //create vectors to uart command parameters for different radars
    std::vector<double> mountingAngles{ 1.0 };
    std::vector<bool> wait{ false };
    std::vector<bool> multicast{ false };
    std::vector<std::string> masterIps{ "192.168.2.20" };
    std::vector <oculii::ModeCommand> modes{ oculii::ModeCommand::SENSOR_MODE_0 };
    std::vector <double> hostYaw{ 0.0 };
    std::vector <double> hostSpeed{ 0.0 };
    std::vector<int> IDs { 1 };
    //mySystem_->UpdateSensor("", IDs);
    //mySystem_->SendHostSetupCfg(IDs, mountingAngles);
    //mySystem_->SendModeSwitchCmd(IDs, modes);
    //mySystem_->SendHostInfo(IDs, hostYaw, hostSpeed);
    //mySystem_->SendPtpRequest(IDs, wait, multicast, masterIps);
    std::cout << "Initialization done" << std::endl;




    mySystem_->StartRadarReceive();





    // Init swap buffers
    enhancedPclPacketsReadPt_ = &enhancedPclPacketsSwapBuffer_[0];
    enhancedPclPacketsDispPt_ = &enhancedPclPacketsSwapBuffer_[1];
    pclPacketsReadPt_ = &pclPacketsSwapBuffer_[0];
    pclPacketsDispPt_ = &pclPacketsSwapBuffer_[1];

    

    std::thread visThread;

    // Initialize synchronization flags
    hasNewData_ = false;
    bufferSwapped_ = true;
    
    // Visualizer thread
    visThread = std::thread(DispFrame);



    while (true)
    {
        if(!bufferSwapped_)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }


        // ================= Read enhanced point cloud in the presence of external IMU ==========================
        if (mySystem_->MeetEnhancedPclCondition())
        {

            if (mySystem_->GetEnhancedPointcloud(*enhancedPclPacketsReadPt_) == oculii::RadarErrorCode::SUCCESS)
            {
                std::cout << "Pcl Timestamp: " << enhancedPclPacketsReadPt_->begin()->second.back().timestamp << " FrameNum: " << enhancedPclPacketsReadPt_->begin()->second.back().frameNumber << " Num of objs: " << enhancedPclPacketsReadPt_->begin()->second.back().numObjOut << std::endl << std::endl;
                if(enhancedPclPacketsReadPt_->begin()->second.back().frameNumber - oldFrameNumber != 1)
                    std::cout << "======================== Frame missed =======================" << std::endl;
                oldFrameNumber = enhancedPclPacketsReadPt_->begin()->second.back().frameNumber;
            }
            
        }
        // ================= Read standard pcl packet ==========================
        else
        {
            if (mySystem_->GetPointcloud(*pclPacketsReadPt_) == oculii::RadarErrorCode::SUCCESS)
            {
                std::cout << "Pcl Timestamp: " << pclPacketsReadPt_->begin()->second.timestamp << " FrameNum: " << pclPacketsReadPt_->begin()->second.frameNumber << " Num of objs: " << pclPacketsReadPt_->begin()->second.numObjOut << std::endl << std::endl;
                if(pclPacketsReadPt_->begin()->second.frameNumber - oldFrameNumber != 1)
                    std::cout << "======================== Frame missed =======================" << std::endl;
                oldFrameNumber = pclPacketsReadPt_->begin()->second.frameNumber;
                // pclPacketsReadPt_->begin()->second.data.at(1).x;
            }
        }

        bufferSwapped_ = false;
        hasNewData_ = true;

    }

    visThread.join();
    return 0;
}
