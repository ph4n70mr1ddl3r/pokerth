#pragma once
#include <QtCore>
#include <QSoundEffect>
#include <QHash>
#include "configfile.h"

class QtAudioPlayer : public QObject
{
    Q_OBJECT
public:
    QtAudioPlayer(ConfigFile* config);
    ~QtAudioPlayer();

    void initAudio();
    void playSound(std::string audioName, int playerID);
    void closeAudio();
    void reInit();

private:
    ConfigFile *myConfig;
    QString myAppDataPath;
    bool audioEnabled;
    QHash<QString, QSharedPointer<QSoundEffect>> effects;
};