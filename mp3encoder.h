#ifndef AAC_ENCODEROBJECT_H_
#define AAC_ENCODEROBJECT_H_

#include <node_api.h>
#include <lame.h>

enum {
    FORMAT_L16 = 0,
    FORMAT_ULAW = 1,
    FORMAT_ALAW = 2
};

class MP3Encoder {
 public:
  static napi_value Init(napi_env env, napi_value exports);
  static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);

 private:
  explicit MP3Encoder(ulong sampleRate_ = 8000, uint numChannels_ = 2, uint format_ = 0);
  ~MP3Encoder();

  static napi_value New(napi_env env, napi_callback_info info);
  static napi_value encode(napi_env env, napi_callback_info info);
  static inline napi_value Constructor(napi_env env);

  ulong sampleRate_;
  uint numChannels_;
  uint format_;
  ulong inputSamples;
  ulong maxOutputBytes;
  lame_t lame;
  unsigned char *outputBuffer;
  short *inputBuffer;

  napi_env env_;
  napi_ref wrapper_;
};

#endif  // AAC_ENCODEROBJECT_H_
