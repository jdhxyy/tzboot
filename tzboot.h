// Copyright 2019-2022 The jdh99 Authors. All rights reserved.
// bootloader
// Authors: jdh99 <jdh821@163.com>

#ifndef TZBOOT_H
#define TZBOOT_H

#include "tztype.h"

// ��Ч��ǩ
#define VALID_LABEL "TZIOT"
// ��ǩ�ֽ���
#define LABEL_LEN 5
// �忨��������ֽ���
#define BOARD_TYPE_LEN_MAX 16
// �忨�������鳤��
#define BOARD_TYPE_ARRAY_LEN 10
// ����β�����ֽ���
#define UPGRADE_TAIL_RESERVED 63

// TZBootRebootFunc ִ��Ӧ�ó���������
typedef void (*TZBootRunApp)(int addr);

#pragma pack(1)

// 256�ֽ��ļ�β
typedef struct {
    uint8_t Label[LABEL_LEN];
    uint16_t DVersion;
    uint8_t VVersion;
    uint32_t FileSize;
    // У�鷽ʽ.0:MD5,1:CRC16,2:CRC32
    uint8_t CheckType;
    uint8_t FileCheckSum[MD5_LEN];
    uint32_t BoardTypeArrayLen;
    char BoardTypeArray[BOARD_TYPE_ARRAY_LEN][BOARD_TYPE_LEN_MAX];
    uint8_t Reserved[UPGRADE_TAIL_RESERVED];
} UpgradeTail;

// У�鷽ʽ
#define UPGRADE_CHECK_TYPE_MD5 0
#define UPGRADE_CHECK_TYPE_CRC16 1
#define UPGRADE_CHECK_TYPE_CRC32 2

// TZUpgradeEnvironment ��������
typedef struct {
    // ��Ա����
    // Ӧ�ó����ַ
    int AppAddr;
    int AppMaxSize;
    // �����������ַ
    int BackupAddr;
    int BackupMaxSize;
    // �����ļ�β��ַ
    int UpgradeTailSaveAddr;
    int UpgradeTailSaveMaxSize;

    // API�ӿ�
    // ִ��Ӧ�ó�����
    TZBootRunApp RunApp;
} TZBootEnvironment;

#pragma pack()

// TZBootLoad ģ������
void TZBootLoad(TZBootEnvironment environment);

// TZBootRun ģ������
void TZBootRun(void);

// TZBootUpdateAppTail ����Ӧ�ó������β
// ���Ӧ�ó��������β��D�汾����V�汾������ֵ��ƥ��������
void TZBootUpdateAppTail(int dversion, int vversion);

#endif
