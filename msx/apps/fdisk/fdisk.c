#define __Z88DK_R2L_CALLING_CONVENTION
#include "fdisk.h"
#include "fdisk2.h"
#include "msxdos.h"
#include "partition.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system_vars.h>

extern uint8_t workingMsxDosBuff[];
uint8_t *      pWorkingBuffer;

// CAPUTED ENUMERATED DRIVER/DEVICE/LUN INFO
static msxdosDriverInfo drivers[MAX_INSTALLED_DRIVERS];
static deviceInfo       devices[MAX_DEVICES_PER_DRIVER];
static msxdosLunInfo    luns[MAX_LUNS_PER_DEVICE];
static partitionInfo    partitions[MAX_PARTITIONS_TO_HANDLE];

static screenConfiguration currentScreenConfig;
static screenConfiguration originalScreenConfig;
static uint8_t             screenLinesCount;
static uint8_t             installedDriversCount;
static msxdosDriverInfo *  selectedDriver;
static char                selectedDriverName[50];
static bool                availableDevicesCount;
static deviceInfo *        currentDevice;
static uint8_t             selectedDeviceIndex;
static uint8_t             availableLunsCount;
static uint8_t             selectedLunIndex;
static msxdosLunInfo *     selectedLun;
static uint8_t             partitionsCount;
static bool                partitionsExistInDisk;
static bool                canCreatePartitions;
static bool                canDoDirectFormat;
static uint32_t            unpartitionnedSpaceInSectors;
static uint32_t            autoPartitionSizeInK;
static char                buffer[1024];

void terminateRightPaddedStringWithZero(char *string, uint8_t length) {
  char *pointer = string + length - 1;
  while (*pointer == ' ' && length > 0) {
    pointer--;
    length--;
  }
  pointer[1] = '\0';
}

void composeSlotString(uint8_t slot, char *destination) {
  if ((slot & 0x80) == 0) {
    destination[0] = slot + '0';
    destination[1] = '\0';
  } else {
    destination[0] = (slot & 3) + '0';
    destination[1] = '-';
    destination[2] = ((slot >> 2) & 3) + '0';
    destination[3] = '\0';
  }
}

void printSize(uint32_t sizeInK) {
  char     buf[3];
  uint32_t dividedSize;

  if (sizeInK < (uint32_t)(10 * 1024)) {
    printf("%dK", sizeInK);
    return;
  }

  dividedSize = sizeInK >> 10;
  if (dividedSize < (uint32_t)(10 * 1024)) {
    printf("%d", dividedSize + getRemainingBy1024String(sizeInK, buf));
    printf("%sM", buf);
  } else {
    sizeInK >>= 10;
    dividedSize = sizeInK >> 10;
    printf("%d", dividedSize + getRemainingBy1024String(sizeInK, buf));
    printf("%sG", buf);
  }
}

uint8_t getRemainingBy1024String(uint32_t value, char *destination) {
  uint8_t remaining2;
  char    remainingDigit;

  int remaining = value & 0x3FF;
  if (remaining >= 950) {
    *destination = '\0';
    return 1;
  }
  remaining2 = remaining % 100;
  remainingDigit = (remaining / 100) + '0';
  if (remaining2 >= 50) {
    remainingDigit++;
  }

  if (remainingDigit == '0') {
    *destination = '\0';
  } else {
    destination[0] = '.';
    destination[1] = remainingDigit;
    destination[2] = '\0';
  }

  return 0;
}

void saveOriginalScreenConfiguration() {
  originalScreenConfig.screenMode = *(uint8_t *)SCRMOD;
  originalScreenConfig.screenWidth = LINLEN;
  originalScreenConfig.functionKeysVisible = (*(uint8_t *)CNSDFG != 0);
}

void composeWorkScreenConfiguration() {
  currentScreenConfig.screenMode = 0;
  currentScreenConfig.screenWidth = 80;
  currentScreenConfig.functionKeysVisible = false;
  screenLinesCount = *(uint8_t *)CRTCNT;
}

void setScreenConfiguration() {
  LINL40 = 80;
  msxbiosInitxt();
}

#define clearScreen()                    printf("\x0C")
#define homeCursor()                     printf("\x0D\x1BK")
#define cursorDown()                     printf("\x1F")
#define deleteToEndOfLine()              printf("\x1BK")
#define deleteToEndOfLineAndCursorDown() printf("\x1BK\x1F");
#define newLine()                        printf("\x0A\x0D");
#define hideCursor()                     printf("\x1Bx5")
#define displayCursor()                  printf("\x1By5")

void locateX(uint8_t x) { msxbiosPosit(x + 1, CSRY); }

void locate(uint8_t x, uint8_t y) { msxbiosPosit(x + 1, y + 1); }

void printCentered(char *string) {
  uint8_t pos = (currentScreenConfig.screenWidth - strlen(string)) / 2;
  homeCursor();
  locateX(pos);
  printf(string);
}

void printRuler() {
  uint8_t i;

  homeCursor();
  for (i = 0; i < currentScreenConfig.screenWidth; i++)
    printf("-");
}

void initializeWorkingScreen(char *header) {
  clearScreen();
  printCentered(header);
  cursorDown();
  printRuler();
  locate(0, screenLinesCount - 2);
  printRuler();
}

char getKey() { return msxdosDirio(0xFF); }

uint8_t waitKey() {
  uint8_t key;

  while ((key = getKey()) == 0)
    ;
  return key;
}

void clearInformationArea() {
  uint8_t i;

  locate(0, 2);
  for (i = 0; i < screenLinesCount - 4; i++) {
    deleteToEndOfLineAndCursorDown();
  }
}

void getDriversInformation() {
  uint8_t           error = 0;
  uint8_t           driverIndex = 1;
  msxdosDriverInfo *currentDriver = &drivers[0];

  installedDriversCount = 0;

  while (error == 0 && driverIndex <= MAX_INSTALLED_DRIVERS) {
    error = msxdosGdrvr(driverIndex, currentDriver);

    if (error == 0 && (currentDriver->flags & (DRIVER_IS_DOS250 | DRIVER_IS_DEVICE_BASED) == (DRIVER_IS_DOS250 | DRIVER_IS_DEVICE_BASED))) {
      installedDriversCount++;
      terminateRightPaddedStringWithZero(currentDriver->driverName, DRIVER_NAME_LENGTH);
      currentDriver++;
    }
    driverIndex++;
  }
}

void printStateMessage(char *string) {
  locate(0, screenLinesCount - 1);
  deleteToEndOfLine();
  printf(string);
}

void showDriverSelectionScreen() {
  uint8_t           i;
  char              slot[4];
  char              rev[3];
  msxdosDriverInfo *currentDriver;
  uint8_t           revByte;
  char *            driverName;

  clearInformationArea();

  if (installedDriversCount == 0) {
    getDriversInformation();
  }

  if (installedDriversCount == 0) {
    locate(0, 7);
    printCentered("There are no device-based drivers");
    cursorDown();
    printCentered("available in the system");
    printStateMessage("Press any key to exit...");
    waitKey();
    return;
  }

  currentDriver = &drivers[0];
  locate(0, 3);
  for (i = 0; i < installedDriversCount; i++) {
    composeSlotString(currentDriver->slot, slot);

    revByte = currentDriver->versionRev;
    if (revByte == 0) {
      rev[0] = '\0';
    } else {
      rev[0] = '.';
      rev[1] = revByte + '0';
      rev[2] = '\0';
    }

    driverName = currentDriver->driverName;

    printf("\x1BK%d. %s%sv%d.%d%s on slot %s", i + 1, driverName, " ", currentDriver->versionMain, currentDriver->versionSec, rev, slot);

    newLine();
    newLine();

    currentDriver++;
  }

  newLine();
  printf("ESC. Exit");

  printStateMessage("Select the device driver");
}

void getDevicesInformation() {
  uint16_t    error = 0;
  uint8_t     deviceIndex = 1;
  deviceInfo *currentDevice = &devices[0];
  char *      currentDeviceName;

  availableDevicesCount = 0;

  while (deviceIndex <= MAX_DEVICES_PER_DRIVER) {

    currentDeviceName = currentDevice->deviceName;
    error = msxdosDrvDevLogicalUnitCount(selectedDriver->slot, deviceIndex, (msxdosDeviceBasicInfo *)currentDevice);
    if (error == 0) {
      availableDevicesCount++;
      error = msxdosDrvDevGetName(selectedDriver->slot, deviceIndex, currentDeviceName);

      if (error == 0)
        terminateRightPaddedStringWithZero(currentDeviceName, MAX_INFO_LENGTH);
      else
        sprintf(currentDeviceName, "(Unnamed device, ID=%d)", deviceIndex);
    } else
      currentDevice->lunCount = 0;

    deviceIndex++;
    currentDevice++;
  }
}

void showDeviceSelectionScreen() {
  deviceInfo *currentDevice;
  uint8_t     i;

  clearInformationArea();
  locate(0, 3);
  printf(selectedDriverName);
  cursorDown();
  cursorDown();

  if (availableDevicesCount == 0) {
    getDevicesInformation();
  }

  if (availableDevicesCount == 0) {
    locate(0, 9);
    printCentered("There are no suitable devices");
    cursorDown();
    printCentered("attached to the driver");
    printStateMessage("Press any key to go back...");
    waitKey();
    return;
  }

  currentDevice = &devices[0];
  for (i = 0; i < MAX_DEVICES_PER_DRIVER; i++) {
    if (currentDevice->lunCount > 0) {
      printf("\x1BK%d. %s\r\n\r\n", i + 1, currentDevice->deviceName);
    }

    currentDevice++;
  }

  if (availableDevicesCount < 7) {
    newLine();
  }
  printf("ESC. Go back to driver selection screen");

  printStateMessage("Select the device");
}

void getLunsInformation() {
  uint16_t       error = 0;
  uint8_t        lunIndex = 1;
  msxdosLunInfo *currentLun = &luns[0];

  while (lunIndex <= MAX_LUNS_PER_DEVICE) {
    error = msxdosDrvLunInfo(selectedDriver->slot, selectedDeviceIndex, lunIndex, currentLun);

    currentLun->suitableForPartitioning =
        (error == 0) && (currentLun->mediumType == BLOCK_DEVICE) && (currentLun->sectorSize == 512) && (currentLun->sectorCount >= MIN_DEVICE_SIZE_IN_K * 2) && ((currentLun->flags & (READ_ONLY_LUN | FLOPPY_DISK_LUN)) == 0);

    if (currentLun->suitableForPartitioning) {
      availableLunsCount++;
    }

    if (currentLun->sectorsPerTrack == 0 || currentLun->sectorsPerTrack > EXTRA_PARTITION_SECTORS) {
      currentLun->sectorsPerTrack = EXTRA_PARTITION_SECTORS;
    }

    lunIndex++;
    currentLun++;
  }
}

void printDeviceInfoWithIndex() { printf(" (Id = %d)", selectedDeviceIndex); }

void showLunSelectionScreen() {
  uint8_t        i;
  msxdosLunInfo *currentLun;

  clearInformationArea();
  locate(0, 3);
  printf(selectedDriverName);
  printf(currentDevice->deviceName);
  printDeviceInfoWithIndex();
  newLine();
  newLine();
  newLine();

  if (availableLunsCount == 0) {
    getLunsInformation();
  }

  if (availableLunsCount == 0) {
    locate(0, 9);
    printCentered("There are no suitable logical units");
    cursorDown();
    printCentered("available in the device");
    printStateMessage("Press any key to go back...");
    waitKey();
    return;
  }

  currentLun = &luns[0];
  for (i = 0; i < MAX_LUNS_PER_DEVICE; i++) {
    if (currentLun->suitableForPartitioning) {
      printf("\x1BK%d. Size: ", i + 1);
      printSize(currentLun->sectorCount / 2);
      newLine();
    }

    i++;
    currentLun++;
  }

  newLine();
  newLine();
  printf("ESC. Go back to device selection screen");

  printStateMessage("Select the logical unit");
}

void recalculateAutoPartitionSize(bool setToAllSpaceAvailable) {
  uint32_t maxAbsolutePartitionSizeInK = (unpartitionnedSpaceInSectors - EXTRA_PARTITION_SECTORS) / 2;

  if (setToAllSpaceAvailable) {
    autoPartitionSizeInK = maxAbsolutePartitionSizeInK;
  }

  if (autoPartitionSizeInK > MAX_FAT16_PARTITION_SIZE_IN_K) {
    autoPartitionSizeInK = MAX_FAT16_PARTITION_SIZE_IN_K;
  } else if (!setToAllSpaceAvailable && autoPartitionSizeInK > maxAbsolutePartitionSizeInK) {
    autoPartitionSizeInK = maxAbsolutePartitionSizeInK;
  }

  if (autoPartitionSizeInK < MIN_PARTITION_SIZE_IN_K) {
    autoPartitionSizeInK = MIN_PARTITION_SIZE_IN_K;
  } else if (autoPartitionSizeInK > maxAbsolutePartitionSizeInK) {
    autoPartitionSizeInK = maxAbsolutePartitionSizeInK;
  }
}

void initializePartitioningVariables(uint8_t lunIndex) {
  selectedLunIndex = lunIndex - 1;
  selectedLun = &luns[selectedLunIndex];
  partitionsCount = 0;
  partitionsExistInDisk = true;
  canCreatePartitions = (selectedLun->sectorCount >= (MIN_DEVICE_SIZE_FOR_PARTITIONS_IN_K * 2));
  canDoDirectFormat = (selectedLun->sectorCount <= MAX_DEVICE_SIZE_FOR_DIRECT_FORMAT_IN_K * 2);
  unpartitionnedSpaceInSectors = selectedLun->sectorCount;
  recalculateAutoPartitionSize(true);
}

void printTargetInfo() {
  locate(0, 3);
  printf(selectedDriverName);
  printf(currentDevice->deviceName);
  printDeviceInfoWithIndex();
  newLine();
  printf("Logical unit %d, size: ", selectedLunIndex + 1);
  printSize(selectedLun->sectorCount / 2);
  newLine();
}

uint8_t getDiskPartitionsInfo() {
  uint8_t        primaryIndex = 1;
  uint8_t        extendedIndex = 0;
  uint8_t        error;
  partitionInfo *currentPartition = &partitions[0];
  GPartInfo      result;

  partitionsCount = 0;

  do {
    result.sectorCount = 0;
    error = msxdosGpart(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, primaryIndex, extendedIndex, false, &result);

    if (error == 0) {
      if (result.typeCode == PARTYPE_EXTENDED)
        extendedIndex = 1;
      else {
        currentPartition->primaryIndex = primaryIndex;
        currentPartition->extendedIndex = extendedIndex;
        currentPartition->partitionType = result.typeCode;
        currentPartition->status = result.status;
        currentPartition->sizeInK = result.sectorCount / 2;
        partitionsCount++;
        currentPartition++;
        extendedIndex++;
      }
    } else if (error == _IPART) {
      primaryIndex++;
      extendedIndex = 0;
    } else
      return error;

  } while (primaryIndex <= 4 && partitionsCount < MAX_PARTITIONS_TO_HANDLE);

  return 0;
}

bool getYesOrNo() {
  char key;

  displayCursor();
  key = waitKey() | 32;
  hideCursor();
  return key == 'y';
}

void printDosErrorMessage(uint8_t code, char *header) {
  locate(0, MESSAGE_ROW);
  printCentered(header);
  newLine();

  msxdosExplain(code, buffer);
  if (strlen(buffer) > currentScreenConfig.screenWidth) {
    printf(buffer);
  } else {
    printCentered(buffer);
  }

  printStateMessage("Press any key to return...");
}

void printOnePartitionInfo(partitionInfo *info) {
  printf("%c %d: ", info->status & 0x80 ? '*' : ' ', info->extendedIndex == 0 ? info->primaryIndex : info->extendedIndex + 1);

  if (info->partitionType == PARTYPE_FAT12) {
    printf("FAT12");
  } else if (info->partitionType == PARTYPE_FAT16 || info->partitionType == PARTYPE_FAT16_SMALL || info->partitionType == PARTYPE_FAT16_LBA) {
    printf("FAT16");
  } else if (info->partitionType == 0xB || info->partitionType == 0xC) {
    printf("FAT32");
  } else if (info->partitionType == 7) {
    printf("NTFS");
  } else {
    printf("Type #%x", info->partitionType);
  }
  printf(", ");
  printSize(info->sizeInK);
  newLine();
}

void addAutoPartition() {
  partitionInfo *partition = &partitions[partitionsCount];

  partition->status = partitionsCount == 0 ? 0x80 : 0;
  partition->sizeInK = autoPartitionSizeInK;
  partition->partitionType = partition->sizeInK > MAX_FAT12_PARTITION_SIZE_IN_K ? PARTYPE_FAT16_LBA : PARTYPE_FAT12;
  if (partitionsCount == 0) {
    partition->primaryIndex = 1;
    partition->extendedIndex = 0;
  } else {
    partition->primaryIndex = 2;
    partition->extendedIndex = partitionsCount;
  }

  unpartitionnedSpaceInSectors -= (autoPartitionSizeInK * 2);
  unpartitionnedSpaceInSectors -= EXTRA_PARTITION_SECTORS;
  partitionsCount++;
  recalculateAutoPartitionSize(false);
}

void togglePartitionActive(uint8_t partitionIndex) {
  uint8_t        status, primaryIndex, extendedIndex;
  partitionInfo *partition;
  uint32_t       partitionTableEntrySector;
  uint8_t        error;
  GPartInfo      result;

  partition = &partitions[partitionIndex];

  if (!partitionsExistInDisk) {
    partition->status ^= 0x80;
    return;
  }

  status = partition->status;
  primaryIndex = partition->primaryIndex;
  extendedIndex = partition->extendedIndex;

  sprintf(buffer, "%set active bit of partition %d? (y/n) ", status & 0x80 ? "Res" : "S", partitionIndex + 1);
  printStateMessage(buffer);
  if (!getYesOrNo()) {
    return;
  }

  error = msxdosGpart(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, partition->primaryIndex, partition->extendedIndex, true, &result);
  if (error != 0)
    return;

  partitionTableEntrySector = result.partitionSector;

  preparePartitioningProcess(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, partitionsCount, partitions, luns[selectedLunIndex].sectorsPerTrack);

  error = toggleStatusBit(extendedIndex == 0 ? primaryIndex - 1 : 0, partitionTableEntrySector);
  if (error == 0) {
    partition->status ^= 0x80;
  } else {
    sprintf(buffer, "Error when accessing device: %d", error);
    clearInformationArea();
    locate(0, 7);
    printCentered(buffer);
    printStateMessage("Press any key...");
    waitKey();
  }
}

void showPartitions() {
  int            i;
  int            firstShownPartitionIndex = 1;
  int            lastPartitionIndexToShow;
  bool           isLastPage;
  bool           isFirstPage;
  bool           allPartitionsArePrimary;
  uint8_t        key;
  partitionInfo *currentPartition;

  if (partitionsExistInDisk) {
    allPartitionsArePrimary = true;
    for (i = 0; i < partitionsCount; i++) {
      currentPartition = &partitions[i];
      if (currentPartition->extendedIndex != 0) {
        allPartitionsArePrimary = false;
        break;
      }
    }
  } else {
    allPartitionsArePrimary = false;
  }

  while (true) {
    isFirstPage = (firstShownPartitionIndex == 1);
    isLastPage = (firstShownPartitionIndex + PARTITIONS_PER_PAGE) > partitionsCount;
    lastPartitionIndexToShow = isLastPage ? partitionsCount : firstShownPartitionIndex + PARTITIONS_PER_PAGE - 1;

    locate(0, screenLinesCount - 1);
    deleteToEndOfLine();
    if (isFirstPage) {
      sprintf(buffer, partitionsCount == 1 ? "1" : partitionsCount > 9 ? "1-9" : "1-%d", partitionsCount);
      if (isLastPage) {
        sprintf(buffer + 4, "ESC = return, %s = toggle active (*)", buffer);
      } else {
        sprintf(buffer + 4, "ESC=back, %s=toggle active (*)", buffer);
      }
      printCentered(buffer + 4);
    } else {
      printCentered("Press ESC to return");
    }

    if (!(isFirstPage && isLastPage)) {
      locate(0, screenLinesCount - 1);
      printf(isFirstPage ? "   " : "<--");

      locate(currentScreenConfig.screenWidth - 4, screenLinesCount - 1);
      printf(isLastPage ? "   " : "-->");
    }

    clearInformationArea();
    locate(0, 3);
    if (partitionsCount == 1) {
      printCentered(partitionsExistInDisk ? "One partition found on device" : "One new partition defined");
    } else {
      if (allPartitionsArePrimary) {
        sprintf(buffer, partitionsExistInDisk ? "%d primary partitions found on device" : "%d new primary partitions defined", partitionsCount);
      } else {
        sprintf(buffer, partitionsExistInDisk ? "%d partitions found on device" : "%d new partitions defined", partitionsCount);
      }
      printCentered(buffer);
    }
    newLine();
    if (partitionsCount > PARTITIONS_PER_PAGE) {
      sprintf(buffer, "Displaying partitions %d - %d", firstShownPartitionIndex, lastPartitionIndexToShow);
      printCentered(buffer);
      newLine();
    }
    newLine();

    currentPartition = &partitions[firstShownPartitionIndex - 1];

    for (i = firstShownPartitionIndex; i <= lastPartitionIndexToShow; i++) {
      printOnePartitionInfo(currentPartition);
      currentPartition++;
    }

    while (true) {
      key = waitKey();
      if (key == ESC) {
        return;
      } else if (key == CURSOR_LEFT && !isFirstPage) {
        firstShownPartitionIndex -= PARTITIONS_PER_PAGE;
        break;
      } else if (key == CURSOR_RIGHT && !isLastPage) {
        firstShownPartitionIndex += PARTITIONS_PER_PAGE;
        break;
      } else if (isFirstPage && key >= KEY_1 && key < KEY_1 + partitionsCount && key < KEY_1 + 9) {
        togglePartitionActive(key - KEY_1);
        break;
      }
    }
  }
}

void initializeScreenForTestDeviceAccess(const char *message) {
  clearInformationArea();
  printTargetInfo();
  locate(0, MESSAGE_ROW);
  printf(message);
  printStateMessage("Press any key to stop...");
}

void testDeviceAccess() {
  uint32_t    sectorNumber = 0;
  const char *message = "Now reading device sector ";
  uint8_t     messageLen = strlen(message);
  uint16_t    error;
  const char *errorMessageHeader = "Error when reading sector ";

  initializeScreenForTestDeviceAccess(message);

  while (getKey() == 0) {
    sprintf(buffer, "%u", sectorNumber);
    locate(messageLen, MESSAGE_ROW);
    printf(buffer);
    printf(" ...\x1BK");

    error = msxdosDevRead(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, sectorNumber, 1, buffer);

    if (error != 0) {
      strcpy(buffer, errorMessageHeader);
      sprintf(buffer + strlen(errorMessageHeader), "%u", sectorNumber);
      strcpy(buffer + strlen(buffer), ":");
      printDosErrorMessage(error, buffer);
      printStateMessage("Continue reading sectors? (y/n) ");
      if (!getYesOrNo()) {
        return;
      }
      initializeScreenForTestDeviceAccess(message);
    }

    sectorNumber++;
    if (sectorNumber >= selectedLun->sectorCount) {
      sectorNumber = 0;
    }
  }
}

void initializeScreenForTestDeviceWriteAccess(const char *message) {
  clearInformationArea();
  printTargetInfo();
  locate(0, MESSAGE_ROW);
  printf(message);
  printStateMessage("Press any key to return...");
}

bool readSector(uint32_t targetSector) {
  uint16_t error = msxdosDevRead(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, targetSector, 1, buffer);
  if (error != 0) {
    printDosErrorMessage(error, "Driver Error:");
    return FALSE;
  }

  return TRUE;
}

bool writeSector(uint32_t targetSector) {
  uint16_t error = msxdosDevWrite(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, targetSector, 1, buffer);
  if (error != 0) {
    printDosErrorMessage(error, "Driver Error:");
    return FALSE;
  }

  return TRUE;
}

void testDeviceWriteAccess() {

#define FOR_BUFFER(f)       \
  for (i = 0; i < 512; i++) \
  f
#define CHECK_BUFFER(c, f) \
  FOR_BUFFER(if (c) {      \
    f;                     \
    break;                 \
  })

  uint16_t error;
  uint16_t i;

  initializeScreenForTestDeviceWriteAccess("Checking write access on last sector\r\n");
  locate(0, MESSAGE_ROW + 1);
  printf("WARNING! Potential data corruption.\r\n");
  printStateMessage("Proceed? (y/n) ");
  if (!getYesOrNo())
    return;

  locate(0, MESSAGE_ROW + 2);

  const uint32_t targetSector = selectedLun->sectorCount - 1;

  printf("Target sector %ld\r\n", targetSector);

  printf("Reading\r\n");
  if (!readSector(targetSector))
    goto abortTest;

  FOR_BUFFER(buffer[i] = 0);

  printf("Writing zeros\r\n");
  if (!writeSector(targetSector))
    goto abortTest;

  printf("Verifiying\r\n");
  if (!readSector(targetSector))
    goto abortTest;

  error = FALSE;
  CHECK_BUFFER(buffer[i] != 0, error = TRUE);

  if (error) {
    printf("Comparision failure at byte %d\r\n", i);
    goto abortTest;
  }

  FOR_BUFFER(buffer[i] = i);

  printf("Writing sequence\r\n");
  if (!writeSector(targetSector))
    goto abortTest;

  printf("Verifiying\r\n");
  if (!readSector(targetSector))
    goto abortTest;

  error = FALSE;
  CHECK_BUFFER(buffer[i] != (uint8_t)i, error = TRUE);

  if (error) {
    printf("Comparision failure at byte %d\r\n", i);
    goto abortTest;
  }

  printStateMessage("Success.  Press any key to exit");

abortTest:
  waitKey();
}

bool confirmDataDestroy(char *action) {
  printStateMessage("");
  clearInformationArea();
  printTargetInfo();
  locate(0, MESSAGE_ROW);

  printf("%s\r\n"
         "\r\n"
         "THIS WILL DESTROY ALL DATA ON THE DEVICE!!\r\n"
         "This action can't be cancelled and can't be undone\r\n"
         "\r\n"
         "Are you sure? (y/n) ",
         action);

  return getYesOrNo();
}

void printDone() {
  printCentered("Done!");
  printf("\x0A\x0D\x0A\x0A\x0A");
  printCentered("If this device had drives mapped,");
  newLine();
  printCentered("please reset the computer.");
}

bool writePartitionTable() {
  uint8_t i;
  uint8_t error = 0;

  sprintf(buffer, "Create %d partitions on device", partitionsCount);

  if (!confirmDataDestroy(buffer))
    return false;

  clearInformationArea();
  printTargetInfo();
  printStateMessage("Please wait...");

  locate(0, MESSAGE_ROW);
  printCentered("Preparing partitioning process...");

  preparePartitioningProcess(selectedDriver->slot, selectedDeviceIndex, selectedLunIndex + 1, partitionsCount, partitions, luns[selectedLunIndex].sectorsPerTrack);

  for (i = 0; i < partitionsCount; i++) {
    locate(0, MESSAGE_ROW);
    sprintf(buffer, "Creating partition %d of %d ...", i + 1, partitionsCount);
    printCentered(buffer);

    error = createPartition(i);
    if (error != 0) {
      sprintf(buffer, "Error when creating partition %d :", i + 1);
      printDosErrorMessage(error, buffer);
      waitKey();
      return false;
    }
  }

  locate(0, MESSAGE_ROW + 2);
  printDone();
  printStateMessage("Press any key to return...");
  waitKey();

  return true;
}

void deleteAllPartitions() {
  sprintf(buffer, "Discard all %s partitions? (y/n) ", partitionsExistInDisk ? "existing" : "defined");
  printStateMessage(buffer);
  if (!getYesOrNo()) {
    return;
  }

  partitionsCount = 0;
  partitionsExistInDisk = false;
  unpartitionnedSpaceInSectors = selectedLun->sectorCount;
  recalculateAutoPartitionSize(true);
}

void addPartition() {
  uint16_t maxPartitionSizeInM;
  uint16_t maxPartitionSizeInK;
  uint8_t  lineLength;
  char *   pointer;
  char     ch;
  bool     validNumberEntered = false;
  uint32_t enteredSizeInK = 0;
  bool     lessThan1MAvailable;
  bool     sizeInKSpecified;
  uint32_t unpartitionnedSpaceExceptAlignmentInK = (unpartitionnedSpaceInSectors - EXTRA_PARTITION_SECTORS) / 2;

  maxPartitionSizeInM = (uint16_t)((unpartitionnedSpaceInSectors / 2) >> 10);
  maxPartitionSizeInK = unpartitionnedSpaceExceptAlignmentInK > (uint32_t)32767 ? (uint16_t)32767 : unpartitionnedSpaceExceptAlignmentInK;

  lessThan1MAvailable = (maxPartitionSizeInM == 0);

  if (maxPartitionSizeInM > (uint32_t)MAX_FAT16_PARTITION_SIZE_IN_M) {
    maxPartitionSizeInM = MAX_FAT16_PARTITION_SIZE_IN_M;
  }

  printStateMessage("Enter size or press ENTER to cancel");

  while (!validNumberEntered) {
    sizeInKSpecified = true;
    clearInformationArea();
    printTargetInfo();
    newLine();
    printf("Add new partition\r\n\r\n");

    if (lessThan1MAvailable) {
      printf("Enter");
    } else {
      printf("Enter partition size in MB (1-%d)\r\nor", maxPartitionSizeInM);
    }
    printf(" partition size in KB followed by \"K\" (%d-%d): ", MIN_PARTITION_SIZE_IN_K, maxPartitionSizeInK);

    fgets(buffer, 6, stdin);
    lineLength = strlen(buffer) - 1; // remove CR
    if (lineLength == 0) {
      return;
    }

    pointer = buffer;
    pointer[lineLength] = '\0';
    enteredSizeInK = 0;

    while (true) {
      ch = (*pointer++) | 32;
      if (ch == 'k') {
        validNumberEntered = true;
        break;
      } else if (ch == '\0' || ch == 13 || ch == 'm') {
        validNumberEntered = true;
        enteredSizeInK <<= 10;
        sizeInKSpecified = false;
        break;
      } else if (ch < '0' || ch > '9') {
        break;
      }

      enteredSizeInK = (enteredSizeInK * 10) + (ch - '0');
      lineLength--;
      if (lineLength == 0) {
        validNumberEntered = true;
        enteredSizeInK *= 1024;
        sizeInKSpecified = false;
        break;
      }
    }

    if (validNumberEntered && (sizeInKSpecified && (enteredSizeInK > maxPartitionSizeInK) || (enteredSizeInK < MIN_PARTITION_SIZE_IN_K)) || (!sizeInKSpecified && (enteredSizeInK > ((uint32_t)maxPartitionSizeInM * 1024)))) {
      validNumberEntered = false;
    }
  }

  autoPartitionSizeInK = enteredSizeInK > unpartitionnedSpaceExceptAlignmentInK ? unpartitionnedSpaceExceptAlignmentInK : enteredSizeInK;
  addAutoPartition();
  unpartitionnedSpaceExceptAlignmentInK = (unpartitionnedSpaceInSectors - EXTRA_PARTITION_SECTORS) / 2;
  autoPartitionSizeInK = enteredSizeInK > unpartitionnedSpaceExceptAlignmentInK ? unpartitionnedSpaceExceptAlignmentInK : enteredSizeInK;
  recalculateAutoPartitionSize(false);
}

void goPartitioningMainMenuScreen() {
  char    key;
  uint8_t error;
  bool    canAddPartitionsNow;
  bool    mustRetrievePartitionInfo = true;

  while (true) {
    if (mustRetrievePartitionInfo) {
      clearInformationArea();
      printTargetInfo();

      if (canCreatePartitions) {
        locate(0, MESSAGE_ROW);
        printCentered("Searching partitions...");
        printStateMessage("Please wait...");
        error = getDiskPartitionsInfo();
        if (error != 0) {
          printDosErrorMessage(error, "Error when searching partitions:");
          printStateMessage("Manage device anyway? (y/n) ");
          if (!getYesOrNo())
            return;
        }
        partitionsExistInDisk = (partitionsCount > 0);
      }
      mustRetrievePartitionInfo = false;
    }

    clearInformationArea();
    printTargetInfo();
    if (!partitionsExistInDisk) {
      printf("Unpartitionned space available: ");
      printSize(unpartitionnedSpaceInSectors / 2);
      newLine();
    }
    newLine();

    printf("Changes are not committed until W is pressed.\r\n\r\n");

    if (partitionsCount > 0) {
      printf("S. Show partitions (%d %s)\r\n"
             "D. Delete all partitions\r\n",
             partitionsCount, partitionsExistInDisk ? "found" : "defined");
    } else if (canCreatePartitions) {
      printf("(No partitions found or defined)\r\n");
    }
    canAddPartitionsNow = !partitionsExistInDisk && canCreatePartitions && unpartitionnedSpaceInSectors >= (MIN_REMAINING_SIZE_FOR_NEW_PARTITIONS_IN_K * 2) + (EXTRA_PARTITION_SECTORS) && partitionsCount < MAX_PARTITIONS_TO_HANDLE;
    if (canAddPartitionsNow) {
      printf("A. Add one ");
      printSize(autoPartitionSizeInK);
      printf(" partition\r\n");
      printf("P. Add partition...\r\n");
    }
    if (!partitionsExistInDisk && partitionsCount > 0) {
      printf("U. Undo add ");
      printSize(partitions[partitionsCount - 1].sizeInK);
      printf(" partition\r\n");
    }
    newLine();
    if (canDoDirectFormat) {
      printf("F. Format device without partitions\r\n\r\n");
    }
    if (!partitionsExistInDisk && partitionsCount > 0) {
      printf("W. Write partitions to disk\r\n\r\n");
    }
    printf("T. Test device access\r\n");
    printf("C: Test write for last sector\r\n");

    printStateMessage("Select an option or press ESC to return");

    while ((key = waitKey()) == 0)
      ;
    if (key == ESC) {
      if (partitionsExistInDisk || partitionsCount == 0) {
        return;
      }
      printStateMessage("Discard changes and return? (y/n) ");
      if (getYesOrNo()) {
        return;
      } else {
        continue;
      }
    }
    key |= 32;
    if (key == 's' && partitionsCount > 0) {
      showPartitions();
      continue;
    }

    if (key == 'd' && partitionsCount > 0) {
      deleteAllPartitions();
      continue;
    }

    if (key == 'p' && canAddPartitionsNow > 0) {
      addPartition();
      continue;
    }

    if (key == 'a' && canAddPartitionsNow > 0) {
      addAutoPartition();
      continue;
    }
    // } else if(key == 'u' && !partitionsExistInDisk && partitionsCount > 0) {
    // 	UndoAddPartition();
    if (key == 't') {
      testDeviceAccess();
      continue;
    }

    if (key == 'c') {
      testDeviceWriteAccess();
      continue;
    }

    // } else if(key == 'f' && canDoDirectFormat) {
    // 	if(FormatWithoutPartitions()) {
    // 		mustRetrievePartitionInfo = true;
    // 	}
    if (key == 'w' && !partitionsExistInDisk && partitionsCount > 0)
      if (writePartitionTable())
        mustRetrievePartitionInfo = true;
  }
}

void goLunSelectionScreen(uint8_t deviceIndex) {
  uint8_t key;

  currentDevice = &devices[deviceIndex - 1];
  selectedDeviceIndex = deviceIndex;

  availableLunsCount = 0;

  while (true) {
    showLunSelectionScreen();
    if (availableLunsCount == 0) {
      return;
    }

    while (true) {
      key = waitKey();
      if (key == ESC)
        return;

      key -= '0';
      if (key >= 1 && key <= MAX_LUNS_PER_DEVICE && luns[key - 1].suitableForPartitioning) {
        initializePartitioningVariables(key);
        goPartitioningMainMenuScreen();
        break;
      }
    }
  }
}

void goDeviceSelectionScreen(uint8_t driverIndex) {
  char    slot[4];
  uint8_t key;

  selectedDriver = &drivers[driverIndex - 1];
  composeSlotString(selectedDriver->slot, slot);
  strcpy(selectedDriverName, selectedDriver->driverName);
  sprintf(selectedDriverName + strlen(selectedDriverName), " on slot %s\r\n", slot);

  availableDevicesCount = 0;

  while (true) {
    showDeviceSelectionScreen();
    if (availableDevicesCount == 0) {
      return;
    }

    while (true) {
      key = waitKey();
      if (key == ESC)
        return;

      key -= '0';
      if (key >= 1 && key <= MAX_DEVICES_PER_DRIVER && devices[key - 1].lunCount != 0) {
        goLunSelectionScreen(key);
        break;
      }
    }
  }
}

void goDriverSelectionScreen() {
  uint8_t key;

  while (true) {
    showDriverSelectionScreen();
    if (installedDriversCount == 0) {
      return;
    }

    while (true) {
      key = waitKey();
      if (key == ESC)
        return;

      key -= '0';
      if (key >= 1 && key <= installedDriversCount) {
        goDeviceSelectionScreen(key);
        break;
      }
    }
  }
}

void main() {
  installedDriversCount = 0;
  selectedDeviceIndex = 0;
  selectedLunIndex = 0;
  availableLunsCount = 0;

  saveOriginalScreenConfiguration();

  composeWorkScreenConfiguration();
  setScreenConfiguration();
  initializeWorkingScreen("Nextor disk partitioning tool");

  goDriverSelectionScreen();
}
