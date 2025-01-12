#pragma once

#include <api/media_stream_interface.h>
#include <api/peer_connection_interface.h>
#include <api/data_channel_interface.h>
#include <string>
#include <thread>
#include <functional>
// WebRTC 中最常用的智能指针分别是 std::unique_ptr 和 rtc::scoped_refptr
// WebRTC（以及 Chromium）并没有使用 std::shared_ptr


class Session : 
public webrtc::PeerConnectionObserver, 
public webrtc::CreateSessionDescriptionObserver, 
public webrtc::DataChannelObserver
{

public:

private:
    // create peerConnection
    webrtc::PeerConnectionFactoryInterface *_pcFactory{nullptr};


    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

    bool enable_audio_ = false;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source_;
    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source_;

    std::shared_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer_;

    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};