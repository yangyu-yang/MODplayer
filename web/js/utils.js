// Utility functions
class Utils {
    // Format file size
    static formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    // Format duration
    static formatDuration(seconds) {
        if (seconds < 60) {
            return seconds + 's';
        } else if (seconds < 3600) {
            const mins = Math.floor(seconds / 60);
            const secs = Math.floor(seconds % 60);
            return mins + ':' + secs.toString().padStart(2, '0');
        } else {
            const hours = Math.floor(seconds / 3600);
            const mins = Math.floor((seconds % 3600) / 60);
            const secs = Math.floor(seconds % 60);
            return hours + ':' + mins.toString().padStart(2, '0') + ':' + secs.toString().padStart(2, '0');
        }
    }

    // Format date
    static formatDate(dateString) {
        const date = new Date(dateString);
        return date.toLocaleDateString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        });
    }

    // Get file extension
    static getFileExtension(filename) {
        return filename.split('.').pop().toUpperCase();
    }

    // Get file icon
    static getFileIcon(format) {
        const icons = {
            'mp4': 'fa-file-video',
            'mkv': 'fa-file-video',
            'avi': 'fa-file-video',
            'mov': 'fa-file-video',
            'mp3': 'fa-file-audio',
            'wav': 'fa-file-audio',
            'flac': 'fa-file-audio',
            'aac': 'fa-file-audio',
            'jpg': 'fa-file-image',
            'jpeg': 'fa-file-image',
            'png': 'fa-file-image',
            'gif': 'fa-file-image',
            'pdf': 'fa-file-pdf',
            'doc': 'fa-file-word',
            'docx': 'fa-file-word',
            'txt': 'fa-file-alt',
            'zip': 'fa-file-archive',
            'rar': 'fa-file-archive'
        };
        return icons[format?.toLowerCase()] || 'fa-file';
    }

    // Debounce function
    static debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }

    // Show toast notification
    static showToast(type, message, title = '', duration = 5000) {
        const container = document.getElementById('toast-container');
        if (!container) return;

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        
        const iconMap = {
            'success': 'fa-check-circle',
            'error': 'fa-exclamation-circle',
            'warning': 'fa-exclamation-triangle',
            'info': 'fa-info-circle'
        };

        const titleMap = {
            'success': 'Success',
            'error': 'Error',
            'warning': 'Warning',
            'info': 'Info'
        };

        toast.innerHTML = `
            <i class="fas ${iconMap[type] || 'fa-info-circle'}"></i>
            <div class="toast-content">
                <div class="toast-title">${title || titleMap[type]}</div>
                <div class="toast-message">${message}</div>
            </div>
            <button class="toast-close">&times;</button>
        `;

        container.appendChild(toast);

        // Add click handler to close button
        toast.querySelector('.toast-close').onclick = () => {
            toast.remove();
        };

        // Auto remove after duration
        if (duration > 0) {
            setTimeout(() => {
                if (toast.parentNode) {
                    toast.remove();
                }
            }, duration);
        }

        return toast;
    }

    // Format bitrate
    static formatBitrate(bps) {
        if (bps < 1000) return bps + ' bps';
        if (bps < 1000000) return (bps / 1000).toFixed(0) + ' kbps';
        return (bps / 1000000).toFixed(1) + ' Mbps';
    }

    // Escape HTML
    static escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    // Validate media file
    static isValidMediaFile(file) {
        const validExtensions = [
            '.mp4', '.mkv', '.avi', '.mov', '.flv', '.webm',
            '.mp3', '.wav', '.flac', '.aac', '.ogg', '.m4a'
        ];
        const extension = '.' + (file.filename || '').split('.').pop().toLowerCase();
        return validExtensions.includes(extension);
    }
}