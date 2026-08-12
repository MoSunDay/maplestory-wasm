/**
 * LazyFS WebSocket lifecycle, retry, request replay, and keep-alive support.
 * Loaded before lazyfs.js and mixed into the LazyFS object.
 */
var LazyFSConnection = {
	ws: null,
	wsConnected: false,
	wsConnecting: false,
	connectionPromise: null,
	connectionGeneration: 0,
	terminalConnectionFailure: false,
	shuttingDown: false,
	lifecycleHooksInstalled: false,
	keepAliveTimer: null,
	KEEP_ALIVE_INTERVAL_MS: 25000,
	RECONNECT_DELAYS_MS: [1000, 2000, 4000],
	pendingRequests: new Map(),

	formatUrlHost: function (host) {
		const value = String(host);
		return value.includes(':') && !value.startsWith('[') ? `[${value}]` : value;
	},

	installLifecycleHooks: function () {
		if (this.lifecycleHooksInstalled || typeof window === 'undefined') {
			return;
		}
		this.lifecycleHooksInstalled = true;
		window.addEventListener('beforeunload', () => {
			this.shuttingDown = true;
			this.stopKeepAlive();
		});
	},

	delay: function (milliseconds) {
		return new Promise(resolve => setTimeout(resolve, milliseconds));
	},

	connect: function () {
		if (this.wsConnected && this.ws) {
			return Promise.resolve();
		}
		if (this.terminalConnectionFailure) {
			return Promise.reject(new Error('LazyFS connection is no longer recoverable'));
		}
		if (this.connectionPromise) {
			return this.connectionPromise;
		}

		if (!this.ASSETS_WS_URL) {
			const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
			const host = this.formatUrlHost(window.location.hostname);
			this.ASSETS_WS_URL = `${protocol}//${host}:8765`;
		}
		this.installLifecycleHooks();

		this.connectionPromise = this.connectWithRetry()
			.finally(() => {
				this.connectionPromise = null;
			});
		return this.connectionPromise;
	},

	connectWithRetry: async function () {
		let lastError = new Error('Unable to connect to assets server');
		for (let attempt = 0; attempt <= this.RECONNECT_DELAYS_MS.length; attempt++) {
			if (this.shuttingDown) {
				throw new Error('Page is unloading');
			}
			if (attempt > 0) {
				const delay = this.RECONNECT_DELAYS_MS[attempt - 1];
				console.warn(`[LazyFS] Reconnecting in ${delay}ms (${attempt}/${this.RECONNECT_DELAYS_MS.length})`);
				await this.delay(delay);
			}

			try {
				await this.openSocket();
				return;
			} catch (error) {
				lastError = error instanceof Error ? error : new Error('WebSocket connection failed');
			}
		}

		this.failConnection(lastError);
		throw lastError;
	},

	openSocket: function () {
		console.log('[LazyFS] Connecting to assets server:', this.ASSETS_WS_URL);
		this.wsConnecting = true;

		return new Promise((resolve, reject) => {
			let opened = false;
			let settled = false;
			let socket;
			const rejectAttempt = (error) => {
				if (settled) {
					return;
				}
				settled = true;
				this.wsConnecting = false;
				reject(error instanceof Error ? error : new Error('WebSocket connection failed'));
			};
			try {
				socket = new WebSocket(this.ASSETS_WS_URL);
				this.ws = socket;
				socket.binaryType = 'arraybuffer';

				socket.onopen = () => {
					if (settled) {
						socket.close();
						return;
					}
					console.log('[LazyFS] WebSocket connected');
					opened = true;
					settled = true;
					this.wsConnected = true;
					this.wsConnecting = false;
					this.connectionGeneration++;
					this.startKeepAlive();
					this.replayPendingRequests();
					resolve();
				};

				socket.onmessage = event => this.handleMessage(event.data);
				socket.onerror = (error) => {
					console.error('[LazyFS] WebSocket error:', error);
					if (!opened) {
						rejectAttempt(new Error('WebSocket connection error'));
						try {
							socket.close();
						} catch (_) {
							// Closing after a failed handshake is best-effort.
						}
					}
				};

				socket.onclose = () => {
					console.log('[LazyFS] WebSocket disconnected');
					if (this.ws === socket) {
						this.wsConnected = false;
						this.wsConnecting = false;
						this.ws = null;
						this.stopKeepAlive();
					}
					if (!opened) {
						rejectAttempt(new Error('WebSocket closed before connecting'));
					} else if (!this.shuttingDown && !this.terminalConnectionFailure) {
						Promise.resolve().then(() => this.connect()).catch(() => {});
					}
				};
			} catch (error) {
				console.error('[LazyFS] Failed to create WebSocket:', error);
				rejectAttempt(error);
			}
		});
	},

	startKeepAlive: function () {
		this.stopKeepAlive();
		this.keepAliveTimer = setInterval(() => {
			if (!this.wsConnected || !this.ws || this.files.size === 0) {
				return;
			}
			const filepath = this.files.keys().next().value;
			try {
				// Browsers cannot originate Ping frames; get_size is read-only.
				this.ws.send(JSON.stringify({ type: 'get_size', file: filepath }));
			} catch (error) {
				console.error('[LazyFS] Keep-alive failed:', error);
				this.ws.close();
			}
		}, this.KEEP_ALIVE_INTERVAL_MS);
	},

	stopKeepAlive: function () {
		if (this.keepAliveTimer !== null) {
			clearInterval(this.keepAliveTimer);
			this.keepAliveTimer = null;
		}
	},

	request: function (key, message, timeoutMs) {
		if (this.terminalConnectionFailure) {
			return Promise.reject(new Error('LazyFS connection is no longer recoverable'));
		}
		const existing = this.pendingRequests.get(key);
		if (existing) {
			return existing.promise;
		}
		let entry;
		const promise = new Promise((resolve, reject) => {
			entry = { resolve, reject, message, lastGeneration: 0, timer: null, promise: null };
			entry.timer = setTimeout(() => {
				if (this.pendingRequests.get(key) === entry) {
					this.pendingRequests.delete(key);
					reject(new Error(`LazyFS request timed out: ${key}`));
				}
			}, timeoutMs);
		});
		entry.promise = promise;
		this.pendingRequests.set(key, entry);
		this.sendPendingRequest(key);
		return promise;
	},

	sendPendingRequest: function (key) {
		this.connect().then(() => {
			const pending = this.pendingRequests.get(key);
			if (!pending || pending.lastGeneration === this.connectionGeneration || !this.ws) {
				return;
			}
			this.ws.send(JSON.stringify(pending.message));
			pending.lastGeneration = this.connectionGeneration;
		}).catch(() => {
			// connectWithRetry rejects pending requests on terminal failure.
		});
	},

	replayPendingRequests: function () {
		for (const key of this.pendingRequests.keys()) {
			this.sendPendingRequest(key);
		}
	},

	resolvePendingRequest: function (key, value) {
		const pending = this.pendingRequests.get(key);
		if (!pending) {
			return;
		}
		this.pendingRequests.delete(key);
		clearTimeout(pending.timer);
		pending.resolve(value);
	},

	failConnection: function (error) {
		if (this.terminalConnectionFailure || this.shuttingDown) {
			return;
		}
		this.terminalConnectionFailure = true;
		this.wsConnected = false;
		this.wsConnecting = false;
		this.stopKeepAlive();
		for (const pending of this.pendingRequests.values()) {
			clearTimeout(pending.timer);
			pending.reject(error);
		}
		this.pendingRequests.clear();
		for (const task of this.prefetchQueue) {
			task.rejectRequest(error);
		}
		this.prefetchQueue = [];
	},

	resetConnectionFailure: function () {
		if (this.shuttingDown) {
			return false;
		}
		this.terminalConnectionFailure = false;
		return true;
	},
};
