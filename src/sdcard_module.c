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

/* Determine the highest ID in the existing file */
static int get_highest_id(const char *file_path)
{
	struct fs_file_t file;
	char buffer[128];
	int highest_id = 0;

	fs_file_t_init(&file);

	if (fs_open(&file, file_path, FS_O_READ) < 0) {
		LOG_ERR("Failed to open file %s to read IDs", file_path);
		return -1;
	}

	while (fs_read(&file, buffer, sizeof(buffer) - 1) > 0) {
		buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination

		/* Parse lines to extract IDs */
		char *line = strtok(buffer, "\n");
		while (line) {
			int id;
			if (sscanf(line, "%d,", &id) == 1 && id > highest_id) {
				highest_id = id;
			}
			line = strtok(NULL, "\n");
		}
	}

	fs_close(&file);
	return highest_id;
}

/* Create and/or append to a CSV file */
static bool create_and_append_csv(const char *base_path)
{
    char csv_path[128];
    struct fs_file_t file;
    int res;

    fs_file_t_init(&file);

    /* Construct the full file path */
    snprintf(csv_path, sizeof(csv_path), "%s/%s", base_path, CSV_FILE_NAME);

    /* Check if file exists */
    res = fs_open(&file, csv_path, FS_O_READ);
    if (res < 0) {
        /* File doesn't exist, create it and write headers */
        LOG_INF("File %s does not exist. Creating new file.", csv_path);

        res = fs_open(&file, csv_path, FS_O_WRITE | FS_O_CREATE);
        if (res < 0) {
            LOG_ERR("Failed to create file %s (err: %d)", csv_path, res);
            return false;
        }

        /* Write CSV headers */
        const char *headers = "ID,Timestamp,Value1,Value2\n";
        res = fs_write(&file, headers, strlen(headers));
        if (res < 0) {
            LOG_ERR("Failed to write headers to %s (err: %d)", csv_path, res);
            fs_close(&file);
            return false;
        }
        fs_close(&file);
    }

    /* Determine the highest ID in the file */
    int highest_id = get_highest_id(csv_path);
    if (highest_id < 0) {
        return false;
    }
    LOG_INF("Highest ID in the file: %d", highest_id);

    fs_file_t_init(&file);

    /* Open file for appending */
    res = fs_open(&file, csv_path, FS_O_WRITE);
    if (res < 0) {
        LOG_ERR("Failed to open file %s for appending (err: %d)", csv_path, res);
        return false;
    }

    /* Append a new row */
    char buffer[64];
    int value1 = 42;
    int value2 = 84;
    int written = snprintf(buffer, sizeof(buffer), "%d,%lu,%d,%d\n",
        highest_id + 1, k_uptime_get(), value1, value2);
    res = fs_write(&file, buffer, written);
    if (res < 0) {
        LOG_ERR("Failed to append data to %s (err: %d)", csv_path, res);
        fs_close(&file);
        return false;
    }

    LOG_INF("Data appended to CSV file: %s", buffer);

    fs_close(&file);
    return true;
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
	return 0;
}

int packet_entry(void) {
  	mp.mnt_point = disk_mount_pt;

  	int res = fs_mount(&mp);

  #if defined(CONFIG_FAT_FILESYSTEM_ELM)
  	if (res == FR_OK) {
  #else
  	if (res == 0) {
  #endif
  		LOG_INF("Disk mounted.\n");

  		/* Create and append to the CSV file */
  		if (create_and_append_csv(disk_mount_pt)) {
  			LOG_INF("CSV operations completed successfully.\n");
  		} else {
  			LOG_ERR("CSV operations failed.\n");
  		}
  	} else {
  		LOG_ERR("Error mounting disk.\n");
  	}

  	fs_unmount(&mp);

  return 0;
}
