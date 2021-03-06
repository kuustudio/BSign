#ifndef SIGNCONFIG_H
#define SIGNCONFIG_H

#include <QString>

class SignConfig
{
public:
    SignConfig();

    QString mobileProvisionPath;

    QString bundleId;

    QString displayName;

    bool useMobileProvsionBundleId = false;
    //是否是推送证书描述文件
    bool isPushMobileProvsion = false;

    bool signNib = false;

    bool signFramwork = false;

    bool signDylib = false;

    QString expireTime;

    QString ccName;

    QString warningMessage;
    //证书UUID
    QString ccUuid;

    bool useAppCount = false;
    //重签名后输出地址
    QString outResignPath;
    //重签名文件名
    QString signFileNames;
};

#endif // SIGNCONFIG_H
