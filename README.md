# 环境安装  
主机环境：ubuntu20及以上  
sudo apt-get install git wget flex bison gperf python python3 python3-venv python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

# 设置工具  
(注意：可能需要梯子)  
./install.sh esp32c3  

# 设置环境变量  
. ./export.sh  

# 配置工程  
cd ./examples/get-started/hello_world  
idf.py set-target esp32 | esp32c3  
idf.py menuconfig  

# 编译  
idf.py build  

# 烧录  
idf.py -p /dev/ttyACM0 flash  

# 监视  
idf.py -p /dev/ttyACM0 monitor  

# 烧录 + 监视  
idf.py -p /dev/ttyACM0 flash monitor  

# 通过USB烧写  
构建DFU镜像：idf.py dfu  
烧写DFU镜像：idf.py dfu-flash  
列出DFU设备：idf.py dfu-list  
烧写DFU设备：idf.py dfu-flash --path 1-1  

# 擦除flash
esp32 : idf.py -p /dev/ttyACM0 erase-flash  
esp32c3 : esptool.py --chip esp32c3 --port /dev/ttyACM0 erase_flash

# 错误解决  
1. A fatal error occurred: Could not open /dev/ttyACM0, the port doesn't exist  
    临时：sudo chmod a+rw /dev/ttyACM0  
    永久：sudo usermod -a -G dialout $USER 然后重启  

2. A fatal error occurred: Packet content transfer stopped (received 24 bytes)  
    esp32c3通过usb下载时遇到这个问题，需选上idf.py menuconfig-->serial flasher config-->Disable download stub  

3. KeyError: 'idfSelectedId'  
    切换idf版本后要先删除环境变量  
    sudo rm ~/.espressif/idf-env.json  
    再安装：./install.sh  
    安装时注意升级pip  