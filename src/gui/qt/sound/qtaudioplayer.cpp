/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/

#include "qtaudioplayer.h"

QtAudioPlayer::QtAudioPlayer(ConfigFile *config)
    : myConfig(config), audioEnabled(false)
{
    myAppDataPath = QString::fromUtf8(myConfig->readConfigString("AppDataDir").c_str());
    initAudio();
}

QtAudioPlayer::~QtAudioPlayer() noexcept
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