// Main application
class MediaApp {
    constructor() {
        this.currentPage = 'media-list';
        this.mediaList = [];
        this.filteredMedia = [];
        this.init();
    }

    async init() {
        this.bindEvents();
        this.loadSettings();
        await this.loadMedia();
        this.setupAutoRefresh();
    }

    bindEvents() {
        // Navigation
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', (e) => {
                e.preventDefault();
                const page = item.dataset.page;
                this.showPage(page);
            });
        });

        // Search
        const searchInput = document.getElementById('search-input');
        if (searchInput) {
            searchInput.addEventListener('input', Utils.debounce((e) => {
                this.filterMedia(e.target.value);
            }, 300));
        }

        // Refresh button
        const refreshBtn = document.getElementById('refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.loadMedia());
        }

        // Rescan button
        const rescanBtn = document.getElementById('rescan-btn');
        if (rescanBtn) {
            rescanBtn.addEventListener('click', () => this.rescanLibrary());
        }

        // Empty state rescan button
        const emptyRescanBtn = document.getElementById('empty-rescan-btn');
        if (emptyRescanBtn) {
            emptyRescanBtn.addEventListener('click', () => this.rescanLibrary());
        }

        // Modal close
        document.querySelectorAll('.modal-close').forEach(btn => {
            btn.addEventListener('click', () => {
                this.closeModal();
            });
        });

        // Volume slider
        const volumeSlider = document.getElementById('default-volume');
        if (volumeSlider) {
            volumeSlider.addEventListener('input', (e) => {
                document.getElementById('volume-value').textContent = e.target.value + '%';
            });
        }

        // Save port button
        const savePortBtn = document.getElementById('save-port-btn');
        if (savePortBtn) {
            savePortBtn.addEventListener('click', () => {
                const port = document.getElementById('server-port').value;
                this.saveSetting('server_port', port);
                Utils.showToast('info', `Port changed to ${port}. Restart server to apply.`);
            });
        }
    }

    async loadMedia() {
        try {
            this.showLoading();
            
            const media = await api.getMediaList();
            this.mediaList = media.media_files || [];
            
            await this.updateStats();
            this.renderMediaGrid();
            
            Utils.showToast('success', 'Media library loaded successfully');
            
        } catch (error) {
            console.error('Failed to load media:', error);
            Utils.showToast('error', 'Failed to load media library');
        }
    }

    async updateStats() {
        const stats = await api.getMediaStats();
        
        // Update stats bar
        document.getElementById('total-files').textContent = stats.totalFiles;
        document.getElementById('total-size').textContent = Utils.formatFileSize(stats.totalSize);
        document.getElementById('total-formats').textContent = stats.formats.size;
        
        // Update server uptime
        const startTime = Date.now();
        setInterval(() => {
            const uptime = Math.floor((Date.now() - startTime) / 1000);
            document.getElementById('uptime').textContent = this.formatUptime(uptime);
        }, 1000);
    }

    formatUptime(seconds) {
        if (seconds < 60) return seconds + 's';
        if (seconds < 3600) return Math.floor(seconds / 60) + 'm ' + (seconds % 60) + 's';
        if (seconds < 86400) return Math.floor(seconds / 3600) + 'h ' + Math.floor((seconds % 3600) / 60) + 'm';
        return Math.floor(seconds / 86400) + 'd ' + Math.floor((seconds % 86400) / 3600) + 'h';
    }

    renderMediaGrid() {
        const grid = document.getElementById('media-grid');
        const emptyState = document.getElementById('empty-state');
        
        if (this.filteredMedia.length === 0) {
            grid.style.display = 'none';
            emptyState.style.display = 'block';
            return;
        }
        
        grid.style.display = 'grid';
        emptyState.style.display = 'none';
        
        grid.innerHTML = this.filteredMedia.map(media => this.createMediaCard(media)).join('');
        
        // Add click handlers to media cards
        grid.querySelectorAll('.media-item').forEach((card, index) => {
            card.addEventListener('click', () => {
                this.showMediaDetails(this.filteredMedia[index]);
            });
        });
    }

    createMediaCard(media) {
        const size = Utils.formatFileSize(parseInt(media.size) || 0);
        const duration = Utils.formatDuration(parseFloat(media.duration) || 0);
        const format = Utils.getFileExtension(media.filename || '');
        const icon = Utils.getFileIcon(media.format || format);
        const created = Utils.formatDate(media.created_time);
        
        return `
            <div class="media-item">
                <div class="media-thumbnail">
                    <i class="fas ${icon}"></i>
                    <span class="duration">${duration}</span>
                </div>
                <div class="media-info">
                    <h3 class="media-title" title="${Utils.escapeHtml(media.filename)}">
                        ${Utils.escapeHtml(media.filename)}
                    </h3>
                    <div class="media-meta">
                        <span>${size}</span>
                        <span>${created}</span>
                    </div>
                    <span class="media-format">${format}</span>
                </div>
            </div>
        `;
    }

    filterMedia(searchTerm) {
        if (!searchTerm) {
            this.filteredMedia = [...this.mediaList];
        } else {
            const term = searchTerm.toLowerCase();
            this.filteredMedia = this.mediaList.filter(media => 
                media.filename.toLowerCase().includes(term) ||
                media.format?.toLowerCase().includes(term)
            );
        }
        this.renderMediaGrid();
    }

    async rescanLibrary() {
        try {
            Utils.showToast('info', 'Rescanning media library...');
            
            const result = await api.rescanLibrary();
            
            if (result.success) {
                await this.loadMedia();
                Utils.showToast('success', 'Media library rescanned successfully');
            } else {
                Utils.showToast('error', 'Failed to rescan media library');
            }
        } catch (error) {
            console.error('Failed to rescan library:', error);
            Utils.showToast('error', 'Failed to rescan media library');
        }
    }

    async showMediaDetails(media) {
        try {
            const details = await api.getMediaById(media.id);
            this.renderMediaDetails(details);
            
            const modal = document.getElementById('media-details-modal');
            modal.classList.add('active');
            
            // Add play button handler
            document.getElementById('play-btn').onclick = () => {
                this.playMedia(media);
            };
        } catch (error) {
            console.error('Failed to load media details:', error);
            Utils.showToast('error', 'Failed to load media details');
        }
    }

    renderMediaDetails(media) {
        const container = document.getElementById('media-details-content');
        
        const size = Utils.formatFileSize(parseInt(media.size) || 0);
        const duration = Utils.formatDuration(parseFloat(media.duration) || 0);
        const bitrate = Utils.formatBitrate(parseInt(media.bitrate) || 0);
        const created = Utils.formatDate(media.created_time);
        const resolution = media.width && media.height ? 
            `${media.width} × ${media.height}` : 'Unknown';
        
        container.innerHTML = `
            <div class="media-details-header">
                <h4>${Utils.escapeHtml(media.filename)}</h4>
                <div class="media-format-badge">${Utils.escapeHtml(media.format || 'Unknown')}</div>
            </div>
            
            <div class="details-grid">
                <div class="detail-item">
                    <span class="detail-label">Size:</span>
                    <span class="detail-value">${size}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Duration:</span>
                    <span class="detail-value">${duration}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Resolution:</span>
                    <span class="detail-value">${resolution}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Bitrate:</span>
                    <span class="detail-value">${bitrate}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Video Codec:</span>
                    <span class="detail-value">${Utils.escapeHtml(media.video_codec || 'Unknown')}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Audio Codec:</span>
                    <span class="detail-value">${Utils.escapeHtml(media.audio_codec || 'Unknown')}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Sample Rate:</span>
                    <span class="detail-value">${media.audio_sample_rate ? media.audio_sample_rate + ' Hz' : 'Unknown'}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Created:</span>
                    <span class="detail-value">${created}</span>
                </div>
                <div class="detail-item">
                    <span class="detail-label">Path:</span>
                    <span class="detail-value" title="${Utils.escapeHtml(media.path)}">
                        ${Utils.escapeHtml(media.path)}
                    </span>
                </div>
            </div>
        `;
    }

    async playMedia(media) {
        try {
            const session = await api.createSession(media.id, media.filename);
            
            if (session.success) {
                Utils.showToast('success', 'Playback session created');
                this.showPlaybackWindow(session);
            } else {
                Utils.showToast('error', 'Failed to create playback session');
            }
        } catch (error) {
            console.error('Failed to create session:', error);
            Utils.showToast('error', 'Failed to create playback session');
        }
    }

    showPlaybackWindow(session) {
        const url = session.stream_url || `http://localhost:8080/stream/${session.session_id}`;
        window.open(url, '_blank');
    }

    closeModal() {
        document.querySelectorAll('.modal').forEach(modal => {
            modal.classList.remove('active');
        });
    }

    showPage(pageName) {
        // Update navigation
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.toggle('active', item.dataset.page === pageName);
        });
        
        // Show target page
        document.querySelectorAll('.page').forEach(page => {
            page.classList.toggle('active', page.id === `${pageName}-page`);
        });
        
        this.currentPage = pageName;
        
        // Load page-specific data
        if (pageName === 'media-list') {
            this.loadMedia();
        } else if (pageName === 'server-status') {
            this.loadServerStatus();
        }
    }

    async loadServerStatus() {
        try {
            const status = await api.getServerStatus();
            // Update status page with real data
            // We already update uptime in updateStats()
        } catch (error) {
            console.error('Failed to load server status:', error);
        }
    }

    showLoading() {
        const grid = document.getElementById('media-grid');
        if (grid) {
            grid.innerHTML = `
                <div class="loading-placeholder">
                    <i class="fas fa-spinner fa-spin"></i>
                    <p>Loading media files...</p>
                </div>
            `;
        }
    }

    loadSettings() {
        const port = this.getSetting('server_port', '8080');
        document.getElementById('server-port').value = port;
        
        const autoPlay = this.getSetting('auto_play', 'true') === 'true';
        document.getElementById('auto-play').checked = autoPlay;
        
        const loopPlayback = this.getSetting('loop_playback', 'false') === 'true';
        document.getElementById('loop-playback').checked = loopPlayback;
        
        const volume = this.getSetting('default_volume', '80');
        document.getElementById('default-volume').value = volume;
        document.getElementById('volume-value').textContent = volume + '%';
        
        // Set build date
        document.getElementById('build-date').textContent = new Date().toISOString().split('T')[0];
    }

    saveSetting(key, value) {
        localStorage.setItem(`media_server_${key}`, value);
    }

    getSetting(key, defaultValue) {
        return localStorage.getItem(`media_server_${key}`) || defaultValue;
    }

    setupAutoRefresh() {
        // Auto-refresh media list every 5 minutes
        setInterval(() => {
            if (this.currentPage === 'media-list') {
                this.loadMedia();
            }
        }, 5 * 60 * 1000);
    }
}

// 在 app.js 中添加 HLS 播放功能
class HLSPlayer {
    constructor(videoElement, options = {}) {
        this.videoElement = videoElement;
        this.options = Object.assign({
            autoplay: true,
            controls: true,
            preload: 'auto',
            debug: false
        }, options);
        
        this.hls = null;
        this.currentStreamId = null;
        
        // 检查 HLS.js 是否可用
        if (typeof Hls === 'undefined') {
            console.error('Hls.js is not loaded');
            return;
        }
    }
    
    // 加载 HLS 流
    loadStream(streamId) {
        if (!streamId) {
            console.error('Stream ID is required');
            return false;
        }
        
        this.currentStreamId = streamId;
        const hlsUrl = `http://${window.location.hostname}:8080/hls/${streamId}/playlist.m3u8`;
        
        console.log('Loading HLS stream:', hlsUrl);
        
        if (Hls.isSupported()) {
            this.setupHlsJs(hlsUrl);
        } else if (this.videoElement.canPlayType('application/vnd.apple.mpegurl')) {
            // Safari 原生支持
            this.setupNativeHLS(hlsUrl);
        } else {
            console.error('HLS is not supported in this browser');
            return false;
        }
        
        return true;
    }
    
    // 设置 Hls.js
    setupHlsJs(hlsUrl) {
        if (this.hls) {
            this.hls.destroy();
        }
        
        this.hls = new Hls({
            enableWorker: true,
            lowLatencyMode: true,
            backBufferLength: 90
        });
        
        this.hls.loadSource(hlsUrl);
        this.hls.attachMedia(this.videoElement);
        
        // 事件监听
        this.hls.on(Hls.Events.MANIFEST_PARSED, () => {
            console.log('HLS manifest loaded');
            if (this.options.autoplay) {
                this.videoElement.play().catch(e => {
                    console.log('Autoplay failed:', e);
                });
            }
        });
        
        this.hls.on(Hls.Events.ERROR, (event, data) => {
            console.error('HLS error:', data);
        });
    }
    
    // 设置原生 HLS (Safari)
    setupNativeHLS(hlsUrl) {
        this.videoElement.src = hlsUrl;
        if (this.options.autoplay) {
            this.videoElement.play().catch(e => {
                console.log('Autoplay failed:', e);
            });
        }
    }
    
    // 播放
    play() {
        return this.videoElement.play();
    }
    
    // 暂停
    pause() {
        this.videoElement.pause();
    }
    
    // 停止
    stop() {
        this.pause();
        this.videoElement.currentTime = 0;
        if (this.hls) {
            this.hls.destroy();
            this.hls = null;
        }
    }
    
    // 销毁
    destroy() {
        this.stop();
        this.videoElement.src = '';
    }
}

// 更新现有的播放功能
async function playMedia(media) {
    try {
        // 1. 创建 HLS 流
        const createResponse = await api.request('GET', `/api/hls/create?media_id=${encodeURIComponent(media.id)}`);
        
        if (!createResponse.success) {
            Utils.showToast('error', 'Failed to create HLS stream');
            return;
        }
        
        const streamId = createResponse.stream_id;
        
        // 2. 等待转码完成
        Utils.showToast('info', 'Preparing stream...');
        
        let isReady = false;
        for (let i = 0; i < 30; i++) {  // 最多等待 60 秒
            await Utils.sleep(2000);
            
            const statusResponse = await api.request('GET', `/api/hls/status/${streamId}`);
            
            if (statusResponse.status === 'ready') {
                isReady = true;
                break;
            } else if (statusResponse.status === 'error') {
                Utils.showToast('error', `Stream error: ${statusResponse.error_message}`);
                return;
            }
        }
        
        if (!isReady) {
            Utils.showToast('warning', 'Stream preparation taking longer than expected');
        }
        
        // 3. 打开播放器页面
        const playerUrl = `/player.html?stream_id=${encodeURIComponent(streamId)}&media_id=${encodeURIComponent(media.id)}&filename=${encodeURIComponent(media.filename)}`;
        window.open(playerUrl, '_blank');
        
    } catch (error) {
        console.error('Failed to play media:', error);
        Utils.showToast('error', 'Failed to play media');
    }
}

// Initialize app when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.app = new MediaApp();
});