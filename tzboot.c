// Copyright 2019-2022 The jdh99 Authors. All rights reserved.
// bootloader
// Authors: jdh99 <jdh821@163.com>

#include "tzboot.h"
#include "tzflash.h"
#include "md5.h"
#include <string.h>

// �����������
#define MAX_UPGRADE_NUM 3
// д��flashʱ���ֽ���
#define WRITE_BYTES_NUM 64

static TZBootEnvironment gEnvironment;

static bool readAppTail(UpgradeTail* appTail);
static bool readBackupTail(UpgradeTail* backupTail);
static bool isNeedUpgrade(void);
static void clearBackupTail(void);
static bool upgrade(void);

// TZBootLoad ģ������
void TZBootLoad(TZBootEnvironment environment) {
    gEnvironment = environment;
}

// TZBootRun ģ������
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
}

static bool isNeedUpgrade(void) {
    UpgradeTail appTail, backupTail;
    if (readAppTail(&appTail) == false || readBackupTail(&backupTail) == false) {
        return false;
    }

    if (appTail.DVersion != backupTail.DVersion) {
        return false;
    }
    // V�汾һ��˵����ͬһ������Ҫ����
    if (appTail.VVersion == backupTail.VVersion) {
        return false;
    }
    if ((int)backupTail.FileSize > gEnvironment.AppMaxSize) {
        return false;
    }

    uint8_t md5[MD5_LEN] = {0};
    MD5Calc((uint8_t*)(intptr_t)gEnvironment.BackupAddr, (int)backupTail.FileSize, md5);
    if (memcmp(backupTail.MD5, md5, MD5_LEN) != 0) {
        // �������Ч�ļ�β�ɱ���ÿ������������MD5����������ٶ�
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
    intptr_t fdApp = TZFlashOpen(gEnvironment.AppAddr, gEnvironment.AppMaxSize, TZFLASH_WRITE_ONLY);
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
    }
    TZFlashClose(fdBackup);
    TZFlashClose(fdApp);
    return isOK;
}

// TZBootUpdateAppTail ����Ӧ�ó������β
// ���Ӧ�ó��������β��D�汾����V�汾������ֵ��ƥ��������
void TZBootUpdateAppTail(int dversion, int vversion) {
    UpgradeTail appTail, backupTail;
    if (readAppTail(&appTail) == false || readBackupTail(&backupTail) == false) {
        return;
    }

    if (memcmp(appTail.Label, (uint8_t*)VALID_LABEL, LABEL_LEN) == 0 && appTail.DVersion == dversion && 
        appTail.VVersion == vversion) {
        return;
    }

    // ����Ӧ�ó��������β
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
