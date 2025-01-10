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

static int file_index = -1;

int create_csv(void)
{
    char csv_folder_path[150];
    char csv_file_path[150];
    struct fs_file_t file;
    struct fs_dir_t dir;
    struct fs_dirent entry;
    int res;

    fs_file_t_init(&file);
    fs_dir_t_init(&dir);

    /* Construct the folder path */
    snprintf(csv_folder_path, sizeof(csv_folder_path), "%s/%s", disk_mount_pt, CSV_FILE_NAME);

    /* Check if the folder exists */
    res = fs_opendir(&dir, csv_folder_path);
    if (res < 0) {
        /* Folder doesn't exist, create it */
        LOG_INF("Folder %s does not exist. Creating new folder.", csv_folder_path);

        res = fs_mkdir(csv_folder_path);
        if (res < 0) {
            LOG_ERR("Failed to create folder %s (err: %d)", csv_folder_path, res);
            return -1;
        }
    } else {
        /* Scan folder to find the highest file index */
        while (fs_readdir(&dir, &entry) == 0 && entry.name[0] != '\0') {
            int current_index;
            if (sscanf(entry.name, "%d.csv", &current_index) == 1) {
                if (current_index > file_index) {
                    file_index = current_index;
                }
            }
        }
        fs_closedir(&dir);
    }

    /* Increment file_index for the new file */
    file_index++;

    /* Construct the new file path */
    snprintf(csv_file_path, sizeof(csv_file_path), "%s/%d.csv", csv_folder_path, file_index);

    /* Create the new file */
    res = fs_open(&file, csv_file_path, FS_O_WRITE | FS_O_CREATE);
    if (res < 0) {
        LOG_ERR("Failed to create file %s (err: %d)", csv_file_path, res);
        return -1;
    }

    LOG_INF("Created file: %s", csv_file_path);

    /* Close the file */
    fs_close(&file);
    return 0;
}



int append_csv(uint16_t number_press, uint16_t tx_delay, uint32_t latitude, uint32_t longitude, 
              uint8_t tx_hour, uint8_t tx_minute, uint8_t tx_second, uint16_t tx_ms, int8_t rssi,
              uint8_t rx_hour, uint8_t rx_minute, uint8_t rx_second, uint16_t rx_ms) {
    char csv_folder_path[128];
    char csv_file_path[128];
    struct fs_file_t file;
    int res;

    fs_file_t_init(&file);

    if (file_index < 0) {
        LOG_ERR("No file has been created yet. Cannot append data.");
        return -1;
    }

    /* Construct the folder and file path */
    snprintf(csv_folder_path, sizeof(csv_folder_path), "%s/%s", disk_mount_pt, CSV_FILE_NAME);
    snprintf(csv_file_path, sizeof(csv_file_path), "%s/%d.csv", csv_folder_path, file_index);

    /* Open the file for appending */
    res = fs_open(&file, csv_file_path, FS_O_WRITE | FS_O_APPEND);
    if (res < 0) {
        LOG_ERR("Failed to open file %s for appending (err: %d)", csv_file_path, res);
        return -1;
    }

    /* Append a new row */
    char buffer[124];
    int written = snprintf(buffer, sizeof(buffer),
                           "%02u%02u%02u%03u,%02u:%02u:%02u.%03u,%u,%02u:%02u:%02u.%03u,%u,%u,%u,%d\n",
                           tx_hour, tx_minute, tx_second, tx_ms,
                           tx_hour, tx_minute, tx_second, tx_ms,
                           tx_delay, rx_hour, rx_minute, rx_second, rx_ms,
                           number_press, latitude, longitude, rssi);

    res = fs_write(&file, buffer, written);
    if (res < 0) {
        LOG_ERR("Failed to append data to %s (err: %d)", csv_file_path, res);
        fs_close(&file);
        return -1;
    }

    fs_close(&file);
    // LOG_INF("Data appended to file: %s", csv_file_path);
    return 0;
}


int sdcard_init(void)
{
	/* Initialize storage and mount the disk */
	// do {
	// 	static const char *disk_pdrv = DISK_DRIVE_NAME;
	// 	uint64_t memory_size_mb;
	// 	uint32_t block_count;
	// 	uint32_t block_size;

	// 	if (disk_access_init(disk_pdrv) != 0) {
	// 		LOG_ERR("Storage init ERROR!");
	// 		break;
	// 	}

	// 	if (disk_access_ioctl(disk_pdrv,
	// 			DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
	// 		LOG_ERR("Unable to get sector count");
	// 		break;
	// 	}
	// 	// LOG_INF("Block count %u", block_count);

	// 	if (disk_access_ioctl(disk_pdrv,
	// 			DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
	// 		LOG_ERR("Unable to get sector size");
	// 		break;
	// 	}
	// 	// LOG_INF("Sector size %u\n", block_size);

	// 	memory_size_mb = (uint64_t)block_count * block_size;
	// 	LOG_INF("Memory Size(MB) %u", (uint32_t)(memory_size_mb >> 20));

  //   LOG_INF("SD card ready");
	// } while (0);

  mp.mnt_point = disk_mount_pt;

    while (1) {  // Repeat indefinitely until successful
        int res = fs_mount(&mp);

#if defined(CONFIG_FAT_FILESYSTEM_ELM)
        if (res == FR_OK) {
#else
        if (res == 0) {
#endif
            LOG_INF("Disk mounted successfully.");
            return 0;  // Exit the function when successful
        } else {
            LOG_ERR("Error mounting disk. Error code: %d", res);
            LOG_INF("Retrying to mount the disk...");
        }

        k_sleep(K_SECONDS(1));  // Delay before retrying
    }
}

int disk_unmount(void){
  fs_unmount(&mp);
  return 0;
}
