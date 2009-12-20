#include "StdAfx.h"
#include "DngDecoderSlices.h"
/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

    http://www.klauspost.com
*/

namespace RawSpeed {

void *DecodeThread(void *_this) {
  DngDecoderThread* me = (DngDecoderThread*)_this;
  DngDecoderSlices* parent = me->parent;
  try {
    parent->decodeSlice(me);
  } catch (...) {
    parent->setError("DNGDEcodeThread: Caught exception.");
  }
  pthread_exit(NULL);
  return NULL;
}


DngDecoderSlices::DngDecoderSlices(FileMap* file, RawImage img) :
    mFile(file), mRaw(img) {
  mFixLjpeg = false;
#ifdef WIN32
  nThreads = pthread_num_processors_np();
#else
  nThreads = 2; // FIXME: Port this to unix
#endif
}

DngDecoderSlices::~DngDecoderSlices(void) {
}

void DngDecoderSlices::addSlice(DngSliceElement slice) {
  slices.push(slice);
}

void DngDecoderSlices::startDecoding() {
  // Create threads

  int slicesPerThread = ((int)slices.size() + nThreads - 1) / nThreads;
//  decodedSlices = 0;
  pthread_attr_t attr;
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_mutex_init(&errMutex, NULL);

  for (guint i = 0; i < nThreads; i++) {
    DngDecoderThread* t = new DngDecoderThread();
    for (int j = 0; j < slicesPerThread ; j++) {
      if (!slices.empty()) {
        t->slices.push(slices.front());
        slices.pop();
      }
    }
    t->parent = this;
    pthread_create(&t->threadid, &attr, DecodeThread, t);
    threads.push_back(t);
  }
  pthread_attr_destroy(&attr);

  void *status;
  for (guint i = 0; i < nThreads; i++) {
    pthread_join(threads[i]->threadid, &status);
    delete(threads[i]);
  }
  pthread_mutex_destroy(&errMutex);

}

void DngDecoderSlices::decodeSlice(DngDecoderThread* t) {
  while (!t->slices.empty()) {
    LJpegPlain l(mFile, mRaw);
    l.mDNGCompatible = mFixLjpeg;
    l.mUseBigtable = false;
    DngSliceElement e = t->slices.front();
    t->slices.pop();
    try {
      l.startDecoder(e.byteOffset, e.byteCount, e.offX, e.offY);
    } catch (RawDecoderException err) {
      setError(err.what());
    } catch (IOException err) {
      setError("DngDecoderSlices::decodeSlice: IO error occurred, probably attempted to read past end of file.");
    }
  }
}

int DngDecoderSlices::size() {
  return (int)slices.size();
}

void DngDecoderSlices::setError( const char* err )
{
  pthread_mutex_lock(&errMutex);
  errors.push_back(_strdup(err));
  pthread_mutex_unlock(&errMutex);
}

} // namespace RawSpeed
