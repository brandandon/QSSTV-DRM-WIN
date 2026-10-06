/**************************************************************************
*   PortAudio sound backend for QSSTV (Windows port)                      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   Notes                                                                 *
*   - Uses the PortAudio blocking API, which maps directly onto the       *
*     read()/write() model that soundBase expects (same as PulseAudio).   *
*   - On Windows the WASAPI host API is used. Each stream is first        *
*     opened in exclusive mode so nothing else (system sounds, other      *
*     apps) can be mixed into the transmit audio and Windows does not     *
*     resample. If the device refuses exclusive mode we fall back to      *
*     shared mode and log it.                                             *
*   - Capture is mono 16 bit. If the device only offers stereo we take    *
*     the left channel.                                                   *
*   - Playback is stereo 16 bit (one quint32 per frame, as produced by    *
*     soundBase).                                                         *
***************************************************************************/
#include "soundportaudio.h"
#include "configparams.h"
#include "soundconfig.h"
#include "logging.h"

#include <QList>
#include <QPair>
#include <QThread>
#include <cstring>

#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif

typedef QPair<QString,PaDeviceIndex> paDeviceEntry;

// The host API we list and open devices on.
static PaHostApiIndex preferredHostApi()
{
#ifdef _WIN32
  PaHostApiIndex idx=Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
  if(idx>=0) return idx;
#endif
  return Pa_GetDefaultHostApi();
}

// Build the list of usable devices for one direction. Duplicate names get a
// " #2", " #3" suffix so every entry is unique and can be saved by name.
static QList<paDeviceEntry> buildDeviceList(bool isInput)
{
  QList<paDeviceEntry> list;
  PaHostApiIndex api=preferredHostApi();
  if(api<0) return list;
  int count=Pa_GetDeviceCount();
  for(int i=0;i<count;i++)
    {
      const PaDeviceInfo *info=Pa_GetDeviceInfo(i);
      if(info==nullptr || info->hostApi!=api) continue;
      if(isInput && info->maxInputChannels<1) continue;
      if(!isInput && info->maxOutputChannels<1) continue;
      QString name=QString::fromUtf8(info->name);
      QString unique=name;
      int n=2;
      bool clash=true;
      while(clash)
        {
          clash=false;
          for(const paDeviceEntry &e : list)
            {
              if(e.first==unique)
                {
                  unique=QString("%1 #%2").arg(name).arg(n++);
                  clash=true;
                  break;
                }
            }
        }
      list.append(paDeviceEntry(unique,i));
    }
  return list;
}

void getPortAudioCardList(QStringList &inputList, QStringList &outputList)
{
  inputList.clear();
  outputList.clear();
  inputList.append(PA_DEFAULT_DEVICE);
  outputList.append(PA_DEFAULT_DEVICE);
  if(Pa_Initialize()!=paNoError) return;
  for(const paDeviceEntry &e : buildDeviceList(true))  inputList.append(e.first);
  for(const paDeviceEntry &e : buildDeviceList(false)) outputList.append(e.first);
  Pa_Terminate();
}

PaDeviceIndex findPortAudioDevice(const QString &name, bool isInput)
{
  PaHostApiIndex api=preferredHostApi();
  if(name.isEmpty() || name==PA_DEFAULT_DEVICE)
    {
      if(api>=0)
        {
          const PaHostApiInfo *apiInfo=Pa_GetHostApiInfo(api);
          if(apiInfo) return isInput ? apiInfo->defaultInputDevice : apiInfo->defaultOutputDevice;
        }
      return isInput ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
    }
  for(const paDeviceEntry &e : buildDeviceList(isInput))
    {
      if(e.first==name) return e.second;
    }
  return paNoDevice;
}


soundPortAudio::soundPortAudio()
{
  captureStream=nullptr;
  playbackStream=nullptr;
  captureChannels=MONOCHANNEL;
  capExclusive=false;
  playExclusive=false;
  paInitialized=(Pa_Initialize()==paNoError);
}

soundPortAudio::~soundPortAudio()
{
  closeDevices();
  if(paInitialized) Pa_Terminate();
}

// Try to open a stream with the given channel count, first exclusive (Windows),
// then shared. Returns paNoError on success.
static PaError tryOpen(PaStream **stream,PaDeviceIndex dev,bool isInput,int channels,
                       double rate,bool &exclusive)
{
  const PaDeviceInfo *info=Pa_GetDeviceInfo(dev);
  if(info==nullptr) return paInvalidDevice;
  PaStreamParameters p;
  p.device=dev;
  p.channelCount=channels;
  p.sampleFormat=paInt16;
  p.suggestedLatency= isInput ? info->defaultHighInputLatency : info->defaultHighOutputLatency;
  p.hostApiSpecificStreamInfo=nullptr;
  PaError err;

#ifdef _WIN32
  if(Pa_GetHostApiInfo(info->hostApi)->type==paWASAPI)
    {
      PaWasapiStreamInfo wasapi;
      memset(&wasapi,0,sizeof(wasapi));
      wasapi.size=sizeof(PaWasapiStreamInfo);
      wasapi.hostApiType=paWASAPI;
      wasapi.version=1;
      wasapi.flags=paWinWasapiExclusive;
      p.hostApiSpecificStreamInfo=&wasapi;
      if(Pa_IsFormatSupported(isInput?&p:nullptr,isInput?nullptr:&p,rate)==paFormatIsSupported)
        {
          err=Pa_OpenStream(stream,isInput?&p:nullptr,isInput?nullptr:&p,rate,
                            paFramesPerBufferUnspecified,paClipOff|paDitherOff,nullptr,nullptr);
          if(err==paNoError)
            {
              exclusive=true;
              return paNoError;
            }
        }
      p.hostApiSpecificStreamInfo=nullptr; // fall back to shared mode
    }
#endif

  exclusive=false;
  err=Pa_IsFormatSupported(isInput?&p:nullptr,isInput?nullptr:&p,rate);
  if(err!=paFormatIsSupported) return err;
  return Pa_OpenStream(stream,isInput?&p:nullptr,isInput?nullptr:&p,rate,
                       paFramesPerBufferUnspecified,paClipOff|paDitherOff,nullptr,nullptr);
}

bool soundPortAudio::openCapture(PaDeviceIndex dev)
{
  PaError err;
  captureChannels=MONOCHANNEL;
  err=tryOpen(&captureStream,dev,true,MONOCHANNEL,sampleRate,capExclusive);
  if(err!=paNoError)
    {
      captureChannels=STEREOCHANNEL;
      err=tryOpen(&captureStream,dev,true,STEREOCHANNEL,sampleRate,capExclusive);
    }
  if(err!=paNoError)
    {
      captureStream=nullptr;
      errorHandler("PortAudio capture open error",Pa_GetErrorText(err));
      return false;
    }
  isStereo=(captureChannels==STEREOCHANNEL);
  err=Pa_StartStream(captureStream);
  if(err!=paNoError)
    {
      errorHandler("PortAudio capture start error",Pa_GetErrorText(err));
      return false;
    }
  addToLog(QString("PortAudio capture: %1 channel(s), %2 mode")
           .arg(captureChannels).arg(capExclusive?"exclusive":"shared"),LOGSOUND);
  return true;
}

bool soundPortAudio::openPlayback(PaDeviceIndex dev)
{
  PaError err;
  err=tryOpen(&playbackStream,dev,false,STEREOCHANNEL,sampleRate,playExclusive);
  if(err!=paNoError)
    {
      playbackStream=nullptr;
      errorHandler("PortAudio playback open error",Pa_GetErrorText(err));
      return false;
    }
  err=Pa_StartStream(playbackStream);
  if(err!=paNoError)
    {
      errorHandler("PortAudio playback start error",Pa_GetErrorText(err));
      return false;
    }
  addToLog(QString("PortAudio playback: stereo, %1 mode").arg(playExclusive?"exclusive":"shared"),LOGSOUND);
  return true;
}

bool soundPortAudio::init(int samplerate)
{
  soundDriverOK=false;
  sampleRate=samplerate;
  isStereo=false;
  closeDevices();
  if(!paInitialized)
    {
      errorHandler("PortAudio","initialization failed");
      return false;
    }
  PaDeviceIndex inDev=findPortAudioDevice(inputAudioDevice,true);
  PaDeviceIndex outDev=findPortAudioDevice(outputAudioDevice,false);
  if(inDev==paNoDevice)
    {
      errorHandler("Input device not found:",inputAudioDevice);
      return false;
    }
  if(outDev==paNoDevice)
    {
      errorHandler("Output device not found:",outputAudioDevice);
      return false;
    }
  if(!openCapture(inDev)) return false;
  if(!openPlayback(outDev)) return false;
  soundDriverOK=true;
  return true;
}

int soundPortAudio::read(int &countAvailable)
{
  if(!soundDriverOK || captureStream==nullptr) return 0;
  long avail=Pa_GetStreamReadAvailable(captureStream);
  if(avail<0)
    {
      errorHandler("PortAudio read error",Pa_GetErrorText((PaError)avail));
      return -1;
    }
  countAvailable=(int)avail;
  if(avail<DOWNSAMPLESIZE) return 0;
  PaError err=Pa_ReadStream(captureStream,tempRXBuffer,DOWNSAMPLESIZE);
  if(err!=paNoError && err!=paInputOverflowed)   // an overflow still delivers data
    {
      errorHandler("PortAudio read error",Pa_GetErrorText(err));
      return -1;
    }
  if(isStereo)
    {
      for(int i=1;i<DOWNSAMPLESIZE;i++) tempRXBuffer[i]=tempRXBuffer[2*i]; // keep left
    }
  return DOWNSAMPLESIZE;
}

int soundPortAudio::write(uint numFrames)
{
  if(!soundDriverOK || playbackStream==nullptr) return 0;
  if(numFrames==0) return 0;
  PaError err=Pa_WriteStream(playbackStream,tempTXBuffer,numFrames);
  if(err!=paNoError && err!=paOutputUnderflowed)
    {
      errorHandler("PortAudio write error",Pa_GetErrorText(err));
      return -1;
    }
  return numFrames;
}

void soundPortAudio::waitPlaybackEnd()
{
  // Let the audio already queued in the device leave before PTT is released.
  if(!soundDriverOK || playbackStream==nullptr) return;
  const PaStreamInfo *info=Pa_GetStreamInfo(playbackStream);
  if(info) QThread::msleep((unsigned long)(info->outputLatency*1000.0)+50);
}

void soundPortAudio::flushCapture()
{
  if(!soundDriverOK || captureStream==nullptr) return;
  long avail;
  while((avail=Pa_GetStreamReadAvailable(captureStream))>=DOWNSAMPLESIZE)
    {
      Pa_ReadStream(captureStream,tempRXBuffer,DOWNSAMPLESIZE);
    }
  if(avail>0) Pa_ReadStream(captureStream,tempRXBuffer,avail);
}

void soundPortAudio::flushPlayback()
{
  if(!soundDriverOK || playbackStream==nullptr) return;
  // abort discards whatever is still queued, then restart for the next transmission
  Pa_AbortStream(playbackStream);
  Pa_StartStream(playbackStream);
}

void soundPortAudio::closeDevices()
{
  if(captureStream)
    {
      Pa_AbortStream(captureStream);
      Pa_CloseStream(captureStream);
      captureStream=nullptr;
    }
  if(playbackStream)
    {
      Pa_AbortStream(playbackStream);
      Pa_CloseStream(playbackStream);
      playbackStream=nullptr;
    }
  soundDriverOK=false;
}
