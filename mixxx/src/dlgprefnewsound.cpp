/**
 * @file dlgprefnewsound.cpp
 * @author Bill Good <bkgood at gmail dot com>
 * @date 20100625
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDebug>
#include "dlgprefnewsound.h"
#include "dlgprefnewsounditem.h"
#include "soundmanager.h"
#include "sounddevice.h"

/**
 * Construct a new sound preferences pane. Initializes and populates all the
 * all the controls to the values obtained from SoundManager.
 */
DlgPrefNewSound::DlgPrefNewSound(QWidget *parent, SoundManager *soundManager,
        ConfigObject<ConfigValue> *config)
    : QWidget(parent)
    , m_pSoundManager(soundManager)
    , m_pConfig(config)
    , m_settingsModified(false)
    , m_api(soundManager->getHostAPI()) {
    setupUi(this);

    connect(m_pSoundManager, SIGNAL(devicesUpdated()),
            this, SLOT(refreshDevices()));

    applyButton->setEnabled(false);
    connect(applyButton, SIGNAL(clicked()),
            this, SLOT(slotApply()));

    apiComboBox->clear();
    apiComboBox->addItem("None", "None");
    foreach (QString api, m_pSoundManager->getHostAPIList()) {
        apiComboBox->addItem(api, api);
    }
    connect(apiComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(apiChanged(int)));

    sampleRateComboBox->clear();
    foreach (unsigned int srate, m_pSoundManager->getSampleRates()) {
        sampleRateComboBox->addItem(QString("%1 Hz").arg(srate), srate);
    }
    sampleRateComboBox->setCurrentIndex(0);
    updateLatencies(0); // take this away when the config stuff is implemented
    connect(sampleRateComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(sampleRateChanged(int)));
    connect(sampleRateComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updateLatencies(int)));
    connect(latencyComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(latencyChanged(int)));
    initializePaths();
//    loadSettings();

    connect(apiComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(settingChanged()));
    connect(sampleRateComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(settingChanged()));
    connect(latencyComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(settingChanged()));
}

DlgPrefNewSound::~DlgPrefNewSound() {

}

/**
 * Slot called when the preferences dialog is opened.
 */
void DlgPrefNewSound::slotUpdate() {
}

/**
 * Slot called when the Apply or OK button is pressed.
 */
void DlgPrefNewSound::slotApply() {
    if (!m_settingsModified) {
        return;
    }
    emit(writePaths(m_config));
    m_pSoundManager->setConfig(m_config); // setConfig will call setupDevices if necessary
    m_settingsModified = false;
    applyButton->setEnabled(false);
}

/**
 * Initializes (and creates) all the path items. Each path item widget allows
 * the user to input a sound device name and channel number given a description
 * of what will be done with that info. Inputs and outputs are grouped by tab,
 * and each path item has an identifier (Master, Headphones, ...) and an index,
 * if necessary.
 */
void DlgPrefNewSound::initializePaths() {
    QList<DlgPrefNewSoundItem*> items;
    foreach (AudioPath::AudioPathType type, AudioSource::getSupportedTypes()) {
        DlgPrefNewSoundItem *toInsert;
        if (AudioPath::isIndexable(type)) {
            for (unsigned int i = 0; i < NUM_DECKS; ++i) {
                toInsert = new DlgPrefNewSoundItem(outputScrollAreaContents, type,
                        m_outputDevices, false, i);
                connect(this, SIGNAL(refreshOutputDevices(QList<SoundDevice*>&)),
                        toInsert, SLOT(refreshDevices(QList<SoundDevice*>&)));
                outputVLayout->insertWidget(outputVLayout->count() - 1, toInsert);
                items.append(toInsert);
            }
        } else {
            toInsert = new DlgPrefNewSoundItem(outputScrollAreaContents, type,
                m_outputDevices, false);
            connect(this, SIGNAL(refreshOutputDevices(QList<SoundDevice*>&)),
                    toInsert, SLOT(refreshDevices(QList<SoundDevice*>&)));
            outputVLayout->insertWidget(outputVLayout->count() - 1, toInsert);
            items.append(toInsert);
        }
    }
    foreach (AudioPath::AudioPathType type, AudioReceiver::getSupportedTypes()) {
        DlgPrefNewSoundItem *toInsert;
        if (AudioPath::isIndexable(type)) {
            for (unsigned int i = 0; i < NUM_DECKS; ++i) {
                toInsert = new DlgPrefNewSoundItem(inputScrollAreaContents, type,
                        m_inputDevices, true, i);
                connect(this, SIGNAL(refreshInputDevices(QList<SoundDevice*>&)),
                        toInsert, SLOT(refreshDevices(QList<SoundDevice*>&)));
                inputVLayout->insertWidget(inputVLayout->count() - 1, toInsert);
                items.append(toInsert);
            }
        } else {
            toInsert = new DlgPrefNewSoundItem(inputScrollAreaContents, type,
                m_inputDevices, true);
            connect(this, SIGNAL(refreshInputDevices(QList<SoundDevice*>&)),
                    toInsert, SLOT(refreshDevices(QList<SoundDevice*>&)));
            inputVLayout->insertWidget(inputVLayout->count() - 1, toInsert);
            items.append(toInsert);
        }
        foreach (DlgPrefNewSoundItem *item, items) {
            connect(item, SIGNAL(settingChanged()),
                    this, SLOT(settingChanged()));
            connect(this, SIGNAL(writePaths(SoundManagerConfig&)),
                    item, SLOT(writePath(SoundManagerConfig&)));
        }
    }
}

/**
 *
 */
void DlgPrefNewSound::loadSettings() {
    m_config = m_pSoundManager->getConfig();

}

/**
 * Slots called when the user selects a different API, or the
 * software changes it programatically (for instance, when it
 * loads a value from SoundManager). Refreshes the device lists
 * for the new API and pushes those to the path items.
 */
void DlgPrefNewSound::apiChanged(int index) {
    m_api = apiComboBox->itemData(index).toString();
    refreshDevices();
    // JACK sets its own latency
    if (m_api == MIXXX_PORTAUDIO_JACK_STRING) {
        latencyLabel->setEnabled(false);
        latencyComboBox->setEnabled(false);
    } else {
        latencyLabel->setEnabled(true);
        latencyComboBox->setEnabled(true);
    }
}

/**
 * Slot called when the sample rate combo box changes to update the
 * sample rate in the config.
 */
void DlgPrefNewSound::sampleRateChanged(int index) {
    m_config.setSampleRate(
            sampleRateComboBox->itemData(index).toUInt());
}

/**
 * Slot called when the latency combo box is chance to update the
 * latency in the config.
 */
void DlgPrefNewSound::latencyChanged(int index) {
    m_config.setLatency(
            latencyComboBox->itemData(index).toUInt());
}

/**
 * Slot called whenever the selected sample rate is changed. Populates the
 * latency input box with MAX_LATENCY values, starting at 1ms, representing
 * a number of frames per buffer, which will always be a power of 2.
 */
void DlgPrefNewSound::updateLatencies(int sampleRateIndex) {
    double sampleRate = sampleRateComboBox->itemData(sampleRateIndex).toDouble();
    if (sampleRate == 0.0) {
        sampleRateComboBox->setCurrentIndex(0); // hope this doesn't recurse!
        return;
    }
    unsigned int framesPerBuffer = 1; // start this at 0 and inf loop happens
    // we don't want to display any sub-1ms latencies (well maybe we do but I
    // don't right now!), so we iterate over all the buffer sizes until we
    // find the first that gives us a latency >= 1 ms -- bkgood
    for (; framesPerBuffer / sampleRate * 1000 < 1.0f; framesPerBuffer *= 2);
    latencyComboBox->clear();
    for (unsigned int i = 0; i < MAX_LATENCY; ++i) {
        unsigned int latency = framesPerBuffer / sampleRate * 1000;
        latencyComboBox->addItem(QString("%1 ms").arg(latency), framesPerBuffer);
        framesPerBuffer *= 2;
    }
    // set it to the max, let the user dig if they need better latency. better
    // than having a user get the pops on first use and thinking poorly of mixxx
    // because of it -- bkgood
    latencyComboBox->setCurrentIndex(latencyComboBox->count() - 1);
}

/**
 * Slot called when device pointers go bad to refresh them all.
 */
void DlgPrefNewSound::refreshDevices() {
    if (m_api == "None") {
        m_outputDevices.clear();
        m_inputDevices.clear();
    } else {
        m_outputDevices =
            m_pSoundManager->getDeviceList(m_api, true, false);
        m_inputDevices =
            m_pSoundManager->getDeviceList(m_api, false, true);
    }
    emit(refreshOutputDevices(m_outputDevices));
    emit(refreshInputDevices(m_inputDevices));
}

/**
 * Called when any of the combo boxes in this dialog are changed. Enables the
 * apply button and marks that settings have been changed so that slotApply
 * knows to apply them.
 */
void DlgPrefNewSound::settingChanged() {
    m_settingsModified = true;
    if (!applyButton->isEnabled()) {
        applyButton->setEnabled(true);
    }
}
