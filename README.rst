
# Bike-to-bike (B2B) communication 
In this project we evaluate the efficiency of BLE for B2B communication. For the BLE implementation, we used the nRF5340 from Nordic Semiconductor. This project refers to the paper "Evaluating Bi-directional Connectionless BLE for Bike-to-Everything Wireless Communications"

Material:
* Laptop with VS Code installed
* nRF5340 DK
* micro USB cable

The files in this reporsitory were used to setup the board for different experiments. The reporsitory is divided as follows

* /src: Source files used to defined the functions used
* /include: Header files with the functions created
* prj.conf: nRF configuration file
* nrf5340dk_nrf5340_cpuapp_ns.overlay: setup for GPIO and LEDs
* CMakeLists.txt: Specify the scripts to be compiled

For each functionality of our system we created a source and header files called"*functionality*_module". The modules created are:

* beacon_module: transmission setup, functions and simulated data generation
* scan_module: reception setup, parsing and package storage
* sdcard_module: read/write functions for the micro SD cards
* uart_module: setup UART and messages to be sent and received for the sychronizaton process
* gnss_module (disable): GNSS setup, necessary if using nRF9160 built in GNSS

Besides the modules, we also created a main.c file that is used to initialize the system and call the functions from the modules. And, to make parameter tuning simpler, we use ble_settings.h where we group all the main tunable parameters.

## Configuration process
Prerequisite:
* For the initial setup of a new board, follow: https://docs.nordicsemi.com/bundle/ncs-2.4.3/page/nrf/working_with_nrf/nrf53/nrf5340_gs.html 
* For a general overview on how to create and application with VS Code, see: https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/get_started/build_app_ncs.html
* For extra tutorials see: https://academy.nordicsemi.com/ (SDK fundamentals and Bluetooth Low Energy fundamentals)

Assiming the first setup is already done, the next steps will show how to copy and tune the B2B project:

1. Download the zip file of the project from GitLab and extract it to the desired folder
2. Open VS Code
3. Navigate to the nRF Connect extension
4. Select "open existing application" in the nRF Connect extension and select the folder that you extracted
5. On the "Application" tab of select "Add build configuration"
6. At the configuration setup, select "nrf5340dk_nrf5340_cpuapp_ns" as the Board taget and "boards/nrf5340dk_nrf5340_cpuapp_ns.overlay" for the Base Devicetree overlays 
7. Click on "Generate and build", once over you will have a build of the project ready to be uploaded to the board (this will take some minutes)
8. Connect the sdcard shield to the nRF5340 DK (if necessary) an connect the board to the computer
9. After the project is build, you can flash an nRF5340 DK with it. To do that, in the nRF Connect extension, select "Flash" under the "Actions" tab. If there's only one board connected the appplication will be flashed to that board, else on the top of the screen you will be asked which board to flash.
10. To display the board serial logs, go to the VS Code terminal, select nRF Serial Terminal (in the menu you get when you click the arrow down sign next to the plus sign). You will be ask which VCOM port to open, the log will be on VCOM1    

## Extra setting in the ble_settings.h file 
* Parameters: 
    * PACKET_COPIES - number of copies sent in each advertising moment
    * INTERVAL - Packege generation interval in milliseconds
    * ADV_INTERVAL - this value times 0.625 will be the interval in milliseconds
    * SCAN_INTERVAL - this value times 0.625 will be the interval in milliseconds
    * SCAN_WINDOW - this value times 0.625 will be the interval in milliseconds
    * SCAN_WINDOW_MAIN - match this value with the total milliseconds values of SCAN_WINDOW

* ROLE setting: as we sychronize the boards over UART and, in some test, only one board has a sdcard, this setting determine which board is being flashed. Make sure to change before building the application. 

## Contact 
For any further questions you can contact this email: pedro.wo@outlook.com


