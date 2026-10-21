'use strict';

const { protocol } = require('electron');

const CONFIG_SCHEME = 'msconfig';
const CONFIG_URL = `${CONFIG_SCHEME}://local/config.json`;

const registerConfigScheme = () => {
  protocol.registerSchemesAsPrivileged([
    {
      scheme: CONFIG_SCHEME,
      privileges: { standard: true, supportFetchAPI: true, corsEnabled: true, stream: true },
    },
  ]);
};

const activateConfigScheme = (getConfig) => {
  protocol.handle(CONFIG_SCHEME, () =>
    new Response(JSON.stringify(getConfig()), {
      headers: {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*',
      },
    })
  );
};

const attachConfigRedirect = (targetSession) => {
  targetSession.webRequest.onBeforeRequest(
    { urls: ['http://127.0.0.1:*/web/config.json', 'http://localhost:*/web/config.json'] },
    (details, callback) => {
      callback(details.method === 'GET' ? { redirectURL: CONFIG_URL } : { cancel: false });
    }
  );
};

module.exports = {
  CONFIG_SCHEME,
  CONFIG_URL,
  registerConfigScheme,
  activateConfigScheme,
  attachConfigRedirect,
};
