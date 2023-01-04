#######################################
#######essential dependencies##########
#######################################
sudo apt-get install build-essential
sudo apt-get install cmake
sudo apt-get install libboost-all-dev

#######################################
#######unzip other dependencies##########
#######################################
sudo apt-get install unzip
unzip dependencies.zip

#######################################
########CANFD dependencies#############
#######################################

#http://www.linuxfromscratch.org/lfs/view/8.3-systemd/chapter06/automake.html
cd automake-1.16
sudo chmod 777 configure
./configure --prefix=/usr --docdir=/usr/share/doc/automake-1.16.1
make
sudo make install
cd ..

#http://www.linuxfromscratch.org/blfs/view/svn/general/popt.html
sudo chmod 777 install_popt.sh
sudo ./install_popt.sh

cd peak-linux-driver-8.10.2
sudo make clean
sudo make
sudo make install

cd ..
cd PCAN-Basic_Linux-4.4/libpcanbasic/pcanbasic
make clean
make 
sudo make install
cd ../../../

#######################################
########Camera or visualization########
#######################################
sudo chmod 777 opencv.sh
./opencv.sh
sudo apt-get install cmake libblkid-dev e2fslibs-dev libboost-all-dev libaudit-dev
