// Copyright 2019-2022 The jdh99 Authors. All rights reserved.
// bootloader
// Authors: jdh99 <jdh821@163.com>

#ifndef TZBOOT_H
#define TZBOOT_H

#include "tztype.h"
#include "md5.h"

// 有效标签
#define VALID_LABEL "TZIOT"
// 标签字节数
#define LABEL_LEN 5
// 板卡类型最大字节数
#define BOARD_TYPE_LEN_MAX 16
// 板卡类型数组长度
#define BOARD_TYPE_ARRAY_LEN 10
// 升级尾保留字节数
#define UPGRADE_TAIL_RESERVED 63

// TZBootRebootFunc 执行应用程序函数类型
typedef void (*TZBootRunApp)(int addr);

#pragma pack(1)

// 256字节文件尾
typedef struct {
    uint8_t Label[LABEL_LEN];
    uint16_t DVersion;
    uint8_t VVersion;
    uint32_t FileSize;
    // 校验方式.0:MD5,1:CRC16,2:CRC32
    uint8_t CheckType;
    uint8_t FileCheckSum[MD5_LEN];
    uint32_t BoardTypeArrayLen;
    char BoardTypeArray[BOARD_TYPE_ARRAY_LEN][BOARD_TYPE_LEN_MAX];
    uint8_t Reserved[UPGRADE_TAIL_RESERVED];
} UpgradeTail;

// 校验方式
#define UPGRADE_CHECK_TYPE_MD5 0
#define UPGRADE_CHECK_TYPE_CRC16 1
#define UPGRADE_CHECK_TYPE_CRC32 2

// TZUpgradeEnvironment 升级环境
typedef struct {
    // 成员变量
    // 应用程序地址
    int AppAddr;
    int AppMaxSize;
    // 备份区程序地址
    int BackupAddr;
    int BackupMaxSize;
    // 保存文件尾地址
    int UpgradeTailSaveAddr;
    int UpgradeTailSaveMaxSize;

    // API接口
    // 执行应用程序函数
    TZBootRunApp RunApp;
} TZBootEnvironment;

#pragma pack()

// TZBootLoad 模块载入
void TZBootLoad(TZBootEnvironment environment);

// TZBootRun 模块运行
void TZBootRun(void);

// TZBootUpdateAppTail 更新应用程序程序尾
// 如果应用程序的升级尾的D版本或者V版本与配置值不匹配则会更新
void TZBootUpdateAppTail(int dversion, int vversion);

// TZBootSetFeedFunction ����ι���ӿ�.���������,�򲻻�ι��
void TZBootSetFeedFunction(TZEmptyFunc feed);

#endif
