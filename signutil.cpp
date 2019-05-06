#include "signutil.h"

bool SignUtil::dylibInjection(QString dylibFilePath,QString machOFilePath,QString ccName){
    QFileInfo fileInfo(dylibFilePath);
    QString dylibFileName=fileInfo.fileName();
    fileInfo=QFileInfo(machOFilePath);
    QString machOAbstractPath=fileInfo.absolutePath();

    bool copyResult=CopyFileToPath(dylibFilePath,machOAbstractPath+"/"+dylibFileName,true);
    if(!copyResult){
        emit execPrint("植入代码迁移失败");
        return false;
    }
    QString cmd="chmod +x \""+machOFilePath+"\"";
    qDebug() << "执行命令："+cmd;
    int flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("签名失败！");
        return false;
    }

    //注意 第三方库要单独重签名
    cmd="/usr/bin/codesign --force --sign \""+ccName+"\" \""+machOAbstractPath+"/"+dylibFileName+"\"";
    flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("植入代码重签名失败");
        return false;
    }
    qDebug() << "开始注入代码";
    fileInfo=QFileInfo(machOFilePath);
    cmd="cd \""+machOAbstractPath+"\";"+"/tmp/optool install -c load -p \"@executable_path/"+dylibFileName+"\" -t \""+fileInfo.fileName()+"\"";
//        cmd="cd "+tmp+"Payload/"+this->appName+";yololib "+machOFileName+" libisigntoolhook.dylib";
    qDebug() << "执行命令："+cmd;
    flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("代码注入失败！");
        return false;
    }
    qDebug() << "代码注入成功";
    return true;
}

SignUtil::SignUtil(QObject *parent) : QObject(parent)
{

}

QString SignUtil::findSpecialFileQprocessParamsHandle(QString params,QString param){
    if(params.isEmpty()){
        params="-name \""+param+"\" ";
    }else{
        params+="-o -name \""+param+"\" ";
    }
    return params;
}


void SignUtil::readIpaInfo(QString filePath){
    QDateTime time = QDateTime::currentDateTime();   //获取当前时间
    uint timeT = time.toTime_t();
    QString tmp = "/tmp/"+QString::number(timeT,10)+"/";
    QDir tmpDir(tmp);
    if(!tmpDir.exists()){
        tmpDir.mkdir(tmp);
    }
    QFileInfo fileInfo(filePath);
//    //获取ipa文件名
    QString ipaName=fileInfo.fileName();
    QFile::copy(filePath,tmp+ipaName);
    QString zipFile = ipaName.split(".")[0]+".zip";
    QFile::rename(tmp+ipaName,tmp+zipFile);

    QString cmd = "unzip \""+tmp+zipFile+"\" -d "+tmp;
    system(cmd.toLocal8Bit().data());

    QDir dir(tmp);
    QFileInfoList file_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(QFileInfo fileInfo:file_list){
        QString floderName=fileInfo.fileName();
        if(floderName=="__MACOSX"){
            continue;
        }
        QString cmd = "mv \""+tmp+floderName+"\" "+tmp+"Payload";
        system(cmd.toLocal8Bit().data());
    }

    QString appPath=Common::execShell("find "+tmp+"Payload -name \"*.app\"");
    QStringList list=appPath.split("/");
    QString appName= list.at(list.size()-1);
    qDebug() << "appName:"+appName;

    QString plistPath=tmp+"Payload/"+appName+"/Info.plist";
    cmd="/usr/libexec/PlistBuddy -c 'print :CFBundleExecutable' \""+plistPath+"\"";
    QString machOFileName = Common::execShell(cmd).trimmed();
    qDebug() << "machOFileName:"+machOFileName;

    cmd = "/usr/libexec/PlistBuddy -c \"Print CFBundleIdentifier\" \""+plistPath+"\"";
    QString bundleId = Common::execShell(cmd).trimmed();
//    system(("rm -rf "+tmp).toLocal8Bit().data());
    //读取应用打包名称
    cmd="/usr/libexec/PlistBuddy -c 'Print :CFBundleDisplayName' \""+tmp+"Payload/"+appName+"/Info.plist\"";
    QString deployAppName=Common::execShell(cmd).trimmed();

    if(deployAppName.isEmpty()){
        deployAppName=appName.split(".")[0];
    }

    IpaInfo *ipaInfo = new IpaInfo;
    ipaInfo->appName=appName;
    ipaInfo->appPath=appPath;
    ipaInfo->deployAppName=deployAppName;
    ipaInfo->bundleId=bundleId.trimmed();
    ipaInfo->ipaName=ipaName;
    ipaInfo->ipaPath=fileInfo.absolutePath();
    ipaInfo->machOFileName=machOFileName;
    ipaInfo->tmpPath=tmp;

    QString machOFilePath=tmp+"/Payload/"+appName+"/"+machOFileName;
    QStringList thirdInjectionList=SignUtil::readThirdInjection(machOFilePath);
    ipaInfo->thirdInjectionInfoList=thirdInjectionList;
    this->ipaInfo=ipaInfo;
}

bool SignUtil::sign(IpaInfo *ipaInfo,SignConfig *signConfig){

    Common::execShell("chmod -R 777  /tmp");

    QString tmp=ipaInfo->tmpPath;
    QString appName=ipaInfo->appName;
    QString bundleId=ipaInfo->bundleId;
    QString machOFileName=ipaInfo->machOFileName;
    QString ipaName=ipaInfo->ipaName;
    QString deployAppName=ipaInfo->deployAppName;

    qDebug() << "machOFileName is "+machOFileName;
    //删除原来签名文件
    QString cmd="rm -rf "+tmp+"Payload/"+appName+"/_CodeSignature";
    qDebug() << "执行命令："+cmd;
    int flag = system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("删除原来签名文件失败");
        return false;
    }
    //生成plist文件
    cmd = "/usr/bin/security cms -D -i \""+signConfig->mobileProvisionPath+"\" > "+tmp+"entitlements_full.plist";
    qDebug() << "执行命令："+cmd;
    flag=system(cmd.toLocal8Bit().data());
    qDebug() << flag;

    //读取UUID
    QStringList readUUIDParams;
    readUUIDParams << "-c";
    readUUIDParams << "/usr/libexec/PlistBuddy -c 'Print:UUID' "+tmp+"entitlements_full.plist";
    QProcess *uuidProcess=new QProcess;
    uuidProcess->start("/bin/bash",readUUIDParams);
    uuidProcess->waitForFinished();
    QString uuid = uuidProcess->readAllStandardOutput().trimmed();
    qDebug() << "UUID ====> "+uuid;
    signConfig->ccUuid=uuid;
    cmd="echo "+uuid+" > "+tmp+"Payload/"+appName+"/uuid";
    qDebug() << "执行命令："+cmd;
    flag = system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("写入uuid文件失败");
        return false;
    }

    cmd="/usr/libexec/PlistBuddy -x -c 'Print:Entitlements' "+tmp+"entitlements_full.plist > "+tmp+"entitlements.plist";
    qDebug() << "执行命令："+cmd;
    flag = system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("生成plist文件失败");
        return false;
    }

    //修改BundleId
    if(!signConfig->bundleId.isEmpty()&&signConfig->bundleId!=ipaInfo->bundleId){
        cmd="plutil -replace CFBundleIdentifier -string "+signConfig->bundleId+" "+tmp+"Payload/"+appName+"/info.plist";
//        qDebug() << "执行命令："+cmd;
        flag = system(cmd.toLocal8Bit().data());
        if(flag!=0){
            emit execPrint("修改BundleId失败");
            return false;
        }
    }

    //修改Display Name
    if(!signConfig->displayName.isEmpty()&&signConfig->displayName!=ipaInfo->deployAppName){
        cmd="plutil -replace CFBundleDisplayName -string "+signConfig->displayName+" "+tmp+"Payload/"+appName+"/info.plist";
        qDebug() << "执行命令："+cmd;
        flag = system(cmd.toLocal8Bit().data());
        if(flag!=0){
            emit execPrint("Display Name Update Fail");
            return false;
        }
    }

    //使用证书BundleID
    if(signConfig->useMobileProvsionBundleId){

        QProcess *p = new QProcess;
        QStringList args;
        args.append("-c");
        args.append("/usr/libexec/PlistBuddy "+tmp+"entitlements.plist "+"-c print | grep application-identifier | awk '{print $3}'");
        p->start("/bin/bash",args);
        p->waitForFinished();
        //读取证书BundleID
        QString mpBundleId=p->readAllStandardOutput().trimmed();
        qDebug() << "证书BundleID："+mpBundleId;
        if(mpBundleId.isEmpty()){
            emit execPrint("读取证书BundleId失败");
            return false;
        }
        cmd="plutil -replace CFBundleIdentifier -string "+mpBundleId+" "+tmp+"Payload/"+appName+"/info.plist";
        qDebug() << "执行命令："+cmd;
        flag = system(cmd.toLocal8Bit().data());
        if(flag!=0){
            emit execPrint("修改证书BundleId失败");
            return false;
        }
        signConfig->bundleId=mpBundleId;
    }

    QFile file(tmp+"Payload/"+appName+"/embedded.mobileprovision");
    if(!file.exists()){
        file.open(QIODevice::WriteOnly | QIODevice::Text );
        QTextStream in(&file);
        in << "";
        file.flush();
        file.close();
    }

    cmd="cp \""+signConfig->mobileProvisionPath+"\" \""+tmp+"Payload/"+appName+"/embedded.mobileprovision\"";
    flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("复制mobileprovision文件失败");
        return false;
    }

    //特殊文件重签名 start
    QString execParam;

    if(signConfig->signNib){
        execParam=findSpecialFileQprocessParamsHandle(execParam,"*.nib");
    }

    if(signConfig->signFramwork){
        execParam=findSpecialFileQprocessParamsHandle(execParam,"*.framework");
    }

    if(signConfig->signDylib){
        execParam=findSpecialFileQprocessParamsHandle(execParam,"*.dylib");
    }

    execParam=findSpecialFileQprocessParamsHandle(execParam,"*.appex");
    execParam=findSpecialFileQprocessParamsHandle(execParam,"*.app");
    execParam=findSpecialFileQprocessParamsHandle(execParam,"*.so");
    execParam=findSpecialFileQprocessParamsHandle(execParam,"*.pvr");
    if(!execParam.isEmpty()){
        QStringList execParams;
        execParams << "-c";
        execParams << "/usr/bin/find "+tmp+"Payload/"+appName+" "+execParam;
        QProcess *findSpecialFile=new QProcess;
        findSpecialFile->start("/bin/bash",execParams);
        findSpecialFile->waitForFinished();
        QString specialFile = findSpecialFile->readAllStandardOutput().trimmed();
        if(!specialFile.isEmpty()){
            QStringList specialFileList= specialFile.split("\n");
            qDebug() << "重签以下文件：";
            for(QString fi : specialFileList){
                cmd="/usr/bin/codesign --force --sign \""+signConfig->ccName+"\" \""+fi+"\"";
                qDebug() << "重签名命令："+cmd;
                flag=system(cmd.toLocal8Bit().data());
                if(flag!=0){
                    emit execPrint(fi+"重签名失败");
                    return false;
                }
            }
        }
    }
    //特殊文件重签名 end

    if (!signConfig->expireTime.isEmpty()){
        bool injection=dylibInjection(libisigntoolhookFilePath,tmp+"Payload/"+appName+"/"+ipaInfo->machOFileName,signConfig->ccName);
        if(!injection){
            return false;
        }
    }

    if(signConfig->useAppCount){
        bool injection=dylibInjection(libisigntoolappcountFilePath,tmp+"Payload/"+appName+"/"+ipaInfo->machOFileName,signConfig->ccName);
        if(!injection){
            return false;
        }
    }

    cmd="/usr/bin/codesign -f --entitlements \"" + tmp+"entitlements.plist\""+ " -s \"" + signConfig->ccName + "\" \"" + tmp+"Payload/"+appName+"\"";
    qDebug() << "执行命令："+cmd;
    flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("签名失败！");
        return false;
    }
    QString newIPA=ipaName.mid(0,ipaName.length()-4)+"_BResigned.ipa";
    QFile isResigned(ipaInfo->ipaPath+"/"+newIPA);
    if(isResigned.exists()){
        qDebug() << "delete old sign package";
        isResigned.remove();
    }

    cmd="cd "+tmp+";zip -qr ../\""+newIPA+"\" Payload";
    qDebug() << "执行命令："+cmd;
    flag=system(cmd.toLocal8Bit().data());
    if(flag!=0){
        emit execPrint("打包失败！");
        return false;
    }

    cmd="rm -rf "+tmp;
    qDebug() << "执行命令："+cmd;
    system(cmd.toLocal8Bit().data());

    if(signConfig->outResignPath.isEmpty()){
        cmd="mv /tmp/\""+newIPA+"\" \""+ipaInfo->ipaPath+"/\"";
    }else{
        cmd="mv /tmp/\""+newIPA+"\" \""+signConfig->outResignPath+"/\"";
    }
    qDebug() << "执行命令："+cmd;
    system(cmd.toLocal8Bit().data());
    emit execPrint("签名完成！新包地址："+ipaInfo->ipaPath+"/"+newIPA);
    return true;
}

QStringList SignUtil::readThirdInjection(QString machOFilePath){
    QString shell="otool -L "+machOFilePath+" | grep -e @executable_path -e @rpath | awk '!a[$0]++'";
    QString result=Common::execShell(shell);
    QStringList list=result.split("\t");
    list.removeOne("");
    QStringList stringList;
    stringList.append("选择第三方文件卸载");
    for(QString info:list){
        int index=info.lastIndexOf("(compatibility");
        QString injectionInfo=info.mid(0,index-1);
        stringList.append(injectionInfo);
    }
    return stringList;
}

bool SignUtil::uninstallThirdInjection(QString machOFilePath,QString path){
    QString shell=optoolFilePath+" uninstall -p \""+path+"\" -t \""+machOFilePath+"\"";
    Common::execShell(shell);
    return true;
}
