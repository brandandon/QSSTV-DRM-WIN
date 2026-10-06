/**************************************************************************
*   PortAudio sound backend for QSSTV (Windows port)                      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/
#ifndef SOUNDPORTAUDIO_H
#define SOUNDPORTAUDIO_H

#include "soundbase.h"
#include <QStringList>
#include <portaudio.h>

// Name shown in the device lists for "let the system pick"
#define PA_DEFAULT_DEVICE "default"

class soundPortAudio : public soundBase
{
public:
  soundPortAudio();
  ~soundPortAudio();
  bool init(int samplerate);
  int read(int &countAvailable);
  int write(uint numFrames);

  // true when the stream was opened in WASAPI exclusive mode (Windows only)
  bool captureExclusive() const {return capExclusive;}
  bool playbackExclusive() const {return playExclusive;}

protected:
  void flushCapture();
  void flushPlayback();
  void closeDevices();
  void waitPlaybackEnd();

private:
  bool openCapture(PaDeviceIndex dev);
  bool openPlayback(PaDeviceIndex dev);
  PaStream *captureStream;
  PaStream *playbackStream;
  int captureChannels;
  bool capExclusive;
  bool playExclusive;
  bool paInitialized;
};

// Fill the device pick lists shown in the sound configuration dialog.
// On Windows only WASAPI devices are listed (one entry per physical device).
void getPortAudioCardList(QStringList &inputList, QStringList &outputList);

// Look up a device by the name saved in the settings. Returns paNoDevice if not found.
PaDeviceIndex findPortAudioDevice(const QString &name, bool isInput);

#endif // SOUNDPORTAUDIO_H
