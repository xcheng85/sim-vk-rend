# All about WebRTC

## Fundamentals

### 1. p2p-hole-punching (NAT)

大部分情况下其中一方或者双方都不是公网地址，而是隐藏在 NAT（Network Address Translation，网络地址转换）之后的内网地址，此时要建立连接，就得使用某种能绕过 NAT 的打洞技术

公网地址和内网地址的映射规则。NAT 通常是个路由器

为了能绕过 NAT 的限制，我们需要借助一台公网上的服务器 S 做地址转发





### 2.  ICE 的交互过程

对于不同的 NAT 类型，我们需要借助 ICE（Interactive Connectivity Establishment，交互式连接建立）框架使用不同的方式进行打洞，这个框架能让两端能够互相找到对方并建立连接


TCP 直接连接时，通过 HTTP 端口或 HTTPS 端口。
UDP 直连时，使用 STUN（Session Traversal Utilities for NAT）服务器做地址转发。
间接连接均使用 TURN（Traversal Using Relays around NAT）服务器做流量中继。

 STUN 协议 RFC 3489
 当 STUN 判断 NAT 为对称型时，就会交由 TURN 处理。

### 3. SDP 全称 Session Description Protocol，即会话描述协议


包含了 WebRTC 建立连接所需的 ICE 服务器信息、音视频编码信息等，而开发者可以使用 WebSocket 等传输协议将其发送到信令服务器



v=0
o=jdoe 2890844526 2890842807 IN IP4 10.47.16.5
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.example.com/seminars/sdp.pdf
e=j.doe@example.com (Jane Doe)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 49170 RTP/AVP 0
m=video 51372 RTP/AVP 99
a=rtpmap:99 h263-1998/90000


### 4. signaling server (信令服务器)

WebRTC 设备之间建立连接先需要获得彼此的 SDP 
第三方服务器交换彼此的 SDP

ICE 服务器可以和信令服务器是同一个服务，也可以是分别独立的服务


### 5. 交换过程 (negotiation) == two peers 设置 SDP

会话发起方(vulkan 3d app): 

    调用生成 offerSdp 并发送到信令服务器: connection_->CreateOffer(create_session_observer_, options);
   
    SetLocalDescription(): //connection_->SetLocalDescription(set_offer_observer_, desc);
    connection_->SetRemoteDescription(set_answer_observer_, desc);

应答方 (html):

    在收到信令服务器消息后被调用生成 answerSdp，然后也发送回信令服务器:  const answer = await this.connection.createAnswer();


当 Peer Initiator 收到 answerSdp 之后便会开始 ICE 流程。在 answerSdp 中可能包含多条 ICE candidates（候选服务器）信息，此时 WebRTC 便会分别和这些 candidates 建立连接，然后选出其中最优的那条连接作为配对结果进行通话。


### 6. WebRTC 使用 RTP 协议传输音视频 (Data Transfer & Control)

RTP 协议分为两种子协议，分别是 RTP Data Transfer Protocol 和 RTP Control Protocol。前者顾名思义，是用来传输实时数据的；后者则是我们常说的 RTCP 协议，可以提供实时传输过程中的统计信息（如网络延迟、丢包率等），WebRTC 正是根据这些信息处理丢包。

### 7. 视频推流过程(采集 / 渲染 / 编码 / 发送 / 滤镜)
VideoCapturer 

帧数据接下来会被以广播的形式发送给各个订阅者，也就是说后续的渲染、编码（和发送）过程是并行处理的。

渲染: VideoTrack.addSink(sink) 时，实际上是添加了一个 VideoBroadcaster 的订阅者。当有可用的帧数据时，VideoBroadcaster 便会回调 VideoSink.onFrame(frame) 

编码 : webrtc::VideoEncoder

Video show: 
– 3 different devices connect to one server app.
– app runs 3 separated Vulkan engines and transmits video streams to each client.

Key features:
– Vulkan headless (offscreen) render, optimized for Cloud rendering.
– WebRTC stack powered by native LibWebRTC.
– Hardware video encoders: NVidia NVEnc GPU, Intel QSV CPU.
– separated Docker containers for Render app and Signaling server.
– UDP data-channel for transmitting users events like mouse move and metrics.

Use cases:
– Cloud gaming.
– Cloud high loaded UI and animation.

Used libraries and frameworks:
– LibWebRTC: https://webrtc.googlesource.com/src/
– Vulkan SDK: https://vulkan.lunarg.com/sdk/home
– NVIDIA Video Codec SDK: https://developer.nvidia.com/nvidia-v...
– Intel Media SDK: https://www.intel.com/content/www/us/...
– Skia 2D Graphics Library: https://skia.org/
– ImGUI: https://github.com/ocornut/imgui
– Abseil library: https://abseil.io
– FMT library: https://fmt.dev
