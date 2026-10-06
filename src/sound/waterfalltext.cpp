#include "waterfalltext.h"
#include "appglobal.h"
#include "configparams.h"
#include "imageviewer.h"

#include "filters.h"
#include "supportfunctions.h"
#include "drm.h"

#include "math.h"

#include <QPainter>
#include <QFileInfo>
#include <QDebug>

//#define FREQ_AMPLITUDE 16E3
#define FREQ_AMPLITUDE 6E3
#define FREQ_OFFSET 300.0
#define FREQ_MAX 2600.

// Waterfall pictures: "img:<path>" anywhere a waterfall text is accepted
// (WF Text dialog, start/end-of-transmission texts) sends a picture instead.
#define WF_IMAGE_PREFIX "img:"
#define WF_IMAGE_MAXLINES 80      // ~10 s of transmit time (each line is 0.128 s)
#define WF_IMAGE_DYNRANGE_DB 30.0 // brightness range shown on a typical waterfall
#define WF_IMAGE_BLACK 0.08       // darker than this is not transmitted at all


waterfallText::waterfallText()
{
  out=NULL;
  outFiltered=NULL;
  dataBuffer=NULL;
  txFilter=NULL;
  phr=phi=NULL;
}

waterfallText::~waterfallText()
{
  fftw_destroy_plan(plan);
  if(out) fftw_free(out);
  if(outFiltered) delete outFiltered;
  if(dataBuffer) fftw_free(dataBuffer);
}

void waterfallText::init()
{
  int i;
  double ph;
  double binSize;
  if(phr!=NULL) delete phr;
  if(phi!=NULL) delete phi;
  fftLength=TXSTRIPE*SUBSAMPLINGFACTOR/2;
  samplingrate=BASESAMPLERATE;
  binSize=(double)(BASESAMPLERATE)/((double)fftLength);
  txFilter= new wfFilter(TXSTRIPE);
  out = (fftw_complex *) fftw_malloc(sizeof(fftw_complex)*fftLength);
  dataBuffer = (fftw_complex *) fftw_malloc(sizeof(fftw_complex)*fftLength);
  outFiltered = new DSPFLOAT [fftLength];
  audioBuf = new DSPFLOAT [fftLength];
  // create the fftw plan
  addToLog("fftw_plan waterfall start",LOGFFT);
  plan = fftw_plan_dft_1d(fftLength, dataBuffer, out, FFTW_BACKWARD, FFTW_ESTIMATE);
  addToLog("fftw_plan waterfall stop",LOGFFT);
  imageWidth=(FREQ_MAX-FREQ_OFFSET)/binSize;

  //  imageWidth=200;
  startFreqIndex=(int)round(FREQ_OFFSET/binSize);

  //Chirp
  phr=new double[imageWidth];
  phi=new double[imageWidth];
  amplitude=FREQ_AMPLITUDE/sqrt(imageWidth);
  for(i=0;i<imageWidth;i++)
  {
    ph=(-M_PI/imageWidth)*i*i;
    phr[i]=amplitude*cos(ph);
    phi[i]=amplitude*sin(ph);
  }



}

double waterfallText::getDuration(QString txt)
{
  if(!txt.isNull())
    {
      setupImage(convert(txt));
    }
  return ((double)(line*3*fftLength))/(double)samplingrate;
}

void waterfallText::setText(QString txt)
{
  QString t=convert(txt);
  setupImage(t);
}


DSPFLOAT * waterfallText::nextLine()
{
  QRgb *cPtr;
  int i,freqIndex;

  if(dLine%3==0)
  {
    line--;
    if(line<0)
    {
      return NULL;
    }

    addToLog(QString("sendingline %1").arg(line),LOGSYNTHES);
    cPtr=(QRgb *)image.scanLine(line);
    for(i=0;i<fftLength;i++)
    {
      dataBuffer[i][0]=0.0;
      dataBuffer[i][1]=0.0;
    }

    for(i=0;i<imageWidth;i++)
    {
      freqIndex=i+startFreqIndex;
      double g=qGray(cPtr[i])/255.;
      if(g>WF_IMAGE_BLACK)
      {
        // waterfalls display power in dB, so map brightness logarithmically.
        // Pure white gives the same amplitude as the original on/off text.
        double gain=pow(10.,(g-1.)*WF_IMAGE_DYNRANGE_DB/20.);
        dataBuffer[freqIndex][0]= phr[i]*gain;
        dataBuffer[freqIndex][1]= phi[i]*gain;
      }
    }
    fftw_execute(plan);
    for(i=0;i<fftLength;i++)
    {
      outFiltered[i]=(DSPFLOAT) out[i][0];
    }
  }
  dLine++;
  return outFiltered;
}



bool waterfallText::isImageText(const QString &txt)
{
  return txt.trimmed().startsWith(WF_IMAGE_PREFIX,Qt::CaseInsensitive);
}

bool waterfallText::setupPicture(QString fileName)
{
  QImage src;
  dLine=0;
  fileName=fileName.trimmed();
  if(fileName.startsWith('"') && fileName.endsWith('"') && fileName.length()>1)
    fileName=fileName.mid(1,fileName.length()-2);
  if(!src.load(fileName) || src.isNull())
  {
    addToLog(QString("waterfall picture not loaded: %1").arg(fileName),LOGSYNTHES);
    return false;
  }
  // one column per FFT bin; keep the aspect ratio but limit the length
  width=imageWidth;
  height=qRound((double)src.height()*width/src.width());
  if(height<1) height=1;
  if(height>WF_IMAGE_MAXLINES) height=WF_IMAGE_MAXLINES;
  QImage scaled=src.convertToFormat(QImage::Format_ARGB32)
                   .scaled(width,height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
  image=QImage(QSize(width,height),QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::black);
  QPainter p(&image);   // flattens any transparency onto black (= no signal)
  p.drawImage(0,0,scaled);
  p.end();
  line=image.height();
  return true;
}

void waterfallText::setupImage(QString txt)
{
  if(isImageText(txt))
  {
    QString fn=txt.trimmed().mid(QString(WF_IMAGE_PREFIX).length());
    if(setupPicture(fn)) return;
    txt=QFileInfo(fn.trimmed()).fileName()+"?"; // fall back to text so the operator notices
  }
  QRect rct;
  QPainter p;
  QPen pen;
  pen.setColor(Qt::white);
  dLine=0;
  image=QImage(QSize(imageWidth,80),QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::black);
  p.begin(&image);
  p.setPen(pen);
  if(wfBold) p.setFont(QFont(wfFont,wfFontSize,QFont::Bold));
  else p.setFont(QFont(wfFont,wfFontSize,QFont::Light));
  rct=p.boundingRect(QRect(0,0,imageWidth,30),Qt::AlignTop|Qt::AlignCenter,txt);
  p.end();
  height=rct.height();
  width=imageWidth;
  image=QImage(QSize(width,height),QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::black);
  p.begin(&image);
  p.setPen(pen);
  if(wfBold) p.setFont(QFont(wfFont,wfFontSize,QFont::Bold));
  else p.setFont(QFont(wfFont,wfFontSize,QFont::Light));
  p.drawText(QRectF(0,0,width,height),Qt::AlignCenter,txt);
  p.end();
  line=image.height();
}


QString  waterfallText::convert(QString txt)
{
  mexp.clear();
  mexp.addConversion('m',myCallsign);
  mexp.addConversion('s',QString::number(lastAvgSNR,'g',2));
  mexp.addConversion('c',lastReceivedCall);

  QString t=mexp.convert(txt);
  return t;
}
