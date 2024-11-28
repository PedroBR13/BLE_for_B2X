/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include "sdcard_module.h"

#if defined(CONFIG_FAT_FILESYSTEM_ELM)

#include <ff.h>

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/"DISK_DRIVE_NAME":"

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

#elif defined(CONFIG_FILE_SYSTEM_EXT2)

#include <zephyr/fs/ext2.h>

#define DISK_DRIVE_NAME "SDMMC"
#define DISK_MOUNT_PT "/ext"

static struct fs_mount_t mp = {
	.type = FS_EXT2,
	.flags = FS_MOUNT_FLAG_NO_FORMAT,
	.storage_dev = (void *)DISK_DRIVE_NAME,
	.mnt_point = "/ext",
};

#endif

LOG_MODULE_REGISTER(sdcard_module);

static const char *disk_mount_pt = DISK_MOUNT_PT;

/* Create a CSV file */
int create_csv(void)
{
    char csv_path[128];
    struct fs_file_t file;
    int res;

    fs_file_t_init(&file);

    /* Construct the full file path */
    snprintf(csv_path, sizeof(csv_path), "%s/%s", disk_mount_pt, CSV_FILE_NAME);

    /* Check if file exists */
    res = fs_open(&file, csv_path, FS_O_READ);
    if (res < 0) {
        /* File doesn't exist, create it and write headers */
        LOG_INF("File %s does not exist. Creating new file.", csv_path);

        /* Open file in write-create mode */
        res = fs_open(&file, csv_path, FS_O_WRITE | FS_O_CREATE);
        if (res < 0) {
            LOG_ERR("Failed to create file %s (err: %d)", csv_path, res);
            return -1;
        }

    }
    /* Close the file */
    fs_close(&file);
    return 0;
}

/* Append to a CSV file */
int append_csv(uint16_t number_press, uint16_t tx_delay, uint32_t latitude, uint32_t longitude, 
              uint8_t tx_hour, uint8_t tx_minute, uint8_t tx_second, uint16_t tx_ms,int8_t rssi,
              uint8_t rx_hour, uint8_t rx_minute, uint8_t rx_second, uint16_t rx_ms) {
  char csv_path[128];
  struct fs_file_t file;
  int res;
  mp.mnt_point = disk_mount_pt;

  fs_file_t_init(&file);

  /* Construct the full file path */
  snprintf(csv_path, sizeof(csv_path), "%s/%s", mp.mnt_point, CSV_FILE_NAME);

  /* Open file for appending */
  res = fs_open(&file, csv_path,  FS_O_WRITE | FS_O_APPEND );
  if (res < 0) {
      LOG_ERR("Failed to open file %s for appending (err: %d)", csv_path, res);
      return false;
  }

  /* Append a new row */
  char buffer[64];

  // timestamp_id, timestamp_tx, tx_delay,timestamp_rx, number_press, latitude, longitude, rssi
  int written = snprintf(buffer, sizeof(buffer), "%02u%02u%02u%03u,%02u:%02u:%02u.%03u,%u,%02u:%02u:%02u.%03u,%u,%u,%u,%d\n",
                        tx_hour, tx_minute, tx_second, tx_ms, tx_hour, tx_minute, tx_second, tx_ms,tx_delay,
                        rx_hour, rx_minute, rx_second, rx_ms,number_press,latitude,longitude, rssi);

  res = fs_write(&file, buffer, written);
  if (res < 0) {
      LOG_ERR("Failed to append data to %s (err: %d)", csv_path, res);
      fs_close(&file);
      return false;
  } else {
      LOG_INF("Data appended to CSV file: %s", buffer);
  }

 

  fs_close(&file);
  return 0;
}

int sdcard_init(void)
{
	/* Initialize storage and mount the disk */
	do {
		static const char *disk_pdrv = DISK_DRIVE_NAME;
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		// LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		// LOG_INF("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		LOG_INF("Memory Size(MB) %u", (uint32_t)(memory_size_mb >> 20));

    LOG_INF("SD card ready");
	} while (0);

  mp.mnt_point = disk_mount_pt;

  int res = fs_mount(&mp);

  #if defined(CONFIG_FAT_FILESYSTEM_ELM)
  	if (res == FR_OK) {
  #else
  	if (res == 0) {
  #endif
  		LOG_INF("Disk mounted.\n");
    } else {
  		LOG_ERR("Error mounting disk.\n");
  	}

	return 0;
}

int disk_unmount(void){
  fs_unmount(&mp);
  return 0;
}
