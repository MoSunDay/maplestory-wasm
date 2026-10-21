'use strict';
(function () {
  var hasLocalAssets = true;
  var busy = false;

  var serverInput = document.getElementById('server-url');
  var assetsInput = document.getElementById('assets-url');
  var saveButton = document.getElementById('save');
  var quitButton = document.getElementById('quit');
  var statusBox = document.getElementById('status');

  var showStatus = function (text, kind) {
    statusBox.textContent = text;
    statusBox.className = 'status ' + kind;
  };
  var hideStatus = function () {
    statusBox.textContent = '';
    statusBox.className = 'status hidden';
  };
  var missingAssets = function () {
    return !hasLocalAssets && assetsInput.value.trim() === '';
  };
  var refresh = function () {
    if (busy) return;
    if (missingAssets()) {
      saveButton.disabled = true;
      showStatus('未检测到本地游戏资源：请填写资源服务器 URL', 'warn');
    } else {
      saveButton.disabled = false;
      hideStatus();
    }
  };
  var setBusyUi = function (flag) {
    busy = flag;
    saveButton.disabled = flag;
    quitButton.disabled = flag;
  };

  window.desktopBridge.onLaunchResult(function (result) {
    if (!result || result.ok) {
      window.close();
      return;
    }
    setBusyUi(false);
    if (missingAssets()) {
      saveButton.disabled = true;
    }
    showStatus(result.error || '启动失败', 'error');
  });

  saveButton.addEventListener('click', function () {
    if (busy || missingAssets()) return;
    setBusyUi(true);
    showStatus('正在启动本地服务…', 'info');
    window.desktopBridge
      .save({ serverUrl: serverInput.value, assetsUrl: assetsInput.value })
      .then(function (r) {
        if (r && r.ok) return;
        setBusyUi(false);
        if (missingAssets()) {
          saveButton.disabled = true;
        }
        showStatus((r && r.error) || '保存失败', 'error');
      })
      .catch(function (err) {
        setBusyUi(false);
        showStatus(String((err && err.message) || err), 'error');
      });
  });

  quitButton.addEventListener('click', function () {
    window.close();
  });

  serverInput.addEventListener('input', refresh);
  assetsInput.addEventListener('input', refresh);

  window.desktopBridge
    .getState()
    .then(function (st) {
      hasLocalAssets = Boolean(st && st.hasLocalAssets);
      var settings = (st && st.settings) || {};
      serverInput.value = settings.serverUrl || '';
      assetsInput.value = settings.assetsUrl || '';
    })
    .catch(function () {})
    .then(function () {
      refresh();
      var reason = new URLSearchParams(window.location.search).get('reason');
      if (reason && !missingAssets()) {
        showStatus(reason, 'error');
      }
    });
})();
