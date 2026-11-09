#include "DirectCableOutput.h"
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <string>

#if !defined(_WIN32)
#error SkellCast direct Windows cable output is Windows-only.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propidl.h>
#include <propsys.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
namespace {
struct ComInit {
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComInit() { if (SUCCEEDED(result)) CoUninitialize(); }
    bool ready() const { return SUCCEEDED(result); }
};
struct EventHandle {
    HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ~EventHandle() { if (handle) CloseHandle(handle); }
};

bool isCableEndpoint(const wchar_t* name) {
    if (!name) return false;
    std::wstring label(name);
    std::transform(label.begin(), label.end(), label.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    // Never guess a speaker/headphone endpoint. Only render into VB-CABLE's input.
    return label.find(L"cable input") != std::wstring::npos &&
           (label.find(L"vb-audio") != std::wstring::npos ||
            label.find(L"virtual cable") != std::wstring::npos);
}

ComPtr<IMMDevice> findVbCable() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(enumerator.GetAddressOf())))) return {};
    ComPtr<IMMDeviceCollection> list;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, list.GetAddressOf()))) return {};
    UINT count = 0;
    if (FAILED(list->GetCount(&count))) return {};
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(list->Item(i, device.GetAddressOf()))) continue;
        ComPtr<IPropertyStore> properties;
        if (FAILED(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf()))) continue;
        PROPVARIANT name;
        PropVariantInit(&name);
        const HRESULT status = properties->GetValue(PKEY_Device_FriendlyName, &name);
        const bool match = SUCCEEDED(status) && name.vt == VT_LPWSTR && isCableEndpoint(name.pwszVal);
        PropVariantClear(&name);
        if (match) return device;
    }
    return {};
}

// Select Windows' standard default playback device, not its default communications device.
ComPtr<IMMDevice> findDesktopEndpoint() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(enumerator.GetAddressOf())))) return {};
    ComPtr<IMMDevice> output;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, output.GetAddressOf())))
        return {};
    return output;
}

std::wstring endpointId(IMMDevice* device) {
    if (!device) return {};
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || !id) return {};
    std::wstring result(id);
    CoTaskMemFree(id);
    return result;
}

// Format is the FL Studio stream: Windows shared-mode does any necessary SRC/matrixing.
WAVEFORMATEX floatStereoFormat(std::uint32_t rate) {
    WAVEFORMATEX format {};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 2;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = 32;
    format.nBlockAlign = static_cast<WORD>(2 * sizeof(float));
    format.nAvgBytesPerSec = rate * format.nBlockAlign;
    return format;
}
} // namespace

DirectCableOutput::DirectCableOutput(std::atomic<float>& sendParam,
                                     std::atomic<float>& desktopParam)
    : send_(sendParam), desktop_(desktopParam) {}
DirectCableOutput::~DirectCableOutput() { stop(); }
void DirectCableOutput::start(std::uint32_t rate) {
    stop();
    sampleRate_.store(rate, std::memory_order_release);
    quit_.store(false, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
}
void DirectCableOutput::stop() {
    quit_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
}
void DirectCableOutput::push(const float* left, const float* right, std::size_t count) noexcept {
    const auto accepted = fifo_.push(left, right, count);
    if (accepted != count) overflows_.fetch_add(count - accepted, std::memory_order_relaxed);
}
void DirectCableOutput::run() noexcept {
    const ComInit com;
    if (!com.ready()) return;
    fifo_.clearConsumer(); // Never replay queued audio from a previous FL transport session.
    while (!quit_.load(std::memory_order_acquire)) {
        if (!sharing()) {
            fifo_.clearConsumer();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        // Device discovery and switching happen on this worker, never in FL's audio callback.
        if (!runOutputStream(selectedOutput()))
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        fifo_.clearConsumer();
    }
}
bool DirectCableOutput::runOutputStream(skellcast::OutputTarget target) noexcept {
    const auto endpoint = target == skellcast::OutputTarget::Desktop
        ? findDesktopEndpoint() : findVbCable();
    if (!endpoint) return false;
    const auto activeEndpointId = target == skellcast::OutputTarget::Desktop
        ? endpointId(endpoint.Get()) : std::wstring {};
    ComPtr<IAudioClient> client;
    if (FAILED(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(client.GetAddressOf())))) return false;

    const auto runningRate = sampleRate_.load(std::memory_order_acquire);
    auto format = floatStereoFormat(runningRate);
    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY |
                        AUDCLNT_STREAMFLAGS_NOPERSIST;
    // 30 ms shared-mode output buffer (actual event period is determined by Windows).
    constexpr REFERENCE_TIME bufferDuration = 30 * 10000;
    if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, bufferDuration,
                                  0, &format, nullptr))) return false;
    EventHandle event;
    if (!event.handle || FAILED(client->SetEventHandle(event.handle))) return false;
    ComPtr<IAudioRenderClient> render;
    if (FAILED(client->GetService(__uuidof(IAudioRenderClient),
                                  reinterpret_cast<void**>(render.GetAddressOf())))) return false;
    UINT32 bufferFrames = 0;
    if (FAILED(client->GetBufferSize(&bufferFrames)) || bufferFrames == 0) return false;
    // Prefill with silence. This avoids consuming frames ahead of the shared engine clock.
    BYTE* data = nullptr;
    if (FAILED(render->GetBuffer(bufferFrames, &data))) return false;
    if (FAILED(render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT))) return false;
    if (FAILED(client->Start())) return false;

    bool healthy = true;
    unsigned timeouts = 0;
    auto nextDeviceCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!quit_.load(std::memory_order_acquire) && sharing() &&
           selectedOutput() == target &&
           sampleRate_.load(std::memory_order_acquire) == runningRate) {
        // Follow a changed Windows default device instead of continuing on old speakers.
        // This check runs only on the WASAPI worker thread, at most once per second.
        if (target == skellcast::OutputTarget::Desktop &&
            std::chrono::steady_clock::now() >= nextDeviceCheck) {
            const auto selected = findDesktopEndpoint();
            if (!selected || endpointId(selected.Get()) != activeEndpointId) {
                healthy = false;
                break;
            }
            nextDeviceCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
        const DWORD wait = WaitForSingleObject(event.handle, 100);
        if (wait != WAIT_OBJECT_0) {
            if (wait != WAIT_TIMEOUT || ++timeouts >= 5) { healthy = false; break; }
            continue;
        }
        timeouts = 0;
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding)) || padding > bufferFrames) { healthy = false; break; }
        const UINT32 count = bufferFrames - padding;
        if (count == 0) continue;
        data = nullptr;
        if (FAILED(render->GetBuffer(count, &data)) || !data) { healthy = false; break; }
        auto* samples = reinterpret_cast<float*>(data);
        // Cap queued latency at about 100 ms. Discard old frames rather than lagging far behind FL.
        const auto queued = fifo_.available();
        const auto maxLatency = static_cast<std::size_t>(runningRate / 10);
        if (queued > maxLatency)
            fifo_.discard(queued - static_cast<std::size_t>(runningRate / 20));
        const auto actual = sharing() ? fifo_.popInterleaved(samples, count) : 0;
        std::fill(samples + actual * 2, samples + count * 2, 0.0f);
        if (FAILED(render->ReleaseBuffer(count, 0))) { healthy = false; break; }
    }
    client->Stop();
    client->Reset();
    return healthy;
}
