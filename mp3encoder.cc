#include <sys/socket.h>
#include <stdio.h>
#include <memory.h>
#include <node_api.h>

#include "mp3encoder.h"
#include "ulaw.h"
#include <assert.h>

MP3Encoder::MP3Encoder(ulong sampleRate, uint numChannels, uint format)
    : sampleRate_(sampleRate), numChannels_(numChannels), format_(format), env_(nullptr), wrapper_(nullptr) 
{
    outputBuffer = NULL;
    inputBuffer = NULL;
}

MP3Encoder::~MP3Encoder()
{
    if (outputBuffer != NULL)
    {
        delete outputBuffer;
        outputBuffer = NULL;
    }
    if (inputBuffer != NULL)
    {
        delete inputBuffer;
        inputBuffer = NULL;
    }
    printf("lame_close\n");
    lame_close(lame);
    napi_delete_reference(env_, wrapper_);
}

void MP3Encoder::Destructor(napi_env env,
                          void* nativeObject,
                          void* /*finalize_hint*/) 
{
  reinterpret_cast<MP3Encoder*>(nativeObject)->~MP3Encoder();
}

#define DECLARE_NAPI_METHOD(name, func)                                        \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

napi_value MP3Encoder::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_property_descriptor properties[] = {
        DECLARE_NAPI_METHOD("encode", encode),
    };

    printf("MP3Encoder::Init\n");

    napi_value cons;
    status = napi_define_class(
        env, "MP3Encoder", NAPI_AUTO_LENGTH, New, nullptr, 1, properties, &cons);
    assert(status == napi_ok);

    printf("napi_define_class\n");

    // We will need the constructor `cons` later during the life cycle of the
    // addon, so we store a persistent reference to it as the instance data for
    // our addon. This will enable us to use `napi_get_instance_data` at any
    // point during the life cycle of our addon to retrieve it. We cannot simply
    // store it as a global static variable, because that will render our addon
    // unable to support Node.js worker threads and multiple contexts on a single
    // thread.
    //
    // The finalizer we pass as a lambda will be called when our addon is unloaded
    // and is responsible for releasing the persistent reference and freeing the
    // heap memory where we stored the persistent reference.
    napi_ref* constructor = new napi_ref;
    status = napi_create_reference(env, cons, 1, constructor);
    assert(status == napi_ok);

    printf("napi_create_reference\n");

    status = napi_set_instance_data(
        env,
        constructor,
        [](napi_env env, void* data, void* hint) {
            napi_ref* constructor = static_cast<napi_ref*>(data);
            napi_status status = napi_delete_reference(env, *constructor);
            assert(status == napi_ok);
            delete constructor;
        },
        nullptr);
    assert(status == napi_ok);

    printf("napi_set_instance_data\n");

    status = napi_set_named_property(env, exports, "MP3Encoder", cons);
    assert(status == napi_ok);
    return exports;
}

napi_value MP3Encoder::Constructor(napi_env env) 
{
    void* instance_data = nullptr;
    napi_status status = napi_get_instance_data(env, &instance_data);
    assert(status == napi_ok);
    napi_ref* constructor = static_cast<napi_ref*>(instance_data);

    napi_value cons;
    status = napi_get_reference_value(env, *constructor, &cons);
    assert(status == napi_ok);
    return cons;
}

const int PCM_SIZE = 8192;
const int MP3_SIZE = 8192;

napi_value MP3Encoder::New(napi_env env, napi_callback_info info) 
{
    napi_status status;

    napi_value target;
    status = napi_get_new_target(env, info, &target);
    assert(status == napi_ok);
    bool is_constructor = target != nullptr;

    if (is_constructor) 
    {
        // Invoked as constructor: `new MP3Encoder(...)`
        size_t argc = 3;
        napi_value args[3];
        napi_value jsthis;
        status = napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
        assert(status == napi_ok);

        uint32_t value = 0;
        ulong sampleRate = 0;
        uint numChannels = 0;
        uint format = 0;

        napi_valuetype valuetype;
        status = napi_typeof(env, args[0], &valuetype);
        assert(status == napi_ok);

        if (valuetype != napi_undefined) {
            status = napi_get_value_uint32(env, args[0], &value);
            assert(status == napi_ok);
            sampleRate = value;
        }

        status = napi_typeof(env, args[1], &valuetype);
        assert(status == napi_ok);
    
        if (valuetype != napi_undefined) {
            status = napi_get_value_uint32(env, args[1], &value);
            assert(status == napi_ok);
            numChannels = value;
        }

        status = napi_typeof(env, args[2], &valuetype);
        assert(status == napi_ok);
    
        if (valuetype != napi_undefined) {
            status = napi_get_value_uint32(env, args[2], &value);
            assert(status == napi_ok);
            format = value;
        }
    
        printf("sampleRate=%lu, numChannels=%u, format = %u\n", sampleRate, numChannels, format);
        MP3Encoder* obj = new MP3Encoder(sampleRate, numChannels, format);

        lame_t lame = lame_init();
        lame_set_in_samplerate(lame, sampleRate);
        lame_set_out_samplerate(lame, 16000);
        lame_set_num_channels(lame, numChannels);
        lame_set_mode(lame, STEREO);
        lame_set_VBR(lame, vbr_default);
        lame_init_params(lame);
        obj->lame = lame;
  
        obj->inputBuffer = new short[PCM_SIZE*4];
        obj->outputBuffer = new unsigned char[MP3_SIZE];

        obj->env_ = env;
        status = napi_wrap(env,
                        jsthis,
                        reinterpret_cast<void*>(obj),
                        MP3Encoder::Destructor,
                        nullptr,  // finalize_hint
                        &obj->wrapper_);
        assert(status == napi_ok);
    
        return jsthis;
    } 
    else 
    {
        // Invoked as plain function `MP3Encoder(...)`, turn into construct call.
        size_t argc_ = 1;
        napi_value args[1];
        status = napi_get_cb_info(env, info, &argc_, args, nullptr, nullptr);
        assert(status == napi_ok);
    
        const size_t argc = 1;
        napi_value argv[argc] = {args[0]};
    
        napi_value instance;
        status = napi_new_instance(env, Constructor(env), argc, argv, &instance);
        assert(status == napi_ok);
    
        return instance;
    }
}

napi_value MP3Encoder::encode(napi_env env, napi_callback_info info)
{
    napi_status status;

    size_t argc = 1;
    napi_value value;
    napi_value jsthis;
    status = napi_get_cb_info(env, info, &argc, &value, &jsthis, nullptr);
    assert(status == napi_ok);

    MP3Encoder* obj;
    status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
    assert(status == napi_ok);

    size_t in_length = 0;
    int samples = 0;
    int nbBytesEncoded = 0;
    unsigned char *iptr = NULL;
    status = napi_get_buffer_info(env, value, (void**)&iptr, &in_length);
    if (status == napi_ok)
    {
        printf("Input %ld bytes (%p) to encode\n", in_length, iptr);
        if (in_length > 0)
        {
            if (obj->format_ == FORMAT_L16)
            {
                memcpy(obj->inputBuffer, iptr, in_length);
                samples = in_length / 4;
            }
            else if (obj->format_ == FORMAT_ULAW)
            {
                for (uint i = 0; i < in_length; i++)
                    obj->inputBuffer[i*2] = obj->inputBuffer[i*2+1] = ulaw_decode[iptr[i]];
                samples = in_length;
            }
            else if (obj->format_ == FORMAT_ALAW)
            {
                for (uint i = 0; i < in_length; i++)
                    obj->inputBuffer[i*2] = obj->inputBuffer[i*2+1] = alaw_decode[iptr[i]];
                samples = in_length;
            }
            nbBytesEncoded = lame_encode_buffer_interleaved(obj->lame, obj->inputBuffer, samples, obj->outputBuffer, MP3_SIZE);
            printf("Encoded %d bytes\n", nbBytesEncoded);
        }
        else
        {
            nbBytesEncoded = lame_encode_flush_nogap(obj->lame, obj->outputBuffer, MP3_SIZE);
            //lame_init_bitstream(obj->lame);
            printf("Flush %d bytes\n", nbBytesEncoded);
        }
    }
    else
    {
        nbBytesEncoded = lame_encode_flush(obj->lame, obj->outputBuffer, MP3_SIZE);
        printf("Flush %d bytes\n", nbBytesEncoded);
    }

    napi_value buffer;
    size_t out_length = nbBytesEncoded;
    unsigned char *dptr = NULL;
    status = napi_create_buffer(env, out_length, (void**) &dptr, &buffer);
    memcpy(dptr, obj->outputBuffer, out_length);

    return buffer;
}

napi_value Init(napi_env env, napi_value exports) 
{
    return MP3Encoder::Init(env, exports);
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
