#include "qtaudioplayer.h"

QtAudioPlayer::QtAudioPlayer(ConfigFile *config)
    : myConfig(config), audioEnabled(false)
{
    myAppDataPath = QString::fromUtf8(myConfig->readConfigString("AppDataDir").c_str());
    initAudio();
}

QtAudioPlayer::~QtAudioPlayer()
{
    closeAudio();
}

void QtAudioPlayer::initAudio()
{
    if (!audioEnabled && myConfig->readConfigInt("PlaySoundEffects")) {
        // QSoundEffect benötigt Qt Multimedia im Build-System.
        audioEnabled = true;
    }
}

void QtAudioPlayer::playSound(std::string audioName, int /*playerID*/)
{
    if (!audioEnabled || !myConfig->readConfigInt("PlaySoundEffects"))
        return;

    const QString key = QString::fromStdString(audioName);
    if (!effects.contains(key)) {
        auto effect = QSharedPointer<QSoundEffect>::create();
        effect->setSource(QUrl::fromLocalFile(myAppDataPath + "sounds/default/" + key + ".wav"));
        effect->setLoopCount(1);
        // Volume 0.0 - 1.0, map your config (0-10 or 0-100) accordingly:
        float vol = myConfig->readConfigInt("SoundVolume") / 100.0f;
        if (vol > 1.0f) vol = vol/10.0f; // safety if original uses 0-10
        effect->setVolume(vol);
        effects.insert(key, effect);
        // optional: wait until loaded by checking effect->isLoaded()
    }

    auto effect = effects.value(key);
    if (effect && effect->isLoaded()) {
        effect->play();
    } else if (effect) {
        // try to play anyway once loaded
        connect(effect.data(), &QSoundEffect::loadedChanged, this, [effect]() {
            if (effect->isLoaded()) effect->play();
        });
    }
}

void QtAudioPlayer::closeAudio()
{
    for (auto e : effects)
        e->stop();
    effects.clear();
    audioEnabled = false;
}

void QtAudioPlayer::reInit()
{
    closeAudio();
    initAudio();
}