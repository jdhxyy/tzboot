// Copyright 2019-2022 The jdh99 Authors. All rights reserved.
// bootloader
// Authors: jdh99 <jdh821@163.com>

#include "tzboot.h"
#include "tzflash.h"
#include <string.h>

// 最大升级次数
#define MAX_UPGRADE_NUM 3
// 写入flash时的字节数
#define WRITE_BYTES_NUM 64

static TZBootEnvironment gEnvironment;
static TZEmptyFunc gFeed = NULL;

static bool readAppTail(UpgradeTail* appTail);
static bool readBackupTail(UpgradeTail* backupTail);
static bool isNeedUpgrade(void);
static void clearBackupTail(void);
static bool upgrade(void);
static void feed(void);

// TZBootLoad 模块载入
void TZBootLoad(TZBootEnvironment environment) {
    gEnvironment = environment;
}

// TZBootRun 模块运行
void TZBootRun(void) {
    if (isNeedUpgrade()) {
        for (int i = 0; i < MAX_UPGRADE_NUM; i++) {
            if (upgrade()) {
                break;
            }
        }
        clearBackupTail();
    }
    gEnvironment.RunApp(gEnvironment.AppAddr);
    feed();
}

static bool isNeedUpgrade(void) {
    UpgradeTail appTail, backupTail;
    if (readAppTail(&appTail) == false || readBackupTail(&backupTail) == false) {
        return false;
    }

    if (appTail.DVersion != backupTail.DVersion) {
        return false;
    }
    // V版本一致说明是同一个不需要升级
    if (appTail.VVersion == backupTail.VVersion) {
        return false;
    }
    if ((int)backupTail.FileSize > gEnvironment.AppMaxSize) {
        return false;
    }

    uint8_t md5[MD5_LEN] = {0};
    MD5Calc((uint8_t*)(intptr_t)gEnvironment.BackupAddr, (int)backupTail.FileSize, md5);
    if (memcmp(backupTail.FileCheckSum, md5, MD5_LEN) != 0) {
        // 清除掉无效文件尾可避免每次启动都计算MD5，提高启动速度
        clearBackupTail();
        return false;
    }
    return true;
}

static bool readAppTail(UpgradeTail* appTail) {
    intptr_t fd = TZFlashOpen(gEnvironment.UpgradeTailSaveAddr, gEnvironment.UpgradeTailSaveMaxSize, TZFLASH_READ_ONLY);
    if (fd == 0) {
        return false;
    }

    if (TZFlashRead(fd, (uint8_t*)appTail, sizeof(UpgradeTail)) == -1) {
        TZFlashClose(fd);
        return false;
    }
    TZFlashClose(fd);
    return true;
}

static bool readBackupTail(UpgradeTail* backupTail) {
    intptr_t fd = TZFlashOpen(gEnvironment.UpgradeTailSaveAddr, gEnvironment.UpgradeTailSaveMaxSize, TZFLASH_READ_ONLY);
    if (fd == 0) {
        return false;
    }

    if (TZFlashSeek(fd, sizeof(UpgradeTail)) == false) {
        TZFlashClose(fd);
        return false;
    }
    if (TZFlashRead(fd, (uint8_t*)backupTail, sizeof(UpgradeTail)) == -1) {
        TZFlashClose(fd);
        return false;
    }
    TZFlashClose(fd);
    return true;
}

static void clearBackupTail(void) {
    UpgradeTail appTail;
    if (readAppTail(&appTail) == false) {
        return;
    }

    intptr_t fd = TZFlashOpen(gEnvironment.UpgradeTailSaveAddr, gEnvironment.UpgradeTailSaveMaxSize, TZFLASH_WRITE_ONLY);
    if (fd == 0) {
        return;
    }
    TZFlashWrite(fd, (uint8_t*)&appTail, sizeof (UpgradeTail));
    TZFlashClose(fd);
    return;
}

static bool upgrade(void) {
    UpgradeTail backupTail;
    if (readBackupTail(&backupTail) == false) {
        return false;
    }
    int needReadSize = (int)backupTail.FileSize;

    intptr_t fdBackup = TZFlashOpen(gEnvironment.BackupAddr, gEnvironment.BackupMaxSize, TZFLASH_READ_ONLY);
    if (fdBackup == 0) {
        return false;
    }

    feed();
    intptr_t fdApp = TZFlashOpen(gEnvironment.AppAddr, gEnvironment.AppMaxSize, TZFLASH_WRITE_ONLY);
    feed();

    if (fdApp == 0) {
        TZFlashClose(fdBackup);
        return false;
    }

    bool isOK = false;
    uint8_t buf[WRITE_BYTES_NUM] = {0};
    int writeNum = 0;
    for (;;) {
        if (needReadSize > WRITE_BYTES_NUM) {
            writeNum = WRITE_BYTES_NUM;
        } else {
            writeNum = needReadSize;
        }
        if (TZFlashRead(fdBackup, buf, writeNum) == -1) {
            break;
        }
        if (TZFlashWrite(fdApp, buf, writeNum) == -1) {
            break;
        }

        needReadSize -= writeNum;
        if (needReadSize <= 0) {
            isOK = true;
            break;
        }

        feed();
    }
    TZFlashClose(fdBackup);
    TZFlashClose(fdApp);
    return isOK;
}

static void feed(void) {
    if (gFeed != NULL) {
        gFeed();
    }
}

// TZBootUpdateAppTail 更新应用程序程序尾
// 如果应用程序的升级尾的D版本或者V版本与配置值不匹配则会更新
void TZBootUpdateAppTail(int dversion, int vversion) {
    UpgradeTail appTail, backupTail;
    if (readAppTail(&appTail) == false || readBackupTail(&backupTail) == false) {
        return;
    }

    if (memcmp(appTail.Label, (uint8_t*)VALID_LABEL, LABEL_LEN) == 0 && appTail.DVersion == dversion && 
        appTail.VVersion == vversion) {
        return;
    }

    // 更新应用程序的升级尾
    memset(&appTail, 0, sizeof(UpgradeTail));
    memcpy(appTail.Label, VALID_LABEL, LABEL_LEN);
    appTail.DVersion = (uint16_t)dversion;
    appTail.VVersion = (uint8_t)vversion;
    appTail.FileSize = 0;

    intptr_t fd = TZFlashOpen(gEnvironment.UpgradeTailSaveAddr, gEnvironment.UpgradeTailSaveMaxSize, TZFLASH_WRITE_ONLY);
    if (fd == 0) {
        return;
    }
    TZFlashWrite(fd, (uint8_t*)&appTail, sizeof(UpgradeTail));
    TZFlashWrite(fd, (uint8_t*)&backupTail, sizeof(UpgradeTail));
    TZFlashClose(fd);
}

// TZBootSetFeedFunction 设置喂狗函数接口.不设置则擦写flash时不会喂狗
void TZBootSetFeedFunction(TZEmptyFunc feed) {
    gFeed = feed;
}
