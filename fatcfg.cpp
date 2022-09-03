/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/* This example is based on "arduino-esp32/libraries/WebServer/examples/FSBrowser"
 * to expose on-board external Flash as USB Mass Storage and webserver. Both interfaces
 * can make changes to file system
 *
 * How to run this example
 * 1. Create secrets.h and define your "SECRET_SSID" and "SECRET_PASSWORD"
 * 2. Compile and upload this sketch
 * 3. Your ESP will be expose as MassStorage device.
 * 4. If it is your first run (otherwise skip this step):
 *   - you may need to format the drive as FAT. Note: If your PC failed to format, you could format
 *     it using follow sketch "https://github.com/adafruit/Adafruit_SPIFlash/tree/master/examples/SdFat_format"
 *   - Copy all files in 'data/' folder of this example to the root directory of the MassStorage disk drive
 * 5. When prompted, open http://esp32fs.local/edit to access the file browser
 * 6. Modify/Update USB drive then refresh your browser to see if the change is updated
 * 7. Upload/Edit a file using web browser then see if the USB Drive is updated. Note: the usb drive could
 * briefly disappear and reappear to force PC to refresh its cache
 *
 * NOTE: Following library is required
 *   - Adafruit_SPIFlash https://github.com/adafruit/Adafruit_SPIFlash
 *   - SdFat https://github.com/adafruit/SdFat
 */

#include "SPI.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_TinyUSB.h"
#include <GxEPD2_BW.h>
#include "fonts/NotoSerif_Regular10pt7b.h"

// ESP32 use same flash device that store code.
// Therefore there is no need to specify the SPI and SS
Adafruit_FlashTransport_ESP32 flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatFileSystem fatfs;

// USB Mass Storage object
Adafruit_USBD_MSC usb_msc;

bool fs_formatted;  // Check if flash is formatted
bool fs_changed;    // Set to true when browser write to flash

FatFile root;
FatFile file;

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and 
// return number of copied bytes (must be multiple of block size) 
static int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.readBlocks(lba, (uint8_t*) buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
static int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize)
{
  // Note: SPIFLash Bock API: readBlocks/writeBlocks/syncBlocks
  // already include 4K sector caching internally. We don't need to cache it, yahhhh!!
  return flash.writeBlocks(lba, buffer, bufsize/512) ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
static void msc_flush_cb (void)
{
  // sync with flash
  flash.syncBlocks();
  // clear file system's cache to force refresh
  fatfs.cacheClear();
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
static bool msc_ready_callback(void)
{
  // if fs has changed, mark unit as not ready temporarily to force PC to flush cache
  bool ret = !fs_changed;
  fs_changed = false;
  return ret;
}

void fatcfg_init()
{
  flash.begin();
  // Init file system on the flash
  fs_formatted = fatfs.begin(&flash);
  if ( !fs_formatted ) {
    // TODO: Filesystem not formatted.
  }
}

void fatcfg_msc_init() {
  // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively
  usb_msc.setID("Adafruit", "External Flash", "1.0");
  // Set callback
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  // Set disk size, block size should be 512 regardless of spi flash page size
  usb_msc.setCapacity(flash.size()/512, 512);
  // MSC is ready for read/write
  fs_changed = false;
  usb_msc.setReadyCallback(0, msc_ready_callback);
  usb_msc.begin();
}

void fatcfg_get_string(char* buf, int maxchars, const char* fname, const char* fallback) {
  File f = fatfs.open(fname, FILE_READ);
  if (!f || !f.available()) {
    File f = fatfs.open(fname, FILE_WRITE);
    f.println(fallback);
    f.close();
    fatcfg_get_string(buf, maxchars, fname, fallback);
    return;
  }
  int i = 0;
  while(f.available() && i < maxchars-1) { // Read first line
    char c = f.read();
    if (c == '\r' || c == '\n' || c == '\0') { break; }
    buf[i++] = c;
  }
  buf[i] = '\0';
  f.close();
  return;
}

bool fatcfg_get_bool(const char* fname, const char* fallback) {
  char buf[8];
  File f = fatfs.open(fname, FILE_READ);
  if (!f || !f.available()) {
    File f = fatfs.open(fname, FILE_WRITE);
    f.println(fallback);
    f.close();
    return fatcfg_get_bool(fname, fallback);
  }
  char c;
  while (f.available()) { // Read first non-whitespace character
    c = f.read();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { continue; }
    else { break; }
  }
  f.close();
  if (c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y') {
    return true;
  }
  return false;
}

uint64_t fatcfg_get_num(const char* fname, const char* fallback) {
  char buf[24];
  File f = fatfs.open(fname, FILE_READ);
  if (!f || !f.available()) {
    File f = fatfs.open(fname, FILE_WRITE);
    f.println(fallback);
    f.close();
    return fatcfg_get_num(fname, fallback);
  }
  int i = 0;
  while(f.available() && i < 24-1) { // Read first line
    char c = f.read();
    if (c == '\r' || c == '\n' || c == '\0') { break; }
    buf[i++] = c;
  }
  buf[i] = '\0';
  return strtoul(buf, NULL, 0);
}


