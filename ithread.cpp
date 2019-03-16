#include "ithread.h"
#include "signutil.h"

IThread::IThread(){}

void IThread::run(){
    emit execPrint("正在读取IPA信息...");
    QDateTime time = QDateTime::currentDateTime();   //获取当前时间
    int timeT = time.toTime_t();
    QString tmp = "/tmp/"+QString::number(timeT,10)+"/";
    QDir tmpDir(tmp);
    if(!tmpDir.exists()){
        tmpDir.mkdir(tmp);
    }
    QFileInfo fileInfo(this->filePath);
//    //获取ipa文件名
    QString ipaName=fileInfo.fileName();
    QFile::copy(this->filePath,tmp+ipaName);
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

    QString plistPath=tmp+"Payload/*.app/Info.plist";
    QStringList matchOParams;
    matchOParams << "-c";
    matchOParams << "plutil -convert xml1 -o - "+plistPath+" | grep -A1 Exec|tail -n1 | cut -f2 -d\\> | cut -f1 -d\\<";
    QProcess *p = new QProcess;
    p->start("/bin/bash",matchOParams);
    p->waitForFinished();
    QString machOFileName = p->readAllStandardOutput().trimmed();
    qDebug() << "machOFileName:"+machOFileName;
    QString appName= machOFileName+".app";
    qDebug() << "appName:"+appName;
    QStringList bundleIdParams;
    bundleIdParams << "-c";
    bundleIdParams << "/usr/libexec/PlistBuddy -c \"Print CFBundleIdentifier\" "+plistPath;
    p->start("/bin/bash",bundleIdParams);
    p->waitForFinished();
    QString bundleId = p->readAllStandardOutput();
//    system(("rm -rf "+tmp).toLocal8Bit().data());
    //读取应用打包名称
    QStringList deployAppNameParams;
    deployAppNameParams << "-c";
    deployAppNameParams << "/usr/libexec/PlistBuddy -c 'Print :CFBundleDisplayName' "+tmp+"Payload/*.app/Info.plist";
    p->start("/bin/bash",deployAppNameParams);
    p->waitForFinished();
    QString deployAppName=p->readAllStandardOutput().trimmed();
    p->close();
    delete p;

    if(deployAppName.isEmpty()){
        deployAppName=appName.split(".")[0];
    }

    IpaInfo *ipaInfo = new IpaInfo;
    ipaInfo->appName=appName;
    ipaInfo->deployAppName=deployAppName;
    ipaInfo->bundleId=bundleId.trimmed();
    ipaInfo->ipaName=ipaName;
    ipaInfo->ipaPath=fileInfo.absolutePath();
    ipaInfo->machOFileName=machOFileName;
    ipaInfo->tmpPath=tmp;

    QString machOFilePath=tmp+"/Payload/"+appName+"/"+machOFileName;
    QStringList thirdInjectionList=SignUtil::readThirdInjection(machOFilePath);
    ipaInfo->thirdInjectionInfoList=thirdInjectionList;
    emit execPrint("IPA信息读取完成");
    emit send(ipaInfo);
}

