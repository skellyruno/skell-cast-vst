#include "DirectCableOutput.h"
#include "OutputSelection.h"
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

#if !defined(_WIN32)
#error SkellCast direct Windows audio output is Windows-only.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
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
struct ChosenDevice {
    ComPtr<IMMDevice> device;
    std::wstring name;
    std::wstring id;
};

ComPtr<IMMDeviceEnumerator> makeEnumerator() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(enumerator.GetAddressOf())))) return {};
    return enumerator;
}

std::wstring endpointId(IMMDevice* device) {
    if (!device) return {};
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || !id) return {};
    std::wstring result(id);
    CoTaskMemFree(id);
    return result;
}

std::wstring deviceName(IMMDevice* device) {
    if (!device) return {};
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf())))
        return {};
    PROPVARIANT prop;
    PropVariantInit(&prop);
    const HRESULT result = properties->GetValue(PKEY_Device_FriendlyName, &prop);
    std::wstring name;
    if (SUCCEEDED(result) && prop.vt == VT_LPWSTR && prop.pwszVal)
        name = prop.pwszVal;
    PropVariantClear(&prop);
    return name;
}

std::wstring lowercase(std::wstring label) {
    std::transform(label.begin(), label.end(), label.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return label;
}

bool isCableEndpoint(const std::wstring& name) {
    const auto label = lowercase(name);
    return label.find(L"cable input") != std::wstring::npos &&
           (label.find(L"vb-audio") != std::wstring::npos ||
            label.find(L"virtual cable") != std::wstring::npos);
}

bool looksVirtual(const std::wstring& name) {
    const auto label = lowercase(name);
    return label.find(L"virtual") != std::wstring::npos ||
           label.find(L"cable") != std::wstring::npos ||
           label.find(L"voicemeeter") != std::wstring::npos ||
           label.find(L"loopback") != std::wstring::npos;
}

ChosenDevice findVbCable() {
    const auto enumerator = makeEnumerator();
    if (enumerator.Get() == nullptr) return {};
    ComPtr<IMMDeviceCollection> list;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, list.GetAddressOf())))
        return {};
    UINT count = 0;
    if (FAILED(list->GetCount(&count))) return {};
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(list->Item(i, device.GetAddressOf()))) continue;
        const auto name = deviceName(device.Get());
        if (isCableEndpoint(name))
            return { device, name, endpointId(device.Get()) };
    }
    return {};
}

// Audio session enumeration is a best-effort signal, not definitive for ASIO,
// exclusive-mode apps or sessions created concurrently. Fail CLOSED if it fails.
bool isEndpointKnownIdle(IMMDevice* endpoint) {
    ComPtr<IAudioSessionManager2> manager;
    if (FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(manager.GetAddressOf()))))
        return false;
    ComPtr<IAudioSessionEnumerator> sessions;
    if (FAILED(manager->GetSessionEnumerator(sessions.GetAddressOf()))) return false;
    int count = 0;
    if (FAILED(sessions->GetCount(&count))) return false;
    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> control;
        if (FAILED(sessions->GetSession(i, control.GetAddressOf()))) return false;
        AudioSessionState state = AudioSessionStateInactive;
        if (FAILED(control->GetState(&state))) return false;
        if (state == AudioSessionStateActive) return false;
    }
    return true;
}

// The default output (including multimedia and communications roles) is never
// eligible. A system with just one output will report NO SPARE OUTPUT rather
// than doubling the music on its speakers.
ChosenDevice findSpareDesktopDevice() {
    const auto enumerator = makeEnumerator();
    if (enumerator.Get() == nullptr) return {};

    std::set<std::wstring> excluded;
    const ERole roles[] = { eConsole, eMultimedia, eCommunications };
    for (const auto role : roles) {
        ComPtr<IMMDevice> current;
        const HRESULT result = enumerator->GetDefaultAudioEndpoint(
            eRender, role, current.GetAddressOf());
        if (SUCCEEDED(result)) {
            const auto id = endpointId(current.Get());
            if (id.empty()) return {};
            excluded.insert(id);
        } else if (result != E_NOTFOUND) {
            return {};
        }
    }
    if (excluded.empty()) return {}; // Unknown defaults: don't guess.

    ComPtr<IMMDeviceCollection> list;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, list.GetAddressOf())))
        return {};
    UINT count = 0;
    if (FAILED(list->GetCount(&count))) return {};

    std::vector<ChosenDevice> found;
    std::vector<skellcast::OutputCandidate> candidates;
    found.reserve(count);
    candidates.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(list->Item(i, device.GetAddressOf()))) continue;
        const auto id = endpointId(device.Get());
        const auto name = deviceName(device.Get());
        if (id.empty() || name.empty()) continue;
        const bool isDefault = excluded.count(id) != 0;
        // Query only non-default endpoints: Windows audio sessions can be
        // inaccurate for exclusive/ASIO drivers, so never trust activity alone.
        const bool idle = !isDefault && isEndpointKnownIdle(device.Get());
        candidates.push_back({ isDefault, !idle, looksVirtual(name), idle });
        found.push_back({ device, name, id });
    }

    const auto chosen = skellcast::selectSpareOutput(candidates.data(), candidates.size());
    return chosen >= 0 ? found[static_cast<std::size_t>(chosen)] : ChosenDevice {};
}

// FL Studio mix format. Windows shared mode converts it when supported.
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

bool isCurrentlyDefault(const std::wstring& id) {
    const auto enumerator = makeEnumerator();
    if (enumerator.Get() == nullptr) return true; // fail closed
    const ERole roles[] = { eConsole, eMultimedia, eCommunications };
    for (const auto role : roles) {
        ComPtr<IMMDevice> current;
        const auto hr = enumerator->GetDefaultAudioEndpoint(eRender, role, current.GetAddressOf());
        if (SUCCEEDED(hr)) {
            const auto currentId = endpointId(current.Get());
            if (currentId.empty() || currentId == id) return true;
        } else if (hr != E_NOTFOUND) return true;
    }
    return false;
}
} // namespace

DirectCableOutput::DirectCableOutput(std::atomic<float>& sendParam,
                                     std::atomic<float>& desktopParam)
    : send_(sendParam), desktop_(desktopParam) {}
DirectCableOutput::~DirectCableOutput() { stop(); }

DirectCableOutput::DeviceStatus DirectCableOutput::deviceStatus() const {
    const std::lock_guard<std::mutex> lock(statusMutex_);
    return deviceStatus_;
}
void DirectCableOutput::setDeviceStatus(DeviceState state, std::wstring name) {
    const std::lock_guard<std::mutex> lock(statusMutex_);
    deviceStatus_.state = state;
    deviceStatus_.name = std::move(name);
}

void DirectCableOutput::start(std::uint32_t rate) {
    stop();
    sampleRate_.store(rate, std::memory_order_release);
    quit_.store(false, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
}
void DirectCableOutput::stop() {
    quit_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    setDeviceStatus(DeviceState::Off);
}
void DirectCableOutput::push(const float* left, const float* right, std::size_t count) noexcept {
    const auto accepted = fifo_.push(left, right, count);
    if (accepted != count) overflows_.fetch_add(count - accepted, std::memory_order_relaxed);
}
void DirectCableOutput::run() noexcept {
    const ComInit com;
    if (!com.ready()) {
        setDeviceStatus(DeviceState::Error, L"Windows audio unavailable");
        return;
    }
    fifo_.clearConsumer();
    while (!quit_.load(std::memory_order_acquire)) {
        if (!sharing()) {
            setDeviceStatus(DeviceState::Off);
            fifo_.clearConsumer();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        setDeviceStatus(DeviceState::Searching);
        // All endpoint selection and Windows API calls occur on the worker.
        if (!runOutputStream(selectedOutput()))
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        fifo_.clearConsumer();
    }
    setDeviceStatus(DeviceState::Off);
}

bool DirectCableOutput::runOutputStream(skellcast::OutputTarget target) noexcept {
    const auto chosen = target == skellcast::OutputTarget::Desktop
        ? findSpareDesktopDevice() : findVbCable();
    if (chosen.device.Get() == nullptr) {
        setDeviceStatus(DeviceState::NotFound, target == skellcast::OutputTarget::Desktop
            ? L"No spare output device" : L"VB-CABLE not found");
        return false;
    }

    ComPtr<IAudioClient> client;
    if (FAILED(chosen.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(client.GetAddressOf())))) {
        setDeviceStatus(DeviceState::Error, L"Cannot open output");
        return false;
    }

    const auto runningRate = sampleRate_.load(std::memory_order_acquire);
    auto format = floatStereoFormat(runningRate);
    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY |
                        AUDCLNT_STREAMFLAGS_NOPERSIST;
    constexpr REFERENCE_TIME bufferDuration = 30 * 10000;
    if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, bufferDuration,
                                  0, &format, nullptr))) {
        setDeviceStatus(DeviceState::Error, L"Unsupported device format");
        return false;
    }
    EventHandle event;
    if (!event.handle || FAILED(client->SetEventHandle(event.handle))) {
        setDeviceStatus(DeviceState::Error, L"Cannot start output");
        return false;
    }
    ComPtr<IAudioRenderClient> render;
    if (FAILED(client->GetService(__uuidof(IAudioRenderClient),
                                  reinterpret_cast<void**>(render.GetAddressOf())))) {
        setDeviceStatus(DeviceState::Error, L"Cannot open audio render");
        return false;
    }
    UINT32 bufferFrames = 0;
    if (FAILED(client->GetBufferSize(&bufferFrames)) || bufferFrames == 0) {
        setDeviceStatus(DeviceState::Error, L"Invalid output buffer");
        return false;
    }
    BYTE* data = nullptr;
    if (FAILED(render->GetBuffer(bufferFrames, &data)) ||
        FAILED(render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT))) {
        setDeviceStatus(DeviceState::Error, L"Cannot prefill output");
        return false;
    }
    if (FAILED(client->Start())) {
        setDeviceStatus(DeviceState::Error, L"Cannot start audio");
        return false;
    }

    setDeviceStatus(DeviceState::Connected, chosen.name);
    bool healthy = true;
    unsigned timeouts = 0;
    auto nextDeviceCheck = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!quit_.load(std::memory_order_acquire) && sharing() &&
           selectedOutput() == target &&
           sampleRate_.load(std::memory_order_acquire) == runningRate) {
        if (target == skellcast::OutputTarget::Desktop &&
            std::chrono::steady_clock::now() >= nextDeviceCheck) {
            // Stop if this spare output becomes the Windows default. The
            // worker will search for a DIFFERENT spare endpoint instead.
            if (isCurrentlyDefault(chosen.id)) {
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
        if (FAILED(client->GetCurrentPadding(&padding)) || padding > bufferFrames) {
            healthy = false;
            break;
        }
        const UINT32 count = bufferFrames - padding;
        if (count == 0) continue;
        data = nullptr;
        if (FAILED(render->GetBuffer(count, &data)) || !data) {
            healthy = false;
            break;
        }
        auto* samples = reinterpret_cast<float*>(data);
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
    setDeviceStatus(DeviceState::Searching);
    return healthy;
}
