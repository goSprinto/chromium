// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_

#include <cstdint>
#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/net/small_message_socket.h"

namespace net {
class IOBuffer;
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {
namespace mixer_service {

// Base class for sending and receiving messages to/from the mixer service.
// Not thread-safe; all usage of a given instance must be on the same sequence.
class MixerSocket : public SmallMessageSocket {
 public:
  class Delegate {
   public:
    // Called when metadata is received from the other side of the connection.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleMetadata(const Generic& message);

    // Called when audio data is received from the other side of the connection.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleAudioData(char* data, int size, int64_t timestamp);

    // Called when audio data is received from the other side of the connection
    // using an IOBufferPool. The |buffer| reference may be held as long as
    // needed by the delegate implementation. The buffer contains the full
    // message header including size; the |data| points to the audio data, and
    // the |size| is the size of the audio data. |data| will always be
    // kAudioMessageHeaderSize bytes past the start of the buffer.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   char* data,
                                   int size,
                                   int64_t timestamp);

    // Called when the connection is lost; no further data will be sent or
    // received after OnConnectionError() is called. It is safe to delete the
    // MixerSocket inside the OnConnectionError() implementation.
    virtual void OnConnectionError() {}

   protected:
    virtual ~Delegate() = default;
  };

  MixerSocket(std::unique_ptr<net::StreamSocket> socket, Delegate* delegate);
  ~MixerSocket() override;

  // Changes the delegate.
  void SetDelegate(Delegate* delegate);

  // 16-bit type and 64-bit timestamp.
  static constexpr size_t kAudioHeaderSize = sizeof(int16_t) + sizeof(int64_t);
  // Includes additional 16-bit size field for SmallMessageSocket.
  static constexpr size_t kAudioMessageHeaderSize =
      sizeof(uint16_t) + kAudioHeaderSize;

  // Fills in the audio message header for |buffer|, so it can later be sent via
  // SendPreparedAudioBuffer(). |buffer| should have |kAudioMessageHeaderSize|
  // bytes reserved at the start of the buffer, followed by |filled_bytes| of
  // audio data.
  static void PrepareAudioBuffer(net::IOBuffer* buffer,
                                 int filled_bytes,
                                 int64_t timestamp);

  // Prepares |audio_buffer| and then sends it across the connection.
  void SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int filled_bytes,
                       int64_t timestamp);

  // Sends |audio_buffer| across the connection. |audio_buffer| should have
  // previously been prepared using PrepareAudioBuffer().
  void SendPreparedAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer);

  // Sends an arbitrary protobuf across the connection.
  void SendProto(const google::protobuf::MessageLite& message);

 private:
  // SmallMessageSocket implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, int size) override;
  bool OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer, int size) override;

  bool ParseMetadata(char* data, int size);
  bool ParseAudio(char* data, int size);
  bool ParseAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                        char* data,
                        int size);

  Delegate* delegate_;

  std::queue<scoped_refptr<net::IOBuffer>> write_queue_;

  DISALLOW_COPY_AND_ASSIGN(MixerSocket);
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
