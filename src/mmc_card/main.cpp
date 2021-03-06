#include <iostream>
#include <string>
#include <sstream>
#include <stdint.h>
#include <fstream>
#include <time.h>
#include <iomanip>

#include <chrono>
#include <thread>
#include <vector>

#include <ftd2xx.h>

#include <common/FTDICommon.h>
#include <common/FTDIInitialization.h>

#include "MMCCommands.h"
#include "MMCDump.h"
#include "MMCInitialization.h"

#define FTDI_DEVICE_STRING_ID "USB FIFO"

bool OpenDevice(FT_HANDLE& ftHandle)
{
   int index = psvcd::GetDeviceIndex(FTDI_DEVICE_STRING_ID);

   if(index < 0)
      return false;  

   if(!psvcd::OpenDevice(index, ftHandle))
      return false;

   if(!psvcd::ConfigureFTDIPort(ftHandle))
      return false;

    if(!psvcd::SyncMMPSE(ftHandle))
       return false;

   if(!psvcd::ConfigureSettings(ftHandle))
      return false;

   return true;
}

int standalone_initialize_mmc()
{
   FT_HANDLE ftHandle;

   if(!OpenDevice(ftHandle))
      return -1;

   //try to initialize mmc card
   psvcd::InitializeMMCCard(ftHandle);

   if(!psvcd::CloseDevice(ftHandle))
      return -1;

   return 0;
}

void print_date_time()
{
   time_t now = time(0);
   tm localtm;

   localtime_s(&localtm, &now);

   const int buffer_size = 256;
   char buffer[buffer_size]; 
   errno_t error_code;
   error_code = asctime_s(buffer,buffer_size, &localtm);

   std::cout << buffer << std::endl;
}

int enter_dumpable_mode()
{
   FT_HANDLE ftHandle;

   if(!OpenDevice(ftHandle))
      return -1;

   if(!psvcd::EnterDumpableMode(ftHandle))
      return -1;

   if(!psvcd::CloseDevice(ftHandle))
      return -1;

   return 0;
}

int dump_mmc_card(std::string filePath,  uint32_t initialClusterArg)
{
   print_date_time();

   FT_HANDLE ftHandle;

   uint32_t initialCluster = initialClusterArg;
   uint32_t failedCluster = 0;

   bool lowFreqRequired = false;

   while(true)
   {
      bool openResult = false;

      //try to open device multiple times
      while(!openResult)
      {
         openResult = OpenDevice(ftHandle);

         if(!openResult)
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      }

      bool res = psvcd::RawBlockDumpMMCCard(ftHandle, initialCluster, failedCluster, filePath, lowFreqRequired);

      //if result is false that is an indication of serios transmission error that requires ft232h reset
      //reset can be done by Reset function from API but this does not help in most cases
      //so last solution would be CyclePort which emulates physical unplug/plug of the device
      if(!res)
      {
         openResult = false;

         initialCluster = failedCluster;
         failedCluster = 0;

         if(!psvcd::CyclePort(ftHandle))
         {
            std::cout << "Failed to cycle port" << std::endl;
            print_date_time();
            return -1;
         }

         if(!psvcd::CloseDevice(ftHandle))
         {
            std::cout << "Failed to close port" << std::endl;
            print_date_time();
            return -1;
         }

         ftHandle = 0;
      }
      else
      {
         break;
      }
   }

   if(!psvcd::CloseDevice(ftHandle))
      return -1;

   print_date_time();
   
	return 0;
}

bool parseBool(const char* val)
{
   std::stringstream ss;

   bool parsedValue = false;
   ss << val;
   ss >> std::boolalpha >> parsedValue;

   return parsedValue;
}

uint32_t parseUint32(const char* val)
{
   std::string initialValueStr(val);

   uint32_t parsedValue = 0;

   std::stringstream ss;
   ss << initialValueStr;
   ss >> parsedValue;

   return parsedValue;
}

uint32_t parseUint32Hex(const char* val)
{
   std::string initialValueStr(val);
   
   if(initialValueStr.find("0x") != std::string::npos)
      initialValueStr = initialValueStr.substr(2);

   uint32_t parsedValue = 0;

   std::stringstream ss;
   ss << initialValueStr;
   ss >> std::hex >> parsedValue;

   return parsedValue;
}

#define ENTER_DUMPABLE_MODE 0
#define DUMP_MMC_CARD 1
#define STANDALONE_INITIALIZE_MMC 2

int main(int argc, char* argv[])
{
   if(argc < 4)
   {
      std::cout << "Usage: mode dest_dump_file_path initial_sector" << std::endl;
      std::cout << "Modes:" << std::endl;
      std::cout << "0 - enter dumpable mode" << std::endl;
      std::cout << "1 - dump mmc card" << std::endl;
      std::cout << "2 - standalone initialization" << std::endl;
      return -1;
   }
   
   //TODO: this can be done much easier and informative with boost
   uint32_t mode = parseUint32(argv[1]);
   std::string filePath(argv[2]);
   uint32_t initialCluster = parseUint32Hex(argv[3]);
  
   switch(mode)
   {
   case ENTER_DUMPABLE_MODE:
      return enter_dumpable_mode();
   case DUMP_MMC_CARD:
      return dump_mmc_card(filePath, initialCluster);
   case STANDALONE_INITIALIZE_MMC:
      return standalone_initialize_mmc();
   default:
      {
         std::cout << "Invalid mode is specified" << std::endl;
         return -1;
      }
   }
}