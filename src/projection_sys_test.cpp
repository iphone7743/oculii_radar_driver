#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include "radar_system.h"
#include <sys/types.h>


std::string configPath = "../config.xml";


// ======================================== Create an instance of the system =======================================================
oculii::RadarSystem* mySystem = oculii::RadarSystem::GetRadarSystemInstance(configPath);
// =========================================== Main Function ====================================================

int main()
{
    std::cout << "Testing application layer stuff" << std::endl;

    oculii::RadarErrorCode status = mySystem->StartSystem();
    if (status == oculii::RadarErrorCode::SUCCESS)
        std::cout << "Start Radar System success! " << std::endl;  // validate the config file and instantiate radar objects in the system
    else
    {
        std::cout << "Start Radar System fail with error " << int(status) << std::endl;
        return 0;
    }



    uint32_t oldFrameNumber = 0;

    //create vectors to receive pcl and tracker from different radars. we create to receive only one radar pcl and tracker here
    std::map< int, oculii::RadarTrackerPacket > trk;

    std::cout << "Initialization done" << std::endl;

    mySystem->StartRadarReceive();


    while (true)
    {
        // ================= Read Tracker ==========================
        if (mySystem->GetTracker(trk) == oculii::RadarErrorCode::SUCCESS)
        {
            std::cout << "Trk Timestamp: " << trk.begin()->second.timestamp << " FrameNum: " << trk.begin()->second.frameNumber << " Num of trks: " << trk.begin()->second.numTrkOut << std::endl;
        }


        std::this_thread::sleep_for(std::chrono::microseconds(int(1e6 / 40.0)));
    }


    return 0;


}
