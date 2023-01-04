cmake_minimum_required(VERSION 3.0.2)
project(oculii_radar_driver)

set (pcl_DIR "/usr/lib/x86_64-linux-gnu/cmake/pcl")
find_package(catkin REQUIRED COMPONENTS
  image_transport
  pcl
  pcl_ros
  roscpp
  sensor_msgs
  std_msgs
)

catkin_package(
)




option(BUILD_STATIC_LIBS "Build static library" OFF)
option(BUILD_DYNAMIC_LIBS "Build shared library" ON)

message("-----------------------")
message("-- BUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}")
message("-- BUILD_DYNAMIC_LIBS=${BUILD_DYNAMIC_LIBS}")
message("-----------------------")


######################
# PACKAGES/VARIABLES #
######################
set(CMAKE_MACOSX_RPATH 0)

#Set boost variable
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Threads REQUIRED)


#############
# LIBRARIES #
#############

set(OCULII_CORE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/ubuntu18/oculii_core/radar/ethernet/libradar_shared.so)
set(OCULII_SDK_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/ubuntu18/oculii_feature/ethernet/liboculii_feat_shared.so)
set(OCULII_CORE_DEP ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})



#############
# INCLUDES #
#############

include_directories(
# include
  ${catkin_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oculii_core/radar/include
)


############
# BINARIES #
############

link_libraries(${OCULII_SDK_LIB} ${OCULII_CORE_LIB})
add_executable(oculii_radar src/main_driver.cpp)
target_link_libraries(oculii_radar ${OCULII_CORE_LIB} ${OCULII_CORE_DEP} ${catkin_LIBRARIES})


unset(BUILD_STATIC_LIBS CACHE)
unset(BUILD_DYNAMIC_LIBS CACHE)
