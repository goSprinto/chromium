// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/serial.mojom-blink.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace base {
class UnguessableToken;
}

namespace blink {

class ReadableStream;
class ScriptPromiseResolver;
class ScriptState;
class Serial;
class SerialOptions;
class SerialOutputSignals;
class SerialPortUnderlyingSink;
class SerialPortUnderlyingSource;
class WritableStream;

class SerialPort final : public ScriptWrappable,
                         public device::mojom::blink::SerialPortClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(SerialPort, Dispose);

 public:
  explicit SerialPort(Serial* parent, mojom::blink::SerialPortInfoPtr info);
  ~SerialPort() override;

  // Web-exposed functions
  ScriptPromise open(ScriptState*,
                     const SerialOptions* options,
                     ExceptionState&);
  ReadableStream* readable(ScriptState*, ExceptionState&);
  WritableStream* writable(ScriptState*, ExceptionState&);
  ScriptPromise getSignals(ScriptState*, ExceptionState&);
  ScriptPromise setSignals(ScriptState*,
                           const SerialOutputSignals*,
                           ExceptionState&);
  void close();

  const base::UnguessableToken& token() const { return info_->token; }

  void UnderlyingSourceClosed();
  void UnderlyingSinkClosed();

  void ContextDestroyed();
  void Trace(Visitor*) override;
  void Dispose();

  // SerialPortClient
  void OnReadError(device::mojom::blink::SerialReceiveError) override;
  void OnSendError(device::mojom::blink::SerialSendError) override;

 private:
  bool CreateDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                      mojo::ScopedDataPipeConsumerHandle* consumer);
  void OnConnectionError();
  void OnOpen(mojo::ScopedDataPipeConsumerHandle,
              mojo::ScopedDataPipeProducerHandle,
              mojo::PendingReceiver<device::mojom::blink::SerialPortClient>,
              bool success);
  void InitializeReadableStream(ScriptState*,
                                mojo::ScopedDataPipeConsumerHandle);
  void InitializeWritableStream(ScriptState*,
                                mojo::ScopedDataPipeProducerHandle);
  void OnGetSignals(ScriptPromiseResolver*,
                    device::mojom::blink::SerialPortControlSignalsPtr);
  void OnSetSignals(ScriptPromiseResolver*, bool success);

  mojom::blink::SerialPortInfoPtr info_;
  Member<Serial> parent_;

  uint32_t buffer_size_ = 0;
  mojo::Remote<device::mojom::blink::SerialPort> port_;
  mojo::Receiver<device::mojom::blink::SerialPortClient> client_receiver_{this};

  Member<ReadableStream> readable_;
  Member<SerialPortUnderlyingSource> underlying_source_;
  Member<WritableStream> writable_;
  Member<SerialPortUnderlyingSink> underlying_sink_;

  // Resolver for the Promise returned by open().
  Member<ScriptPromiseResolver> open_resolver_;
  // Resolvers for the Promises returned by getSignals() and setSignals() to
  // reject them on Mojo connection failure.
  HeapHashSet<Member<ScriptPromiseResolver>> signal_resolvers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
