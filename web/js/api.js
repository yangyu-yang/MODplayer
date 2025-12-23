// API service
class ApiService {
    constructor(baseUrl = '') {
        this.baseUrl = baseUrl;
    }

    async request(endpoint, options = {}) {
        const url = endpoint.startsWith('http') ? endpoint : `${this.baseUrl}${endpoint}`;
        
        const defaultOptions = {
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            ...options
        };

        try {
            const response = await fetch(url, defaultOptions);
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const contentType = response.headers.get('content-type');
            if (contentType && contentType.includes('application/json')) {
                return await response.json();
            } else {
                return await response.text();
            }
        } catch (error) {
            console.error('API request failed:', error);
            throw error;
        }
    }

    // Server status
    async getServerStatus() {
        return this.request('/api/status');
    }

    // Get all media files
    async getMediaList() {
        return this.request('/api/media/list');
    }

    // Get specific media file
    async getMediaById(id) {
        return this.request(`/api/media/${id}`);
    }

    // Rescan media library
    async rescanLibrary() {
        return this.request('/api/media/scan');
    }

    // Create playback session
    async createSession(mediaId, filename = '') {
        const params = new URLSearchParams();
        if (mediaId) params.append('media_id', mediaId);
        if (filename) params.append('filename', filename);
        
        return this.request(`/api/session/create?${params.toString()}`);
    }

    // Get media statistics
    async getMediaStats() {
        const media = await this.getMediaList();
        const stats = {
            totalFiles: 0,
            totalSize: 0,
            formats: new Set(),
            durations: []
        };

        if (media.media_files) {
            stats.totalFiles = media.media_files.length;
            
            media.media_files.forEach(file => {
                stats.totalSize += parseInt(file.size) || 0;
                if (file.format) stats.formats.add(file.format.toLowerCase());
                if (file.duration) stats.durations.push(parseFloat(file.duration));
            });
        }

        return stats;
    }
}

// Create global API instance
const api = new ApiService();