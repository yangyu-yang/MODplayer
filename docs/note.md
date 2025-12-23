## 目录结构

```shell
MODplayer/
├── server/                 # C++ 服务端
├── client/                 # Web 客户端
├── shared/                 # 共享代码和协议定义
├── docs/                   # 项目文档
├── scripts/                # 构建和部署脚本
└── README.md               # 项目说明


server/
├── src/                    # 源代码目录
│   ├── core/              # 核心基础设施
│   │   ├── logging/       # 日志系统
│   │   ├── utils/         # 工具函数
│   │   ├── config/        # 配置管理
│   │   └── threading/     # 线程管理
│   ├── media/             # 媒体处理核心
│   │   ├── decoder/       # 音视频解码
│   │   ├── encoder/       # 音视频编码
│   │   ├── effects/       # 音效处理引擎
│   │   ├── filters/       # 音视频滤镜
│   │   └── format/        # 格式处理
│   ├── streaming/         # 流媒体输出
│   │   ├── hls/           # HLS 输出模块
│   │   ├── http_flv/      # HTTP-FLV 输出模块
│   │   └── output_manager.cpp
│   ├── session/           # 会话管理
│   │   ├── session_manager.cpp
│   │   ├── session.cpp
│   │   └── session_pool.cpp
│   ├── api/               # API 接口
│   │   ├── http/          # HTTP REST API
│   │   ├── websocket/     # WebSocket 实时控制
│   │   └── middleware/    # 中间件
│   ├── storage/           # 存储管理
│   │   ├── cache/         # 缓存系统
│   │   └── media_library/ # 媒体库管理
│   └── main.cpp           # 程序入口
├── include/               # 头文件目录
│   └── (与src相同的结构)
├── third_party/           # 第三方依赖
│   ├── ffmpeg/            # FFmpeg 头文件和库
│   ├── crow/              # Crow HTTP 库
│   ├── websocketpp/       # WebSocketPP 库
│   └── json/              # nlohmann_json
├── tests/                 # 测试代码
│   ├── unit_tests/        # 单元测试
│   ├── integration_tests/ # 集成测试
│   └── performance_tests/ # 性能测试
├── build/                 # 构建输出目录
├── config/                # 配置文件
│   ├── config.yaml        # 主配置文件
│   ├── log_config.yaml    # 日志配置
│   └── nginx.conf         # Nginx 配置示例
└── resources/             # 资源文件
    ├── web/               # Web 客户端静态文件
    └── media/             # 测试媒体文件
    

client/
├── public/                # 静态资源
│   ├── index.html         # 主页面
│   ├── favicon.ico        # 网站图标
│   └── assets/           # 静态资源
│       ├── images/        # 图片资源
│       ├── fonts/         # 字体文件
│       └── styles/        # 全局样式
├── src/                   # 源代码
│   ├── main.js            # 应用入口
│   ├── App.vue            # 根组件
│   ├── api/               # API 接口
│   │   ├── http.js        # HTTP 请求封装
│   │   ├── websocket.js   # WebSocket 封装
│   │   └── media.js       # 媒体相关 API
│   ├── components/        # 通用组件
│   │   ├── ui/           # 基础 UI 组件
│   │   │   ├── Button.vue
│   │   │   ├── Slider.vue
│   │   │   └── Modal.vue
│   │   └── media/        # 媒体相关组件
│   │       ├── VideoPlayer.vue      # 视频播放器
│   │       ├── AudioVisualizer.vue  # 音频可视化
│   │       └── QualitySelector.vue  # 画质选择器
│   ├── views/             # 页面组件
│   │   ├── Home.vue       # 首页
│   │   ├── Library.vue    # 媒体库
│   │   ├── Player.vue     # 播放器页面
│   │   └── Settings.vue   # 设置页面
│   ├── stores/            # 状态管理 (Pinia)
│   │   ├── index.js       # 主 store
│   │   ├── session.js     # 会话状态
│   │   ├── player.js      # 播放器状态
│   │   └── effects.js     # 音效状态
│   ├── composables/       # Vue 组合式函数
│   │   ├── useWebSocket.js
│   │   ├── useMediaSession.js
│   │   └── useAudioEffects.js
│   ├── utils/             # 工具函数
│   │   ├── time.js        # 时间格式化
│   │   ├── storage.js     # 本地存储
│   │   └── validators.js  # 数据验证
│   └── router/            # 路由配置
│       └── index.js
├── tests/                 # 测试文件
│   ├── unit/              # 单元测试
│   └── e2e/               # 端到端测试
├── dist/                  # 构建输出
└── package.json           # 项目配置



shared/
├── protocol/              # 通信协议定义
│   ├── api_types.h       # C++ API 数据类型
│   ├── api_types.ts      # TypeScript API 类型
│   ├── websocket_msg.h   # WebSocket 消息格式 (C++)
│   └── websocket_msg.ts  # WebSocket 消息格式 (TS)
└── config/               # 共享配置
    ├── constants.h       # C++ 常量定义
    └── constants.ts      # TypeScript 常量定义
```





## 构建过程



#### 依赖安装

```shell
sudo apt  install cmake
sudo apt install npm

sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libavfilter-dev \
    libswscale-dev
sudo apt install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libavfilter-dev \
    libswscale-dev \
    libavdevice-dev \
    ffmpeg
# 验证安装
ffmpeg -version
pkg-config --cflags libavcodec libavformat libavutil
```

#### Build & Run

```shell
./build.sh

# 如果编译成功，运行程序
./start-server.sh

# 打开浏览器访问
#    http://localhost:8080
```


