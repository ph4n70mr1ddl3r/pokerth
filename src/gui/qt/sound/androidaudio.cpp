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
#include "androidaudio.h"
#include "androidsoundeffect.h"
#include "configfile.h"

#include <QDebug>

AndroidAudio::AndroidAudio(ConfigFile *c, QObject *parent) :
	QObject(parent), mEngineObject(nullptr), mEngineEngine(nullptr), mOutputMixObject(nullptr), mSounds(), mPlayerObject(nullptr), myConfig(c), audioEnabled(false)
{
	initAudio();
}

AndroidAudio::~AndroidAudio() noexcept
{
	closeAudio();
}

void AndroidAudio::initAudio()
{
	if (!audioEnabled && myConfig->readConfigInt("PlaySoundEffects")) {
		createEngine();
		if (!mEngineObject) return; // createEngine failed
		startSoundPlayer();
		if (!mPlayerObject) return; // startSoundPlayer failed
		audioEnabled = true;
	}
}

void AndroidAudio::closeAudio()
{

	if(audioEnabled) {
		destroyEngine();

		qDeleteAll(mSounds);
		mSounds.clear();
		audioEnabled = false;
	}
}

void AndroidAudio::reInit()
{
	initAudio();
}

// create the engine and output mix objects
void AndroidAudio::createEngine()
{
	SLresult result;

	// create engine
	result = slCreateEngine(&mEngineObject, 0, nullptr, 0, nullptr, nullptr);
	if (SL_RESULT_SUCCESS != result) {
		qWarning() << "Failed to create OpenSL engine:" << result;
		mEngineObject = nullptr;
		return;
	}

	// realize the engine
	result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
	if (SL_RESULT_SUCCESS != result) {
		qWarning() << "Failed to realize OpenSL engine:" << result;
		(*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
		mEngineEngine = nullptr;
		return;
	}

	// get the engine interface, which is needed in order to create other objects
	result = (*mEngineObject)->GetInterface(mEngineObject, SL_IID_ENGINE, &mEngineEngine);
	if (SL_RESULT_SUCCESS != result) {
		qWarning() << "Failed to get OpenSL engine interface:" << result;
		(*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
		mEngineEngine = nullptr;
		return;
	}

	// create output mix
	const SLInterfaceID ids[] = {};
	const SLboolean req[] = {};
	result = (*mEngineEngine)->CreateOutputMix(mEngineEngine, &mOutputMixObject, 0, ids, req);
	if (SL_RESULT_SUCCESS != result) {
		qWarning() << "Failed to create OpenSL output mix:" << result;
		(*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
		mEngineEngine = nullptr;
		mOutputMixObject = nullptr;
		return;
	}

	// realize the output mix
	result = (*mOutputMixObject)->Realize(mOutputMixObject, SL_BOOLEAN_FALSE);
	if (SL_RESULT_SUCCESS != result) {
		qWarning() << "Failed to realize OpenSL output mix:" << result;
		(*mOutputMixObject)->Destroy(mOutputMixObject);
		mOutputMixObject = nullptr;
		(*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
		mEngineEngine = nullptr;
		return;
	}

	qDebug() << "Created Android Audio Engine";
}

void AndroidAudio::destroyEngine()
{
	if (mOutputMixObject != nullptr) {
		(*mOutputMixObject)->Destroy(mOutputMixObject);
		mOutputMixObject = nullptr;
	}

	if (mEngineObject != nullptr) {
		(*mEngineObject)->Destroy(mEngineObject);
		mEngineObject = nullptr;
		mEngineEngine = nullptr;
	}

	if (mPlayerObject != nullptr) {
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
		mPlayerPlay = nullptr;
		mPlayerQueue = nullptr;
	}

	for (auto* sound : mSounds) {
		sound->unload();
	}

	qDebug() << "Destroyed Android Audio Engine";
}

void AndroidAudio::registerSound(const QString& path, const QString& name)
{
//    qDebug() << "registerSound:" << path << name;
	if (mSounds.contains(name)) {
		delete mSounds[name];
		mSounds.remove(name);
	}
	AndroidSoundEffect *lSound = new AndroidSoundEffect(path, this);
//    qDebug() << "registerSound:created";
	mSounds[name] = lSound;

//    qDebug() << "registerSound:loading";
	lSound->load();
//    qDebug() << "registerSound:loaded";
}

void AndroidAudio::startSoundPlayer()
{
//    qDebug() << "Starting Sound Player";

	SLresult lRes;

	//Configure the sound player input/output
	SLDataLocator_AndroidSimpleBufferQueue lDataLocatorIn;
	lDataLocatorIn.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
	lDataLocatorIn.numBuffers = 1;

	//Set the data format as mono-pcm-16bit-44100
	SLDataFormat_PCM lDataFormat;
	lDataFormat.formatType = SL_DATAFORMAT_PCM;
	lDataFormat.numChannels = 2;
	lDataFormat.samplesPerSec = SL_SAMPLINGRATE_44_1;
	lDataFormat.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	lDataFormat.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	lDataFormat.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	lDataFormat.endianness = SL_BYTEORDER_LITTLEENDIAN;

	SLDataSource lDataSource;
	lDataSource.pLocator = &lDataLocatorIn;
	lDataSource.pFormat = &lDataFormat;

	SLDataLocator_OutputMix lDataLocatorOut;
	lDataLocatorOut.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	lDataLocatorOut.outputMix = mOutputMixObject;

	SLDataSink lDataSink;
	lDataSink.pLocator = &lDataLocatorOut;
	lDataSink.pFormat = nullptr;

	//Create the sound player
	const SLuint32 lSoundPlayerIIDCount = 2;
	const SLInterfaceID lSoundPlayerIIDs[] = { SL_IID_PLAY, SL_IID_BUFFERQUEUE };
	const SLboolean lSoundPlayerReqs[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

//    qDebug() << "Configured Sound Player";

	lRes = (*mEngineEngine)->CreateAudioPlayer(mEngineEngine, &mPlayerObject, &lDataSource, &lDataSink, lSoundPlayerIIDCount, lSoundPlayerIIDs, lSoundPlayerReqs);
	if (SL_RESULT_SUCCESS != lRes) {
		qWarning() << "Failed to create OpenSL audio player:" << lRes;
		mPlayerObject = nullptr;
		return;
	}

//    qDebug() << "Created Sound Player";

	lRes = (*mPlayerObject)->Realize(mPlayerObject, SL_BOOLEAN_FALSE);
	if (SL_RESULT_SUCCESS != lRes) {
		qWarning() << "Failed to realize OpenSL audio player:" << lRes;
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
		return;
	}

	qDebug() << "Realised Sound Player";
	lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_PLAY, &mPlayerPlay);
	if (SL_RESULT_SUCCESS != lRes) {
		qWarning() << "Failed to get OpenSL play interface:" << lRes;
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
		mPlayerPlay = nullptr;
		mPlayerQueue = nullptr;
		return;
	}

	lRes = (*mPlayerObject)->GetInterface(mPlayerObject, SL_IID_BUFFERQUEUE, &mPlayerQueue);
	if (SL_RESULT_SUCCESS != lRes) {
		qWarning() << "Failed to get OpenSL buffer queue interface:" << lRes;
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
		mPlayerPlay = nullptr;
		mPlayerQueue = nullptr;
		return;
	}

	lRes = (*mPlayerPlay)->SetPlayState(mPlayerPlay, SL_PLAYSTATE_PLAYING);
	if (SL_RESULT_SUCCESS != lRes) {
		qWarning() << "Failed to set OpenSL play state:" << lRes;
		(*mPlayerObject)->Destroy(mPlayerObject);
		mPlayerObject = nullptr;
		mPlayerPlay = nullptr;
		mPlayerQueue = nullptr;
		return;
	}

//    qDebug() << "Created Buffer Player";
}

void AndroidAudio::playSound(const std::string& name, int i)
{

	if(audioEnabled && myConfig->readConfigInt("PlaySoundEffects")) {

			if (!mSounds.contains(QString::fromStdString(name)))
				this->registerSound(QString(":/android/android-data/sounds/default/"+QString::fromStdString(name)+".wav"), QString::fromStdString(name));
		this->reallyPlaySound(QString::fromStdString(name));
	}
}

void AndroidAudio::reallyPlaySound(const QString& name)
{
	SLresult lRes;
	SLuint32 lPlayerState;

	AndroidSoundEffect* sound = mSounds.value(name, nullptr);

	if (!sound) {
		qDebug() << "No such sound:" << name;
		return;
	}
	if (!mPlayerObject) return;
	//Get the current state of the player
	(*mPlayerObject)->GetState(mPlayerObject, &lPlayerState);

	//If the player is realised
	if (lPlayerState == SL_OBJECT_STATE_REALIZED) {
		//Get the buffer and length of the effect
		int16_t* lBuffer = reinterpret_cast<int16_t*>(sound->mBuffer.data());
		off_t lLength = sound->mLength;

		//Remove any sound from the queue
		lRes = (*mPlayerQueue)->Clear(mPlayerQueue);
		if (SL_RESULT_SUCCESS != lRes) {
			qWarning() << "Failed to clear OpenSL buffer queue:" << lRes;
			return;
		}

		//Play the new sound
		lRes = (*mPlayerQueue)->Enqueue(mPlayerQueue, lBuffer, lLength);
		if (SL_RESULT_SUCCESS != lRes) {
			qWarning() << "Failed to enqueue sound buffer:" << lRes;
		}
	}
}
